#include "stdafx.h"
#include "CodeGenModule.h"

namespace intro
{

CodeGenModule *CodeGenModule::root=nullptr;

CodeGenModule::CodeGenModule(CodeGenModule *parent_)
	: CodeGenEnvironment(parent_, CodeGenEnvironment::GlobalScope)
	, lastModule(nullptr)
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