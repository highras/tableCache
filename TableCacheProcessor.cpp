#include <stdlib.h>
#include <stdexcept>
#include "FPLog.h"
#include "Setting.h"
#include "FPZKClient.h"
#include "TableCacheErrorInfo.h"
#include "TableCacheProcessor.h"
#include "TableCacheCallbacks.inc.cpp"

FPZKClientPtr gc_fpzk;

void enableFPZK()
{
	std::string serverList = Setting::getString("TableCache.cluster.FPZK.serverList");
	std::string projectName = Setting::getString("TableCache.cluster.FPZK.projectName");
	std::string projectToken = Setting::getString("TableCache.cluster.FPZK.projectToken");
	std::string version = Setting::getString("TableCache.cluster.FPZK.selfInfo.version");

	if (serverList.empty())
		return;

	gc_fpzk = FPZKClient::create(serverList, projectName, projectToken);
	gc_fpzk->registerService("", version);
}

void TableCacheProcessor::configure()
{
	_clusterNotifier = ClusterNotifier::create();
	std::string dbproxyEndpoint = Setting::getString("TableCache.dbproxy.endpoint");
	_dbproxy = TCPClient::createClient(dbproxyEndpoint);
	if (!_dbproxy)
	{
		LOG_FATAL("Invalid dbproxy endpoint: %s", dbproxyEndpoint.c_str());
		exit(1);
	}

	int timeout = Setting::getInt("TableCache.dbproxy.questTimeout", 15);
	_dbproxy->setQuestTimeout(timeout);

	//-- _cachaMap
	int64_t hash_size = Setting::getInt("TableCache.cache.hashSize", 1024*1024*64);
	if (hash_size < 1024)
		hash_size = 1024;
	_cachaMap.reset(new CacheMap(hash_size));

	enableFPZK();
}

bool TableCacheProcessor::loadTableScheme(const std::string& tableName, std::vector<std::vector<std::string>>& scheme)
{
	FPQWriter qw(2, "query");
	qw.param("hintId", 0);
	qw.param("sql", std::string("desc ").append(tableName));
	FPQuestPtr quest = qw.take();
	
	FPAnswerPtr answer = _dbproxy->sendQuest(quest);
	if (!answer)
	{
		LOG_ERROR("Query scheme for table %s failed.", tableName.c_str());
		return false;
	}

	FPAReader ar(answer);
	if (ar.status())
	{
		LOG_ERROR("Query scheme for table %s failed.", tableName.c_str());
		return false;
	}
	
	scheme = ar.want("rows", std::vector<std::vector<std::string>>());
	return true;
}

std::string TableCacheProcessor::loadSplitColumn(const std::string& tableName)
{
	FPQWriter qw(1, "splitInfo");
	qw.param("tableName", tableName);
	FPQuestPtr quest = qw.take();
	
	FPAnswerPtr answer = _dbproxy->sendQuest(quest);
	if (!answer)
	{
		LOG_ERROR("Query splitInfo for table %s failed.", tableName.c_str());
		return std::string();
	}

	FPAReader ar(answer);
	if (ar.status())
	{
		LOG_ERROR("Query splitInfo for table %s failed.", tableName.c_str());
		return std::string();
	}

	return ar.wantString("splitHint");
}

TABLEPtr TableCacheProcessor::loadTableInfo(const std::string& tableName)
{
	std::vector<std::vector<std::string>> scheme;
	if (!loadTableScheme(tableName, scheme))
	{
		if (!loadTableScheme(tableName, scheme))
			return nullptr;
	}

	std::string splitHint = loadSplitColumn(tableName);
	if (splitHint.empty())
	{
		LOG_FATAL("Table %s has invalid configure (empty value) for hint_field", tableName.c_str());
		return nullptr;
	}
	return std::make_shared<TABLE>(tableName, splitHint, scheme);
}

