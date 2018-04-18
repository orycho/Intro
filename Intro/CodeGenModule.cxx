#include "stdafx.h"
#include "CodeGenModule.h"

namespace intro
{

size_t CodeGenModule::currentModule=0;
CodeGenModule *CodeGenModule::root=nullptr;

CodeGenModule::CodeGenModule(CodeGenModule *parent_)
	: CodeGenEnvironment(parent_, CodeGenEnvironment::GlobalScope)
	, lastModule(0)
{
}


CodeGenModule::~CodeGenModule()
{
	for (auto child : children)
		delete child.second;
}

CodeGenModule *CodeGenModule::getRoot(void)
{
	if (root == nullptr)
	{
		root = new CodeGenModule(nullptr);
	}
	return root;
}
}