#ifndef CODEGENMODULE_H
#define CODEGENMODULE_H

#include "CodeGenEnvironment.h"
#include <unordered_map>
#include <string>

namespace intro 
{

/** Class representing a Module during code generation.
	Inherits from CodeGenEnvironment and adds methods
	and members to represent a hierarchy of modules.
	This class follows the singledton pattern, instances are only created
	with getRoot() and getPath().
*/
class CodeGenModule : public CodeGenEnvironment
{
	std::unordered_map<std::wstring, CodeGenModule*> children;

	/// Use get Root or getPath to create and access modules?!
	CodeGenModule(CodeGenModule *parent_);
	/// The root module is static
	static CodeGenModule *root;
	/// Iterator for child modules
	typedef std::unordered_map<std::wstring, CodeGenModule*>::iterator child_iter;
	/// Store the last seen Module
	//llvm::Module *lastModule;
	size_t lastModule;
	static size_t currentModule;
public:
	
	static void nextLLVMModule(void) { currentModule++;  }

	virtual ~CodeGenModule();

	/// Returns the interpreters root module (creates if needed)
	static CodeGenModule *getRoot(void);

	static void deleteRootModule(void)
	{
		delete root;
		root = nullptr;
	}
	
	/// Return the module with the given path, creates all intermediate if necessary.
	CodeGenModule *getRelativePath(std::vector<std::wstring> path)
	{
		CodeGenModule *current = this;
		for (const auto module_name : path)
		{
			child_iter iter = current->children.find(module_name);
			if (iter == current->children.end())
			{
				CodeGenModule *next = new CodeGenModule(current);
				iter = current->children.insert(std::make_pair(module_name, next)).first;
			}
			current = iter->second;
		}
		return current;
	}

	static CodeGenModule *getAbsolutePath(std::vector<std::wstring> path)
	{
		return getRoot()->getRelativePath(path);
	}

	virtual CodeGenModule *getCurrentModule(void)
	{
		return this;
	}

	//void declareInterfaceIfNeeded(llvm::Module *current)
	void declareInterfaceIfNeeded(void)
	{
		if (lastModule == currentModule) return;
		addExternalsForGlobals();
		lastModule = currentModule;
	}
};

}
#endif