TABLEPtr TableCacheProcessor::getTableScheme(const std::string& tableName)
{
	{
		RKeeper rlock(&_rwlocker);
		auto it = _tableInfo.find(tableName);
		if (it != _tableInfo.end())
			return it->second;
	}

	TABLEPtr scheme = loadTableInfo(tableName);
	if (scheme)
	{
		WKeeper wlock(&_rwlocker);
		auto it = _tableInfo.find(tableName);
		if (it != _tableInfo.end())
			return it->second;
		else
			_tableInfo[tableName] = scheme;
	}
	return scheme;
}

void TableCacheProcessor::cleanCache(const std::string& tableName, int64_t hintId)
{	
	_clusterNotifier->invalidate(tableName, hintId);

	TableKey key;
	key.hintId = hintId;
	key.tableName = tableName;

	WKeeper wlock(&_rwlocker);
	CacheMap::node_type* node = _cachaMap->find(key);
	if (node)
	{
		_tableDataIndexes[tableName].erase(node);
		if (_tableDataIndexes[tableName].empty())
			_tableDataIndexes.erase(tableName);

		_cachaMap->remove_node(node);
	}
}

void TableCacheProcessor::addRows(TABLEPtr orginalScheme, const std::vector<std::vector<std::string>>& data)
{
	std::string tableName = orginalScheme->get_table_name();
	std::string keyCloumn = orginalScheme->get_key_name();
	std::vector<uint16_t> index = orginalScheme->get_fields_index(std::vector<std::string>{keyCloumn});

	bool stringKey = orginalScheme->isStringField(keyCloumn);
	std::vector<int64_t> hintIds;
	hintIds.reserve(data.size());

	for (size_t i = 0; i < data.size(); i++)
	{
		int64_t hintId;
		if (stringKey)
			hintId = (int64_t)jenkins_hash(data[i][index[0]].c_str(), data[i][index[0]].length(), 0);
		else
			hintId = atoll(data[i][index[0]].c_str());

		hintIds.push_back(hintId);
	}

	WKeeper wlock(&_rwlocker);
	auto it = _tableInfo.find(tableName);
	if (it == _tableInfo.end())
		return;		//-- Table invalidated.

	TABLEPtr scheme = it->second;
	if (scheme.get() != orginalScheme.get())
		return;		//-- Table invalidated.

	for (size_t i = 0; i < data.size(); i++)
	{
		TableKey key;
		key.hintId = hintIds[i];
		key.tableName = tableName;

		CacheMap::node_type* node = _cachaMap->find(key);
		if (node)
			continue;

		ROWPtr rowptr = std::make_shared<ROW>(data[i]);
		node = _cachaMap->insert(key, rowptr);
		if (node)
			_tableDataIndexes[tableName].insert(node);
	}
}

