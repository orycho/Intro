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
//#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h"
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
	llvm::orc::ObjectLinkingLayer<> ObjectLayer;
	llvm::orc::IRCompileLayer<decltype(ObjectLayer)> CompileLayer;
	
	typedef std::function<std::unique_ptr<llvm::Module>(std::unique_ptr<llvm::Module>)>
		OptimizeFunction;

	llvm::orc::IRTransformLayer<decltype(CompileLayer), OptimizeFunction> OptimizeLayer;

	public:

	typedef decltype(CompileLayer)::ModuleSetHandleT ModuleHandleT;

	JIT()
		: TM(llvm::EngineBuilder().selectTarget())
		, DL(TM->createDataLayout())
		, CompileLayer(ObjectLayer, llvm::orc::SimpleCompiler(*TM))
		, OptimizeLayer(CompileLayer,[this](std::unique_ptr<llvm::Module> M) {
			return optimizeModule(std::move(M));
		})
	{
		llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
	}
	
	llvm::TargetMachine &getTargetMachine() { return *TM; }
	/*
	ModuleHandle createModule(void)
	{
		
	};
	*/
	ModuleHandleT addModule(std::unique_ptr<llvm::Module> M) 
	{
		// Build our symbol resolver:
		// Lambda 1: Look back into the JIT itself to find symbols that are part of
		//           the same "logical dylib".
		// Lambda 2: Search for external symbols in the host process.
		auto Resolver = llvm::orc::createLambdaResolver(
			[&](const std::string &Name) {

				if (auto Sym = findMangledSymbol(Name))
					return Sym;
				return llvm::JITSymbol(nullptr);
					//return llvm::RuntimeDyld::SymbolInfo(Sym.getAddress(), Sym.getFlags());
				//return llvm::RuntimeDyld::SymbolInfo(nullptr);
			},
			[](const std::string &S) {
				// Following code actually hidden in findMangledSymbol
				//if (auto SymAddr = llvm::RTDyldMemoryManager::getSymbolAddressInProcess(S))
				//	return llvm::RuntimeDyld::SymbolInfo(SymAddr, llvm::JITSymbolFlags::Exported);
				//return llvm::RuntimeDyld::SymbolInfo(nullptr);
				return llvm::JITSymbol(nullptr);
			});
		// Build a singlton module set to hold our module.
		std::vector<std::unique_ptr<llvm::Module>> Ms;
		Ms.push_back(std::move(M));

		// Add the set to the JIT with the resolver we created above and a newly
		// created SectionMemoryManager.
		auto H=OptimizeLayer.addModuleSet(std::move(Ms),
			llvm::make_unique<llvm::SectionMemoryManager>(),std::move(Resolver));
		ModuleHandles.push_back(H);
		return H;
	}
	
	llvm::JITSymbol findSymbol(const std::string Name) 
	{
		return findMangledSymbol(mangle(Name));
		//std::string MangledName;
		//llvm::raw_string_ostream MangledNameStream(MangledName);
		//llvm::Mangler::getNameWithPrefix(MangledNameStream, Name, DL);
		//return OptimizeLayer.findSymbol(MangledNameStream.str(), true);
	}

	void removeModule(ModuleHandleT H) 
	{
		ModuleHandles.erase(
			std::find(ModuleHandles.begin(), ModuleHandles.end(), H));
		OptimizeLayer.removeModuleSet(H);
	}

private:
	std::unique_ptr<llvm::Module> optimizeModule(std::unique_ptr<llvm::Module> M) 
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
		//FPM->add(llvm::createDeadCodeEliminationPass());
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
	
	std::vector<ModuleHandleT> ModuleHandles;

	
	std::string mangle(const std::string &Name) 
	{
		std::string MangledName;
		{
			llvm::raw_string_ostream MangledNameStream(MangledName);
			llvm::Mangler::getNameWithPrefix(MangledNameStream, Name, DL);
		}
		return MangledName;
	}
  
	llvm::JITSymbol findMangledSymbol(const std::string &Name) 
	{
		if (auto Sym = OptimizeLayer.findSymbol(Name, false))
			return Sym;
		// If we can't find the symbol in the JIT, try looking in the host process.
		if (auto SymAddr = llvm::RTDyldMemoryManager::getSymbolAddressInProcess(Name))
			return llvm::JITSymbol(SymAddr, llvm::JITSymbolFlags::Exported);
		return nullptr;
	}
};
	
} // end namespace intro


#endif // LLVM_EXECUTIONENGINE_ORC_KALEIDOSCOPEJIT_H