#ifndef JIT_H
#define JIT_H

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
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
namespace intro 
{

/// Simple JIT class based on LLVM Tutorials
class JIT {
private:

	std::unique_ptr<llvm::TargetMachine> TM;
	const llvm::DataLayout DL;
	llvm::orc::RTDyldObjectLinkingLayer ObjectLayer;
	llvm::orc::IRCompileLayer<decltype(ObjectLayer), llvm::orc::SimpleCompiler> CompileLayer;

	using OptimizeFunction =
		std::function<std::shared_ptr<llvm::Module>(std::shared_ptr<llvm::Module>)>;

	llvm::orc::IRTransformLayer<decltype(CompileLayer), OptimizeFunction> OptimizeLayer;

public:

	using ModuleHandle = decltype(OptimizeLayer)::ModuleHandleT;

	//typedef decltype(CompileLayer)::ModuleSetHandleT ModuleHandleT;

	JIT()
		: TM(llvm::EngineBuilder().selectTarget())
		, DL(TM->createDataLayout())
		, ObjectLayer([]() { return std::make_shared<llvm::SectionMemoryManager>(); 
		})
		, CompileLayer(ObjectLayer, llvm::orc::SimpleCompiler(*TM))
		, OptimizeLayer(CompileLayer,[this](std::shared_ptr<llvm::Module> M) {
			return optimizeModule(std::move(M));
		}) 
	{
		llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
	}

	
	llvm::TargetMachine &getTargetMachine() { return *TM; }

	ModuleHandle addModule(std::unique_ptr<llvm::Module> M)
	{
		auto Resolver = llvm::orc::createLambdaResolver(
			[&](const std::string &Name) {
			if (auto Sym = OptimizeLayer.findSymbol(Name, false))
				return Sym;
			return llvm::JITSymbol(nullptr);
		},
			[](const std::string &Name) {
			if (auto SymAddr =
				llvm::RTDyldMemoryManager::getSymbolAddressInProcess(Name))
				return llvm::JITSymbol(SymAddr, llvm::JITSymbolFlags::Exported);
			return llvm::JITSymbol(nullptr);
		});

		// Add the set to the JIT with the resolver we created above and a newly
		// created SectionMemoryManager.
		return cantFail(OptimizeLayer.addModule(std::move(M),
			std::move(Resolver)));
	}
	
	llvm::JITSymbol findSymbol(const std::string Name) 
	{
		std::string MangledName;
		llvm::raw_string_ostream MangledNameStream(MangledName);
		llvm::Mangler::getNameWithPrefix(MangledNameStream, Name, DL);
		return OptimizeLayer.findSymbol(MangledNameStream.str(), false);
	}

	void removeModule(ModuleHandle H) 
	{
		cantFail(OptimizeLayer.removeModule(H));
	}

private:
	std::shared_ptr<llvm::Module> optimizeModule(std::shared_ptr<llvm::Module> M) 
	{
		// Create a function pass manager.
		auto FPM = llvm::make_unique<llvm::legacy::FunctionPassManager>(M.get());

		// Add some optimizations.
		FPM->add(llvm::createVerifierPass());
		FPM->add(llvm::createPromoteMemoryToRegisterPass());
		FPM->add(llvm::createInstructionCombiningPass());
		FPM->add(llvm::createReassociatePass());
		//FPM->add(llvm::createJumpThreadingPass());
		//FPM->add(llvm::createGVNPass());
		FPM->add(llvm::createDeadCodeEliminationPass());
		FPM->add(llvm::createCFGSimplificationPass());
		FPM->doInitialization();

		// Run the optimizations over all functions in the module being added to
		// the JIT.
		for (auto &F : *M)
		{
			FPM->run(F);
		}
		//M->dump();
		return M;
	}
};
	
} // end namespace intro


#endif // LLVM_EXECUTIONENGINE_ORC_KALEIDOSCOPEJIT_H