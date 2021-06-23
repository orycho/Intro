#ifndef JIT_H
#define JIT_H

#include "llvm/ADT/StringRef.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/IRTransformLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/TargetProcessControl.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/Scalar/GVN.h"

#include <memory>
#include <string>
#include <vector>

namespace intro 
{

/// Simple JIT class based on LLVM Tutorials
class JIT {
private:

	std::unique_ptr<llvm::orc::TargetProcessControl> TPC;
	std::unique_ptr<llvm::orc::ExecutionSession> ES;

	llvm::DataLayout DL;
	llvm::orc::MangleAndInterner Mangle;


	llvm::orc::RTDyldObjectLinkingLayer ObjectLayer;
	llvm::orc::IRCompileLayer CompileLayer;
	llvm::orc::IRTransformLayer OptimizeLayer;


	llvm::orc::JITDylib& MainJD;

public:

	JIT(std::unique_ptr<llvm::orc::TargetProcessControl> TPC,
		std::unique_ptr<llvm::orc::ExecutionSession> ES,
		llvm::orc::JITTargetMachineBuilder JTMB, llvm::DataLayout DL)
		: TPC(std::move(TPC))
		, ES(std::move(ES))
		, DL(std::move(DL))
		, Mangle(*this->ES, this->DL)
		, ObjectLayer(*this->ES,
			[]() { return std::make_unique<llvm::SectionMemoryManager>(); })
		, CompileLayer(*this->ES, ObjectLayer,
			std::make_unique<llvm::orc::ConcurrentIRCompiler>(std::move(JTMB)))
		, OptimizeLayer(*this->ES, CompileLayer, optimizeModule)
		, MainJD(this->ES->createBareJITDylib("<main>")) 
	{
		MainJD.addGenerator(
			cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
				DL.getGlobalPrefix())));
		ObjectLayer.setOverrideObjectFlagsWithResponsibilityFlags(true);
	}

	~JIT() {
		if (auto Err = ES->endSession())
			ES->reportError(std::move(Err));
	}

	static llvm::Expected<std::unique_ptr<JIT>> create() {
		auto SSP = std::make_shared<llvm::orc::SymbolStringPool>();
		auto TPC = llvm::orc::SelfTargetProcessControl::Create(SSP);
		if (!TPC)
			return TPC.takeError();

		auto ES = std::make_unique<llvm::orc::ExecutionSession>(std::move(SSP));

		llvm::orc::JITTargetMachineBuilder JTMB((*TPC)->getTargetTriple());

		auto DL = JTMB.getDefaultDataLayoutForTarget();
		if (!DL)
			return DL.takeError();

		return std::make_unique<JIT>(std::move(*TPC), std::move(ES),
			std::move(JTMB), std::move(*DL));
	}
	
	const llvm::DataLayout &getDataLayout() const { return DL; }

	llvm::orc::JITDylib &getMainJITDylib() { return MainJD; }

	llvm::Error addModule(llvm::orc::ThreadSafeModule TSM, llvm::orc::ResourceTrackerSP RT = nullptr) {
		if (!RT)
			RT = MainJD.getDefaultResourceTracker();

		return OptimizeLayer.add(RT, std::move(TSM));
	}

	llvm::Expected<llvm::JITEvaluatedSymbol> lookup(llvm::StringRef Name) {
		std::cout << "Name: " << Name.str() << " mangled to " << (*Mangle(Name.str())).str() << std::endl;
		return ES->lookup({ &MainJD },Name.str());
		//return ES->lookup({ &MainJD }, Mangle(Name.str()));
	}

private:
	static llvm::Expected<llvm::orc::ThreadSafeModule> optimizeModule(llvm::orc::ThreadSafeModule TSM
		, const llvm::orc::MaterializationResponsibility& R)
	{
		TSM.withModuleDo([](llvm::Module& M) 
		{
			// Create a function pass manager.
			auto FPM = std::make_unique<llvm::legacy::FunctionPassManager>(&M);

			// Add some optimizations.
			FPM->add(llvm::createVerifierPass());
			//FPM->add(llvm::createPromoteMemoryToRegisterPass());
			FPM->add(llvm::createInstructionCombiningPass());
			FPM->add(llvm::createReassociatePass());
			//FPM->add(llvm::createJumpThreadingPass());
			FPM->add(llvm::createGVNPass());
			FPM->add(llvm::createDeadCodeEliminationPass());
			FPM->add(llvm::createCFGSimplificationPass());
			FPM->doInitialization();

			// Run the optimizations over all functions in the module being added to
			// the JIT.
			for (auto &F : M)
			{
				FPM->run(F);
			}
			M.dump();
		});
		//M->dump();
		return std::move(TSM);
	}
};
	
} // end namespace intro


#endif