#ifndef Cluster_Notifier_H
#define Cluster_Notifier_H

#include <map>
#include <set>
#include <string>
#include <thread>
#include "TCPClient.h"

using namespace fpnn;

class ClusterNotifier;
typedef std::shared_ptr<ClusterNotifier> ClusterNotifierPtr;

class ClusterNotifier: public std::enable_shared_from_this<ClusterNotifier>
{
	struct InvalidateInfo
	{
		TCPClientPtr client;
		std::map<std::string, std::set<int64_t>> invalidateData;	//-- tablename, hintIds. If hintIds is empty, mean all.
	};
	typedef std::shared_ptr<InvalidateInfo> InvalidateInfoPtr;

	class NotifyAnswerCallback: public AnswerCallback
	{
		bool _processed;
		std::string _endpoint;
		std::string _tableName;
		std::set<int64_t> _hintIds;
		ClusterNotifierPtr _clusterNotifier;

	public:
		NotifyAnswerCallback(ClusterNotifierPtr clusterNotifier, const std::string& endpoint, const std::string& tableName, const std::set<int64_t>& hintIds);
		~NotifyAnswerCallback();

		virtual void onAnswer(FPAnswerPtr) { _processed = true; }
		virtual void onException(FPAnswerPtr answer, int errorCode);

	};
	typedef std::shared_ptr<NotifyAnswerCallback> NotifyAnswerCallbackPtr;

private:
	std::set<std::string> _selfEndpoints;
	std::map<std::string, InvalidateInfoPtr> _clients;
	std::thread _notifyThread;
	std::mutex _mutex;
	bool _running;

	ClusterNotifier();

	std::vector<std::string> loadEndpoints(const std::string& endpoints_file);
	void initSelfEndpoints();
	void buildNotifyClients(std::map<std::string, InvalidateInfoPtr>& notifyClients);
	void addNotifyClient(const std::string& endpoint, std::map<std::string, InvalidateInfoPtr>& notifyClients);
	void notify_thread();

	void reinvalidate(const std::string& endpoint, const std::string& tableName);
	void reinvalidate(const std::string& endpoint, const std::string& tableName, std::set<int64_t>& hintIds);
	FPQuestPtr buildQuest(const std::string& tableName, const std::set<int64_t>& hintIds);
	TCPClientPtr getClient(const std::string& endpoint);

public:
	static ClusterNotifierPtr create() { return ClusterNotifierPtr(new ClusterNotifier); }
	~ClusterNotifier();

	void refreshCluster();
	void invalidate(const std::string& tableName, int64_t hintId);
	void invalidateTable(const std::string& tableName);
};

#endif