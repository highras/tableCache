#include <iostream>
#include <string.h>
#include <stdlib.h>
#include "ignoreSignals.h"
#include "TCPClient.h"

using namespace fpnn;

void showUsage(const char* appname)
{
	std::cout<<"Usage: "<<std::endl;
	std::cout<<"\t"<<appname<<" host:port <table> < -i | -s > <hintId> key1:value1 [key2:value2 ...]"<<std::endl;
	exit(1);
}
int main(int argc, const char* argv[])
{
	if (argc < 6)
		showUsage(argv[0]);

	ignoreSignals();
	std::shared_ptr<TCPClient> client = TCPClient::createClient(argv[1]);
	FPQWriter qw(3, "modify");
	qw.param("table", argv[2]);

	if (strcmp(argv[3], "-i") == 0)
		qw.param("hintId", atoll(argv[4]));
	else if (strcmp(argv[3], "-s") == 0)
		qw.param("hintId", argv[4]);
	else
		showUsage(argv[0]);

	std::map<std::string, std::string> condiations;
	for (int i = 5; i < argc; i++)
	{
		std::string param(argv[i]);
		size_t pos = param.find_first_of(':');
		if (pos == std::string::npos)
		{
			std::cout<<"Error!"<<std::endl<<"\t"<<"param: "<<param<<" cannot find delimiter ':'"<<std::endl;
			exit(1);
		}
		if (pos == 0)
		{
			std::cout<<"Error!"<<std::endl<<"\t"<<"param: "<<param<<" key is invalid!"<<std::endl;
			exit(1);
		}
		std::string key = std::string(param, 0, pos);
		std::string value = std::string(param, pos + 1);

		condiations[key] = value;
	}

	qw.param("values", condiations);
	FPQuestPtr quest = qw.take();
	FPAnswerPtr answer = client->sendQuest(quest);

	if (!answer->status())
		std::cout<<"modify success!"<<std::endl;
	else
	{
		FPAReader ar(answer);
		std::cout<<"Exception!"<<std::endl;
		std::cout<<"Raiser: "<<ar.wantString("raiser")<<std::endl;
		std::cout<<"Error code: "<<ar.wantInt("code")<<std::endl;
		std::cout<<"Expection: "<<ar.wantString("ex")<<std::endl;
	}

	return 0;
}
