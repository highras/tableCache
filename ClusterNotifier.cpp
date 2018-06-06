#include <fstream>
#include "Setting.h"
#include "FPLog.h"
#include "FpnnError.h"
#include "ServerInfo.h"
#include "StringUtil.h"
#include "ClusterNotifier.h"

std::vector<std::string> ClusterNotifier::loadEndpoints(const std::string& endpoints_file)
{
	std::vector<std::string> endpoints;

	std::ifstream fin(endpoints_file.c_str()); 
	if (fin.is_open())
	{
		while(fin.good())
		{
			std::string line;
			std::getline(fin, line);
			StringUtil::trim(line);
			
			if (line.empty())
				continue;

			size_t pos = line.find_first_of(":");
			if(pos == std::string::npos)
			{
				LOG_ERROR("Bad endpoints %s in config file: %s", line.c_str(), endpoints_file.c_str());
				continue;
			}

			endpoints.push_back(line);
		}
		fin.close();
		return endpoints;
	}
	LOG_FATAL("Cannot open endpoints config file: %s", endpoints_file.c_str());
	return endpoints;
}

void ClusterNotifier::initSelfEndpoints()
{
	std::string port = Setting::getString("FPNN.server.listening.port");
	_selfEndpoints.insert(std::string("127.0.0.1:").append(port));
	
	std::string ip = ServerInfo::getServerLocalIP4();
	if (ip.empty() == false)
		_selfEndpoints.insert(ip.append(":").append(port));

	ip = ServerInfo::getServerPublicIP4();
	if (ip.empty() == false)
		_selfEndpoints.insert(ip.append(":").append(port));

	_selfEndpoints.insert(std::string("localhost:").append(port));

	std::string domain = ServerInfo::getServerDomain();
	if (domain.empty() == false)
		_selfEndpoints.insert(domain.append(":").append(port));
}
void ClusterNotifier::buildNotifyClients(std::map<std::string, InvalidateInfoPtr>& notifyClients)
{
	std::string endpointConfigFile = Setting::getString("TableCache.cluster.endpointsSet.configFile");
	std::vector<std::string> endpoints = loadEndpoints(endpointConfigFile);

	for (auto& endpoint: endpoints)
	{
		bool matched = false;
		for (const auto& self: _selfEndpoints)
			if (strcasecmp(self.c_str(), endpoint.c_str()) == 0)
			{
				matched = true;
				break;
			}

		if (matched)
			continue;
		if (notifyClients.find(endpoint) != notifyClients.end())
			continue;
		addNotifyClient(endpoint, notifyClients);
	}

	if (notifyClients.empty())
		LOG_WARN("Load empty endpoints! TableCache will running in singleton mode.");
}

void ClusterNotifier::addNotifyClient(const std::string& endpoint, std::map<std::string, InvalidateInfoPtr>& notifyClients)
{

	std::vector<std::string> ip_port;
	StringUtil::split(endpoint, ":", ip_port);
	if (ip_port.size() != 2)
	{
		LOG_ERROR("Bad endpoint %s when adding notify client.", endpoint.c_str());
		return;
	}

	if (notifyClients.find(endpoint) != notifyClients.end())
		return;

	InvalidateInfoPtr iip(new InvalidateInfo);

	iip->client = TCPClient::createClient(ip_port[0], atoi(ip_port[1].c_str()));
	iip->client->setQuestTimeout(2);

	notifyClients[endpoint] = iip;
}

ClusterNotifier::ClusterNotifier()
{
	initSelfEndpoints();
	buildNotifyClients(_clients);
	
	_running = true;
	_notifyThread = std::thread(&ClusterNotifier::notify_thread, this);
}

ClusterNotifier::~ClusterNotifier()
{
	_running = false;
	_notifyThread.join();
}

void ClusterNotifier::refreshCluster()
{
	std::map<std::string, InvalidateInfoPtr> notifyClients;
	buildNotifyClients(notifyClients);

	std::unique_lock<std::mutex> lck(_mutex);
	for (auto& cliPair: notifyClients)
	{
		auto it = _clients.find(cliPair.first);
		if (it != _clients.end())
			cliPair.second = it->second;
	}

	_clients.swap(notifyClients);
}

