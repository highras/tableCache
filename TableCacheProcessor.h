#ifndef Table_Cache_Processor_H
#define Table_Cache_Processor_H

#include <atomic>
#include <unordered_map>
#include "jenkins.h"
#include "hashint.h"
#include "TableRow.h"
#include "LruHashMap.h"
#include "RWLocker.hpp"
#include "IQuestProcessor.h"
#include "ClusterNotifier.h"

using namespace fpnn;

struct TableKey
{
	int64_t hintId;
	std::string tableName;

	bool operator == (const struct TableKey& key)
	{
		return this->hintId == key.hintId && this->tableName == key.tableName;
	}

	bool operator < (const struct TableKey& right) const
	{
		if(this->hintId != right.hintId) 
			return this->hintId < right.hintId;
		return this->tableName < right.tableName;
	}

	unsigned int hash() const
	{
		uint32_t val = hash32_uint64(hintId);
		return jenkins_hash(tableName.data(), tableName.length(), val);
	}
};

class WriteCallback;
template<typename TYPE>
class FetchRowCallback;

struct FetchStatistics
{
	std::atomic<uint64_t> fetchCount;
	std::atomic<uint64_t> partHitCount;
	std::atomic<uint64_t> fullHitCount;

	std::atomic<uint64_t> itemFetchCount;
	std::atomic<uint64_t> itemHitCount;

	FetchStatistics(): fetchCount(0), partHitCount(0), fullHitCount(0), itemFetchCount(0), itemHitCount(0) {}
};

class TableCacheProcessor: virtual public IQuestProcessor, virtual public std::enable_shared_from_this<TableCacheProcessor>
{
	QuestProcessorClassPrivateFields(TableCacheProcessor)

	ClusterNotifierPtr _clusterNotifier;
	TCPClientPtr _dbproxy;

	RWLocker _rwlocker;
	std::unordered_map<std::string, TABLEPtr> _tableInfo;

	typedef LruHashMap<TableKey, ROWPtr> CacheMap;
	typedef std::shared_ptr<CacheMap> CacheMapPtr;
	CacheMapPtr _cachaMap;

	std::unordered_map<std::string, std::set<CacheMap::node_type*>> _tableDataIndexes;

	FetchStatistics _statistics;

	void configure();
	bool loadTableScheme(const std::string& tableName, std::vector<std::vector<std::string>>& scheme);
	std::string loadSplitColumn(const std::string& tableName);
	TABLEPtr loadTableInfo(const std::string& tableName);
	TABLEPtr getTableScheme(const std::string& tableName);
	void cleanCache(const std::string& tableName, int64_t hintId);

	FPAnswerPtr real_fetch(const FPQuestPtr quest, const std::string& tableName, TABLEPtr scheme,
		const std::vector<std::string>& fields, const std::set<int64_t>& hintIds);
	FPAnswerPtr real_fetch(const FPQuestPtr quest, const std::string& tableName, TABLEPtr scheme,
		const std::vector<std::string>& fields, const std::set<std::string>& hintStrings);

	FPAnswerPtr real_fetch_from_database(const FPQuestPtr quest,
		const std::string& tableName, TABLEPtr scheme, std::vector<uint16_t>& fieldIndexes,
		const std::set<int64_t>& lackedHintIds, std::map<int64_t, std::vector<std::string>>& result, bool jsonCompatible);
	FPAnswerPtr real_fetch_from_database(const FPQuestPtr quest,
		const std::string& tableName, TABLEPtr scheme, std::vector<uint16_t>& fieldIndexes,
		const std::set<std::string>& lackedHintStrings, std::map<std::string, std::vector<std::string>>& result);

	friend class WriteCallback;
	friend class FetchRowCallback<int64_t>;
	friend class FetchRowCallback<std::string>;

	void addRows(TABLEPtr orginalScheme, const std::vector<std::vector<std::string>>& data);

public:
	FPAnswerPtr modify(const FPReaderPtr args, const FPQuestPtr quest, const ConnectionInfo& ci);
	FPAnswerPtr fetch(const FPReaderPtr args, const FPQuestPtr quest, const ConnectionInfo& ci);
	FPAnswerPtr deleteData(const FPReaderPtr args, const FPQuestPtr quest, const ConnectionInfo& ci);
	FPAnswerPtr invalidateTable(const FPReaderPtr args, const FPQuestPtr quest, const ConnectionInfo& ci);
	FPAnswerPtr refreshCluster(const FPReaderPtr args, const FPQuestPtr quest, const ConnectionInfo& ci);
	FPAnswerPtr invalidate(const FPReaderPtr args, const FPQuestPtr quest, const ConnectionInfo& ci);

	virtual std::string infos();

	TableCacheProcessor()
	{
		registerMethod("modify", &TableCacheProcessor::modify);
		registerMethod("fetch", &TableCacheProcessor::fetch);
		registerMethod("delete", &TableCacheProcessor::deleteData);
		registerMethod("invalidateTable", &TableCacheProcessor::invalidateTable);
		registerMethod("refreshCluster", &TableCacheProcessor::refreshCluster);
		registerMethod("invalidate", &TableCacheProcessor::invalidate);

		configure();
	}

	QuestProcessorClassBasicPublicFuncs
};
typedef std::shared_ptr<TableCacheProcessor> TableCacheProcessorPtr;

#endif
