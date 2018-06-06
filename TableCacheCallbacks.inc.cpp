#ifndef Table_Cache_Callbacks_h
#define Table_Cache_Callbacks_h

#include <set>
#include <string>
#include <vector>
#include "TableRow.h"
#include "IQuestProcessor.h"
#include "TableCacheErrorInfo.h"

#define FPNN_MAX_ERROR_CODE 29999

inline FPAnswerPtr dumpErrorAnswer(IAsyncAnswerPtr async, FPAnswerPtr answer)
{
	FPAReader ar(answer);
	int64_t code = ar.wantInt("code");
	std::string ex = ar.wantString("ex");
	std::string raiser = ar.wantString("raiser");

	return FPAWriter::errorAnswer(async->getQuest(), code, ex, raiser);
}

template <typename TYPE>
class FetchRowCallback: public AnswerCallback
{
private:
	int _retryTimes;
	TABLEPtr _scheme;
	FPQuestPtr _dbQuest;
	IAsyncAnswerPtr _async;
	TableCacheProcessorPtr _processor;
	std::vector<uint16_t> _requiredIndex;
	std::map<TYPE, std::vector<std::string>> _cachedResult;

	void addRowDataToResult(const std::string& key, int64_t&, const std::vector<std::string>& row)
	{
		_cachedResult[(int64_t)atoll(key.c_str())] = row;
	}
	void addRowDataToResult(const std::string& key, const std::string&, const std::vector<std::string>& row)
	{
		_cachedResult[key] = row;
	}

public:
	FetchRowCallback(IAsyncAnswerPtr async, TableCacheProcessorPtr processor, FPQuestPtr dbQuest,
		TABLEPtr scheme, std::vector<uint16_t>& requiredIndex, 
		std::map<TYPE, std::vector<std::string>>& cachedResult):
		_retryTimes(0), _scheme(scheme), _dbQuest(dbQuest), _async(async), _processor(processor)
		{
			_requiredIndex.swap(requiredIndex);
			_cachedResult.swap(cachedResult);
		}

	virtual void onAnswer(FPAnswerPtr answer)
	{
		TYPE kindSign;
		std::string keyCloumn = _scheme->get_key_name();
		std::vector<uint16_t> index = _scheme->get_fields_index(std::vector<std::string>{keyCloumn});

		FPAReader ar(answer);
		std::vector<std::vector<std::string>> rows = ar.want("rows", std::vector<std::vector<std::string>>());
		for (const auto& rowData: rows)
		{
			std::vector<std::string> result;
			result.reserve(_requiredIndex.size());

			for (size_t i = 0; i < _requiredIndex.size(); i++)
				result.push_back(rowData[_requiredIndex[i]]);

			const std::string& hintValue = rowData[index[0]];

			addRowDataToResult(hintValue, kindSign, result);
		}

		FPAWriter aw(1, _async->getQuest());
		aw.param("data", _cachedResult);
		answer = aw.take();
		_async->sendAnswer(answer);

		_processor->addRows(_scheme, rows);
	}

	virtual void onException(FPAnswerPtr answer, int errorCode)
	{
		if (_retryTimes == 0)
		{
			if (errorCode <= FPNN_MAX_ERROR_CODE)
			{
				FetchRowCallback* callback = new FetchRowCallback(
					_async, _processor, _dbQuest, _scheme, _requiredIndex, _cachedResult);
				callback->_retryTimes = 1;

				if (_processor->_dbproxy->sendQuest(_dbQuest, callback))
					return;

				delete callback;
				answer = nullptr;
			}
		}

		if (!answer)
			answer = ErrorInfo::queryDBProxyFailedAnswer(_async->getQuest());
		else
			//answer->setSeqNum(_async->getQuest()->seqNum());
			answer = dumpErrorAnswer(_async, answer);
		
		_async->sendAnswer(answer);
	}
};

class WriteCallback: public AnswerCallback
{
private: 
	int _retryTimes;
	FPQuestPtr _dbQuest;
	TCPClientPtr _dbproxy;
	IAsyncAnswerPtr _async;
	int64_t _hintId;
	std::string _tableName;
	TableCacheProcessorPtr _processor;

	void cleanCache();

public:
	WriteCallback(IAsyncAnswerPtr async, TCPClientPtr dbproxy, FPQuestPtr dbQuest):
		_retryTimes(0), _dbQuest(dbQuest), _dbproxy(dbproxy), _async(async), _processor(nullptr) {}

	virtual void onAnswer(FPAnswerPtr answer);
	virtual void onException(FPAnswerPtr answer, int errorCode);
	void cleanCacheAfterGotResponse(int64_t hintId, const std::string& tableName, TableCacheProcessorPtr processor)
	{
		_hintId = hintId; _tableName = tableName; _processor = processor;
	}
};

void WriteCallback::onAnswer(FPAnswerPtr)
{
	cleanCache();
	FPAnswerPtr answer = FPAWriter::emptyAnswer(_async->getQuest());
	_async->sendAnswer(answer);
}

void WriteCallback::onException(FPAnswerPtr answer, int errorCode)
{
	if (_retryTimes == 0)
	{
		if (errorCode <= FPNN_MAX_ERROR_CODE)
		{
			WriteCallback* callback = new WriteCallback(_async, _dbproxy, _dbQuest);
			callback->_retryTimes = 1;

			if (_dbproxy->sendQuest(_dbQuest, callback))
				return;

			delete callback;
			answer = nullptr;
		}
	}

	if (!answer)
		answer = ErrorInfo::queryDBProxyFailedAnswer(_async->getQuest());
	else
		//answer->setSeqNum(_async->getQuest()->seqNum());
		answer = dumpErrorAnswer(_async, answer);
	
	_async->sendAnswer(answer);
	cleanCache();
}

void WriteCallback::cleanCache()
{
	if (_processor)
		_processor->cleanCache(_tableName, _hintId);
}

#endif