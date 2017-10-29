#include "stdafx.h"
#include "../Intro/Parser.h"
#include "utils.h"

TEST(UtilityTests, CollectFunctionsTest) {
	char *test="var f(a,b) -> return fun(x) -> return a*x+b; end; end;";
	parse::Parser::iterator iter;
	parse::Scanner scanner((unsigned char *)test,strlen(test));
	parse::Parser parser(&scanner);
	parser.Parse();
	iter=parser.parseResult.begin();
	std::list<intro::Function*> funcs;
	(*iter)->collectFunctions(funcs);
	ASSERT_EQ(funcs.size(),2);
	intro::VariableSet free,bound;
	funcs.front()->getFreeVariables(free,bound);
	ASSERT_EQ(free.size(),0);
	ASSERT_EQ(bound.size(),0);
	free.clear();
	bound.clear();
	funcs.back()->getFreeVariables(free,bound);
	ASSERT_EQ(free.size(),2);
	ASSERT_EQ(bound.size(),0);
	int y=0;
}
