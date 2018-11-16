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
#include "ErrorLogger.h"

#include "Runtime.h"

void runBasicTests(void);

namespace intro
{
	void initModule(void);
	void initRuntime(void);
	void deleteRuntime(void);
	void dumpModule(bool optimize=false);
	void runStatements(const std::vector<intro::Statement*> &statements);
	void cleanupSourceFiles();

	//extern llvm::LLVMContext theContext;
	//extern std::unique_ptr<llvm::Module> TheModule;
	//extern intro::CodeGenEnvironment global;
	//extern llvm::IRBuilder<> Builder;
}

namespace llvm
{
	void llvm_shutdown();
}

/*
	#include <cstdlib>
	std::string output_path;
	std::string cmdbuffer("llvm-link -o " + output_path + "-linked.buf.bc IntroRuntime.bc " + gerated_code_file);
	std::system(cmdbuffer.c_str());
	cmdbuffer="llc -O3 -o" + output_path +" "+ output_path + "-linked.buf.bc";
	std::system(cmdbuffer.c_str());
*/

llvm::cl::opt<std::string> Script(llvm::cl::Positional, llvm::cl::desc("<input script>"), llvm::cl::init("-"));
llvm::cl::list<std::string>  Argv(llvm::cl::ConsumeAfter, llvm::cl::desc("<program arguments>..."));
llvm::cl::opt<bool> runTests("test", llvm::cl::desc("Run hard-coded basic tests"));
llvm::cl::opt<bool> runTestsOpt("testopt", llvm::cl::desc("Run hard-coded basic tests with optimization (overrides test)"));

//#include <dlfcn.h>

int main(int argc,const char *argv[])
{
	llvm::cl::ParseCommandLineOptions(argc, argv);
	
	intro::initRuntime();
	
	//std::string error;
	//bool ok=llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr,&error);
	//std::cout << "Loaded OK: " << ok << " with msg "<<error<<std::endl;
	/*
	void* handle = ::dlopen(nullptr, RTLD_LAZY|RTLD_GLOBAL);
	std::cout << "Handle is " << (uint64_t)handle << " vs default " << (uint64_t)RTLD_DEFAULT  <<std::endl;
	
	uint64_t sym1 = (uint64_t) ::dlsym(handle, "allocString");
	uint64_t sym2 = (uint64_t) ::dlsym(RTLD_DEFAULT, "allocString");
	std::cout << "Sym from handle is "<< sym1 << " vs RTLD_DEFAULT " << sym2  <<std::endl;
	
	uint64_t ptr=(uint64_t)llvm::sys::DynamicLibrary::SearchForAddressOfSymbol("allocString");
	std::cout << "Pointer is "<<ptr<<std::endl;
	*/
	if (runTests || runTestsOpt)
	{
		intro::initModule();
		runBasicTests();
		intro::dumpModule(runTestsOpt);
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
					std::wstring pathbuf;
					pathbuf.assign(Script.begin(), Script.end());
					intro::ErrorLocation *logger = new intro::ErrorLocation(0, 0, std::wstring(L"file ") + pathbuf);
					intro::Environment global;
					bool isOK = true;
					for (auto iter = parser.parseResult.begin();isOK && iter != parser.parseResult.end();iter++)
					{
						isOK = (*iter)->makeType(&global,logger);
						/*
						if (!isOK)
						{
							std::wcout << L"Error in line:\n";
							(*iter)->print(std::wcout);
							std::wcout << L"\n";
						}
						*/
					}
					if (isOK)
						runStatements(parser.parseResult);
					else
					{
						std::wcout << L"Type errors detected!!!\n";
						logger->print(std::wcout,0);
						delete logger;
						logger = nullptr;
					}
				}
				//getchar();
				parser.deleteStatements();
			}
		}
	}
	// should decrement all gloals to make VLD shut up about them
	intro::Environment::deleteAllModules();
	intro::cleanupSourceFiles();
	intro::Environment::clearTypeVariables();
	intro::deleteRuntime();
	intro::CodeGenModule::deleteRootModule();
	intro::Environment::deleteRootModule();
	llvm::llvm_shutdown();
	
	return 0;
}

