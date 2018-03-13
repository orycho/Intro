// Intro.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Parser.h"

#if defined(_MSC_VER) && defined(_DEBUG)
#include <vld.h>
#endif

#include <stdio.h>
#include "Environment.h"
#include <string>
#include "JIT.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/IR/Verifier.h"
#include "CodeGenEnvironment.h"
#include "CodeGenModule.h"

#include "Runtime.h"

void runBasicTests(void);

namespace intro
{
	void initModule(void);
	void initRuntime(void);
	void deleteRuntime(void);
	void dumpModule(void);
	void runStatements(const std::list<intro::Statement*> &statements);

	extern llvm::LLVMContext theContext;
	extern std::unique_ptr<llvm::Module> TheModule;
	extern intro::CodeGenEnvironment global;
	extern llvm::IRBuilder<> Builder;
}

namespace llvm
{
	void llvm_shutdown();
}

void cleanupSourceFiles();

llvm::cl::opt<std::string> Script(llvm::cl::Positional, llvm::cl::desc("<input script>"), llvm::cl::init("-"));
llvm::cl::list<std::string>  Argv(llvm::cl::ConsumeAfter, llvm::cl::desc("<program arguments>..."));
llvm::cl::opt<bool> runTests("test", llvm::cl::desc("Run hard-coded basic tests"));

//int _tmain(int argc, _TCHAR* argv[])
#ifdef WIN32
int main(int argc,const char *argv[])
//int wmain(int argc, const wchar_t *argv[])
#else
int main(int argc, char *argv[])
#endif
{
	llvm::cl::ParseCommandLineOptions(argc, argv);
	
	intro::initRuntime();
	if (runTests)
	{
		intro::initModule();
		runBasicTests();
		intro::dumpModule();
		printf("Done!\n");
		//return 0;
	}
	else
	{
		intro::Environment global;
		if (Script=="-") // Interactive Mode
		{
			// run interactive command line
			intro::initModule();
			parse::Scanner scanner(stdin);
			parse::Parser parser(&scanner);
			parser.isInteractive=true;
			parser.Parse();
			parser.deleteStatements();
		}
		else // Run script
		{
			intro::initModule();
			parse::Scanner scanner(Script.c_str());
			if (!scanner.isOk())
			{
				std::cout << "Error: File '" << Script << "'not found!\n";
			}
			else
			{
				parse::Parser parser(&scanner);
				parser.isInteractive = false;
				parser.Parse();

				if (parser.errors->count > 0)
				{
					std::wcout << L"Found " << parser.errors->count << L" errors while parsing!\n";
				}
				else
				{
					intro::Environment global;
					bool isOK = true;
					for (auto iter = parser.parseResult.begin();isOK && iter != parser.parseResult.end();iter++)
					{
						isOK = (*iter)->makeType(&global);
						if (!isOK)
						{
							std::wcout << L"Error in line:\n";
							(*iter)->print(std::wcout);
							std::wcout << L"\n";
						}
					}
					if (isOK)
						runStatements(parser.parseResult);
					else
						std::wcout << L"Type errors detected!\n";
				}
				//getchar();
				parser.deleteStatements();
			}
		}
	}
	intro::Environment::deleteAllModules();
	cleanupSourceFiles();
	intro::Environment::clearTypeVariables();
	intro::deleteRuntime();
	intro::CodeGenModule::deleteRootModule();
	intro::Environment::deleteRootModule();
	llvm::llvm_shutdown();
	
	return 0;
}