FPAnswerPtr TableCacheProcessor::modify(const FPReaderPtr args, const FPQuestPtr quest, const ConnectionInfo& ci)
{
	std::string tableName = args->wantString("table");
	TABLEPtr scheme = getTableScheme(tableName);
	if (!scheme)
		return ErrorInfo::tableNotFoundAnswer(quest);

	std::map<std::string, std::string> kvpairs = args->want("values", std::map<std::string, std::string>());

	int64_t hintId;
	std::string hintStr;
	std::string keyName = scheme->get_key_name();
	bool strKey = scheme->isStringField(keyName);
	if (!strKey)
		hintId = args->wantInt("hintId");
	else
	{
		hintStr = args->wantString("hintId");
		hintId = (int64_t)jenkins_hash(hintStr.c_str(), hintStr.length(), 0);
	}

	if (kvpairs.find(keyName) != kvpairs.end())
		return ErrorInfo::disabledAnswer(quest, std::string("Hint/split field ").append(keyName).append(" will be processed by inferface function, it cannot be set in values parameter.").c_str());

	//-- build sql
	std::vector<std::string> fields;
	std::vector<std::string> values;
	std::vector<std::string> placeholds;

	std::string sql("insert into ");
	sql.append(tableName).append(" (").append(keyName);
	if (!strKey)
		values.push_back(std::to_string(hintId));
	else
		values.push_back(hintStr);

	for (auto& kvpair: kvpairs)
	{
		sql.append(",");

		sql.append(kvpair.first);
		fields.push_back(kvpair.first);
		values.push_back(kvpair.second);

		if (scheme->isStringField(kvpair.first))
			placeholds.push_back("'?'");
		else
			placeholds.push_back("?");
	}
	sql.append(") values (");
	if (!strKey)
		sql.append("?");
	else
		sql.append("'?'");

	for (size_t i = 0; i < placeholds.size(); i++)
	{
		sql.append(",");
		sql.append(placeholds[i]);
	}
	sql.append(") ON DUPLICATE KEY UPDATE ");

	size_t itemCount = placeholds.size();
	for (size_t i = 0; i < itemCount; i++)
	{
		if (i)
			sql.append(",");

		sql.append(fields[i]).append("=").append(placeholds[i]);
		values.push_back(values[i+1]);
	}

	//-- additional check for SQL Injection
	try {
		scheme->get_fields_index(fields);
	}
	catch (const std::out_of_range& oor) {
		return ErrorInfo::disabledAnswer(quest, "Found invalid filed(s) in inputted params.");
	}

	//-- build quest
	FPQuestPtr dbQuest;
	if (strKey)
	{
		FPQWriter qw(4, "sQuery");
		qw.param("hintIds", std::vector<std::string>{hintStr});
		qw.param("sql", sql);
		qw.param("params", values);
		qw.param("tableName", tableName);
		dbQuest = qw.take();
	}
	else
	{
		FPQWriter qw(4, "query");
		qw.param("hintId", hintId);
		qw.param("sql", sql);
		qw.param("params", values);
		qw.param("tableName", tableName);
		dbQuest = qw.take();
	}

	//-- send insert on duplicate key update sql to DBProxy
	std::shared_ptr<IAsyncAnswer> async = genAsyncAnswer(quest);
	WriteCallback* callback = new WriteCallback(async, _dbproxy, dbQuest);
	callback->cleanCacheAfterGotResponse(hintId, tableName, shared_from_this());

	if (_dbproxy->sendQuest(dbQuest, callback) == false)
	{
		if (_dbproxy->sendQuest(dbQuest, callback) == false)
		{
			delete callback;
			FPAnswerPtr answer = ErrorInfo::queryDBProxyFailedAnswer(quest);
			async->sendAnswer(answer);
			return nullptr;
		}
	}

	return nullptr;
}

FPAnswerPtr TableCacheProcessor::fetch(const FPReaderPtr args, const FPQuestPtr quest, const ConnectionInfo& ci)
{
	std::string tableName = args->wantString("table");
	TABLEPtr scheme = getTableScheme(tableName);
	if (!scheme)
		return ErrorInfo::tableNotFoundAnswer(quest);

	std::vector<std::string> fields = args->want("fields", std::vector<std::string>());

	std::string keyName = scheme->get_key_name();
	bool strKey = scheme->isStringField(keyName);
	if (!strKey)
	{
		std::set<int64_t> hintIds = args->get("hintIds", std::set<int64_t>());
		if (hintIds.empty())
		{
			int64_t hintId = args->wantInt("hintId");
			hintIds.insert(hintId);
		}

		return real_fetch(quest, tableName, scheme, fields, hintIds);
	}
	else
	{
		std::set<std::string> hintStrings = args->get("hintIds", std::set<std::string>());
		if (hintStrings.empty())
		{
			std::string hintStr = args->wantString("hintId");
			hintStrings.insert(hintStr);
		}

		return real_fetch(quest, tableName, scheme, fields, hintStrings);
	}
}

