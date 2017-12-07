#include "stdafx.h"

#include <sstream>
#include <string>

#include "Expression.h"
#include "Statement.h"
#include "Module.h"
#include "Generators.h"
#include "CodeGen.h"
#include "CodeGenEnvironment.h"
#include "CodeGenModule.h"
#include "Runtime.h"

#include  "PerfHash.h"

#ifdef _MSC_VER

__pragma(warning(push))
__pragma(warning(disable:4355))
__pragma(warning(disable:4244))

#include "llvm/IR/Verifier.h"

__pragma(warning(pop))


using namespace std::tr1;

#else 

#include "llvm/IR/Verifier.h"

#endif

#include "JIT.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Attributes.h>

#include <unordered_map>

using namespace llvm;
using namespace std;

namespace intro {

	llvm::LLVMContext theContext;
	std::unique_ptr<llvm::Module> TheModule;
	static std::unique_ptr<llvm::legacy::FunctionPassManager> TheFPM;
	static std::unique_ptr<JIT> TheJIT;

	void dumpModule(void)
	{
		TheModule->dump();
	}

	//CodeGenEnvironment global(NULL,CodeGenEnvironment::GlobalScope);


	llvm::Type *void_t;
	llvm::PointerType *builtin_t;
	llvm::Type *integer_t,*int32_t;
	llvm::Type *charptr_t;
	llvm::Type *double_t;
	llvm::Type *boolean_t;
	llvm::StructType *closure_t, *generator_t, *field_t;
	llvm::Type *rttype_t;
	llvm::FunctionType *genfunc_t;
	
/*
	/// Regular Expression used to tokenize string literals for 'string interpolation'.
	//const std::tr1::wregex tokens(L"(\\\\(?:u[[:xdigit:]]{4}|x[[:xdigit:]]+|[[:graph:]])|\\$\\{[[:graph:]]+\\}|[[:space:]]+|[^\\\\$ ]+|\\$(?!\\{))");
*/


	/////////////////////////////////////////////////////////////////////
	// Code Generation Utilities
	//

