#ifndef CODEGEN_H
#define CODEGEN_H

#ifdef _MSC_VER
__pragma(warning(push))
__pragma(warning(disable:4355))
__pragma(warning(disable:4244))
#endif

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/Scalar.h"
#include "JIT.h"

#ifdef _MSC_VER
__pragma(warning(pop))
#endif

#ifdef WIN32
typedef unsigned __int64 __uint64;
#else
typedef long long int __int64;
typedef unsigned long long int __uint64;
#endif

#include "Type.h"
#include "Intro.h"

namespace intro
{
	extern std::unique_ptr<llvm::LLVMContext> theContext;
	extern llvm::IRBuilder<> Builder;
	extern std::unique_ptr<llvm::Module> TheModule;

	void initRuntime(void);

	enum ClosureParts
	{
		// GCData=0,
		ClosureFunction = 1, ///< Pointer to function holding code for closure
		ClosureFieldCount,
		ClosureFields
	};

	enum GeneratorParts
	{
		// GCData=0,
		GeneratorFunction = 1,
		GeneratorClosureCount,
		GeneratorFieldCount,
		GeneratorRetVal,
		GeneratorRetValType,
		GeneratorState,
		GeneratorFields
	};
	///@}
	void executeStatement(Statement *stmt);
	/// Create a constant global string in the llvm module
	llvm::Constant *createGlobalString(const std::wstring &s);
	/// Construct a llvm::Type that  reprsents the given Type during runtime
	llvm::Type *toTypeLLVM(Type::pointer_t type);
	/// Create code for type specific unboxing of a value
	llvm::Value *createUnboxValue(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env, llvm::Value *field, intro::Type::Types type);
	/// Create code for type specific boxing of a value
	llvm::Value *createBoxValue(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env, llvm::Value *field, intro::Type::Types type);
}

#endif