FPAnswerPtr TableCacheProcessor::real_fetch_from_database(const FPQuestPtr quest,
	const std::string& tableName, TABLEPtr scheme, std::vector<uint16_t>& fieldIndexes,
	const std::set<int64_t>& lackedHintIds, std::map<int64_t, std::vector<std::string>>& result, bool jsonCompatible)
{
	std::string sql("select ");
	sql.append(scheme->get_select_string()).append(" from ").append(tableName);
	sql.append(" where ").append(scheme->get_key_name()).append(" in (");
	int needComna = false;
	for (int64_t hintId: lackedHintIds)
	{
		if (needComna)
			sql.append(",");
		else
			needComna = true;

		sql.append(std::to_string(hintId));
	}
	sql.append(")");

	FPQWriter qw(3, "iQuery");
	qw.param("hintIds", lackedHintIds);
	qw.param("sql", sql);
	qw.param("tableName", tableName);
	FPQuestPtr dbQuest = qw.take();

	std::shared_ptr<IAsyncAnswer> async = genAsyncAnswer(quest);
	AnswerCallback * callback = NULL;
	if (!jsonCompatible)
		callback = new FetchRowCallback<int64_t>(
			async, shared_from_this(), dbQuest, scheme, fieldIndexes, result);
	else
	{
		std::map<std::string, std::vector<std::string>> skeyResult;
		for (auto& resultPair: result)
			skeyResult[std::to_string(resultPair.first)] = resultPair.second;

		callback = new FetchRowCallback<std::string>(
			async, shared_from_this(), dbQuest, scheme, fieldIndexes, skeyResult);
	}

	if (_dbproxy->sendQuest(dbQuest, callback) == false)
	{
		if (_dbproxy->sendQuest(dbQuest, callback) == false)
		{
			delete callback;
			FPAnswerPtr answer = ErrorInfo::queryDBProxyFailedAnswer(quest);
			async->sendAnswer(answer);
		}
	}
	return nullptr;
}

FPAnswerPtr TableCacheProcessor::real_fetch_from_database(const FPQuestPtr quest,
	const std::string& tableName, TABLEPtr scheme, std::vector<uint16_t>& fieldIndexes,
	const std::set<std::string>& lackedHintStrings, std::map<std::string, std::vector<std::string>>& result)
{
	std::string sql("select ");
	sql.append(scheme->get_select_string()).append(" from ").append(tableName);
	sql.append(" where ").append(scheme->get_key_name()).append(" in (");
	int needComna = false;
	//for (const std::string& hintString: lackedHintStrings)
	for (int i = 0; i < (int)lackedHintStrings.size(); i++)
	{
		if (needComna)
			sql.append(",");
		else
			needComna = true;

		sql.append("'?'");
	}
	sql.append(")");

	FPQWriter qw(4, "sQuery");
	qw.param("hintIds", lackedHintStrings);
	qw.param("sql", sql);
	qw.param("tableName", tableName);
	qw.param("params", lackedHintStrings);
	FPQuestPtr dbQuest = qw.take();

	std::shared_ptr<IAsyncAnswer> async = genAsyncAnswer(quest);
	FetchRowCallback<std::string>* callback = new FetchRowCallback<std::string>(
		async, shared_from_this(), dbQuest, scheme, fieldIndexes, result);

	if (_dbproxy->sendQuest(dbQuest, callback) == false)
	{
		if (_dbproxy->sendQuest(dbQuest, callback) == false)
		{
			delete callback;
			FPAnswerPtr answer = ErrorInfo::queryDBProxyFailedAnswer(quest);
			async->sendAnswer(answer);
		}
	}
	return nullptr;
}

FPAnswerPtr TableCacheProcessor::real_fetch(const FPQuestPtr quest, const std::string& tableName,
	TABLEPtr scheme, const std::vector<std::string>& fields, const std::set<int64_t>& hintIds)
{
	FPQReader qr(quest);
	bool jsonCompatible = qr.getBool("jsonCompatible", false);

	std::set<int64_t> lackedIds;
	std::map<int64_t, std::vector<std::string>> result;
	std::vector<uint16_t> indexes = scheme->get_fields_index(fields);
	{
		WKeeper wlock(&_rwlocker);

		for (int64_t hintId: hintIds)
		{
			TableKey key;
			key.hintId = hintId;
			key.tableName = tableName;

			CacheMap::node_type* node = _cachaMap->find(key);
			if (node)
			{
				_cachaMap->fresh_node(node);

				ROWPtr row = node->data;
				result[hintId] = row->get_data(indexes);
			}
			else
				lackedIds.insert(hintId);
		}
	}

	_statistics.fetchCount++;
	_statistics.itemFetchCount.fetch_add((uint64_t)hintIds.size());
	_statistics.itemHitCount.fetch_add((uint64_t)(hintIds.size() - lackedIds.size()));

	if (lackedIds.empty())
	{
		_statistics.fullHitCount++;

		FPAWriter aw(1, quest);
		if (!jsonCompatible)
			aw.param("data", result);
		else
		{
			std::map<std::string, std::vector<std::string>> skeyResult;
			for (auto& resultPair: result)
				skeyResult[std::to_string(resultPair.first)] = resultPair.second;

			aw.param("data", skeyResult);
		}
		return aw.take();
	}

	if (result.size())
		_statistics.partHitCount++;

	return real_fetch_from_database(quest, tableName, scheme, indexes, lackedIds, result, jsonCompatible);
}