	llvm::Function *getRootFunction(std::string &fun_name)
	{
		static size_t anonCount = 0;
		// Set up anonymous function to run.
		fun_name = "__AnonFunc";
		fun_name += std::to_string(anonCount);
		std::vector<llvm::Type*> args;
		llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getVoidTy(theContext), args, false);
		anonCount++;
		return llvm::Function::Create(FT, llvm::Function::ExternalLinkage, fun_name, TheModule.get());
	}
	
	/// Called by basic testing and runStatements
	llvm::Function *generateCode(const std::list<intro::Statement*> &statements)
	{
		std::vector<llvm::Type*> args;
		llvm::IRBuilder<> Builder(theContext);
		llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getVoidTy(theContext),args, false);
		llvm::Function *func = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "rootfunc", TheModule.get());
		BasicBlock *BB = BasicBlock::Create(theContext, "entry", func);
		Builder.SetInsertPoint(BB);
		//CodeGenEnvironment env(nullptr,CodeGenEnvironment::GlobalScope);
		CodeGenEnvironment *env = CodeGenModule::getRoot();
		env->setAnonFunction(func);
		for (std::list<intro::Statement*>::const_iterator iter = statements.begin();iter != statements.end();iter++)
			if (!(*iter)->codeGen(Builder, env)) return nullptr;
			//if (!(*iter)->codeGen(Builder, &env)) return nullptr;
		Builder.CreateRetVoid();
		verifyFunction(*func);
		//func->dump();
		return func;
	}

	/// Called by non-interactive mode to execute all statements
	void runStatements(const std::list<intro::Statement*> &statements)
	{
		llvm::Function *root = intro::generateCode(statements);
		//TheModule->dump();
		auto H = TheJIT->addModule(std::move(TheModule));
		auto ExprSymbol = TheJIT->findSymbol("rootfunc"); // name by agreement with codegen.cxx
		void(*FP)() = (void(*)())(intptr_t)ExprSymbol.getAddress();
		FP();
	}

	void initModule(void);

	/// Called by parser to execute a single statement
	void executeStatement(Statement *stmt)
	{
		std::string fun_name;
		llvm::Function *func = getRootFunction(fun_name);
		llvm::BasicBlock *BB = llvm::BasicBlock::Create(theContext, "entry", func);
		llvm::IRBuilder<> builder(theContext);
		builder.SetInsertPoint(BB);
		CodeGenEnvironment *env = CodeGenModule::getRoot();
		env->setAnonFunction(func);
		//bool result = stmt->codeGen(builder, &global);
		bool result = stmt->codeGen(builder, env);
		if (!result)
		{
			std::cout << "Error while generating code!\n";
			return;
		}
		builder.CreateRetVoid();
		llvm::verifyFunction(*func);
		auto H = TheJIT->addModule(std::move(TheModule));
		initModule();
		env->addExternalsForGlobals();
		//global.addExternalsForGlobals();
		// Search the JIT for the "fun_name" symbol.
		auto ExprSymbol = TheJIT->findSymbol(fun_name);
		assert(ExprSymbol && "Function not found");
		// Get the symbol's address and cast it to the right type so we can call it as a native function.
		if (!ExprSymbol) printf("ExprSymbol '%s' not found!\n", fun_name.c_str());
		else if (!ExprSymbol.getAddress()) printf("ExprSymbol has no address!\n");
		else
		{
			void(*FP)() = (void(*)())(intptr_t)ExprSymbol.getAddress();
			FP();
		}
	}

	llvm::Constant *createGlobalString(const wstring &s)
	{
		static std::unordered_map<wstring,llvm::GlobalVariable*> global_strings;
		std::unordered_map<wstring,llvm::GlobalVariable*>::iterator iter=global_strings.find(s);
		llvm::GlobalVariable *GV=nullptr;
		if (iter!= global_strings.end()) 
		{
			GV=iter->second;
		}
		else
		{
			std::vector<uint16_t> elements;
			elements.reserve(s.size()+1);
			for (size_t i=0;i<s.size();i++) 
				elements.push_back((uint16_t)s[i]);
			elements.push_back(0); // make sure we have a null terminator for strings.

			llvm::Constant *genstr=ConstantDataArray::get(theContext,elements);
			GV = new llvm::GlobalVariable(*TheModule,genstr->getType(), false,
						llvm::GlobalValue::ExternalLinkage,genstr, "global");
			GV->setAlignment(sizeof(wchar_t));
			//global_strings.insert(make_pair(s,GV));
		}
		llvm::Type *string_t=llvm::ArrayType::get(llvm::Type::getInt16Ty(theContext),0);
		llvm::Constant *zeros[]=
		{
			llvm::ConstantInt::get(llvm::Type::getInt32Ty(theContext),0,false),
			llvm::ConstantInt::get(llvm::Type::getInt32Ty(theContext),0,false)
		};
		return ConstantExpr::getBitCast(GV, llvm::Type::getInt16Ty(theContext)->getPointerTo());
		//return GV;
	}

	void createInternalTypes()
	{
		void_t=llvm::Type::getVoidTy(theContext);
		integer_t=llvm::Type::getInt64Ty(theContext);
		int32_t=llvm::Type::getInt32Ty(theContext);
		charptr_t=llvm::Type::getInt16Ty(theContext)->getPointerTo();
		double_t=llvm::Type::getDoubleTy(theContext);
		boolean_t=llvm::Type::getInt1Ty(theContext);
		rttype_t=llvm::Type::getInt16Ty(theContext);
		
		builtin_t=llvm::Type::getInt8Ty(theContext)->getPointerTo();
		//llvm::Type *v_i[]={integer_t};
		//builtin_t=llvm::StructType::create(theContext,v_i,"builtin_t");

		//std::vector<llvm::Type*> vField {
		llvm::Type *vField[] = {
			builtin_t,	// result value
			rttype_t, 	// result rtt
		};
		field_t = llvm::StructType::create(theContext,vField,"field_t");

		closure_t=llvm::StructType::create(theContext,"closure_t");
		//std::vector<llvm::Type*> vClose {
		llvm::Type *vClose[]= {
			integer_t,	// GC
			builtin_t,	// stand in for actual function type...
			integer_t,	// numberof fields?!
			llvm::ArrayType::get(field_t,0)
		};
		closure_t->setBody(vClose);

		generator_t=llvm::StructType::create(theContext,"generator_t");
		//std::vector<llvm::Type*> vGenArg {
		llvm::Type *vGenArg[] = {
			generator_t->getPointerTo()
		};
		genfunc_t = llvm::FunctionType::get(boolean_t,	vGenArg, false);
		
		//std::vector<llvm::Type*> vGenerator {
		llvm::Type *vGenerator[] = {
			integer_t,	// GC
			genfunc_t->getPointerTo(),	// generators have a fixed type? But is it legal?
			integer_t,	// number of fields?!
			builtin_t,	// result value
			rttype_t, 	// result rtt
			integer_t,	// state
			llvm::ArrayType::get(field_t,0)
		};
		generator_t->setBody(vGenerator);
	}
	
	void makeReturnTypeZExt(llvm::Function *fun)
	{
		SmallVector<AttributeSet, 1> Attrs;
		AttrBuilder B;
		B.addAttribute(Attribute::ZExt);
		llvm::AttributeSet attrs=AttributeSet::get(theContext,llvm::AttributeSet::ReturnIndex,B);
		fun->setAttributes(attrs);
	}

	void declareRuntimeFunctions(void)
	{
		/*
			i := integer
			i32:= int32
			u := builtin
			t := runtime type
			s := size int 32
			o := offset array
			l := labels array
		*/
		// Bunch of arrays of types usefull later to define runtime library functions (parameters).
		llvm::Type *v_i[]={integer_t};
		llvm::Type *v_i32[]={int32_t};
		llvm::Type *v_u[]={builtin_t};
		llvm::Type *v_it[]={integer_t,rttype_t};
		llvm::Type *v_uci[]={builtin_t,charptr_t,integer_t};
		llvm::Type *v_uu[]={builtin_t,builtin_t};
		llvm::Type *v_ui[]={builtin_t,integer_t};
		llvm::Type *v_us[]={builtin_t,int32_t};
		llvm::Type *v_ut[]={builtin_t,rttype_t};
		llvm::Type *v_tt[]={rttype_t,rttype_t};
		llvm::Type *v_uut[]={builtin_t,builtin_t,rttype_t};
		llvm::Type *v_uuu[]={builtin_t,builtin_t,builtin_t};
		llvm::Type *v_usut[]={builtin_t,int32_t,builtin_t,rttype_t};
		llvm::Type *v_utut[]={builtin_t,rttype_t,builtin_t,rttype_t};
		llvm::Type *v_uii[]={builtin_t,integer_t,integer_t};
		llvm::Type *v_ols[]={int32_t->getPointerTo(),charptr_t->getPointerTo(),int32_t};
		llvm::Type *v_olis[]={int32_t->getPointerTo(),charptr_t->getPointerTo(),integer_t,int32_t};		
		llvm::Type *v_sg[] = { int32_t, genfunc_t->getPointerTo() };
		// Import runtime ,library functions into llvm Module - constant array data driven ;)
		/* The ReadOnly function attribute states that "A readonly function always returns 
		   the same value (or unwinds an exception identically) when called with the same 
		   set of arguments and global state."
		   
		   That means it is pure and can e.g. be lifted out of a loop if it does not depend in any way on the loop variables?
		*/
		struct builtin { 
			const char *name;
			llvm::FunctionType *type;
			// bool isPure; // Indicates that the ReadOnly attribute should be set - this enables important optimizations on those builtins.
		} builtins[] = {
			// string intrinsics
			{ "allocString",		llvm::FunctionType::get(builtin_t,	v_i,	false) },
			{ "appendString",		llvm::FunctionType::get(void_t,		v_uci,	false) },
			{ "subString",			llvm::FunctionType::get(builtin_t,	v_uii,	false) },
			{ "sizeString",			llvm::FunctionType::get(integer_t,	v_u,	false) },
			{ "lessString",			llvm::FunctionType::get(boolean_t,	v_uu,	false) },
			{ "lessEqString",		llvm::FunctionType::get(boolean_t,	v_uu,	false) },
			{ "greaterString",		llvm::FunctionType::get(boolean_t,	v_uu,	false) },
			{ "greaterEqString",	llvm::FunctionType::get(boolean_t,	v_uu,	false) },
			// list intrinsics
			{ "allocList",			llvm::FunctionType::get(builtin_t,	v_it,	false) },
			{ "getElemTypeList",	llvm::FunctionType::get(rttype_t,	v_u,	false) },
			{ "setElemTypeList",	llvm::FunctionType::get(void_t,		v_ut,	false) },
			{ "appendList",			llvm::FunctionType::get(void_t,		v_uu,	false) },
			{ "sizeList",			llvm::FunctionType::get(integer_t,	v_u,	false) },
			{ "resizeList",			llvm::FunctionType::get(void_t,		v_u,	false) },
			{ "itemList",			llvm::FunctionType::get(builtin_t,	v_ui,	false) },
			{ "subList",			llvm::FunctionType::get(builtin_t,	v_uii,	false) },
			// dictionary intrinsics
			{ "allocDict",			llvm::FunctionType::get(builtin_t,	v_tt,	false) },
			{ "sizeDict",			llvm::FunctionType::get(integer_t,	v_u,	false) },
			{ "getKeyTypeDict",		llvm::FunctionType::get(rttype_t,	v_u,	false) },
			{ "setKeyTypeDict",		llvm::FunctionType::get(void_t,		v_ut,	false) },
			{ "getValueTypeDict",	llvm::FunctionType::get(rttype_t,	v_u,	false) },
			{ "setValueTypeDict",	llvm::FunctionType::get(void_t,		v_ut,	false) },
			{ "getCreateSlotDict",	llvm::FunctionType::get(builtin_t->getPointerTo(),	v_uu,	false) },
			{ "insertDict",			llvm::FunctionType::get(void_t,		v_uuu,	false) },
			{ "findDict",			llvm::FunctionType::get(builtin_t,	v_uu,	false) },
			{ "eraseDict",			llvm::FunctionType::get(void_t,		v_uu,	false) },
			{ "clearDict",			llvm::FunctionType::get(void_t,		v_u,	false) },
			// record intrinsics
			{ "allocRecord",		llvm::FunctionType::get(builtin_t,	v_ols,	false) },
			{ "setFieldRecord",		llvm::FunctionType::get(builtin_t,	v_usut,	false) },
			{ "getSlotRecord",		llvm::FunctionType::get(int32_t,	v_uci,	false) },
			{ "getFieldRecord",		llvm::FunctionType::get(builtin_t->getPointerTo(),	v_us,	false) },
			{ "getFieldRTTRecord",	llvm::FunctionType::get(rttype_t->getPointerTo(),	v_us,	false) },
			// variant intrinsics
			{ "allocVariant",		llvm::FunctionType::get(builtin_t,	v_olis,	false) },
			{ "getTagVariant",		llvm::FunctionType::get(integer_t,	v_u,	false) },
			{ "setFieldVariant",	llvm::FunctionType::get(builtin_t,	v_usut,	false) },
			{ "getSlotVariant",		llvm::FunctionType::get(int32_t,	v_uci,	false) },
			{ "getFieldVariant",	llvm::FunctionType::get(builtin_t->getPointerTo(),	v_us,	false) },
			{ "getFieldRTTVariant",	llvm::FunctionType::get(rttype_t->getPointerTo(),	v_us,	false) },
			// closure intrinsics
			{ "allocClosure",		llvm::FunctionType::get(builtin_t,	v_i32,	false) },
			// Generator Intrinsics
			{ "allocGenerator",		llvm::FunctionType::get(builtin_t,	v_sg,	false) },
			{ "allocListGenerator",	llvm::FunctionType::get(builtin_t,	v_u,	false) },
			{ "allocStringGenerator", llvm::FunctionType::get(builtin_t,	v_u,	false) },
			{ "allocDictGenerator", llvm::FunctionType::get(builtin_t,	v_u,	false) },
			{ "callGenerator",		llvm::FunctionType::get(boolean_t,	v_u,	false) } ,
			{ "getResultGenerator",	llvm::FunctionType::get(builtin_t,	v_u,	false) },
			{ "getResultTypeGenerator",	llvm::FunctionType::get(rttype_t,	v_u,	false) },
			{ "setResultGenerator",	llvm::FunctionType::get(void_t,	v_uut,	false) },
			{ "getStateGenerator",	llvm::FunctionType::get(integer_t,	v_u,	false) },
			{ "setStateGenerator",	llvm::FunctionType::get(void_t,	v_ui,	false) },
			//polymorphic intrinsics
			{ "toStringPoly",		llvm::FunctionType::get(void_t,		v_uut,	false) },
			{ "hashPoly",			llvm::FunctionType::get(integer_t,	v_ut,	false) },
			{ "equalPoly",			llvm::FunctionType::get(integer_t,	v_utut,	false) },
			{ "increment",			llvm::FunctionType::get(void_t,		v_ut,	false) },
			{ "decrement",			llvm::FunctionType::get(void_t,		v_ut,	false) },
			// builtin functions
			{ "print",				llvm::FunctionType::get(void_t,		v_u,	false) },
			// end
			{ "\0",NULL }
		};
		for (size_t i=0;builtins[i].name[0]!='\0';++i)
		{
			llvm::Function *fun=llvm::Function::Create(builtins[i].type,llvm::Function::ExternalLinkage, builtins[i].name, TheModule.get());
			if (strcmp(builtins[i].name,"callGenerator")==0)
				makeReturnTypeZExt(fun);
			/*
			void llvm::Function::addFnAttr 	( Attribute::AttrKind  	Kind	) 	
			llvm::Attribute::ReadOnly
			*/
			//if (builtins[i].isPure) bi->addFnAttr(llvm::Attribute::ReadOnly);
		}

		//callGenFunc->addAttribute(Attribute::ZExt);
		//callGenFunc->setAttributes(callGenAttrs);
	}

	void initModule(void)
	{
		TheModule = llvm::make_unique<llvm::Module>("Intro jit", theContext);
		TheModule->setDataLayout(TheJIT->getTargetMachine().createDataLayout());
		declareRuntimeFunctions();
	}

	void initRuntime(void)
	{
		llvm::InitializeNativeTarget();
		llvm::InitializeNativeTargetAsmPrinter();
		llvm::InitializeNativeTargetAsmParser();
		
		createInternalTypes();
		
		TheJIT = llvm::make_unique<JIT>();
		

		// Create a new pass manager attached to it.
		TheFPM = llvm::make_unique<llvm::legacy::FunctionPassManager>(TheModule.get());

		// Provide basic AliasAnalysis support for GVN.
		//TheFPM->add(llvm::createBasicAliasAnalysisPass());
		// Do simple "peephole" optimizations and bit-twiddling optzns.
		TheFPM->add(llvm::createInstructionCombiningPass());
		// Reassociate expressions.
		TheFPM->add(llvm::createReassociatePass());
		// Eliminate Common SubExpressions.
		TheFPM->add(llvm::createGVNPass());
		// Simplify the control flow graph (deleting unreachable blocks, etc).
		TheFPM->add(llvm::createCFGSimplificationPass());

		TheFPM->doInitialization();
		
 	}

	void deleteRuntime(void)
	{
		//delete TheModule;
		TheModule.reset();
	}

	/////////////////////////////////////////////////////////////////////
	// Code Generation Utilities
	//
	
	static Expression::cgvalue makeRTValue(llvm::Value *value,rtt::tag_t rtt)
	{
		return make_pair(value,llvm::ConstantInt::get(rttype_t, rtt,false));
	}
	
	/// Convert Intro Type to LLVM type used by runtime to represent the Intro type.
	llvm::Type *toTypeLLVM(intro::Type *type)
	{
		switch(type->getKind())
		{
		case Type::Unit: // ??
			return builtin_t;
		case Type::Boolean:
			return boolean_t;
		case Type::Integer:
			return integer_t;
		case Type::Real:
			return double_t;
		case Type::String:
		case Type::Function:
		case Type::List:
		case Type::Dictionary:
		case Type::Record:
		case Type::Variant:
		case Type::Generator:
			return builtin_t;
		default:
			return NULL;
		};
	}

	/////////////////////////////////////////////////////////////////////
	// Code Generation Utilities - All built-ins
	//

	/// Initialize a vector of LLVM Types to the reference counting members
	void initBuiltinFields(std::vector<llvm::Type*> &fields)
	{
		//llvm::StructType *value=llvm::StructType::create(theContext,"dictelem");
		fields.clear();
		fields.push_back(llvm::Type::getInt64Ty(theContext));
	}

	/////////////////////////////////////////////////////////////////////

	llvm::Value *createUnboxValue(IRBuilder<> &TmpB,CodeGenEnvironment *env,llvm::Value *field,intro::Type *type)
	{
		std::vector<Value*> ArgsV;
		//ArgsV.push_back(1);
		//retval=Builder.CreateCall(createStringReserveF, ArgsV, "newstring");
		// Conversion function to be decided upon below
		switch(type->getKind())
		{
		case Type::Unit:
			return field;
		case Type::Boolean:
			//return TmpB.CreateZExtOrTrunc(field,boolean_t,"toBoolean");
			return TmpB.CreatePtrToInt(field,boolean_t,"unboxedBoolean");
		case Type::Integer:
			//return field;
			//return TmpB.CreateBitCast(field,integer_t,"toInteger");
			return TmpB.CreatePtrToInt(field,integer_t,"unboxedInteger");
		case Type::Real:
			{
				llvm::AllocaInst *dblbuf=env->getDoubleBuffer();
				TmpB.CreateStore(field,dblbuf);
				llvm::Value *castbuf=TmpB.CreateBitCast(dblbuf,double_t->getPointerTo());
				return TmpB.CreateLoad(castbuf,"unboxedReal");
				//return TmpB.CreateBitCast(field,double_t,"unboxedReal");
			}
		case Type::String:
		case Type::List:
		case Type::Dictionary:
			return field;
			//return TmpB.CreateIntToPtr(field,builtin_t,"toBuiltin");
		case Type::Function:
			//return TmpB.CreateBitCast(field,closure_t->getPointerTo(),"toClosure");
			return field;
		case Type::Record:
		case Type::Variant:
			//return TmpB.CreateBitCast(field,record_t->getPointerTo(),"toRecord");
			return field;
		case Type::Generator:
			return field;
		default:
			return field;
			// Code Gen Error!
		};
		return field;
	}

	llvm::Value *createBoxValue(IRBuilder<> &TmpB,CodeGenEnvironment *env,llvm::Value *field,intro::Type *type)
	{
		std::vector<Value*> ArgsV;
		//ArgsV.push_back(1);
		//retval=Builder.CreateCall(createStringReserveF, ArgsV, "newstring");
		// Conversion function to be decided upon below
		switch(type->getKind())
		{
		case Type::Unit:
			return field;
		case Type::Boolean:
			//return TmpB.CreateZExtOrTrunc(field,builtin_t,"fromBoolean");
			return TmpB.CreateIntToPtr(field,builtin_t,"boxedBoolean");
		case Type::Integer:
			return TmpB.CreateIntToPtr(field,builtin_t,"boxedInteger");
			//return TmpB.CreateBitCast(field,builtin_t,"fromInteger");
		case Type::Real:
			{
				llvm::AllocaInst *dblbuf=env->getDoubleBuffer();
				llvm::Value *castbuf=TmpB.CreateBitCast(dblbuf,double_t->getPointerTo());
				TmpB.CreateStore(field,castbuf);
				return TmpB.CreateLoad(dblbuf,"boxedReal");
				//return TmpB.CreateBitCast(field,builtin_t,"boxedReal");
			}
		case Type::String:
		case Type::List:
		case Type::Dictionary:
			//return TmpB.CreateIntToPtr(field,builtin_t,"fromBuiltin");
		case Type::Function:
		case Type::Record:
		case Type::Variant:
			//return TmpB.CreateBitCast(field,builtin_t,"fromRecord");
		case Type::Generator:
		default:
			return field;
			// Code Gen Error!
		};
		return field;
	}


	/**
		Generate code that converts an int to a real, if needed:
	*/
	static llvm::Value *cgCoerceIntIfNeeded(llvm::IRBuilder<> &builder,CodeGenEnvironment *env,llvm::Function *TheFunction,llvm::Value *value, llvm::Value *otherIsInt)
	{
		intro::Type typeInt(intro::Type::Integer);
		intro::Type typeReal(intro::Type::Real);
		llvm::BasicBlock *convert = llvm::BasicBlock::Create(theContext, "convert");
		llvm::BasicBlock *keep = llvm::BasicBlock::Create(theContext, "keep");
		llvm::BasicBlock *merge = llvm::BasicBlock::Create(theContext, "mergecoerce");
		builder.CreateCondBr(otherIsInt,convert,keep);
		
		TheFunction->getBasicBlockList().push_back(convert);
		builder.SetInsertPoint(keep);
		llvm::Value *keepbuf=createUnboxValue(builder,env,value,&typeReal);
		builder.CreateBr(merge);
		
		TheFunction->getBasicBlockList().push_back(keep);
		builder.SetInsertPoint(convert);
		llvm::Value *convbuf=createUnboxValue(builder,env,value,&typeInt);
		convbuf = builder.CreateSIToFP(convbuf,double_t,"int2real");
		builder.CreateBr(merge);
		
		TheFunction->getBasicBlockList().push_back(merge);
		builder.SetInsertPoint(merge);
		llvm::PHINode *result=builder.CreatePHI(double_t,2,"lhscoerced");
		result->addIncoming(convbuf,convert);
		result->addIncoming(keepbuf,keep);
		return result;
	}
	
	/**
		@param builder	The llvm Builder object to use
		@param env	The code generation environment representing the current scope
		@param lhs	Left hand side parameter
		@param lhstype Inferred Type of the left hand side parameter
		@param rhs	Right hand side parameter
		@param rhstype Inferred Type of the right hand side parameter
	*/
	static Expression::cgvalue genNumberOps(llvm::IRBuilder<> &builder,CodeGenEnvironment *env,
		Expression::cgvalue &lhs,intro::Type *lhst,Expression::cgvalue &rhs,intro::Type *rhst,
		Application::codegen_cb int_op,intro::Type * int_op_type, Application::codegen_cb real_op,intro::Type *real_op_type)
	{
		intro::Type typeInt(intro::Type::Integer);
		intro::Type typeReal(intro::Type::Real);
		intro::rtt::tag_t int_op_rtt = int_op_type->getRTKind();
		intro::rtt::tag_t real_op_rtt = real_op_type->getRTKind();
		llvm::Function *TheFunction = env->currentFunction();
		std::vector<llvm::Value*> args{ lhs.first,rhs.first };
		if (lhst->getKind()==intro::Type::Integer && rhst->getKind()==intro::Type::Integer)
		{
			args[0]=createUnboxValue(builder,env,args[0],lhst);
			args[1]=createUnboxValue(builder,env,args[1],rhst);
			llvm::Value *buf=int_op(builder,args);
			buf=createBoxValue(builder,env,buf,&typeInt);
			return makeRTValue(buf,int_op_rtt);
		}
		if (lhst->getKind()==intro::Type::Real && rhst->getKind()==intro::Type::Real)
		{
			args[0]=createUnboxValue(builder,env,args[0],lhst);
			args[1]=createUnboxValue(builder,env,args[1],rhst);
			llvm::Value *buf=real_op(builder,args);
			buf=createBoxValue(builder,env,buf,real_op_type);
			return makeRTValue(buf,real_op_rtt);
		}
		else if (lhst->getKind()!=intro::Type::Variable && rhst->getKind()!=intro::Type::Variable)
		{
			args[0]=createUnboxValue(builder,env,args[0],lhst);
			if (lhst->getKind()==intro::Type::Integer) 
			{
				args[0]=builder.CreateSIToFP(args[0],double_t,"lint2real");
			}
			args[1]=createUnboxValue(builder,env,args[1],rhst);
			if (rhst->getKind()==intro::Type::Integer) 
			{
				args[1]=builder.CreateSIToFP(args[1],double_t,"rint2real");
			}
			llvm::Value *buf=real_op(builder,args);
			buf=createBoxValue(builder,env,buf,real_op_type);
			return makeRTValue(buf,real_op_rtt);
		}
		else // at least one variables -have to check runtime types.
		{
			//if (lhst->getKind()==intro::Type::Variable || rhst->getKind()==intro::Type::Variable)
			{
				llvm::Value *lhsInt=builder.CreateICmpEQ(lhs.second, llvm::ConstantInt::get(rttype_t, intro::rtt::Integer,false), "lhsIntDesKa");
				llvm::Value *rhsInt=builder.CreateICmpEQ(rhs.second, llvm::ConstantInt::get(rttype_t, intro::rtt::Integer,false), "rhsIntDesKa");
				llvm::Value *bothInt=builder.CreateAnd(lhsInt,rhsInt,"bothintdeska");
				llvm::BasicBlock *intopblock = llvm::BasicBlock::Create(theContext, "intopblock");
				llvm::BasicBlock *realopblock = llvm::BasicBlock::Create(theContext, "realopblock");
				llvm::BasicBlock *mergeblock = llvm::BasicBlock::Create(theContext, "mergeblock");
				builder.CreateCondBr(bothInt,intopblock,realopblock);
				
				TheFunction->getBasicBlockList().push_back(intopblock);
				builder.SetInsertPoint(intopblock);
				std::vector<llvm::Value*> argsbuf{
					createUnboxValue(builder,env,args[0],&typeInt),
					createUnboxValue(builder,env,args[1],&typeInt)
				};
				llvm::Value *intresult=int_op(builder,argsbuf);
				intresult=createBoxValue(builder,env,intresult,int_op_type);
				builder.CreateBr(mergeblock);
				
				TheFunction->getBasicBlockList().push_back(realopblock);
				builder.SetInsertPoint(realopblock);

				args[0]=cgCoerceIntIfNeeded(builder,env,TheFunction,args[0], lhsInt);
				args[1]=cgCoerceIntIfNeeded(builder,env,TheFunction,args[1], rhsInt);
				
				llvm::Value *realresult=real_op(builder,args);
				realresult=createBoxValue(builder,env,realresult,real_op_type);
				builder.CreateBr(mergeblock);
				
				// Arg generation for operand added blocks, find actual last block!
				realopblock=builder.GetInsertBlock();
				
				TheFunction->getBasicBlockList().push_back(mergeblock);
				builder.SetInsertPoint(mergeblock);
				llvm::PHINode *result=builder.CreatePHI(builtin_t,2,"result");
				result->addIncoming(intresult,intopblock);
				result->addIncoming(realresult,realopblock);
				
				llvm::PHINode *resultrtt=builder.CreatePHI(rttype_t,2,"resultrtt");
				resultrtt->addIncoming(llvm::ConstantInt::get(rttype_t, int_op_rtt,false),intopblock);
				resultrtt->addIncoming(llvm::ConstantInt::get(rttype_t, real_op_rtt,false),realopblock);
				return std::make_pair(result,resultrtt);
			}
		}
	}
	
	llvm::Value *cgTestRTT(IRBuilder<> &builder,llvm::Value *rtt,rtt::tag_t testrtt)
	{
		return builder.CreateICmpEQ(rtt, llvm::ConstantInt::get(rttype_t, testrtt,false), "testrtt");
	}

	/// Generate code for operations that have one operator and return the input type (negation, splice, ...)
	static Expression::cgvalue genTypeChoiceOps(llvm::IRBuilder<> &builder,CodeGenEnvironment *env,
		Expression::cgvalue &input,intro::Type *inferred,
		Application::cgcb_elem ops[],size_t op_count,bool ignoreResult=false,
		Expression::cgvalue extrainput[]=nullptr,size_t extras_count=0)
	{
		std::vector<llvm::Value*> args{ input.first };
		for (size_t i=0;i<extras_count;++i)
			args.push_back(extrainput[i].first);
		// Not a variable type, code only the op we know we'll need
		if (inferred->getKind()!=Type::Variable)
		{
			rtt::RTType rttag=inferred->getRTKind();
			size_t i;
			for (i=0;i<op_count;++i)
				if (ops[i].type==rttag) break;
			assert(i<op_count);
			llvm::Value *result=ops[i].callback(builder,args);
			return makeRTValue(result,rttag);
		}
		
		// Variable type, switch over rtt and create a case for each possible type
		// Get Function llvm style - but we do not guarantee that blocks already have a parent :(
		//llvm::Function *TheFunction = builder.GetInsertBlock()->getParent();
		llvm::Function *TheFunction = env->currentFunction();
		
		llvm::BasicBlock *postcase = llvm::BasicBlock::Create(theContext, "postrtt");
		llvm::BasicBlock *startblock = builder.GetInsertBlock();
		llvm::SwitchInst *jumptable=builder.CreateSwitch(input.second,postcase); // add to current block
		// add phi nodes after switch to collect values and their rtt.
		builder.SetInsertPoint(postcase);
		llvm::PHINode *result=nullptr;
		llvm::PHINode *resultrtt=nullptr;
		if (!ignoreResult)
		{
			result=builder.CreatePHI(builtin_t,op_count+1,"result");
			resultrtt=builder.CreatePHI(rttype_t,op_count+1,"resultrtt");
			result->addIncoming(llvm::Constant::getNullValue(builtin_t),startblock);
			resultrtt->addIncoming(llvm::ConstantInt::get(llvm::Type::getInt16Ty(theContext), rtt::Undefined,false),startblock);
		}
		for (size_t i=0;i<op_count;++i)
		{
			llvm::BasicBlock *current = llvm::BasicBlock::Create(theContext, "typecase");
			TheFunction->getBasicBlockList().push_back(current);
			builder.SetInsertPoint(current);
			llvm::ConstantInt *rttval=llvm::ConstantInt::get(llvm::Type::getInt16Ty(theContext), ops[i].type,false);
			jumptable->addCase(rttval, current);
			llvm::Value *output=ops[i].callback(builder,args);
			builder.CreateBr(postcase);
			current=builder.GetInsertBlock();
			if (!ignoreResult)
			{
				result->addIncoming(output,current);
				resultrtt->addIncoming(rttval,current);
			}
		}
		// Continue after PHI
		TheFunction->getBasicBlockList().push_back(postcase);
		builder.SetInsertPoint(postcase);
		return std::make_pair(result,resultrtt);
	}

	/////////////////////////////////////////////////////////////////////
	// Code Generation Methods (Expressions)
	//

	Expression::cgvalue IntegerConstant::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		llvm::Value *constant=llvm::ConstantInt::get(llvm::Type::getInt64Ty(theContext), value,true);
		constant=createBoxValue(TmpB,env,constant,getType());
		return makeRTValue(constant,rtt::Integer);
	}

	Expression::cgvalue BooleanConstant::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		llvm::Value *constant=llvm::ConstantInt::get(llvm::Type::getInt1Ty(theContext), value,false);
		constant=createBoxValue(TmpB,env,constant,getType());
		return makeRTValue(constant,rtt::Boolean);
	}

	Expression::cgvalue RealConstant::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		llvm::Value *constant=llvm::ConstantFP::get(theContext, llvm::APFloat(value));
		constant=createBoxValue(TmpB,env,constant,getType());
		return makeRTValue(constant,rtt::Real);
	}

	Expression::cgvalue StringConstant::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		//// Prepare tokenization of string based on regular expressions (regex does efficient preprocessing here...)
		Value *strsz=NULL;
		Value *retval=NULL;
		Value *zero=llvm::ConstantInt::get(llvm::Type::getInt64Ty(theContext), 0,true);
		vector<Value*> doubleo(2,zero);
		std::list<std::pair<llvm::Value*,Value*> > stringparts;
		std::list<std::pair<llvm::Value*,Value*> >::iterator iter;
		// Provide easy acces to intrinsic functions we might need
		llvm::Function *appendStringF = TheModule->getFunction("appendString");
		llvm::Function *toStringF = TheModule->getFunction("toStringPoly");
		llvm::Function *allocStringF = TheModule->getFunction("allocString");
		// Generate call to string allocation intrisic, guesstimate length by taking source string length
		// For each part, it's a constant string, or a variable interpolation
		std::vector<Value*> ArgsV;
		Value *targetsize = llvm::ConstantInt::get(llvm::Type::getInt64Ty(theContext),value.size(),false);
		ArgsV.push_back(targetsize);
		retval=TmpB.CreateCall(allocStringF, ArgsV, "newstring");
		for (iterator i=parts.begin();i!=parts.end();i++) 
		{
			if (i->t!=NULL)
			{
				// Extract variable name (and the variable)
				CodeGenEnvironment::iterator vi=env->find(i->s);
				// convert to string: codegen intrisic call with toString helper, which appends always (even another string)
				Value *value=TmpB.CreateLoad(vi->second.address, "loadvarval");
				Value *rtt=TmpB.CreateLoad(vi->second.rtt, "loadvarrtt");
				ArgsV.clear();
				ArgsV.push_back(retval);
				ArgsV.push_back(value);
				ArgsV.push_back(rtt);
				TmpB.CreateCall(toStringF, ArgsV);
			}
			else
			{
				// Append constant string element
				//std::wcout << L"String Part: '" << i->s << L"'\n";
				//llvm::GlobalVariable *GV=createGlobalString(i->s);
				llvm::Constant *GV=createGlobalString(i->s);
				strsz=ConstantInt::get(llvm::Type::getInt64Ty(theContext), i->s.size(),false);
				ArgsV.clear();
				ArgsV.push_back(retval);
				ArgsV.push_back(GV);
				//ArgsV.push_back(conststr);
				//llvm::Type *string_t=llvm::ArrayType::get(llvm::Type::getInt16Ty(theContext),0);
				//ArgsV.push_back(ConstantExpr::getGetElementPtr(string_t,GV,doubleo));
				//ArgsV.push_back(TmpB.CreateGEP(GV,doubleo,"rawstring"));
				//ArgsV.push_back(TmpB.CreateGEP(GV,zero,"rawstring"));
				ArgsV.push_back(strsz);
				TmpB.CreateCall(appendStringF, ArgsV);
			}
		}
		//return retval;
		return makeRTValue(retval,rtt::String);
	}

	Expression::cgvalue ListConstant::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		llvm::Function *appendListF = TheModule->getFunction("appendList");
		llvm::Function *allocListF = TheModule->getFunction("allocList");
		// Generate call to string allocation intrisic, guesstimate length by taking source string length
		// For each part, it's a constant string, or a variable interpolation
		std::vector<llvm::Value*> ArgsV;
		Value *targetsize = llvm::ConstantInt::get(llvm::Type::getInt64Ty(theContext),elements.size(),false);
		rtt::tag_t rttbuf=myType->getFirstParameter()->find()->getRTKind();
		Value *elem_rtt = llvm::ConstantInt::get(llvm::Type::getInt16Ty(theContext),rttbuf,false);
		ArgsV.push_back(targetsize);
		ArgsV.push_back(elem_rtt);
		llvm::Value *retval=TmpB.CreateCall(allocListF, ArgsV, "newlist");
		ArgsV.resize(2,nullptr);
		ArgsV[0]=retval;
		if (generators==NULL) // append fixed initializer expressions
		{
			
			std::list<intro::Expression*>::iterator iter;
			for (iter=elements.begin();iter!=elements.end();iter++)
			{
				Expression::cgvalue elemval = (*iter)->codeGen(TmpB,env);
				ArgsV[1]=elemval.first;
				// if number/integer, need to coerce to int
				TmpB.CreateCall(appendListF, ArgsV);
			}
		}
		else 
		{
			generators->setBodyCallback([&](llvm::IRBuilder<> &builder,CodeGenEnvironment *env) {
				Expression::cgvalue elemval = elements.front()->codeGen(builder,env);
				if (myType->getFirstParameter()->find()->getKind()==Type::Variable)
				{
					llvm::Function *setTypeF = TheModule->getFunction("setElemTypeList");
					llvm::Value *stargs[]={retval, elemval.second };
					builder.CreateCall(setTypeF, stargs);
				}
				ArgsV[1]=elemval.first;
				// if number/integer, need to coerce to int
				builder.CreateCall(appendListF, ArgsV);
				return false;
			});
			generators->codeGen(TmpB,env);
		}
		return makeRTValue(retval,rtt::List);
	}

	Expression::cgvalue DictionaryConstant::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		llvm::Function *allocDictF = TheModule->getFunction("allocDict");
		llvm::Function *insertDictF = TheModule->getFunction("insertDict");
		// Generate call to string allocation intrisic, guesstimate length by taking source string length
		// For each part, it's a constant string, or a variable interpolation
		std::vector<llvm::Value*> ArgsV;
		rtt::tag_t rttbuf=myType->getFirstParameter()->find()->getRTKind();
		Value *key_rtt = llvm::ConstantInt::get(llvm::Type::getInt16Ty(theContext),rttbuf,false);
		rttbuf=myType->getParameter(1)->find()->getRTKind();
		Value *value_rtt = llvm::ConstantInt::get(llvm::Type::getInt16Ty(theContext),rttbuf,false);
		ArgsV.push_back(key_rtt);
		ArgsV.push_back(value_rtt);
		llvm::Value *retval=TmpB.CreateCall(allocDictF, ArgsV, "newdict");
		ArgsV.resize(3,nullptr); // dict, key, value
		ArgsV[0]=retval;

		if (generators==NULL) // append fixed initializer expressions
		{
			
			memberlist::iterator iter;
			for (iter=elements.begin();iter!=elements.end();iter++)
			{
				Expression::cgvalue key_val = iter->first->codeGen(TmpB,env);
				ArgsV[1]=key_val.first;
				Expression::cgvalue value_val = iter->second->codeGen(TmpB,env);
				ArgsV[2]=value_val.first;
				TmpB.CreateCall(insertDictF, ArgsV);
			}
		}
		else 
		{
			generators->setBodyCallback([&](llvm::IRBuilder<> &builder,CodeGenEnvironment *env) {
				Expression::cgvalue key_val = elements.front().first->codeGen(builder,env);
				if (myType->getFirstParameter()->find()->getKind()==Type::Variable)
				{
					llvm::Function *setTypeF = TheModule->getFunction("setKeyTypeDict");
					llvm::Value *stargs[]={retval, key_val.second };
					builder.CreateCall(setTypeF, stargs);
				}
				ArgsV[1]=key_val.first;

				Expression::cgvalue value_val = elements.front().second->codeGen(builder,env);
				if (myType->getFirstParameter()->find()->getKind()==Type::Variable)
				{
					llvm::Function *setTypeF = TheModule->getFunction("setValueTypeDict");
					llvm::Value *stargs[]={retval, value_val.second };
					builder.CreateCall(setTypeF, stargs);
				}
				ArgsV[2]=value_val.first;
				// if number/integer, need to coerce to int
				builder.CreateCall(insertDictF, ArgsV);
				return false;
			});
			generators->codeGen(TmpB,env);
		}
		return makeRTValue(retval,rtt::Dictionary);
	}
	
	/// (Use after type inference) Generate the address that can be written to, if this expression is writable;
	Expression::cgvalue Variable::getWriteAddress(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		// Find address for relative and absolute paths, too
		CodeGenEnvironment::iterator iter=env->find(name);
		return std::make_pair(iter->second.address,iter->second.rtt);
	}
	
	Expression::cgvalue Variable::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		CodeGenEnvironment::iterator elem;
		llvm::Value *value=nullptr, *rtt=nullptr;
		if (relative && path.empty())
		{
			elem=env->find(name);
		} 
		else
		{
			CodeGenModule *base = relative ?
				env->getCurrentModule() : CodeGenModule::getRoot();
			CodeGenModule *myself = base->getRelativePath(path);
			myself->declareInterfaceIfNeeded(TheModule.get());
			elem = myself->find(name);
		}
		if (elem->second.isParameter)
		{
			value=elem->second.address;
			rtt=elem->second.rtt;
		}
		else
		{
			std::string rtname(name.begin(),name.end());
			value=TmpB.CreateLoad(elem->second.address, rtname);
			rtt=TmpB.CreateLoad(elem->second.rtt, rtname+"!rtt");
		}
		return std::make_pair(value,rtt);
	}

	template<class T>
	llvm::Value *createGlobal(llvm::ArrayRef<T> &elements,std::string name)
	{
		llvm::Constant *genstr=ConstantDataArray::get(theContext,elements);
		llvm::GlobalVariable *GV =
			new llvm::GlobalVariable(*TheModule,genstr->getType(), true,
					llvm::GlobalValue::ExternalLinkage,genstr, name);
		GV->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
		GV->setAlignment(sizeof(T));
		//global_strings.insert(make_pair(s,GV));
		return GV;
	}
	// Make a global constant holding the offsets for a perfect hash for the given set of keys.
	// Returns the offsets for latter use in last parameter.
	llvm::Value *cgOffsets(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env,
		std::set<std::wstring> keys,std::int32_t offsets[])
	{
		util::mkPerfectHash(keys,offsets);
		const size_t size=keys.size();
		llvm::ArrayRef<std::uint32_t> offset_ref((std::uint32_t*)offsets,size);
		llvm::Value *val= createGlobal(offset_ref,"recoffsets");
		return TmpB.CreateBitCast(val, llvm::Type::getInt32Ty(theContext)->getPointerTo());
	}

	// Code Gen a constant array with pointers to strings for the labels
	llvm::Value *cgLabelArray(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env,
		std::set<std::wstring> keys,const std::int32_t offsets[])
	{
		std::vector<llvm::Constant*> labels;
		size_t size=keys.size();
		labels.resize(size,nullptr);
		llvm::Value *zeros[]=
		{
			llvm::ConstantInt::get(llvm::Type::getInt64Ty(theContext),0,false),
			llvm::ConstantInt::get(llvm::Type::getInt64Ty(theContext),0,false)
		};
		for (const std::wstring &label : keys)
		{
			std::int32_t slot=util::find(label.c_str(),label.size(),offsets,size);
			llvm::Constant *gstr=createGlobalString(label);
			labels[slot]=gstr;
			//labels[slot]=ConstantExpr::getGetElementPtr(llvm::Type::getInt16Ty(theContext),gstr,zeros);
		}
		llvm::ArrayType *ty=llvm::ArrayType::get(llvm::Type::getInt16Ty(theContext)->getPointerTo(),size);
		llvm::Constant *constants=llvm::ConstantArray::get(ty,labels);
		llvm::GlobalVariable *GV =
			new llvm::GlobalVariable(*TheModule,constants->getType(), true,
					llvm::GlobalValue::ExternalLinkage,constants, "reclabels");
		GV->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
		GV->setAlignment(sizeof(wchar_t*));
		//return GV;
		return TmpB.CreateGEP(GV,zeros);
	}
	
	Expression::cgvalue RecordExpression::codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		std::int32_t *offsets=nullptr;
		//rtt_t *fields=nullptr;
		const size_t size=myType->size();
		std::vector<llvm::Value*> args;
		// do we have fields? ...
		if (size>0)
		{
			// ... yes, Compute offsets for perfect hash and put in first param for allocation
			offsets=new std::int32_t[size];
			// generate perfect hash
			std::set<std::wstring> keys;
			RecordType::iterator iter;
			for (iter=myType->begin();iter!=myType->end();++iter)
				keys.insert(iter->first);
			
			args.push_back(cgOffsets(TmpB,env,keys,offsets));
			args.push_back(cgLabelArray(TmpB,env,keys,offsets));
		}
		else
		{
				// ... nope, pass a nullptr
			//llvm::ArrayType *ty=llvm::ArrayType::get(llvm::Type::getInt16Ty(theContext)->getPointerTo(),size);
			args.push_back(llvm::Constant::getNullValue(llvm::Type::getInt32Ty(theContext)->getPointerTo()));
			args.push_back(llvm::Constant::getNullValue(charptr_t->getPointerTo()));
		}
		// Now allocate the record, after adding size argument
		args.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(theContext),size,false));
		llvm::Function *allocRecordF = TheModule->getFunction("allocRecord");
		llvm::Value *record=TmpB.CreateCall(allocRecordF, args, "newrecord");
		// also write fields
		llvm::Function *setFieldRecordF = TheModule->getFunction("setFieldRecord");
		args.resize(4,nullptr);
		// Set Fields
		args[0]=record;
		for (iterator iter=begin();iter!=end();++iter)
		{
			// void setFieldRecord(rtrecord *record,std::uint32_t slot,rtdata value,rtt_t rtt)
			std::int32_t slot=util::find(iter->first.c_str(),iter->first.size(),offsets,size);
			args[1]=llvm::ConstantInt::get(llvm::Type::getInt32Ty(theContext),slot,false);
			Expression::cgvalue fieldval=iter->second->codeGen(TmpB,env);
			args[2]=fieldval.first;
			args[3]=fieldval.second;
			TmpB.CreateCall(setFieldRecordF, args);
		}
		delete [] offsets;
		return makeRTValue(record,rtt::Record);
	}


	Expression::cgvalue VariantExpression::codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		std::int32_t *offsets=nullptr;
		//rtt_t *fields=nullptr;
		intro::RecordType *rectype=(RecordType*)record->getType()->find();
		const size_t size=rectype->size();
		std::vector<llvm::Value*> args;
		// do we have fields? ...
		if (size>0)
		{
			// ... yes, Compute offsets for perfect hash and put in first param for allocation
			offsets=new std::int32_t[size];
			// generate perfect hash
			std::set<std::wstring> keys;
			RecordType::iterator iter;
			for (iter=rectype->begin();iter!=rectype->end();++iter)
				keys.insert(iter->first);
			
			args.push_back(cgOffsets(TmpB,env,keys,offsets));
			args.push_back(cgLabelArray(TmpB,env,keys,offsets));
		}
		else
		{
			// ... nope, pass a nullptr
			llvm::ArrayType *ty=llvm::ArrayType::get(llvm::Type::getInt16Ty(theContext)->getPointerTo(),size);
			args.push_back(llvm::Constant::getNullValue(llvm::Type::getInt32Ty(theContext)->getPointerTo()));
			args.push_back(llvm::Constant::getNullValue(ty->getPointerTo()));
		}
		// tag param for allocator
		args.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(theContext),getTag(tag),false));
		// size param for allocator
		args.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(theContext),size,false));
		llvm::Function *allocVariantF = TheModule->getFunction("allocVariant");
		llvm::Value *variant=TmpB.CreateCall(allocVariantF, args, "newvariant");
		// also write fields
		llvm::Function *setFieldVariantF = TheModule->getFunction("setFieldVariant");
		args.resize(4,nullptr);
		// Set Fields
		args[0]=variant;
		for (RecordExpression::iterator iter=record->begin();iter!=record->end();++iter)
		{
			std::int32_t slot=util::find(iter->first.c_str(),iter->first.size(),offsets,size);
			args[1]=llvm::ConstantInt::get(llvm::Type::getInt32Ty(theContext),slot,false);
			Expression::cgvalue fieldval=iter->second->codeGen(TmpB,env);
			args[2]=fieldval.first;
			args[3]=fieldval.second;
			TmpB.CreateCall(setFieldVariantF, args);
		}
		delete [] offsets;
		return makeRTValue(variant,rtt::Variant);
	}

	Expression::cgvalue RecordAccess::getWriteAddress(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		Expression::cgvalue source=record->codeGen(TmpB,env);
		std::vector<llvm::Value*> argsSlot {
			source.first,
			createGlobalString(label),
			llvm::ConstantInt::get(llvm::Type::getInt32Ty(theContext),label.size(),false)
		};
		llvm::Function *getSlotRecordF = TheModule->getFunction("getSlotRecord");
		llvm::Value *slot = TmpB.CreateCall(getSlotRecordF, argsSlot,"fieldslot");
		std::vector<llvm::Value*> argsGet {
			source.first,
			slot
		};
		llvm::Function *getFieldRecordF = TheModule->getFunction("getFieldRecord");
		llvm::Function *getFieldRecordRTTF = TheModule->getFunction("getFieldRTTRecord");
		llvm::Value *retval=TmpB.CreateCall(getFieldRecordF, argsGet,"fieldptr");
		llvm::Value *rettype=TmpB.CreateCall(getFieldRecordRTTF, argsGet,"fieldrtt");
		return std::make_pair(retval,rettype);
	}
	
	Expression::cgvalue RecordAccess::codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		Expression::cgvalue source=record->codeGen(TmpB,env);
		std::vector<llvm::Value*> argsSlot {
			source.first,
			createGlobalString(label),
			llvm::ConstantInt::get(llvm::Type::getInt64Ty(theContext),label.size(),false)
		};
		llvm::Function *getSlotRecordF = TheModule->getFunction("getSlotRecord");
		llvm::Value *slot = TmpB.CreateCall(getSlotRecordF, argsSlot,"fieldslot");
		std::vector<llvm::Value*> argsGet {
			source.first,
			slot
		};
		llvm::Function *getFieldRecordF = TheModule->getFunction("getFieldRecord");
		llvm::Function *getFieldRecordRTTF = TheModule->getFunction("getFieldRTTRecord");
		llvm::Value *retval=TmpB.CreateCall(getFieldRecordF, argsGet,"fieldptr");
		llvm::Value *rettype=TmpB.CreateCall(getFieldRecordRTTF, argsGet,"fieldrttptr");
		retval = TmpB.CreateLoad(retval,"field_value");
		rettype = TmpB.CreateLoad(rettype,"field_rtt");
		return std::make_pair(retval,rettype);
	}
	
	// Initialize variables in closure from current environment by copying values
	// note that coping referenced values increases the reference count
	// also, closures need cleanup to dereference those values
	void populateClosure(IRBuilder<> &builder,CodeGenEnvironment *env, llvm::StructType *myclosure_t,
		llvm::Value *closure, VariableSet &free,bool isGenerator=false)
	{
		std::uint32_t field_base=isGenerator?(std::uint32_t)GeneratorFields:(std::uint32_t)ClosureFields;
		std::uint32_t field_index=0;
		llvm::Value *idxs[]= {
			llvm::ConstantInt::get(llvm::Type::getInt32Ty(theContext), 0), // the struct directly pointed to
			llvm::ConstantInt::get(llvm::Type::getInt32Ty(theContext), field_base), // index of fields array in struct
			nullptr,
			nullptr
		};	
		for (auto vars : free)
		{
			// Find the free variable in the outer environment
			//CodeGenEnvironment::iterator iter=env->find(variables[i]);
			CodeGenEnvironment::iterator iter=env->find(vars);
			// Copy the value to the closure
			idxs[2]=llvm::ConstantInt::get(llvm::Type::getInt32Ty(theContext), field_index); // index of field in array
			
			idxs[3]=llvm::ConstantInt::get(llvm::Type::getInt32Ty(theContext), 0);	// Value of field
			llvm::Value *fieldptr=builder.CreateGEP(myclosure_t,closure,idxs,"closure_field_ptr");
			idxs[3]=llvm::ConstantInt::get(llvm::Type::getInt32Ty(theContext), 1);	// Type of  field
			llvm::Value *fieldrtt=builder.CreateGEP(myclosure_t,closure,idxs,"closure_field_rtt");
			
			llvm::Value *closurevar,*closurertt;
			if (iter->second.isParameter) 
			{
				closurevar=iter->second.address;
				closurertt=iter->second.rtt;
			}
			else 
			{
				closurevar=builder.CreateLoad(iter->second.address,"freevarval");
				closurertt=builder.CreateLoad(iter->second.rtt,"freevarrtt");
			}
			builder.CreateStore(closurevar,fieldptr);
			builder.CreateStore(closurertt,fieldrtt);
			field_index++;
			// copy value into fieldptr, apply indexing
		}
	}

	/// Function code generation, one of the trickier parts of the language implementation
	/** Functions are implemented by creating an instance of the body for each
		possible instantiation of the type variables in the function's parameters.
		The good news first, we need not necessarily look at all parameters.
		We in fact have to look at the free type variables in the type, and their bounds.

		The above means that in addition to the functions parameters, two inernal values are passed:
		 - the closure which assigns values to free variables in the function body (unless global value)
		 - the runtime typeof the function invocation, specifically the parameters to choose the right function body from
		
		Further, the function may return a generator, which is itself a function that has no parameters
		but instead has internal state. All variables of a generator function are incldued in the closure,
		since the state must be saved with each yield statement.
		How to call? Reset in for statement, or create new generator each time?

		In addition to creating the runtime incarnations of the function body, 
		which occurs at compile time,
		this statement generates code to populate a closure for the function, 
		implementing lexical scope in higher order functions.

		Future Optimizations are possible for:
		 - functions with an empty closure (just use c calling convention) - no caller cannot know
		  - functions with no polymorphic parameters that require rewriting...(non-general instrinsics except in record subtypes or monotypes)
		 If we have neitehr free variables, nor multiple instances are needed, we can create a simple function
	
	*/
	Expression::cgvalue Function::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		// Totall need the free variables, and maybe also the bound ones.
		VariableSet free,bound;
		if (!env->getOuterValueName().empty())
			bound.insert(env->getOuterValueName());
		getFreeVariables(free,bound);
		std::vector<std::wstring> variables(free.begin(),free.end());
		// Allocate closure with space for all free variables
		Value *ArgsAlloc[]={
			llvm::ConstantInt::get(llvm::Type::getInt32Ty(theContext),free.size(),false)
		};
		llvm::Value *closure=TmpB.CreateCall(TheModule->getFunction("allocClosure"), ArgsAlloc, "closureAlloc" );
		llvm::Value *cast_closure=TmpB.CreateBitCast(closure,closure_t->getPointerTo(),"closure");
		// Create actual function
		llvm::Function *fun=buildFunction(env,variables);

		// Write function pointer into closure
		llvm::Value *castfun=TmpB.CreateBitCast(fun,builtin_t,"funptr");
		llvm::Value *funptr=TmpB.CreateStructGEP(closure_t,cast_closure,ClosureFunction,"funptr_in_closure");
		TmpB.CreateStore(castfun,funptr);
		
		// WRite free variables into closure, then done
		populateClosure(TmpB,env,closure_t,cast_closure, free);
		return makeRTValue(closure,rtt::Function);
	}

	/// internal helper function to convert an Intro Function Type to an LLVM Function Type
	static llvm::FunctionType *buildFunctionType(Type *somefun)
	{
		FunctionType *fun;
		if (somefun->getKind() == Type::Variable)
		{
			TypeVariable *var = dynamic_cast<TypeVariable *>(somefun);
			fun= dynamic_cast<FunctionType*>(var->getSupertype()->find());
		}
		else 
			fun=dynamic_cast<FunctionType*>(somefun);
		bool returnsValue = fun->getReturnType()->find()->getKind()!=Type::Unit;
		// Create LLVM Function type, and Function with entry block
		std::vector<llvm::Type*> paramtypes;
		paramtypes.push_back(closure_t->getPointerTo()); // closure ptr
		if (returnsValue)
			paramtypes.push_back(rttype_t->getPointerTo()); // return value type
		for (size_t i=0;i< fun->parameterCount();++i)
		{
			paramtypes.push_back(builtin_t); // rt parameter types
			paramtypes.push_back(rttype_t); // rt parameter types
		}
		// Return type is either void when unit is returned, or boxed value.
		llvm::Type *returned;
		if (returnsValue)
			returned=builtin_t;
		else returned=llvm::Type::getVoidTy(theContext);
		return llvm::FunctionType::get(returned, paramtypes, false);
	}
	
	// Pass the actual environment - we will add the function and then the number of variables to allocate will be known after.
	llvm::Value *buildGeneratorStub(llvm::IRBuilder<> &closure_builder,CodeGenEnvironment *closure_env,const std::vector<std::wstring> &free_
		,BlockStatement *body)
	{
		VariableSet freeset,bound;
		body->getFreeVariables(freeset,bound);
		std::vector<std::wstring> free(freeset.begin(),freeset.end());
		// Set up a function for the generator body
		llvm::Function *GF = llvm::Function::Create(genfunc_t, llvm::Function::ExternalLinkage, "function", TheModule.get());
		makeReturnTypeZExt(GF);
		llvm::BasicBlock *entry = llvm::BasicBlock::Create(theContext, "entry", GF);

		// Create new builder for generator body...
		llvm::IRBuilder<> builder(theContext);
		builder.SetInsertPoint(entry);
		CodeGenEnvironment local(closure_env,CodeGenEnvironment::Generator);
		local.setGenerator(builder,GF,free);
		body->codeGen(builder,&local);
		// Generators' yield statements always create a ret instruction, so we don't need it here.
		GF->getBasicBlockList().push_back(local.getExitBlock());
		builder.SetInsertPoint(local.getExitBlock());
		// Always return false if we know not what to to with an iteration function.
		builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt1Ty(theContext), 0,false));
		llvm::verifyFunction(*GF);
		
		std::vector<llvm::Value*> ArgsAlloc {
			llvm::ConstantInt::get(llvm::Type::getInt32Ty(theContext), local.getClosureSize(),false),
			GF
		};
		llvm::Value *generator=closure_builder.CreateCall(TheModule->getFunction("allocGenerator"), ArgsAlloc, "generator" );
		llvm::Value *cast_generator = closure_builder.CreateBitCast(generator,generator_t->getPointerTo());
		populateClosure(closure_builder,closure_env,generator_t, cast_generator,freeset,true);
		return generator;
	}
	
	// Helper function building a function or generator body
	// In case of generators, this returns the actual iteration function.
	// The iteration function should be wrapped in a constructor...
	llvm::Function *Function::buildFunction(CodeGenEnvironment *parent,const std::vector<std::wstring> &free)
	{
		bool returnsValue = myType->getReturnType()->find()->getKind()!=Type::Unit;
		bool returnsGenerator = false;
		if (returnsValue) returnsGenerator = myType->getReturnType()->find()->getKind()==Type::Generator;
		llvm::FunctionType *FT = buildFunctionType(myType);
		//llvm::BasicBlock *currentBB=TmpB.GetInsertBlock();
		llvm::Function *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "function", TheModule.get());

		llvm::BasicBlock *entry = llvm::BasicBlock::Create(theContext, "entry", F);
		// Create new builder for function body...
		llvm::IRBuilder<> builder(theContext);
		builder.SetInsertPoint(entry);

		CodeGenEnvironment local(parent,CodeGenEnvironment::Closure);
		// Set up the function specific environment (parameters and closure values)
		local.setFunction(builder,F,parameters,returnsValue,free);
		// For Generators, the generated function here builds the generator closure
		// allocating space for each free and each bound variable.
		// The local env should be marked as a Generator, so that it collects the number of fields needed.
		// For normal functions, just generate body
		if (returnsGenerator)
		{
			llvm::Value *retval=buildGeneratorStub(builder,&local,free,body);
			CodeGenEnvironment::iterator iter=local.getReturnVariable();
			builder.CreateStore(retval,iter->second.address);
			builder.CreateStore(local.getRTT(rtt::Generator),iter->second.rtt);
			builder.CreateBr(local.getExitBlock());
		}
		else 
		{
			body->codeGen(builder,&local);
		}
		// Put the exit block at the end of the functions block list.
		F->getBasicBlockList().push_back(local.getExitBlock());
		builder.SetInsertPoint(local.getExitBlock());
		// ... and create the appropriate return function
		if (returnsValue) 
		{
				CodeGenEnvironment::iterator iter=local.getReturnVariable();
				llvm::Value *retval=builder.CreateLoad(iter->second.address,"return_value");
				builder.CreateRet(retval);
		}
		else
			builder.CreateRetVoid();
		llvm::verifyFunction(*F);
		return F;
	}

	Expression::cgvalue Application::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		// Set up mapping from parameter types passed to parameters in
		// check if the application function is non-generinc (function in function...)??
		// May not be needed? Why? If it is not a direct application of specific values, 
		// it can only be triggered by a lower level application this one is nested in,
		// and by transitive closure, there is a root application that is direct with specific values.
		bool returnsValue=getType()->getKind()!=Type::Unit;
		// 1) Execute expression evaluating to function (closure) - TODO: Builtin functions?!
		Expression::cgvalue rtfun=func->codeGen(TmpB,env);
		llvm::Value *closure=TmpB.CreateBitCast(rtfun.first,closure_t->getPointerTo(),"closure");
		// 2) Create type for called function to bit cast i8* to
		llvm::FunctionType *FT=buildFunctionType(func->getType());
		// 3) Extract function pointer from closure and bitcast to function type
		// Unless this is a recusrive call, in that case we know the function!
		llvm::Value *fun;
		// A little optimization for recursive functions,
		// where the current function can just be picked from the environment.
		// The env also knows the current closure, so this is all super simple:
		if (env->isCurrentClosure(rtfun.first))
			fun = env->currentFunction();
		else
		{
			llvm::Value *funptr = TmpB.CreateStructGEP(closure_t, closure, ClosureFunction, "funptr_in_closure");
			fun = TmpB.CreateLoad(funptr, "funbuf");
			fun = TmpB.CreateBitCast(fun, FT->getPointerTo(), "function");
		}
		std::vector<Value*> ArgsV;
		// Build array of arguments for function call
		ArgsV.reserve(myFuncType->parameterCount()*2+2);	// parameters with rtt plus closure + reserving one more should not hurt...
		ArgsV.push_back(closure); // first arg is always the closure!
		llvm::Value *returntypeptr=nullptr; // hold return value type if needed
		if (returnsValue) // second is return type, if a value is returnd
		{
			returntypeptr=env->createRetValRttDest();
			ArgsV.push_back(returntypeptr);
		}
		for (Expression* param : params) // for each param a type...
		{
			Expression::cgvalue paramval=param->codeGen(TmpB,env);
			ArgsV.push_back(paramval.first);
			ArgsV.push_back(paramval.second);
		}
		// 5) if the function returns a value, we want it: call 
		llvm::Value *retval=nullptr;
		
		if (returnsValue)
		{
			retval=TmpB.CreateCall(fun, ArgsV, "application");
			returntype=TmpB.CreateLoad(returntypeptr,"retvalrtt");
		}
		else TmpB.CreateCall(fun, ArgsV, "application"); 
		return std::make_pair(retval,returntype);
		
		//return std::make_pair(retval,returntype);
	}

	Expression::cgvalue Extraction::getWriteAddress(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		Expression::cgvalue dict=params[0]->codeGen(TmpB,env);
		Expression::cgvalue key=params[1]->codeGen(TmpB,env);
		// If key type in dict is variable, create code to write key rtt to dict.
		if (params[0]->getType()->getKind()==Type::Variable)
		{
			llvm::Function *setKeyTypeDictF = TheModule->getFunction("setKeyTypeDict");
			llvm::Value *args[]={ dict.first, key.second };
			TmpB.CreateCall(setKeyTypeDictF, args);
		}
		llvm::Function *getCreateSlotDictF = TheModule->getFunction("getCreateSlotDict");
		llvm::Function *getValueTypeDictF = TheModule->getFunction("getValueTypeDict");
		std::vector<llvm::Value*> args{ dict.first };
		// should return ptr to rtt in dict, so assign can set for variables
		llvm::Value *itemrtt=TmpB.CreateCall(getValueTypeDictF, args, "slotptrrtt");
		args.push_back(key.first);
		llvm::Value *item=TmpB.CreateCall(getCreateSlotDictF, args, "slotptr");
		return std::make_pair(item,itemrtt);
	}
	
	Expression::cgvalue Extraction::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		// Based on type of value being extracted from.
		// Dictionary only
		Expression::cgvalue dict=params[0]->codeGen(TmpB,env);
		Expression::cgvalue key=params[1]->codeGen(TmpB,env);;
		if (params[0]->getType()->getKind()==Type::Variable)
		{
			llvm::Function *setKeyTypeDictF = TheModule->getFunction("setKeyTypeDict");
			llvm::Value *args[]={ dict.first, key.second };
			TmpB.CreateCall(setKeyTypeDictF, args);
		}
		// If key type in dict is variable, create code to write key rtt to dict.
		llvm::Function *findDictF = TheModule->getFunction("findDict");
		std::vector<llvm::Value*> args{
			dict.first,
			key.first
		};
		llvm::Value *retval=TmpB.CreateCall(findDictF, args, "dict_value");
		return makeRTValue(retval,rtt::Variant);
	}
	
	Expression::cgvalue DictionaryErase::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		// Based on type of value being extracted from.
		// Dictionary only
		Expression::cgvalue dict=params[0]->codeGen(TmpB,env);
		Expression::cgvalue key=params[1]->codeGen(TmpB,env);;
		/*
		if (params[0]->getType()->getKind()==Type::Variable)
		{
			llvm::Function *setKeyTypeDictF = TheModule->getFunction("setKeyTypeDict");
			llvm::Value *args[]={ dict.first, key.second };
			TmpB.CreateCall(setKeyTypeDictF, args);
		}
		*/
		// If key type in dict is variable, create code to write key rtt to dict.
		llvm::Function *eraseDictF = TheModule->getFunction("eraseDict");
		std::vector<llvm::Value*> args{
			dict.first,
			key.first
		};
		TmpB.CreateCall(eraseDictF, args, "dict_value");
		return std::make_pair(dict.first,dict.second);
	}

	Expression::cgvalue Splice::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		// Generate the input sequence code.
		intro::Type *pt=params.front()->getType()->find();
		Expression::cgvalue source=params.front()->codeGen(TmpB,env);
		// If "last" is used, generate value... need local env!
		static cgcb_elem getlen[] =
		{
			{
				rtt::String,
				[](llvm::IRBuilder<> &builder,std::vector<llvm::Value*> &args){
					llvm::Function *sizeF = TheModule->getFunction("sizeString");
					llvm::Value *retval=builder.CreateCall(sizeF, args,"substr");
					return retval;
				}
			},
			{
				rtt::List,
				[](llvm::IRBuilder<> &builder,std::vector<llvm::Value*> &args){
					llvm::Function *sizeF = TheModule->getFunction("sizeList");
					llvm::Value *retval=builder.CreateCall(sizeF, args,"sublist");
					return retval;
				}
			}
		};
		std::vector<llvm::Value*> args {
			source.first
		};
		Expression::cgvalue length=genTypeChoiceOps(TmpB,env,source,pt,getlen,2);
		Type tI(Type::Integer);
		llvm::Value *lastval=TmpB.CreateSub(length.first,llvm::ConstantInt::get(integer_t, -1,true),"lastitem");
		lastval = createBoxValue(TmpB, env, lastval, &tI);
		// Add last (which is size-1) to fresh env for parameters
		CodeGenEnvironment local(env);
		CodeGenEnvironment::iterator localvar=local.createVariable(L"last");
		TmpB.CreateStore(lastval,localvar->second.address);
		TmpB.CreateStore(env->getRTT(&counter),localvar->second.rtt);
		Expression::cgvalue extras[2];
		extras[0]=params[1]->codeGen(TmpB,&local);
		extras[0].first = createUnboxValue(TmpB, env, extras[0].first, &tI);
		if (params.size() == 3)
		{
			extras[1] = params[2]->codeGen(TmpB, &local);
			extras[1].first = createUnboxValue(TmpB, env, extras[1].first, &tI);
		}
		else extras[1]=extras[0];
		// Call the actual splice operator
		static cgcb_elem splice[] =
		{
			{
				rtt::String,
				[](llvm::IRBuilder<> &builder,std::vector<llvm::Value*> &args){
					llvm::Function *subF = TheModule->getFunction("subString");
					llvm::Value *retval=builder.CreateCall(subF, args,"substr");
					return retval;
				}
			},
			{
				rtt::List,
				[](llvm::IRBuilder<> &builder,std::vector<llvm::Value*> &args){
					llvm::Function *subF = TheModule->getFunction("subList");
					llvm::Value *retval=builder.CreateCall(subF, args,"sublist");
					return retval;
				}
			}
		};
		return genTypeChoiceOps(TmpB,env,source,pt,splice,2,false,extras,2);
	}

	Expression::cgvalue Assignment::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		// check subexpression type
		Expression::cgvalue ret=value->codeGen(TmpB,env);
		// may need coercion... also increment old and decrement new
		Expression::cgvalue destval = destination->getWriteAddress(TmpB,env);
		TmpB.CreateStore(ret.first,destval.first);
		//TmpB.CreateStore(ret.first,destval.first);
		return ret;
	}
	
	Expression::cgvalue UnaryOperation::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		Expression::cgvalue p=params.front()->codeGen(TmpB,env);
		switch(op)
		{
		case Not:
		{
			intro::Type TBoolean(intro::Type::Boolean);
			llvm::Value *val = createUnboxValue(TmpB, env, p.first, &TBoolean);
			val = TmpB.CreateNot(p.first, "nottmp");
			val = createBoxValue(TmpB, env, p.first, &TBoolean);
			return makeRTValue(val, rtt::Boolean);
			//return makeRTValue(TmpB.CreateNot(p.first, "nottmp"), rtt::Boolean);
		}
		case Negate:
			{
				cgcb_elem ops[] =
				{
					{
						rtt::Integer,
						[&env](llvm::IRBuilder<> &builder,std::vector<llvm::Value*> &args){
							intro::Type t(intro::Type::Integer);
							llvm::Value *val=createUnboxValue(builder,env, args.front(),&t);
							val=builder.CreateNeg(val,"intnegtmp");
							return createBoxValue(builder, env, val, &t);
						}
					},
					{
						rtt::Real,
						[&env](llvm::IRBuilder<> &builder,std::vector<llvm::Value*> &args) {
							intro::Type t(intro::Type::Real);
							llvm::Value *val = createUnboxValue(builder, env, args.front(), &t);
							val = builder.CreateFNeg(val, "relnegtmp");
							return createBoxValue(builder, env, val, &t);
						}
					}
				};
				intro::Type *pt=params.front()->getType(env);
				return genTypeChoiceOps(TmpB,env,p,pt,ops,2);
			}
			break;
		}
		return makeRTValue(nullptr,rtt::Undefined); // Should not occur in practice due to type checking
	}

	Expression::cgvalue BooleanBinary::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		Expression::cgvalue L = params.front()->codeGen(TmpB,env);
		Expression::cgvalue R = params.back()->codeGen(TmpB,env);
		intro::Type TBoolean(intro::Type::Boolean);
		L.first=createUnboxValue(TmpB,env,L.first,&TBoolean);
		R.first=createUnboxValue(TmpB,env,R.first,&TBoolean);
		llvm::Value *retval=nullptr;
		switch(op)
		{
		case And: retval=TmpB.CreateAnd(L.first,R.first,"andtmp"); break;
		case Or: retval=TmpB.CreateOr(L.first,R.first,"ortmp"); break;
		case Xor: retval=TmpB.CreateXor(L.first,R.first,"xortmp"); break;
		}
		return makeRTValue(createBoxValue(TmpB,env,retval,&TBoolean),rtt::Boolean);
	}
	
	Expression::cgvalue CompareOperation::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		
		intro::Type *lt=params.front()->getType(env)->find();
		intro::Type *rt=params.back()->getType(env)->find();
		Expression::cgvalue lhs = params.front()->codeGen(TmpB,env);
		Expression::cgvalue rhs = params.back()->codeGen(TmpB,env);
		intro::Type TBoolean(intro::Type::Boolean);
		// If we can specialize for number, we will do it ;) Because we can!
		if (op==Greater || op==GreaterEqual || op==Less || op==LessEqual)
		{
			bool isVariable = lt->getKind()==Type::Variable || rt->getKind()==Type::Variable;
			bool isNumber=lt->getSupertype()->getKind()==Type::Number || rt->getSupertype()->getKind()==Type::Number;
			Expression::cgvalue strresult,numresult;
			
			llvm::Function *TheFunction = env->currentFunction();
			llvm::BasicBlock *numopblock = nullptr;
			llvm::BasicBlock *stropblock = nullptr;
			llvm::BasicBlock *mergeblock = nullptr;

			if (isVariable && !isNumber)
			{
				// May be a number due to var being top bound!
				llvm::Value *lhsString=cgTestRTT(TmpB,lhs.second, intro::rtt::String);
				stropblock = llvm::BasicBlock::Create(theContext, "stropblock");
				numopblock = llvm::BasicBlock::Create(theContext, "numopblock");
				mergeblock = llvm::BasicBlock::Create(theContext, "mergeblock");

				TmpB.CreateCondBr(lhsString,stropblock,numopblock);
				TheFunction->getBasicBlockList().push_back(stropblock);
				TmpB.SetInsertPoint(stropblock);
			}
			
			// Any comparable, have to check (possibly at runtime) for string or number
			if (lt->getKind()==Type::String || rt->getKind()==Type::String || (isVariable && !isNumber))
			{
				// we can use string compare
				llvm::Function *strop;
				switch(op)
				{
				case Greater: 
					strop = TheModule->getFunction("greaterString");
					break;
				case GreaterEqual: 
					strop = TheModule->getFunction("greaterEqString");
					break;
				case Less: 
					strop = TheModule->getFunction("lessString");
					break;
				case LessEqual: 
					strop = TheModule->getFunction("lessEqString");
					break;
				default:
					break;
				}
				std::vector<llvm::Value*> args{
					lhs.first,
					rhs.first
				};
				llvm::Value *strrel=TmpB.CreateCall(strop, args, "strop");
				strrel=createBoxValue(TmpB,env,strrel,&TBoolean);
				strresult=makeRTValue(strrel,rtt::Boolean);
			}
			
			if (isVariable && !isNumber)
			{
				TmpB.CreateBr(mergeblock);
				//numopblock=TmpB.GetInsertBlock();
				TheFunction->getBasicBlockList().push_back(numopblock);
				TmpB.SetInsertPoint(numopblock);
			}

			if (lt->getKind()!=Type::String && rt->getKind()!=Type::String)
			{
				Application::codegen_cb int_op,real_op;
				switch(op)
				{
				case Greater: 
					int_op=[](llvm::IRBuilder<> &builder,std::vector<llvm::Value*>&args)
					{ return builder.CreateICmpSGT(args[0],args[1], "igttmp");};
					real_op=[](llvm::IRBuilder<> &builder,std::vector<llvm::Value*>&args)
					{ return builder.CreateFCmpUGT(args[0],args[1], "rgttmp");};
					break;
				case GreaterEqual: 
					int_op=[](llvm::IRBuilder<> &builder,std::vector<llvm::Value*>&args)
					{ return builder.CreateICmpSGE(args[0],args[1], "igttetmp");};
					real_op=[](llvm::IRBuilder<> &builder,std::vector<llvm::Value*>&args)
					{ return builder.CreateFCmpUGE(args[0],args[1], "rgtetmp");};
					break;
				case Less: 
					int_op=[](llvm::IRBuilder<> &builder,std::vector<llvm::Value*>&args)
					{ return builder.CreateICmpSLT(args[0],args[1], "ilstmp");};
					real_op=[](llvm::IRBuilder<> &builder,std::vector<llvm::Value*>&args)
					{ return builder.CreateFCmpULT(args[0],args[1], "rlstmp");};
					break;
				case LessEqual: 
					int_op=[](llvm::IRBuilder<> &builder,std::vector<llvm::Value*>&args) 
					{ return builder.CreateICmpSLE(args[0],args[1], "ilsetmp");};
					real_op=[](llvm::IRBuilder<> &builder,std::vector<llvm::Value*>&args) 
					{ return builder.CreateFCmpULE(args[0],args[1], "rlsetmp");};
					break;
				default:
					break;
				}
				numresult=genNumberOps(TmpB,env,lhs,lt,rhs,rt,int_op,&TBoolean,real_op,&TBoolean);
				numopblock=TmpB.GetInsertBlock();
			}
			
			if (isVariable && !isNumber)
			{
				TmpB.CreateBr(mergeblock);
				TheFunction->getBasicBlockList().push_back(mergeblock);
				TmpB.SetInsertPoint(mergeblock);
				llvm::PHINode *result=TmpB.CreatePHI(builtin_t,2,"result");
				result->addIncoming(strresult.first,stropblock);
				result->addIncoming(numresult.first,numopblock);
				return makeRTValue(result,rtt::Boolean);
			}
			else if (lt->getKind()==Type::String)
			{
				return strresult;
			}
			else return numresult;
			//}	
		}
		// if it's strings, we can compare using strcmp
		// all others can only be (not) equal
		// At this point, just hand it off to the runtime compare function?
		if ((lt->getKind()==Type::Real || lt->getKind()==Type::Integer || lt->getSupertype()->getKind()==Type::Number) &&
			(rt->getKind()==Type::Real || rt->getKind()==Type::Integer || rt->getSupertype()->getKind()==Type::Number))
		{
			Application::codegen_cb int_op,real_op;
			switch(op)
			{
			case Equal:	
				int_op=[](llvm::IRBuilder<> &builder,std::vector<llvm::Value*>&args) 
				{ return builder.CreateICmpEQ(args[0],args[1], "ieqtmp");};
				real_op=[](llvm::IRBuilder<> &builder,std::vector<llvm::Value*>&args)
				{ return builder.CreateFCmpUEQ(args[0],args[1], "reqtmp");};
				break;
			case Different:	
				int_op=[](llvm::IRBuilder<> &builder,std::vector<llvm::Value*>&args)
				{ return builder.CreateICmpNE(args[0],args[1], "ineqtmp");};
				real_op=[](llvm::IRBuilder<> &builder,std::vector<llvm::Value*>&args)
				{ return  builder.CreateFCmpUNE(args[0],args[1], "rneqtmp");};
				break;
			default:
				break;
			}
			return genNumberOps(TmpB,env,lhs,lt,rhs,rt,int_op,&TBoolean,real_op,&TBoolean);
		}
		// Give up and just go with the runtime function
		llvm::Function *equalPolyF = TheModule->getFunction("equalPoly");
		std::vector<llvm::Value*> args{
			lhs.first,
			lhs.second,
			rhs.first,
			rhs.second
		};
		llvm::Value *retval=TmpB.CreateCall(equalPolyF, args, "equalPolyOp");
		if (op==Different)
			retval=TmpB.CreateNot(retval,"notequal");
		retval=createBoxValue(TmpB,env,retval,&TBoolean);
		return makeRTValue(retval,rtt::Boolean);
		//return NULL;
	}

	Expression::cgvalue ArithmeticBinary::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		intro::Type *lhst=params[0]->getType()->find();
		intro::Type *rhst=params[1]->getType()->find();
		Expression::cgvalue lhs=params[0]->codeGen(TmpB,env);
		Expression::cgvalue rhs=params[1]->codeGen(TmpB,env);
		intro::Type int_type(intro::Type::Integer);
		intro::Type real_type(intro::Type::Real);
		switch(op)
		{
		case Add: return genNumberOps(TmpB,env,lhs,lhst,rhs,rhst,gen_add_int,&int_type,gen_add_real,&real_type); break;
		case Sub: return genNumberOps(TmpB,env,lhs,lhst,rhs,rhst,gen_sub_int,&int_type,gen_sub_real,&real_type); break;
		case Mul: return genNumberOps(TmpB,env,lhs,lhst,rhs,rhst,gen_mul_int,&int_type,gen_mul_real,&real_type); break;
		case Div: return genNumberOps(TmpB,env,lhs,lhst,rhs,rhst,gen_div_int,&int_type,gen_div_real,&real_type); break;
		case Mod: return genNumberOps(TmpB,env,lhs,lhst,rhs,rhst,gen_rem_int,&int_type,gen_rem_real,&real_type); break;
		}
		return makeRTValue(nullptr,rtt::Undefined);
	}

	/////////////////////////////////////////////////////////////////////
	// Code Generation Methods (Statements)
	//

	bool ExpressionStatement::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		Expression::cgvalue val=expr->codeGen(TmpB,env);
		// In interactive mode, we print the result
		if (isInteractive && env->isGlobal() && expr->getType()->getKind() != Type::Unit)
		{
			// Allocate a string
			llvm::Function *allocStringF = TheModule->getFunction("allocString");
			llvm::Value *argsAlloc[] = { llvm::ConstantInt::get(llvm::Type::getInt64Ty(theContext), 0,true) };
			llvm::Value *theStr = TmpB.CreateCall(allocStringF, argsAlloc, "valstr");
			// Serialize value into it
			llvm::Value *argsToStr[] =
			{
				theStr,
				val.first,
				val.second
			};
			llvm::Function *toStringF = TheModule->getFunction("toStringPoly");
			TmpB.CreateCall(toStringF, argsToStr);
			// Print the result
			llvm::Value *argsPrint[] = { theStr };
			llvm::Function *subF = TheModule->getFunction("print");
			TmpB.CreateCall(subF, argsPrint);
			llvm::Value *argsDecr[] = { theStr, llvm::ConstantInt::get(llvm::Type::getInt16Ty(theContext), rtt::String,false) };
			// Then get rid of the allocated buffer again...
			subF = TheModule->getFunction("decrement");
			TmpB.CreateCall(subF, argsDecr);
		}
		return true;
	}

	bool ValueStatement::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env) 
	{
		Type *t=value->getType(env);
		if (isInteractive && env->isGlobal())
		{
			std::wcout << name << L":";
			t->find()->print(std::wcout);
			std::wcout << std::endl;
		}
		// For recursive functions, set name of function in env (pass to function)
		// => Check if t is function (and if this variable's name is free in the expression?)
		if (t->find()->getKind() == Type::Function)
		{
			env->setOuterValueName(name);
		}
		Expression::cgvalue rhs=value->codeGen(TmpB,env);
		// Clear function name in CGEnv...
		env->setOuterValueName(L"");
		// Create actual variable
		CodeGenEnvironment::iterator iter = env->createVariable(name);
		if (iter == env->end()) return false;
		TmpB.CreateStore(rhs.first, iter->second.address);
		TmpB.CreateStore(rhs.second, iter->second.rtt);
		return true;
	}

	bool IfStatement::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		size_t i;
		iterator iter;
		intro::Type TBoolean(intro::Type::Boolean);
		std::vector<BasicBlock *> alternatives;
		alternatives.reserve(conditions.size()+1);
		// Create blocks for every condition, then for the else clause. The else case occurs zero or one time,
		// The then cases are placed in a vector, the else case is always present,
		// even if empty.
		llvm::Function *TheFunction = env->currentFunction();
		for (i=0;i<conditions.size();i++)
		{
			alternatives.push_back(BasicBlock::Create(theContext, "test_if"));
		}
		llvm::BasicBlock *post_ite;
		if (isReturnLike()) post_ite = env->getExitBlock();
		else post_ite = BasicBlock::Create(theContext, "post_ite");
		if (otherwise!=NULL)
			alternatives.push_back(BasicBlock::Create(theContext, "else"));
		else 
			alternatives.push_back(post_ite);
		
		// since we organize all parts of if-then-else in blocks, we need to jump to the first
		TmpB.CreateBr(alternatives.front());
		//Because alternatives is guaranteed to be one or two less than the conditions size
		// using i++ without checks is safe - eliminates a condition inside the loop!
		for (i=0,iter=conditions.begin();iter!=conditions.end();i++,iter++)
		{
			// Generate test in if in block from alternatives
			TheFunction->getBasicBlockList().push_back(alternatives[i]);
			TmpB.SetInsertPoint(alternatives[i]);
			Expression::cgvalue condition=iter->first->codeGen(TmpB,env);
			llvm::Value *condval=createUnboxValue(TmpB,env,condition.first,&TBoolean);
			// Body block and conditional branch
			llvm::BasicBlock *block=BasicBlock::Create(theContext, "if_body");
			TmpB.CreateCondBr(condval, block, alternatives[i+1]);
			// Generate body
			CodeGenEnvironment local(env);
			TheFunction->getBasicBlockList().push_back(block);
			TmpB.SetInsertPoint(block);
			iter->second->codeGen(TmpB,&local);
			if (post_ite != env->getExitBlock())
				TmpB.CreateBr(post_ite);
		}
		if (otherwise!=NULL)
		{
			// Else only has a body, no more checks
			CodeGenEnvironment local(env);
			TheFunction->getBasicBlockList().push_back(alternatives.back());
			TmpB.SetInsertPoint(alternatives.back());
			//iter->second->codeGen(TmpB,&local);
			otherwise->codeGen(TmpB, &local);
			if (post_ite != env->getExitBlock())
				TmpB.CreateBr(post_ite);
		}
		if (post_ite!=env->getExitBlock())
			TheFunction->getBasicBlockList().push_back(post_ite);
		TmpB.SetInsertPoint(post_ite);
			
		return true;
	}

	bool BlockStatement::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		// Create Codegen Env, and apply to body statements.
		CodeGenEnvironment local(env);
		std::list<Statement*>::iterator stmts;
		for (stmts=body.begin();stmts!=body.end();stmts++)
		{
			(*stmts)->codeGen(TmpB,&local);
		}
		if (isReturnLike())
		{
			// If his is a return-like statement, then there was a return statement on every possible
			// execution path, which has decremented references for all referenced values in the 
			// environemnt of the function.
		}
		else
		{
			// If it is not return like, it must decrement reference counts for all referecned values 
			// in he current environent
		}
		return true;
	}

	bool ForStatement::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		// Super simple due to heavy-weight components :-D and lambdas!
		generators->setBodyCallback([&](llvm::IRBuilder<> &builder,CodeGenEnvironment *env) {
			body->codeGen(TmpB,env);
			return body->isReturnLike();
		});
		generators->codeGen(TmpB,env);
		return false;
	}

	bool WhileStatement::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		// Create blocks for while condition test, loop body and the block after
		//llvm::Function *TheFunction = TmpB.GetInsertBlock()->getParent();
		llvm::Function *TheFunction = env->currentFunction();
		BasicBlock *test = BasicBlock::Create(theContext, "test", TheFunction);
		BasicBlock *loopbody = BasicBlock::Create(theContext, "body", TheFunction);
		BasicBlock *after= BasicBlock::Create(theContext, "after", TheFunction);
		TmpB.CreateBr(test); // Go to next basic block - it has another entry point, hence the branch
		// Test the condition, if false skip body and done
		TmpB.SetInsertPoint(test);
		Expression::cgvalue cond=condition->codeGen(TmpB,env);	
		intro::Type TBoolean(intro::Type::Boolean);
		cond.first=createUnboxValue(TmpB,env,cond.first,&TBoolean);
		TmpB.CreateCondBr(cond.first,loopbody ,after); // go to returning NULL if the element is NULL
		// The loop body comes next in it's block
		TmpB.SetInsertPoint(loopbody );
		CodeGenEnvironment localenv(env);
		body->codeGen(TmpB,&localenv);
		TmpB.CreateBr(test); // Go to next basic block - it has another entry point, hence the branch
		TmpB.SetInsertPoint(after);
		return false;
	}

	bool CaseStatement::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		// get variant tag
		bool isRetLike = isReturnLike();
		llvm::Function *TheFunction = env->currentFunction();
		Expression::cgvalue input=caseof->codeGen(TmpB,env);
		llvm::Function *getTagVariantF = TheModule->getFunction("getTagVariant");
		llvm::Function *getSlotVariantF = TheModule->getFunction("getSlotVariant");
		llvm::Function *getFieldVariantF = TheModule->getFunction("getFieldVariant");
		llvm::Function *getFieldRTTVariantF = TheModule->getFunction("getFieldRTTVariant");
		std::vector<llvm::Value*> args{
			input.first
		};
		llvm::Value *variant_tag=TmpB.CreateCall(getTagVariantF, args, "theTag");
		llvm::BasicBlock *postcase = nullptr;
		llvm::SwitchInst *jumptable;
		if (isRetLike) // Return oike statement, default to exit block
		{
			jumptable=TmpB.CreateSwitch(variant_tag,env->getExitBlock()); 
		}
		else // not ret like, so create a block to go on in. that becomes our default
		{
			postcase = llvm::BasicBlock::Create(theContext, "postcase");
			jumptable=TmpB.CreateSwitch(variant_tag,postcase); // add to current block
		}