void ClusterNotifier::invalidate(const std::string& tableName, int64_t hintId)
{
	std::unique_lock<std::mutex> lck(_mutex);
	for (auto& clientPair: _clients)
	{
		auto& invalidMap = clientPair.second->invalidateData;
		if (invalidMap.find(tableName) == invalidMap.end())
			invalidMap[tableName].insert(hintId);
		else
		{
			if (invalidMap[tableName].empty() == false)
				invalidMap[tableName].insert(hintId);
		}
	}
}

void ClusterNotifier::invalidateTable(const std::string& tableName)
{
	std::unique_lock<std::mutex> lck(_mutex);
	for (auto& clientPair: _clients)
		clientPair.second->invalidateData[tableName].clear();
}


void ClusterNotifier::reinvalidate(const std::string& endpoint, const std::string& tableName)
{
	std::unique_lock<std::mutex> lck(_mutex);
	_clients[endpoint]->invalidateData[tableName].clear();
}
void ClusterNotifier::reinvalidate(const std::string& endpoint, const std::string& tableName, std::set<int64_t>& hintIds)
{
	std::unique_lock<std::mutex> lck(_mutex);
	auto& invalidMap = _clients[endpoint]->invalidateData;

	if (invalidMap.find(tableName) == invalidMap.end())
		invalidMap[tableName] = hintIds;
	else
	{
		auto &idSet = invalidMap[tableName];
		if (idSet.empty() == false)
		{
			for (int64_t hintId: hintIds)
				idSet.insert(hintId);
		}
	}
}

FPQuestPtr ClusterNotifier::buildQuest(const std::string& tableName, const std::set<int64_t>& hintIds)
{
	if (hintIds.empty())
	{
		FPQWriter qw(2, "invalidateTable");
		qw.param("table", tableName);
		qw.param("internal", true);
		return qw.take();
	}
	else
	{
		FPQWriter qw(2, "invalidate");
		qw.param("table", tableName);
		qw.param("hintIds", hintIds);
		return qw.take();
	}
}

TCPClientPtr ClusterNotifier::getClient(const std::string& endpoint)
{
	std::unique_lock<std::mutex> lck(_mutex);
	return _clients[endpoint]->client;
}

ClusterNotifier::NotifyAnswerCallback::NotifyAnswerCallback(ClusterNotifierPtr clusterNotifier,
	const std::string& endpoint, const std::string& tableName, const std::set<int64_t>& hintIds):
	_processed(false), _endpoint(endpoint), _tableName(tableName), _hintIds(hintIds),
	_clusterNotifier(clusterNotifier)
{
}

ClusterNotifier::NotifyAnswerCallback::~NotifyAnswerCallback()
{
	if (!_processed)
		onException(nullptr, FPNN_EC_CORE_UNKNOWN_ERROR);
}

void ClusterNotifier::NotifyAnswerCallback::onException(FPAnswerPtr answer, int errorCode)
{
	if (_hintIds.empty())
		_clusterNotifier->reinvalidate(_endpoint, _tableName);
	else
		_clusterNotifier->reinvalidate(_endpoint, _tableName, _hintIds);

	_processed = true;
}

void ClusterNotifier::notify_thread()
{
	while (_running)
	{
		std::map<std::string, InvalidateInfoPtr> notifyInfo;

		{
			std::unique_lock<std::mutex> lck(_mutex);
			for (auto& cliPair: _clients)
			{
				InvalidateInfoPtr iip = std::make_shared<InvalidateInfo>();
				notifyInfo[cliPair.first] = iip;
				iip->client = cliPair.second->client;
				iip->invalidateData.swap(cliPair.second->invalidateData);
			}
		}

		for (auto& noPair: notifyInfo)
		{
			TCPClientPtr client = noPair.second->client;
			std::map<std::string, std::set<int64_t>>& tableInfos = noPair.second->invalidateData;

			for (auto& tableInfo: tableInfos)
			{
				FPQuestPtr quest = buildQuest(tableInfo.first, tableInfo.second);
				NotifyAnswerCallback* callback = new NotifyAnswerCallback(
					shared_from_this(), noPair.first, tableInfo.first, tableInfo.second);
				
				bool status = client->sendQuest(quest, callback);
				if (!status)
				{
					status = client->sendQuest(quest, callback);
					if (!status)
					{
						delete callback;
						LOG_WARN("Resend invalid notification to %s failed. Retry later.", noPair.first.c_str());
						break;
					}
				}
			}
		}

		usleep(100 * 1000);
	}
}