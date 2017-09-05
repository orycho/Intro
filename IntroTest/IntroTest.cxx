// IntroTest.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#ifdef _MSC_VER
__pragma(warning(push))
__pragma(warning(disable:4355))
__pragma(warning(disable:4244))
#endif

#include "../Intro/CodeGen.h"

#include "llvm/Analysis/Passes.h"
#include "llvm/IR/Verifier.h"
//#include "llvm/ExecutionEngine/ExecutionEngine.h"
//#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
//#include "llvm/PassManager.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/Scalar.h"

#ifdef _MSC_VER

__pragma(warning(pop))

#endif

#ifdef WIN32
int wmain(int argc,wchar_t *argv[])
#else
int main(int argc,char *argv[])
#endif
{
	::testing::InitGoogleTest(&argc, argv);
	//llvm::InitializeNativeTarget();
	intro::initRuntime();
	
	//initLLVM();

	return RUN_ALL_TESTS();
}