//TheFunction->dump();
		for (iterator iter=cases.begin();iter!=cases.end();++iter)
		{
			// create case for iter, then bind variables it extracts from variaat
			size_t current_tag=getTag((*iter)->tag);
			llvm::BasicBlock *current = llvm::BasicBlock::Create(theContext, "variantcase");
			jumptable->addCase(llvm::ConstantInt::get(llvm::Type::getInt64Ty(theContext), current_tag,false),current);
			TheFunction->getBasicBlockList().push_back(current);
			TmpB.SetInsertPoint(current);
			// add bound fields from variants to local env
			CodeGenEnvironment local(env);
			intro::RecordType *record = (*iter)->myRecord;
			intro::RecordType::iterator fit;
			for (fit=record->begin();fit!=record->end();++fit) // Must load from slot
			{
				std::vector<llvm::Value*> argsSlot {
					input.first,
					createGlobalString(fit->first),
					llvm::ConstantInt::get(llvm::Type::getInt64Ty(theContext),fit->first.size(),false)
				};
				llvm::Value *slot=TmpB.CreateCall(getSlotVariantF, argsSlot,"fieldptr");
				std::vector<llvm::Value*> argsGet {
					input.first,
					slot
				};
				llvm::Value *fieldaddr=TmpB.CreateCall(getFieldVariantF, argsGet,"fieldptr");
				fieldaddr=TmpB.CreateLoad(fieldaddr,"varfieldrtt");
				llvm::Value *fieldrtt=TmpB.CreateCall(getFieldRTTVariantF, argsGet,"fieldvalue");
				fieldrtt=TmpB.CreateLoad(fieldrtt,"varfieldrtt");
				// Put into environment
				CodeGenEnvironment::iterator iter=local.createVariable(fit->first);
				TmpB.CreateStore(fieldaddr,iter->second.address);
				TmpB.CreateStore(fieldrtt,iter->second.rtt);
				//iter->second.address=fieldaddr;
				//iter->second.rtt=fieldrtt;
			}
			// The environment is set up, generate the body
			(*iter)->body->codeGen(TmpB,&local);
			// done, jump to end of case if we did not have a return statement in here
			if (!isRetLike) TmpB.CreateBr(postcase);
		}
		// Create block after csae statement if needed
		if (!isRetLike) 
		{
			TheFunction->getBasicBlockList().push_back(postcase);
			TmpB.SetInsertPoint(postcase);
		}
		return false;
	}

	bool ReturnStatement::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		// If we're returning values...
		if (expr!=NULL)
		{
			/// Expect alloc'ed value "retval" from env
			CodeGenEnvironment::iterator v=env->getReturnVariable();
			Expression::cgvalue result=expr->codeGen(TmpB,env);
			TmpB.CreateStore(result.first,v->second.address);
			TmpB.CreateStore(result.second,v->second.rtt);
		}
		// Create cleanup from cgenv, jump to next cgenv.
		// Needs pointer to it's function, which can then keep track of next cleanup block needed
		// Or maybe have it done by CGEnv - if it knows it's a functions local environment, 
		// everything above can clean up, accordig to current state.
		TmpB.CreateBr(env->getExitBlock());
		return false;
	}

	bool YieldStatement::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		llvm::Function *setResultF = TheModule->getFunction("setResultGenerator");
		llvm::Function *setStateF = TheModule->getFunction("setStateGenerator");
		llvm::Function *TheFunction = env->currentFunction();
		// The yield statement puts the result value and rtt in the generator closure
		// then returns true or false, depending on if a value was produced.
		// Then prepare the next state
		llvm::Value *generator=env->getGeneratorClosure(TmpB);
		
		llvm::SwitchInst *dispatch = env->getStateDispatch();
		size_t nextStateId=dispatch->getNumCases();
		
		if (expr!=nullptr) // return value from expression
		{
			Expression::cgvalue result=expr->codeGen(TmpB,env);
			std::vector<llvm::Value*> argsRet{ generator, result.first, result.second };
			TmpB.CreateCall(setResultF, argsRet);
			llvm::ConstantInt *nextStateVal=llvm::ConstantInt::get(llvm::Type::getInt64Ty(theContext), nextStateId,false);
			std::vector<llvm::Value*> argsNextState{ generator, nextStateVal };
			TmpB.CreateCall(setStateF, argsNextState);
			TmpB.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt1Ty(theContext), 1,false));
			// Set up the new state for continuation
			llvm::BasicBlock *nextState = llvm::BasicBlock::Create(theContext, std::string("gen_state_") + std::to_string(nextStateId));
			dispatch->addCase(nextStateVal, nextState);
			TheFunction->getBasicBlockList().push_back(nextState);
			TmpB.SetInsertPoint(nextState);
		}
		else // done, cleanup
		{
			std::vector<llvm::Value*> args{ 
				generator,
				llvm::Constant::getNullValue(builtin_t),
				llvm::ConstantInt::get(llvm::Type::getInt16Ty(theContext), rtt::Undefined,false)
			};
			TmpB.CreateCall(setResultF, args);
			llvm::ConstantInt *nextStateVal=llvm::ConstantInt::get(llvm::Type::getInt64Ty(theContext), nextStateId,false);
			std::vector<llvm::Value*> argsNextState{ generator, nextStateVal };
			TmpB.CreateCall(setStateF, argsNextState);
			TmpB.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt1Ty(theContext), 0,false));
			// Set up the new state that just returns false
			llvm::BasicBlock *nextState = llvm::BasicBlock::Create(theContext, std::string("gen_state_done"));
			dispatch->addCase(nextStateVal, nextState);
			TheFunction->getBasicBlockList().push_back(nextState);
			TmpB.SetInsertPoint(nextState);
			TmpB.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt1Ty(theContext), 0,false));
		}
		return true;
	}

	bool RangeGen::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		// 1: We need the function, and we'll create some blocks
		llvm::Function *TheFunction = env->currentFunction();
		loop = llvm::BasicBlock::Create(theContext, "range_next");
		llvm::BasicBlock *test = llvm::BasicBlock::Create(theContext, "range_test");
		llvm::BasicBlock *body = llvm::BasicBlock::Create(theContext, "range_body");
		exit = llvm::BasicBlock::Create(theContext, "range_exit"); // Added to function later by genstmt

		// 2: Compute iteration paramters (from, to and optional by defaults to 1)
		// and store to variables... from value goes directly into iteration variable, that is stored
		std::wstring param_vals_base(getVariableName());
		param_vals_base+=L"_gen_";
		Expression::cgvalue elem=from->codeGen(TmpB,env);
		CodeGenEnvironment::iterator from_ptr=env->createVariable(getVariableName());
		TmpB.CreateStore(elem.first,from_ptr->second.address);
		TmpB.CreateStore(elem.second,from_ptr->second.rtt);
				
		Expression::cgvalue to_val=to->codeGen(TmpB,env);
		CodeGenEnvironment::iterator to_ptr=env->createVariable(param_vals_base+L"to");
		TmpB.CreateStore(to_val.first,to_ptr->second.address);
		TmpB.CreateStore(to_val.second,to_ptr->second.rtt);

		Type tI(Type::Integer);
		Expression::cgvalue by_val;
		if (by == nullptr)
		{
			by_val.first = llvm::ConstantInt::get(llvm::Type::getInt64Ty(theContext), 1, true);
			by_val.first = createBoxValue(TmpB, env, by_val.first, &tI);
		}
		else by_val=by->codeGen(TmpB,env);
		CodeGenEnvironment::iterator by_ptr=env->createVariable(param_vals_base+L"by");
		TmpB.CreateStore(by_val.first,by_ptr->second.address);
		TmpB.CreateStore(env->getRTT(rtt::Integer),by_ptr->second.rtt);

		TmpB.CreateBr(test);
				
		// 4: next block, Increment the iteration variable and branch to test as well.
		TmpB.SetInsertPoint(loop);
		TheFunction->getBasicBlockList().push_back(loop);
		llvm::Value *iter_val=TmpB.CreateLoad(from_ptr->second.address, "iterval");
		llvm::Value *incr_val=TmpB.CreateLoad(by_ptr->second.address, "incrval");
		iter_val = createUnboxValue(TmpB, env, iter_val, &tI);
		incr_val = createUnboxValue(TmpB, env, incr_val, &tI);
		iter_val=TmpB.CreateAdd(iter_val, incr_val,"incr_iter");
		iter_val = createBoxValue(TmpB, env, iter_val, &tI);
		TmpB.CreateStore(iter_val,from_ptr->second.address);
		TmpB.CreateBr(test);
		
		// 5: Test if we're done
		TmpB.SetInsertPoint(test);
		TheFunction->getBasicBlockList().push_back(test);
		iter_val=TmpB.CreateLoad(from_ptr->second.address, "iterval");
		iter_val = createUnboxValue(TmpB, env, iter_val, &tI);
		llvm::Value *last_val=TmpB.CreateLoad(to_ptr->second.address, "lastval");
		last_val = createUnboxValue(TmpB, env, last_val, &tI);
		llvm::Value *notdone=TmpB.CreateICmpSLE(iter_val,last_val,"condpassed");
		TmpB.CreateCondBr(notdone,body,exit);
		
		TmpB.SetInsertPoint(body);
		TheFunction->getBasicBlockList().push_back(body);

		return true;
	}

	void RangeGen::codeGenExitBlock(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		llvm::Function *TheFunction = env->currentFunction();
		TmpB.SetInsertPoint(exit);
		TheFunction->getBasicBlockList().push_back(exit);
	}
	
	bool ContainerGen::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		// 1: We need the function, and we'll create some blocks
		llvm::Function *TheFunction = env->currentFunction();
		loop = llvm::BasicBlock::Create(theContext, "container_next");
		llvm::BasicBlock *body = llvm::BasicBlock::Create(theContext, "container_body");
		exit = llvm::BasicBlock::Create(theContext, "container_exit"); // Added to function later by genstmt
		rawcontval=container->codeGen(TmpB,env);
		//llvm::BasicBlock *rawcontblock=TmpB.GetInsertBlock();
		// All our possible input types
		static Application::cgcb_elem toGenerator[] =
		{
			{
				rtt::List,
				[](llvm::IRBuilder<> &builder,std::vector<llvm::Value*> &args){
					llvm::Function *subF = TheModule->getFunction("allocListGenerator");
					llvm::Value *retval=builder.CreateCall(subF, args,"listgen");
					return retval;
				}
			},
			{
				rtt::String,
				[](llvm::IRBuilder<> &builder,std::vector<llvm::Value*> &args){
					llvm::Function *subF = TheModule->getFunction("allocStringGenerator");
					llvm::Value *retval=builder.CreateCall(subF, args,"strgen");
					return retval;
				}
			},
			{
				rtt::Dictionary,
				[](llvm::IRBuilder<> &builder,std::vector<llvm::Value*> &args){
					llvm::Function *subF = TheModule->getFunction("allocDictGenerator");
					llvm::Value *retval=builder.CreateCall(subF, args,"dictgen");
					return retval;
				}
			},
			{
				rtt::Generator,
				[](llvm::IRBuilder<> &builder,std::vector<llvm::Value*> &args){
					// reset generator by setting state to 0
					// Also, prevent being deleted, as it is not allocated...
					// pass env internally to other callbacks?
					// needs changing cg_callback typedef to std::Function based... no big deal?!
					//return builder.CreateBitCast(args.front(),builtin_t,"noopforphi");
					return args.front();
				}
			}
		};
		// put generator in variable
		std::wstring genvar_name(getVariableName());
		genvar_name+=L"_variable";
		generator=genTypeChoiceOps(TmpB,env,rawcontval,source_type,toGenerator,4);
		CodeGenEnvironment::iterator gen_ptr=env->createVariable(genvar_name);
		TmpB.CreateStore(generator.first,gen_ptr->second.address);
		TmpB.CreateStore(generator.second,gen_ptr->second.rtt);
		// Exit block needs to load from those variables, they may be in a generator closure...
		// with the block sequence possibly messed up by it's state machine
		rawcontval.first = gen_ptr->second.address;
		rawcontval.second = gen_ptr->second.rtt;
		TmpB.CreateBr(loop);
		
		TmpB.SetInsertPoint(loop);
		TheFunction->getBasicBlockList().push_back(loop);
		// loop as long as iterator produces values.
		llvm::Function *callGenF = TheModule->getFunction("callGenerator");
		//std::vector<llvm::Value*> args { finalgen };
		llvm::Value *gen_val=TmpB.CreateLoad(gen_ptr->second.address,"load_gen");
		std::vector<llvm::Value*> args { gen_val };
		//std::vector<llvm::Value*> args { generator.first };
		llvm::CallInst *hasmore=TmpB.CreateCall(callGenF, args,"hasmore");
		hasmore->addAttribute(llvm::AttributeSet::ReturnIndex,Attribute::ZExt);

		TmpB.CreateCondBr(hasmore,body,exit);
		
		TmpB.SetInsertPoint(body);
		TheFunction->getBasicBlockList().push_back(body);
		// Start the body with setting up the new value
		llvm::Function *getValGenF = TheModule->getFunction("getResultGenerator");
		llvm::Function *getResultTypeGenF = TheModule->getFunction("getResultTypeGenerator");
		llvm::Value *genval=TmpB.CreateCall(getValGenF, args,"genval");
		llvm::Value *genvalrtt=TmpB.CreateCall(getResultTypeGenF, args,"genvalrtt");
		CodeGenEnvironment::iterator elem=env->find(getVariableName());		
		TmpB.CreateStore(genval,elem->second.address);
		TmpB.CreateStore(genvalrtt,elem->second.rtt);
		
		return true;
	}
	
	void ContainerGen::codeGenExitBlock(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		llvm::Function *TheFunction = env->currentFunction();
		TmpB.SetInsertPoint(exit);
		TheFunction->getBasicBlockList().push_back(exit);
		auto callDecrement = [](llvm::IRBuilder<> &builder,std::vector<llvm::Value*> &args){
			llvm::Function *subF = TheModule->getFunction("decrement");
			llvm::Value *myArgs[] = {
				args[0],
				llvm::ConstantInt::get(llvm::Type::getInt16Ty(theContext), rtt::Generator,false)
			};
			builder.CreateCall(subF, myArgs);
			return llvm::Constant::getNullValue(builtin_t);
		};
		static Application::cgcb_elem resetGen[] =
		{
			{
				rtt::List,
				callDecrement
			},
			{
				rtt::String,
				callDecrement
			},
			{
				rtt::Dictionary,
				callDecrement
			},
			{
				rtt::Generator,
				[](llvm::IRBuilder<> &builder,std::vector<llvm::Value*> &args){
					llvm::Function *subF = TheModule->getFunction("setStateGenerator");
					llvm::Value *myVals[] = {
						args[0],
						llvm::ConstantInt::get(llvm::Type::getInt64Ty(theContext), 0,false)
					};
					builder.CreateCall(subF, myVals);
					return llvm::Constant::getNullValue(builtin_t);
				}
			}
		};
		// reset the generator according to the original rtt and type
		Expression::cgvalue resetval = make_pair(
			//generator.first,
			TmpB.CreateLoad(rawcontval.first, "src"),
			TmpB.CreateLoad(rawcontval.second, "srcrtt"));
		genTypeChoiceOps(TmpB,env,resetval,source_type,resetGen,4,true);
	}
	
	bool GeneratorStatement::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		// We need to create a nested loop for every generator
		// Inlining range generators means they need an increment after the loop body
		// regular generators are instantiated, and then called for every value,
		// returning true(?!) when done
		// Or increment before test, with ore-loop skipping test.
		
		// Execute all expressions representing a (container) generator
		// result is called, value extracted unless done.
		
		// Range generator execute the three expressions representing from, to and by parameters
		// initialize variable to from
		// test if not done (compare to to) add by after body.
		
		// Body always includes any nested loops
		// as well as conditions which may skip to 
		// the next value block (same as start for )
		
		// ie, for every generator in the statement:
		// create stack with entry and exit blocks for body to jump to:
		// condition and end of loop jump to exit block for
		
		// 1st: Create variables for each generator in the statement
		intro::Type TBoolean(intro::Type::Boolean);
		llvm::Function *TheFunction = env->currentFunction();
		CodeGenEnvironment local(env);
		for (GenCond op : generators)
		{
			if (op.iscondition) continue;
			std::wstring name=op.generator->getVariableName();
			Type *type=op.generator->getVariableType();
			CodeGenEnvironment::iterator iter=local.createVariable(name);
			TmpB.CreateStore(env->getRTT(type),iter->second.rtt);
		}
		Generator *lastgen=nullptr;
		for (size_t i=0; i!=generators.size(); ++i)
		{
			// generate loop code
			if (generators[i].iscondition && lastgen!=nullptr)
			{
				// Note that grammar does not allow the statement to begin with a condition!
				llvm::BasicBlock *postcondbody = llvm::BasicBlock::Create(theContext, "postcondbody");
				Expression::cgvalue cond=generators[i].condition->codeGen(TmpB,&local);
				// If the condition is violated, jump to the current loop's loop start
				TmpB.CreateCondBr(createUnboxValue(TmpB,env,cond.first,&TBoolean),postcondbody,lastgen->loop);
				TheFunction->getBasicBlockList().push_back(postcondbody);
				TmpB.SetInsertPoint(postcondbody);
			}
			else
			{
				generators[i].generator->codeGen(TmpB,&local);
				lastgen=generators[i].generator;
			}
		}
		// Create the body. If the function returns true, we expect it to end on a reurn statement
		// which is "br leave" in LLVM. Ifthat is the case, we do not need to create it's branch up back...
		bool skipBranch=cgbody(TmpB,&local);	
		// Put in all the branches to the loop blocks and add the exit blocks to the function..
		std::vector<GenCond>::reverse_iterator iter;
		for (iter=generators.rbegin();iter!=generators.rend();++iter)
		{
			if (iter->iscondition) continue;
			if (!skipBranch) 
			{
				TmpB.CreateBr(iter->generator->loop);
				skipBranch=false;
			}
			iter->generator->codeGenExitBlock(TmpB,env);
		}
		return false;
	}

	bool ImportStatement::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		CodeGenModule *base = relative ?
			env->getCurrentModule() : CodeGenModule::getRoot();
		// Type inference should verify that the module imported from already exists.
		CodeGenModule *myself = base->getRelativePath(path);
		myself->declareInterfaceIfNeeded(TheModule.get());
		// Iterate over interface and copy imports to the current environment
		CodeGenEnvironment::iterator eit;
		for (eit = myself->begin();eit != myself->end();eit++)
		{
			//std::wcout << "Found import: " << eit->first << "!\n";
			CodeGenEnvironment::iterator iter = env->importElement(eit->first, eit->second);
		}
		return true;
	}

	/* Module code generation:
		The product of this method is a CodeGenModule, derived class from
		CodeGenEnvironment as it holds runtime values, in this case the
		variables making up the public interface of the module.
		* Private variables can be contained in a sub-CGEnv
		* Or the module is created in an environment of it's own,
		  then the interface contents are copied to result module
		  -> This method interacts better with filling an interface from the runtime lib

		The CodeGenModule is created in-place in the module hierarchy

	*/
	bool ModuleStatement::codeGen(IRBuilder<> &TmpB,CodeGenEnvironment *env)
	{
		CodeGenEnvironment localenv(env);
		// Iterate over the statements comprising the module body and infer their types.
		std::list<Statement*>::iterator stmt;
		bool success = true;
		for (stmt = contents.begin();stmt != contents.end();stmt++)
		{
			success &= (*stmt)->codeGen(TmpB,&localenv);
		}
		if (!success)
		{
			std::wcout << "Error building module ";
			printPath(std::wcout);
			std::wcout << "!\n";
			return false;
		}
		//Environment::pushModule(module);

		// Get/Create target module
		CodeGenModule *base = relative ?
			env->getCurrentModule() : CodeGenModule::getRoot();
		CodeGenModule *myself = base->getRelativePath(path);
		// Iterate over interface and copy exports to the module 
		std::list<ExportDeclaration*>::iterator eit;
		for (eit = exports.begin();eit != exports.end();eit++)
		{
			CodeGenEnvironment::iterator exportvalue = localenv.find((*eit)->getName());
			if (exportvalue == localenv.end())
			{
				if (!(*eit)->isExport())
					continue;
				std::wcout << "Error in Module: failed to export interface member "
					<< (*eit)->getName() << "!\n";
				return false;
			}
			//std::wcout << "Found export: " << (*eit)->getName() << "!\n";
			CodeGenEnvironment::iterator iter = myself->importElement(exportvalue->first, exportvalue->second);
		}
		return true;
	}
}