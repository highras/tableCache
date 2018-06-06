#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include "ignoreSignals.h"
#include "TCPClient.h"
#include "FPWriter.h"
#include "FPReader.h"

using namespace fpnn;

void printResult(const std::vector<std::string>& fields, const std::map<std::string, std::vector<std::string>>& data)
{
	const char *hintIdFiled = "hintId";
	std::vector<size_t> fieldLens(fields.size() + 1, 0);

	fieldLens[0] = strlen(hintIdFiled);
	for (size_t i = 0; i < fields.size(); ++i)
		fieldLens[i+1] = fields[i].length();

	for (const auto pr: data)
	{
		if (pr.first.length() > fieldLens[0])
				fieldLens[0] = pr.first.length();

		for(size_t j = 0; j < pr.second.size(); ++j)
			if (pr.second[j].length() > fieldLens[j+1])
				fieldLens[j+1] = pr.second[j].length();
	}

	//-- top
	std::cout<<"+";
	for (size_t i = 0; i < fieldLens.size(); i++)
		std::cout<<std::string(fieldLens[i] + 2, '-')<<'+';
	std::cout<<std::endl;

	//-- fiels
	std::cout<<"|";
	std::cout<<' '<<hintIdFiled;
	if (strlen(hintIdFiled) < fieldLens[0])
		std::cout<<std::string(fieldLens[0] - strlen(hintIdFiled), ' ');

	std::cout<<" |";
	for(size_t i = 0; i < fields.size(); ++i)
	{
		std::cout<<' '<<fields[i];
		if (fields[i].length() < fieldLens[i+1])
			std::cout<<std::string(fieldLens[i+1] - fields[i].length(), ' ');

		std::cout<<" |";
	}
	std::cout<<std::endl;

	//-- separator
	std::cout<<"+";
	for (size_t i = 0; i < fieldLens.size(); i++)
		std::cout<<std::string(fieldLens[i] + 2, '=')<<'+';
	std::cout<<std::endl;

	//-- data
	for (const auto pr: data)
	{
		std::cout<<"|";

		std::cout<<' '<<pr.first;
		if (pr.first.length() < fieldLens[0])
			std::cout<<std::string(fieldLens[0] - pr.first.length(), ' ');

		std::cout<<" |";
		for(size_t j = 0; j < pr.second.size(); ++j)
		{
			std::cout<<' '<<pr.second[j];
			if (pr.second[j].length() < fieldLens[j+1])
				std::cout<<std::string(fieldLens[j+1] - pr.second[j].length(), ' ');

			std::cout<<" |";
		}
		std::cout<<std::endl;
	}

	//-- tail line
	std::cout<<"+";
	for (size_t i = 0; i < fieldLens.size(); i++)
		std::cout<<std::string(fieldLens[i] + 2, '-')<<'+';
	std::cout<<std::endl;

	std::cout<<data.size()<<" rows in results."<<std::endl;
}

void showUsage(const char* appname)
{
	std::cout<<"Usage: "<<std::endl;
	std::cout<<"\t"<<appname<<" host:port <table> < -i | -s > <hintId> field1 [field2 ...]"<<std::endl;
	std::cout<<"\t"<<appname<<" host:port <table> < -mi | -ms > <hintId> -f field1 [field2 ...]"<<std::endl;
	exit(1);
}
int main(int argc, const char* argv[])
{
	if (argc < 6)
		showUsage(argv[0]);
	
	ignoreSignals();
	bool hintIsInt = true;
	int fieldStartIdx = 4;
	std::vector<std::string> fields;
	std::shared_ptr<TCPClient> client = TCPClient::createClient(argv[1]);
	FPQWriter qw(3, "fetch");
	qw.param("table", argv[2]);

	if (strcmp(argv[3], "-i") == 0)
	{
		qw.param("hintId", atoll(argv[4]));
		fieldStartIdx++;
	}
	else if (strcmp(argv[3], "-s") == 0)
	{
		hintIsInt = false;
		qw.param("hintId", argv[4]);
		fieldStartIdx++;
	}
	else if (strcmp(argv[3], "-mi") == 0)
	{
		std::vector<int64_t> hintIds;
		while (fieldStartIdx < argc)
		{
			if (strcmp(argv[fieldStartIdx], "-f") == 0)
			{
				fieldStartIdx++;
				break;
			}
			hintIds.push_back(atoll(argv[fieldStartIdx]));
			fieldStartIdx++;
		}
		qw.param("hintIds", hintIds);
	}
	else if (strcmp(argv[3], "-ms") == 0)
	{
		hintIsInt = false;
		std::vector<std::string> hintIds;
		while (fieldStartIdx < argc)
		{
			if (strcmp(argv[fieldStartIdx], "-f") == 0)
			{
				fieldStartIdx++;
				break;
			}
			hintIds.push_back(argv[fieldStartIdx]);
			fieldStartIdx++;
		}
		qw.param("hintIds", hintIds);

	}
	else
		showUsage(argv[0]);

	while (fieldStartIdx < argc)
	{
		fields.push_back(argv[fieldStartIdx]);
		fieldStartIdx++;
	}

	qw.param("fields", fields);

	FPQuestPtr quest = qw.take();
	FPAnswerPtr answer = client->sendQuest(quest);
	FPAReader ar(answer);

	if (!answer->status())
	{
		std::map<std::string, std::vector<std::string>> data;
		if (!hintIsInt)
			data = ar.want("data", data);
		else
		{
			std::map<int64_t, std::vector<std::string>> tmp;
			tmp = ar.want("data", tmp);

			for (auto& pr: tmp)
				data[std::to_string(pr.first)] = pr.second;
		}

		printResult(fields, data);
	}
	else
	{
		std::cout<<"Exception!"<<std::endl;
		std::cout<<"Raiser: "<<ar.wantString("raiser")<<std::endl;
		std::cout<<"Error code: "<<ar.wantInt("code")<<std::endl;
		std::cout<<"Expection: "<<ar.wantString("ex")<<std::endl;
	}
	return 0;
}
