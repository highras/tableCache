#include <iostream>
#include <string.h>
#include <stdlib.h>
#include "ignoreSignals.h"
#include "TCPClient.h"

using namespace fpnn;

void printExceptionAnswer(FPAnswerPtr answer)
{
	FPAReader ar(answer);
	std::cout<<"Exception!"<<std::endl;
	std::cout<<"Raiser: "<<ar.wantString("raiser")<<std::endl;
	std::cout<<"Error code: "<<ar.wantInt("code")<<std::endl;
	std::cout<<"Expection: "<<ar.wantString("ex")<<std::endl;
}

void invalidateTable(const char *endpoint, const char *tableName)
{
	std::shared_ptr<TCPClient> client = TCPClient::createClient(endpoint);
	FPQWriter qw(1, "invalidateTable");
	qw.param("table", tableName);
	FPQuestPtr quest = qw.take();
	FPAnswerPtr answer = client->sendQuest(quest);
	if (!answer->status())
		std::cout<<"invalidateTable table success!"<<std::endl;
	else
		printExceptionAnswer(answer);
}

template <typename TYPE>
void deleteData(const char *endpoint, const char *tableName, const TYPE & hintId)
{
	std::shared_ptr<TCPClient> client = TCPClient::createClient(endpoint);
	FPQWriter qw(2, "delete");
	qw.param("table", tableName);
	qw.param("hintId", hintId);
	FPQuestPtr quest = qw.take();
	FPAnswerPtr answer = client->sendQuest(quest);
	if (!answer->status())
		std::cout<<"delete data success!"<<std::endl;
	else
		printExceptionAnswer(answer);
}

int main(int argc, const char* argv[])
{
	ignoreSignals();

	if (argc == 3)
	{
		invalidateTable(argv[1], argv[2]);
		return 0;
	}

	if (argc == 5)
	{
		if (strcmp(argv[3], "-i") == 0)
		{
			deleteData(argv[1], argv[2], atoll(argv[4]));
			return 0;
		}

		if (strcmp(argv[3], "-s") == 0)
		{
			deleteData(argv[1], argv[2], argv[4]);
			return 0;
		}
	}

	std::cout<<"Usage: "<<std::endl;
	std::cout<<"\tInvalid data:\t"<<argv[0]<<" host:port <table>"<<std::endl;
	std::cout<<"\tDelete data:\t"<<argv[0]<<" host:port <table> < -i | -s > <hintId>"<<std::endl;
	return 0;
}
