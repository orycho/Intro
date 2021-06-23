#ifndef CODE_GENERATION_ENVIRONMENT
#define CODE_GENERATION_ENVIRONMENT

#include <map>
#include <unordered_map>
#include <string>
#include "Intro.h"
#include "Type.h"
#include "Statement.h"
#include "RTType.h"

#ifdef WIN32
__pragma(warning(push))
__pragma(warning(disable:4355))
__pragma(warning(disable:4244))
#endif

#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"

#ifdef WIN32
__pragma(warning(pop))
#endif 

namespace intro
{

// We need this from CodeGen.cxx to assign types to variables
llvm::Type *toTypeLLVM(intro::Type::pointer_t type);

class CodeGenModule;

class CodeGenEnvironment
{
public:
	/// The various types of scopes determine where and how variables are allocated.
	/// Make local and function scopes same, 
	enum ScopeType
	{
		GlobalScope,	///< A global scope, values LLVM global constants (are only dereferenced on termination).
		Closure,		///< Contains parameters (not derefenced, owned by caller) and closure values (dereferenced in closure release)
		LocalScope,		///< Variables created with alloca in a block scope, all released when scope ends.
		Generator		///< Scopes with this type, or enclosed by this type, create their variables in the closure and calculate addresses there.
	};
	/// Information relating to a variable name in a scope during code gen.
	struct element
	{
		
		llvm::Value *address;	///< The current address of the variable
		llvm::Value *rtt;		///< runtime type associated with the variable
		bool isParameter;		///< Indicates the variable is not allocated on stack, but a parameter.
		std::string altname;	///< Alternate name to use (if set), for run time library use
		/// NULL default type sensible??
		element(llvm::Value *address_,llvm::Value *rtt_,bool isParam=false)
			: address(address_)
			, rtt(rtt_)
			, isParameter(isParam)
		{
		}

		element(const element &other)
			: address(other.address)
			, rtt(other.rtt)
			, isParameter(other.isParameter)
		{
		}

		Expression::cgvalue load(llvm::IRBuilder<> &builder)
		{
			if (isParameter)
			{
				return std::make_pair(address, rtt);
			}
			return std::make_pair(
				builder.CreateLoad(address, "variable"), 
				builder.CreateLoad(rtt, "variable!rtt"));
		}

		void store(llvm::IRBuilder<> &builder, const Expression::cgvalue &value)
		{
			builder.CreateStore(value.first, address);
			builder.CreateStore(value.second, rtt);
		}

		void store(llvm::IRBuilder<> &builder, llvm::Value *value, llvm::Value *value_rtt)
		{
			builder.CreateStore(value, address);
			builder.CreateStore(value_rtt, rtt);
		}
	};
	typedef std::unordered_map<std::wstring,element>::iterator iterator;
	
private:

	CodeGenEnvironment *parent;
	/// The current function or generator iterator being built.
	llvm::Function *function;
	/// Provide return statements the block to jump to for cleanup?!
	llvm::BasicBlock *leave;
	/// Counts number of fields in closure already used, incresed by variable allocation
	size_t closuresize;
	// Scope type, important are mostly Global, CLosure and Generator. All else is vanlla Local
	ScopeType scope_type;
	/// Variables declared in this scope, also with parameters.
	std::unordered_map<std::wstring,element> elements;
	/// Variables in generators are aliased into locals from the 
	std::vector<std::pair<std::wstring, size_t> > generatorVarMap;
	/// State based dispatch to entry points for yield statements
	llvm::SwitchInst *generatorStateDispatch;
	/// Generators get their variables allocated from the closure
	llvm::Value *closure;
	/// Used by boxing/unboxing helpers to get doubles from an i8*. Created in every non-localScope and hopefully not used much after optimizations
	llvm::AllocaInst *double_buffer;
	/// The name of the variable defined in the child environment
	/** This is used by the variable statement to communicate the name of recursive functions
		to the function itself.
	*/
	std::wstring outerValueName;
	/// Basic blocks provided by loops to break/continue.
	llvm::BasicBlock *onContinue, *onBreak;

	/// Return the Environment that represents a function, if any
	inline CodeGenEnvironment *getWrappingEnvironment()
	{
		CodeGenEnvironment *env=this;
		while (env!=NULL && env->function==NULL) env=env->parent;
		return env;
	}
	
	llvm::AllocaInst *createEntryBlockAlloca(const std::wstring &VarName);
	iterator createGeneratorVariable(const std::wstring &VarName);

