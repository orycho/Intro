#include "stdafx.h"
#include "Statement.h"
#include "Parser.h"

bool intro::SourceStatement::makeType(Environment *env)
{
	parse::Scanner scanner(path.c_str());
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();

	if (parser.errors->count>0)
	{
		std::wcout << L"Found " << parser.errors->count << L" errors while parsing file "
			<< path <<"!\n";
		return false;
	}
	statements.insert(statements.end(), parser.parseResult.begin(), parser.parseResult.end());
	parser.parseResult.clear();
	bool isOK = true;
	for (auto iter = statements.begin();isOK && iter != statements.end();iter++)
	{
		isOK = (*iter)->makeType(env);
	}
	if (!isOK)
	{
		std::wcout << L"Type errors encountered while parsing file "
			<< path << "!\n";
		return false;
	}
	return true;
};