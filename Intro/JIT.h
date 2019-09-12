#ifndef JIT_H
#define JIT_H
/*
#include "llvm/ADT/STLExtras.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/RuntimeDyld.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/IRTransformLayer.h"
#include "llvm/ExecutionEngine/Orc/LambdaResolver.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Mangler.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
*/

#include "llvm/ADT/StringRef.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/IRTransformLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include <memory>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>
namespace intro 
{

/// Simple JIT class based on LLVM Tutorials
class JIT {
private:
	llvm::orc::ExecutionSession ES;
	llvm::orc::RTDyldObjectLinkingLayer ObjectLayer;
	llvm::orc::IRCompileLayer CompileLayer;
	llvm::orc::IRTransformLayer TransformLayer;
	llvm::DataLayout DL;
	llvm::orc::MangleAndInterner Mangle;
	llvm::orc::ThreadSafeContext Ctx;

public:

	JIT(llvm::orc::JITTargetMachineBuilder JTMB, llvm::DataLayout DL)
		: ObjectLayer(ES,
			[]() { return llvm::make_unique<llvm::SectionMemoryManager>(); })
		, CompileLayer(ES, ObjectLayer, llvm::orc::ConcurrentIRCompiler(std::move(JTMB)))
		, TransformLayer(ES, CompileLayer, optimizeModule)
		, DL(std::move(DL)), Mangle(ES, this->DL)
		, Ctx(llvm::make_unique<llvm::LLVMContext>())
	{
		ES.getMainJITDylib().setGenerator(
			llvm::cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(DL)));
	}

	llvm::LLVMContext &getContext() { return *Ctx.getContext(); }
	const llvm::DataLayout &getDataLayout() const { return DL; }

	static llvm::Expected<std::unique_ptr<JIT>> Create() 
	{
		auto JTMB = llvm::orc::JITTargetMachineBuilder::detectHost();

		if (!JTMB)
			return JTMB.takeError();

		auto DL = JTMB->getDefaultDataLayoutForTarget();
		if (!DL)
			return DL.takeError();

		return llvm::make_unique<JIT>(std::move(*JTMB), std::move(*DL));
	}


	llvm::Error addModule(std::unique_ptr<llvm::Module> M) 
	{
		return TransformLayer.add(ES.getMainJITDylib(),
			llvm::orc::ThreadSafeModule(std::move(M), Ctx));
	}

	llvm::Expected<llvm::JITEvaluatedSymbol> lookup(llvm::StringRef Name) 
	{
		return ES.lookup({ &ES.getMainJITDylib() }, Mangle(Name.str()));
	}

private:
	static llvm::Expected<llvm::orc::ThreadSafeModule>
		optimizeModule(llvm::orc::ThreadSafeModule M, const llvm::orc::MaterializationResponsibility &R)
	{
		// Create a function pass manager.
		auto FPM = llvm::make_unique<llvm::legacy::FunctionPassManager>(M.getModule());

		// Add some optimizations.
		FPM->add(llvm::createInstructionCombiningPass());
		FPM->add(llvm::createReassociatePass());
		FPM->add(llvm::createGVNPass());
		FPM->add(llvm::createCFGSimplificationPass());
		FPM->doInitialization();

		// Run the optimizations over all functions in the module being added to
		// the JIT.
		for (auto &F : *M.getModule())
			FPM->run(F);

		return M;
	}
};
	
} // end namespace intro


#endif // LLVM_EXECUTIONENGINE_ORC_KALEIDOSCOPEJIT_H