FPAnswerPtr TableCacheProcessor::real_fetch(const FPQuestPtr quest, const std::string& tableName,
	TABLEPtr scheme, const std::vector<std::string>& fields, const std::set<std::string>& hintStrings)
{
	std::set<std::string> lackedIds;
	std::map<std::string, std::vector<std::string>> result;
	std::vector<uint16_t> indexes = scheme->get_fields_index(fields);

	std::vector<int64_t> hintIds;
	std::vector<std::string> hintStrs;

	hintIds.reserve(hintStrings.size());
	hintStrs.reserve(hintStrings.size());

	for (auto& hintString: hintStrings)
	{
		int64_t hintId = (int64_t)jenkins_hash(hintString.c_str(), hintString.length(), 0);
		hintIds.push_back(hintId);
		hintStrs.push_back(hintString);
	}

	{
		WKeeper wlock(&_rwlocker);

		for (size_t i = 0; i < hintIds.size(); i++)
		{
			TableKey key;
			key.hintId = hintIds[i];
			key.tableName = tableName;

			CacheMap::node_type* node = _cachaMap->find(key);
			if (node)
			{
				_cachaMap->fresh_node(node);

				ROWPtr row = node->data;
				result[hintStrs[i]] = row->get_data(indexes);
			}
			else
				lackedIds.insert(hintStrs[i]);
		}
	}

	_statistics.fetchCount++;
	_statistics.itemFetchCount.fetch_add((uint64_t)hintStrings.size());
	_statistics.itemHitCount.fetch_add((uint64_t)(hintStrings.size() - lackedIds.size()));

	if (lackedIds.empty())
	{
		FPAWriter aw(1, quest);
		aw.param("data", result);

		_statistics.fullHitCount++;
		return aw.take();
	}

	if (result.size())
		_statistics.partHitCount++;

	return real_fetch_from_database(quest, tableName, scheme, indexes, lackedIds, result);
}

FPAnswerPtr TableCacheProcessor::deleteData(const FPReaderPtr args, const FPQuestPtr quest, const ConnectionInfo& ci)
{
	std::string tableName = args->wantString("table");
	TABLEPtr scheme = getTableScheme(tableName);
	if (!scheme)
		return ErrorInfo::tableNotFoundAnswer(quest);

	std::string delete_sql("delete from ");
	delete_sql.append(tableName);
	delete_sql.append(" where ");

	int64_t hintId;
	std::string hintString;
	std::string keyName = scheme->get_key_name();
	delete_sql.append(keyName).append(" = ");

	FPQuestPtr dbQuest;
	bool strKey = scheme->isStringField(keyName);
	if (!strKey)
	{
		hintId = args->wantInt("hintId");
		delete_sql.append(std::to_string(hintId));

		FPQWriter qw(3, "query");
		qw.param("hintId", hintId);
		qw.param("sql", delete_sql);
		qw.param("tableName", tableName);
		dbQuest = qw.take();
	}
	else
	{
		hintString = args->wantString("hintId");
		hintId = (int64_t)jenkins_hash(hintString.c_str(), hintString.length(), 0);

		delete_sql.append("'?'");

		FPQWriter qw(4, "sQuery");
		qw.param("hintIds", std::set<std::string>{hintString});
		qw.param("sql", delete_sql);
		qw.param("tableName", tableName);
		qw.param("params", std::vector<std::string>{hintString});
		dbQuest = qw.take();
	}

	std::shared_ptr<IAsyncAnswer> async = genAsyncAnswer(quest);
	WriteCallback* callback = new WriteCallback(async, _dbproxy, dbQuest);
	callback->cleanCacheAfterGotResponse(hintId, tableName, shared_from_this());

	if (_dbproxy->sendQuest(dbQuest, callback) == false)
	{
		if (_dbproxy->sendQuest(dbQuest, callback) == false)
		{
			delete callback;
			FPAnswerPtr answer = ErrorInfo::queryDBProxyFailedAnswer(quest);
			async->sendAnswer(answer);
		}
	}

	return nullptr;
}