	CodeGenEnvironment *getLoopEnv(void)
	{
		if (onBreak != nullptr) return this;
		assert(parent != nullptr);
		return parent->getLoopEnv();
	}
	/// Closure vars are special, as the address of value and it's rtt come from function parameters.
	/// The are dereferenced when the closure itself is released.
	void setClosure(llvm::IRBuilder<> &builder,llvm::Value *closure, llvm::StructType *myclosure_t,const VariableSet &freeVars);
	/// Adds a special variable that will be returned as the function value, 
	// with special rtt holding the address passed (can be written unlike usual rtts)
	void createReturnVariable(llvm::Value *retValType);

	/// Set of intermediates, mapping the address of the value to the rtt of the value
	/**
		- not intermidates: values beonging to value types
		Intermediates are
		- Expressions creating new reference types (string, list, record, etc., splice, fun)
		- Results of function applications, unless known value types.
		- But is that really so? What about the identity function, which does not change intermediateness
		  (intermediate input is intermediate output, e.g. 'id("Hello");'
		  Idea: return statement also performs removeIntermediateOrIncrement, then the
		  passed value is returned with a count of 2 as it is not intermediate in the function.
		  Then, we can safely decrement the value passed and the value returned (if it is still intermediate).
		
		values stop being intermediate once they are assigned to a variable, label or are placed in a container.
		When known in advance that a value will stpo being intermediate (e.g. list "constant" expression)
		then the value can be not made intermediate.
		
		Whenever a CGEnv is about to go out of scope, the cleanup function (which generates calls
		to the runtime decrement function) should be called. It needs an up-to-date IRBuilder object,
		so is tricky to do in the destructor. Furthermore, some statements, like if-then-else,
		may have several sub-expressions that are independent of each other, and which are not usable 
		by the blocks inside the statement. So the if statement may cleanup intermediates several times.
	*/
	std::unordered_map<llvm::Value*,llvm::Value*> intermediates;
	typedef std::unordered_map<llvm::Value*,llvm::Value*>::iterator im_iter;

	const static wchar_t *retvarname;
public:

	CodeGenEnvironment(CodeGenEnvironment *parent_,ScopeType st=LocalScope) 
		: parent(parent_)
		, function(nullptr)
		, leave(nullptr)
		, closuresize(0)
		, scope_type(st)
		, generatorStateDispatch(nullptr)
		, closure(nullptr)
		, double_buffer(nullptr)
		, onContinue(nullptr)
		, onBreak(nullptr)
	{
	}

	virtual ~CodeGenEnvironment()
	{}

	virtual CodeGenModule *getCurrentModule(void)
	{
		if (parent == nullptr) return nullptr;
		return parent->getCurrentModule();
	}
	
	void addIntermediate(llvm::Value *address,llvm::Value *rtt)
	{
		intermediates.insert(std::make_pair(address,rtt));
	}
	
	bool removeIntermediateOrIncrement(llvm::IRBuilder<> &builder, Type::pointer_t type,llvm::Value *address,llvm::Value *rtt);
	inline bool removeIntermediateOrIncrement(llvm::IRBuilder<> &builder, Type::pointer_t type, Expression::cgvalue &value)
	{
		return removeIntermediateOrIncrement(builder, type, value.first, value.second);
	}
	void decrementIntermediates(llvm::IRBuilder<> &builder, bool clear=true);
	void decrementValue(llvm::IRBuilder<> &builder, llvm::Value *value, llvm::Value *rtt);

	/// Used by interpreter to add elements of the global scope to mdoules for each line.
	void addExternalsForGlobals(void);
	void closeScope(llvm::IRBuilder<> &builder);
	void closeAllScopes(llvm::IRBuilder<> &builder);
	
	/// 
	//inline void setParent(CodeGenEnvironment *p) { parent=p; }

	inline ScopeType getScopeType(void) 
	{ 
		CodeGenEnvironment *env=getWrappingEnvironment();
		if (env==nullptr) 
			return scope_type; // either global or local, but not in any function or generator
			//return GlobalScope; // either global or local, but not in any function or generator
		return env->scope_type;
	}

	void makeLoopScope(llvm::BasicBlock *onContinue_, llvm::BasicBlock  *onBreak_)
	{
		onContinue = onContinue_;
		onBreak = onBreak_;
	}

	inline llvm::BasicBlock *getContinueBlock(void) 
	{
		CodeGenEnvironment *env = getLoopEnv();
		return env->onContinue;
	}

