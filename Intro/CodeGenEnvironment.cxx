#include "stdafx.h"
#include "CodeGenEnvironment.h"
#include "CodeGen.h"

using namespace llvm;
using namespace std;

namespace intro 
{

	extern llvm::LLVMContext theContext;
	
	extern llvm::PointerType *builtin_t;
	extern llvm::Type *rttype_t;
	extern llvm::StructType *closure_t, *generator_t, *field_t;
	
	llvm::AllocaInst *CodeGenEnvironment::createEntryBlockAlloca(const std::wstring &VarName) 
	{
		llvm::Function *TheFunction = currentFunction();
		if (TheFunction==NULL) return NULL;
		llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),TheFunction->getEntryBlock().begin());
		return TmpB.CreateAlloca(builtin_t, 0,std::string(VarName.begin(),VarName.end()));
	}

	void CodeGenEnvironment::createReturnVariable(llvm::Value *retValType)
	{
		llvm::Value *address = createEntryBlockAlloca(L"$retval");
		elements.insert(std::make_pair(L"$retval", element(address, retValType)));
	}

	llvm::AllocaInst *CodeGenEnvironment::createRetValRttDest() 
	{
		llvm::Function *TheFunction = currentFunction();
		if (TheFunction==NULL) return NULL;
		llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),TheFunction->getEntryBlock().begin());
		return TmpB.CreateAlloca(rttype_t, 0,"$retvalrtt");
	}

	llvm::Value *CodeGenEnvironment::getGeneratorClosure(llvm::IRBuilder<> &builder)
	{
		CodeGenEnvironment *env = getWrappingEnvironment();
		if (env == nullptr) return nullptr;
		return builder.CreateBitCast(env->closure, builtin_t, "boxed_gen");
		//return env->generatorClosure; 
	}

	
	CodeGenEnvironment::iterator CodeGenEnvironment::createGeneratorVariable(const std::wstring &VarName) 
	{
		// If in generator, allocate data in closure, store adress from GEP into struct
		// index in closuresize
		CodeGenEnvironment *env=getWrappingEnvironment();
		if (env==nullptr)
			return elements.end();
		std::string name(VarName.begin(),VarName.end());
		name+="ingen";
		llvm::Function *TheFunction = env->function;
		llvm::Value *generator=env->closure;
		std::uint32_t field_index=(std::uint32_t)env->closuresize;
		++(env->closuresize);
		llvm::BasicBlock::iterator instr_iter = TheFunction->getEntryBlock().begin();
		instr_iter++;
		llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(), instr_iter);
		//Builder.SetInsertPoint(I->getNextNode());
		llvm::Value *idxs[]= {
			llvm::ConstantInt::get(llvm::Type::getInt32Ty(theContext), 0), // the struct directly pointed to
			llvm::ConstantInt::get(llvm::Type::getInt32Ty(theContext), GeneratorFields), // index of fields array in struct
			llvm::ConstantInt::get(llvm::Type::getInt32Ty(theContext), field_index), // index of field in array
			llvm::ConstantInt::get(llvm::Type::getInt32Ty(theContext), 0)	// Value of field
		};
		llvm::Value *fieldptr=TmpB.CreateGEP(generator_t,generator,idxs,name);
		idxs[3]=llvm::ConstantInt::get(llvm::Type::getInt32Ty(theContext), 1);	// Type of  field
		llvm::Value *fieldrtt=TmpB.CreateGEP(generator_t,generator,idxs,name+"!rtt");
		//fieldrtt=TmpB.CreateLoad(fieldrtt,"gen_variable_rttval");
		
		return elements.insert(std::make_pair(VarName,element(fieldptr,fieldrtt))).first;
	}

	llvm::Value *CodeGenEnvironment::getRTT(rtt::RTType rtt)
	{
		return llvm::ConstantInt::get(llvm::Type::getInt16Ty(theContext), rtt,false);
	}
	
	llvm::Value *CodeGenEnvironment::getRTT(Type *type)
	{
		return getRTT(type->find()->getRTKind());
	}
	
	void CodeGenEnvironment::addExternalsForGlobals(void)
	{
		assert(scope_type==GlobalScope);
		
		for (auto &&elem : elements)
		{
			std::string name;
			if (elem.second.altname.empty()) name.append(elem.first.begin(), elem.first.end());
			else name = elem.second.altname;

			std::string rttname = name;
			if (elem.second.altname.empty()) rttname+="!rtt";
			else rttname += "_rtt"; // Done so that RTL function defined in C can have their rtt defined.

			TheModule->getOrInsertGlobal(name, builtin_t);
			GlobalVariable *gVar = TheModule->getNamedGlobal(name);
			gVar->setLinkage(GlobalValue::ExternalLinkage);
			elem.second.address=gVar;
			
			TheModule->getOrInsertGlobal(rttname, rttype_t);
			GlobalVariable *gRtt = TheModule->getNamedGlobal(rttname);
			gRtt->setLinkage(GlobalValue::ExternalLinkage);
			elem.second.rtt=gRtt;
		}
	}
	
	
	CodeGenEnvironment::iterator CodeGenEnvironment::createVariable(const std::wstring &name)
	{
		llvm::Value *address=nullptr;
		llvm::Value *rtt=nullptr;
		std::string rtname(name.begin(),name.end());
		switch(getScopeType())
		{
		case GlobalScope:
		{
			address =
				new llvm::GlobalVariable(*TheModule,builtin_t, false,
									 llvm::GlobalValue::CommonLinkage,
									 llvm::ConstantPointerNull::get(builtin_t),
									 rtname);
			rtt =
				new llvm::GlobalVariable(*TheModule,rttype_t, false,
									 llvm::GlobalValue::CommonLinkage,
									 llvm::ConstantInt::get(llvm::Type::getInt16Ty(theContext), 0,false),
									 rtname+"!rtt");
		}
			break;
		case Closure:
		case LocalScope:
		{
			llvm::Function *TheFunction = currentFunction();
			if (TheFunction==NULL) return elements.end();
			llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),TheFunction->getEntryBlock().begin());
			address=TmpB.CreateAlloca(builtin_t, 0,rtname);
			rtt=TmpB.CreateAlloca(rttype_t, 0,rtname+"!rtt");
		}
			break;
		case Generator:
			return createGeneratorVariable(name);
		}
		if (address==NULL) return elements.end();
		address->setName(rtname);
		rtt->setName(rtname+"!rtt");
		return elements.insert(std::make_pair(name,element(address,rtt))).first;
	}

	void CodeGenEnvironment::setGenerator(llvm::IRBuilder<> &builder,llvm::Function *TheGenerator,
		const std::vector<std::wstring> &free) 
	{ 
		assert(scope_type == Generator);
		function=TheGenerator;
		double_buffer=createEntryBlockAlloca(L"$double_bufferG");
		llvm::Function::arg_iterator AI = function->arg_begin();
		// only Param: generator pointer
		AI->setName("generator_in");
		closure=&*AI;
		setClosure(builder,closure,generator_t,free);

		llvm::Function *getStateF = TheModule->getFunction("getStateGenerator");
		std::vector<llvm::Value*> argsGetState { 
			builder.CreateBitCast(closure, builtin_t, "boxed_gen")
		};
		llvm::Value *gen_state=builder.CreateCall(getStateF, argsGetState ,"gen_state");
		leave = llvm::BasicBlock::Create(theContext, "leave");
		generatorStateDispatch=builder.CreateSwitch(gen_state,leave); 
		llvm::BasicBlock *statezero = llvm::BasicBlock::Create(theContext, "state_zero",function);
		generatorStateDispatch->addCase(llvm::ConstantInt::get(llvm::Type::getInt64Ty(theContext), 0,false),statezero);
		builder.SetInsertPoint(statezero);
	}
	
	void CodeGenEnvironment::setFunction(llvm::IRBuilder<> &builder,llvm::Function *TheFunction,
		Function::ParameterList &params,bool returnsValue, const std::vector<std::wstring> &free)
	{
		//assert(scope_type==LocalScope || scope_type==Generator);
		assert(scope_type == Closure);
		function=TheFunction;
		double_buffer=createEntryBlockAlloca(L"$double_bufferF");
		llvm::Function::arg_iterator AI = function->arg_begin();
		// 1st Param: Closure pointer
		AI->setName("closure_in");
		closure = &*AI;
		// If the parent is defining a name (usual case), it is this function
		if (!parent->outerValueName.empty())
		{
			//elements.insert(std::make_pair(parent->outerValueName, element(closure, getRTT(rtt::Function))));
			addParameter(builder, parent->outerValueName, closure, getRTT(rtt::Function));
		}
		// replace reference to myclosure with closure_t??
		llvm::Value *closureparam=builder.CreateBitCast(&*AI,closure_t->getPointerTo(),"closurecast");
		// 2nd Param: return value type, if a value is returned
		++AI;
		llvm::Value *retvalTypePtr=nullptr;
		if (returnsValue) 
		{
			AI->setName("retvalrtt");
			retvalTypePtr=&*AI;
			++AI;
		}
		// 3rd: All regular parameters, value and rtt.
		Function::ParameterList::iterator pit;
		for (pit=params.begin(); 
			pit!=params.end();
			++AI,++pit) 
		{
			AI->setName(std::string( pit->name.begin(), pit->name.end() ));
			llvm::Value *value=&*AI;
			++AI; // Move to run time type
			std::wstring rttname=pit->name+L"!rtt"; // give rtt a nice name
			AI->setName(std::string( rttname.begin(), rttname.end() ));
			llvm::Value *rtt=&*AI;
			addParameter(builder,pit->name,value,rtt);
		}
		// If we return a value, create return value in environment
		// (allocated on stack so that mem2reg pass can phi it out.)
		if (returnsValue) 
			createReturnVariable(retvalTypePtr);
		// Finally, add free variables from closure
		setClosure(builder,closureparam,closure_t,free);
		// We get to add the leave block for the function, not to the function
		leave = llvm::BasicBlock::Create(theContext, "leave");
	}
	
	void CodeGenEnvironment::setClosure(llvm::IRBuilder<> &builder,llvm::Value *closure, llvm::StructType *myclosure_t,const std::vector<std::wstring> &freeVars)
	{
		closuresize=freeVars.size();
		if (freeVars.empty()) return;
		std::uint32_t field_base = isInGenerator() ? (std::uint32_t)GeneratorFields:(std::uint32_t)ClosureFields;
		std::uint32_t field_index=0;
		llvm::Value *idxs[]= {
			llvm::ConstantInt::get(llvm::Type::getInt32Ty(theContext), 0), // the struct directly pointed to
			llvm::ConstantInt::get(llvm::Type::getInt32Ty(theContext), field_base), // index of fields array in struct
			nullptr, // index of field in array
			nullptr	// element of field
		};
		for (const std::wstring vars : freeVars)
		{
			idxs[2]=llvm::ConstantInt::get(llvm::Type::getInt32Ty(theContext), field_index);
			idxs[3]=llvm::ConstantInt::get(llvm::Type::getInt32Ty(theContext), 0);
			llvm::Value *fieldptr=builder.CreateGEP(myclosure_t,closure,idxs,"closure_field_ptr");
			idxs[3]=llvm::ConstantInt::get(llvm::Type::getInt32Ty(theContext), 1);	// Type of  field
			llvm::Value *fieldrtt=builder.CreateGEP(myclosure_t,closure,idxs,"closure_field_rtt");
			// Probably need to load the actual rtt value here...
			elements.insert(std::make_pair(vars,element(fieldptr,fieldrtt)));
			field_index++;
			// copy value into fieldptr, apply indexing
		}
	}

}