FPAnswerPtr TableCacheProcessor::invalidateTable(const FPReaderPtr args, const FPQuestPtr quest, const ConnectionInfo& ci)
{
	std::string tableName = args->wantString("table");
	if (!args->getBool("internal", false))
		_clusterNotifier->invalidateTable(tableName);

	{
		WKeeper wlock(&_rwlocker);
		_tableInfo.erase(tableName);

		std::set<CacheMap::node_type *> nodes;
		nodes.swap(_tableDataIndexes[tableName]);
		_tableDataIndexes.erase(tableName);

		for (auto node: nodes)
			_cachaMap->remove_node(node);
	}
	return FPAWriter::emptyAnswer(quest);
}

FPAnswerPtr TableCacheProcessor::refreshCluster(const FPReaderPtr args, const FPQuestPtr quest, const ConnectionInfo& ci)
{
	_clusterNotifier->refreshCluster();
	return FPAWriter::emptyAnswer(quest);
}

FPAnswerPtr TableCacheProcessor::invalidate(const FPReaderPtr args, const FPQuestPtr quest, const ConnectionInfo& ci)
{
	std::string tableName = args->wantString("table");
	std::set<int64_t> hintIds = args->want("hintIds", std::set<int64_t>());
	{
		WKeeper wlock(&_rwlocker);
		auto it = _tableDataIndexes.find(tableName);
		if (it != _tableDataIndexes.end())
		{
			for (int64_t hintId: hintIds)
			{
				TableKey key;
				key.hintId = hintId;
				key.tableName = tableName;

				CacheMap::node_type* node = _cachaMap->find(key);
				if (node)
				{
					it->second.erase(node);
					if (it->second.empty())
						_tableDataIndexes.erase(it);

					_cachaMap->remove_node(node);
				}
			}
		}
	}

	return FPAWriter::emptyAnswer(quest);
}

std::string TableCacheProcessor::infos()
{
	std::string infos("{\"fetchStatus\":{");
	infos.append("\"fetchCount\":").append(std::to_string(_statistics.fetchCount));
	infos.append(",\"partHitCount\":").append(std::to_string(_statistics.partHitCount));
	infos.append(",\"fullHitCount\":").append(std::to_string(_statistics.fullHitCount));
	infos.append(",\"itemFetchCount\":").append(std::to_string(_statistics.itemFetchCount));
	infos.append(",\"itemHitCount\":").append(std::to_string(_statistics.itemHitCount));

	infos.append("},\"cacheStatus\":{");

	int64_t globalItemCount = 0;
	std::map<std::string, int64_t> tableItemCount;

	{
		RKeeper rlock(&_rwlocker);
		globalItemCount = (int64_t)_cachaMap->count();

		for (const auto& tablePair: _tableDataIndexes)
			tableItemCount[tablePair.first] = (int64_t)tablePair.second.size();
	}

	infos.append("\"totalCachedItems\":").append(std::to_string(globalItemCount));
	infos.append(",\"cachedTableItems\":{");

	bool needComma = false;
	for (auto& stPair: tableItemCount)
	{
		if (needComma)
			infos.append(",");
		else
			needComma = true;

		infos.append("\"").append(stPair.first).append("\":").append(std::to_string(stPair.second));
	}

	infos.append("}}}");
	return infos;
}