	inline llvm::BasicBlock *getBreakBlock(void)
	{
		CodeGenEnvironment *env = getLoopEnv();
		return env->onBreak;
	}
	/// Checks if this is not in a function
	inline bool isGlobal(void) { return getScopeType()==GlobalScope; }
	/// Returns true is this specific scope is local
	inline bool isLocal(void) { return scope_type == LocalScope; }
	
	inline CodeGenEnvironment *getParent(void) { return parent; }

	inline void setOuterValueName(const std::wstring &name)
	{
		outerValueName = name;
	}

	inline const std::wstring &getOuterValueName()
	{
		return outerValueName;
	}

	inline bool isCurrentClosure(llvm::Value *val)
	{
		CodeGenEnvironment *env = getWrappingEnvironment();
		if (env == nullptr) return false;
		return env->closure != nullptr && env->closure == val;
	}

	/// Set the function wrapping the anonmyous root statement, global scope only!
	void setAnonFunction(llvm::Function *func)
	{
		assert(scope_type==GlobalScope);
		function=func;
		double_buffer=createEntryBlockAlloca(L"$double_bufferA");
	}
	
	llvm::AllocaInst *getDoubleBuffer()
	{
		CodeGenEnvironment *env=getWrappingEnvironment();
		assert(env!=nullptr);
		return env->double_buffer;
	}

	///@name Function and Generator codegen helpers
	///@{

	inline size_t getClosureSize(void) { return closuresize; }

	inline llvm::Function *currentFunction(void) 
	{
		CodeGenEnvironment *env=getWrappingEnvironment();
		if (env==nullptr) return nullptr;
		else return env->function;
	}
	
	inline llvm::BasicBlock *getExitBlock(void) 
	{ 
		CodeGenEnvironment *env=getWrappingEnvironment();
		if (env==nullptr) return nullptr;
		return env->leave; 
	}
	///@}
	
	///@name Generator codegen helpers
	///@{
	void setGenerator(llvm::IRBuilder<> &builder,llvm::Function *TheGenerator,
		const VariableSet &free);
	
	inline llvm::SwitchInst *getStateDispatch(void) 
	{ 
		CodeGenEnvironment *env=getWrappingEnvironment();
		if (env==nullptr) return nullptr;
		return env->generatorStateDispatch; 
	}
	
	inline bool isInGenerator(void) 
	{ 
		CodeGenEnvironment *env=getWrappingEnvironment();
		if (env==nullptr) return false;
		return env->scope_type==Generator; 
	}
	
	llvm::Value *getGeneratorClosure(llvm::IRBuilder<> &builder);

	void storeGeneratorValues(llvm::IRBuilder<> &builder);

	///@}
	
	inline iterator begin() { return elements.begin(); }
	
	inline iterator end() { return elements.end(); }

	/// The find function also searches parents
	inline iterator find(const std::wstring &name)
	{
		iterator iter=elements.find(name);
		if (iter!=end()) return iter;
		if (parent==NULL) return elements.end();
		return parent->find(name);
	}
	
	//inline iterator getReturnVariable() 
	inline element *getReturnVariable()
	{ 
		CodeGenEnvironment *env = getWrappingEnvironment();
		if (env == nullptr) return nullptr;
		auto iter=env->find(retvarname);
		if (iter == env->end()) return nullptr;
		return &(iter->second);
	}

	/// Insert named addresses from outside of scope,usually function parameters
	inline void addParameter(llvm::IRBuilder<> &builder,const std::wstring &name,llvm::Value *value,llvm::Value *rtt)
	{
		elements.insert(std::make_pair(name,element(value,rtt,true)));
	}

	/// Get a types runtime encoding
	llvm::Value *getRTT(rtt::RTType rtt);
	llvm::Value *getRTT(Type::pointer_t type);

	/// Create a new variable in the current scope (global or local)
	iterator createVariable(const std::wstring &name);

	iterator createVariable(void);
	/// Import a variable that resides in an module
	iterator importElement(const std::wstring &name, const element &elem)
	{
		//return elements.insert(std::make_pair(name, element(elem.address, elem.rtt))).first;
		return elements.insert(std::make_pair(name, elem)).first;
	};
	/// Create local variable holding a function call's return type
	llvm::AllocaInst *createRetValRttDest(void);
	
	void setFunction(llvm::IRBuilder<> &builder,llvm::Function *TheFunction,
		Function::ParameterList &params,bool returnsValue, const VariableSet &free);
		
	/// This function generates the finalization code of a block scope
	/** It's main responsibility is to decrement the reference counters of those
		values that have types that are handled as references.
	*/
	llvm::Value *genExitCode()
	{
		return nullptr;
	}
};

}

#endif