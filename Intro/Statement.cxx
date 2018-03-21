#include "stdafx.h"
#include "Statement.h"
#include "Parser.h"
#include "CodeGenEnvironment.h"

#include <unordered_map>
struct SourceFile
{
	enum State
	{
		Loading,
		Done
	};

	std::wstring path; // move to hashmap key
	/// The statements that where parsed from the file
	std::list<intro::Statement*> statements;
	/// The environment used for type inference of the statements
	intro::Environment env;
	/** The CGEnv containing any identifiers the file did not put in modules.
		Allocate this later, as we don't have the parent CGEnv when creating this
	*/
	intro::CodeGenEnvironment *cgenv;
	State state;
	llvm::Module *current;

	SourceFile()
		: cgenv(nullptr)
		, state(Loading)
		, current(nullptr)
	{}

	~SourceFile()
	{
		delete cgenv;
		for (auto stmt : statements)
			delete stmt;
	}
};

static std::unordered_map<std::wstring, SourceFile*> files;

void cleanupSourceFiles()
{
	for (auto file : files)
	{
		delete file.second;
	}
}

/* TODO:
	change behavior to store env created locally when successfully compiled.
	Storage by file name, so multiple source-ing does not cause 
	the file to be loaded multiple times.
	instead just import the contents into the current scope
	when sourced multiple times.
	Also adapt codegen appropriately, generate the code only
	if not already stored as CodeGenEnv.
*/
bool intro::SourceStatement::makeType(Environment *env, ErrorLocation *errors)
{
	std::unordered_map<std::wstring, SourceFile*>::iterator iter=files.find(path);
	if (iter == files.end())
	{
		parse::Scanner scanner(path.c_str());
		if (!scanner.isOk())
		{
			//std::wcout << L"Error: Could not load file '"
			//	<< path << "'!\n";
			errors->addError(new ErrorDescription(getLine(), getColumn(), std::wstring(L"Failed to load file")));
			return false;
		}

		SourceFile *file = new SourceFile();
		iter = files.insert(std::make_pair(path, file)).first;
		// make sure we can find files in progress

		parse::Parser parser(&scanner);
		parser.isInteractive = false;
		parser.Parse();

		if (parser.errors->count > 0)
		{
			std::wcout << L"Found " << parser.errors->count << L" errors while parsing file "
				<< path << "!\n";
			delete file;
			return false;
		}
		file->statements.insert(file->statements.end(), parser.parseResult.begin(), parser.parseResult.end());
		parser.parseResult.clear();
		bool isOK = true;
		ErrorLocation *logger = new ErrorLocation(0, 0, std::wstring(L"source file ") + path);
		for (auto iter = file->statements.begin();isOK && iter != file->statements.end();iter++)
		{
			isOK = (*iter)->makeType(&(file->env),logger);
		}
		if (!isOK)
		{
			//std::wcout << L"Type errors encountered while parsing file "
			//	<< path << "!\n";
			errors->addError(logger);
			delete file;
			return false;
		}
		delete logger;
		file->state = SourceFile::Done;
	}
	else if (iter->second->state != SourceFile::Done)
	{
		//std::wcout << L"Circular dependency for file '"
		//	<< path << "'(it tries to source itself, possibly indirectly)! \n";
		errors->addError(new ErrorDescription(getLine(), getColumn(), L""));
		return false;

	}
	if (!iter->second->env.importInto(env))
	{
		std::wcout << L"Error while importing symbols from file '"
			<< path << "': a symbol defined in it already exists! \n";
		return false;
	}
	return true;
};

extern std::unique_ptr<llvm::Module> TheModule;

bool intro::SourceStatement::codeGen(llvm::IRBuilder<>& TmpB, intro::CodeGenEnvironment * env)
{
	bool isOK = true;
	std::unordered_map<std::wstring, SourceFile*>::iterator iter = files.find(path);
	if (iter->second->cgenv == nullptr)
	{
		intro::CodeGenEnvironment *cgenv = new intro::CodeGenEnvironment(nullptr, intro::CodeGenEnvironment::GlobalScope);
		iter->second->cgenv = cgenv;
		for (auto inner = iter->second->statements.begin()
			;isOK && inner != iter->second->statements.end()
			;inner++)
			isOK &= (*inner)->codeGen(TmpB, cgenv);
	}
	CodeGenEnvironment::iterator eit;
	if (iter->second->current != TheModule.get())
	{
		iter->second->cgenv->addExternalsForGlobals();
		iter->second->current = TheModule.get();
	}
	for (eit = iter->second->cgenv->begin();eit != iter->second->cgenv->end();eit++)
	{
		//std::wcout << "Found import: " << eit->first << "!\n";
		CodeGenEnvironment::iterator iter = env->importElement(eit->first, eit->second);
	}
	return isOK;
}