#ifndef INTRO_H
#define INTRO_H

#include <iostream>
#include "Type.h"
#include "Environment.h"
// LLVM causes compiler warning 4355 in VC++2010...

#ifdef _MSC_VER

__pragma(warning(push))
__pragma(warning(disable:4355))
__pragma(warning(disable:4244))

#endif

#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"

#ifdef _MSC_VER

__pragma(warning(pop))

#endif

/// This namespace contains all parts of the intro programming language (syntax tree and type inference).
namespace intro 
{

class Variable;
class CodeGenEnvironment;
class Function;
/// A Variable set contains, by name, all references to a variable, 
/// hence a multiset (does that even make sense!?).
typedef std::set<std::wstring> VariableSet;

/// The base class for all statements of the Intro language
class Statement
{
	int line, pos;
public:

	/// Statements remember lina and position passed from parser, for error reporting.
	Statement(int l,int p) : line(l), pos(p)
	{};

	/// Detructor does nothing, except being virtual.
	virtual ~Statement()
	{};

	inline int getLine(void) { return line; };
	inline int getPosition(void) { return pos; };

	// Simulate the statemetns effect on the types in the environment.
	virtual bool makeType(Environment *env)=0;
	virtual void print(std::wostream &s)=0;
	virtual bool codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env)=0;

	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)=0;
	virtual void collectFunctions(std::list<intro::Function*> &funcs)=0;
	virtual bool isReturnLike(void) { return false; };
	/// Count the number of variables allocated in the statement
	virtual size_t countVariableStmts(void)=0;
};

/// Base Class for all kinds of Expressions available in Intro
/** Expressions are formally all value definitions. Each expression
	has a type, which is infered unless given. The classes derived from
	Expression are used to construct a syntax tree during parsing.

	A "pretty" printer is also provided,which is mainly used for debuging type inference.
*/
class Expression
{
	// Holds the expression's type.
	Type *type;

	int line,pos;
	ErrorType *error;
protected:
	/// Purely virtual fnction that is used to create an expressions type.
	virtual Type *makeType(Environment *env)=0;
	/// Derived classes use this method to create a new error type with the appropriate message
	inline ErrorType *getError(std::wstring msg)
	{
		if (error!=NULL) delete error; // should not happen
		error= new ErrorType(line,pos,msg);
		error->print(std::wcout);
		return error;
	};

public:
	typedef std::pair<llvm::Value *,llvm::Value *> cgvalue;

	// Initialize base class object members
	Expression(int l,int p) : type(NULL), line(l), pos(p)
	{
		error=NULL;
	};
	/// Get the line where the token associated with this expression was found.
	inline int getLine(void) { return line; };
	/// Get the position on the line where the token associated with this expression was found.
	inline int getPosition(void) { return pos; };
	/// Destroy an expression - deletes an error if created.
	virtual ~Expression()
	{
		if (error!=NULL) delete error;
	};
	/// Front end for type inference virtual function, buffing result.
	inline Type *getType(Environment *env)
	{
		if (type==NULL) 
			type=makeType(env);
		return type->find();
	};
	/// Return the type that was inferred for this expression, as is.
	inline Type *getType(void)
	{
		return type!=NULL?type->find():NULL;
	};

	/// Return the current type that specializes variables to concrete types
	Type *getType(CodeGenEnvironment *env);

	/// (Use after type inference) Expressions may be l-values, in that case they are writable. Otherwise not.
	/** Note that writable expressons need to check the writability of the contained type, which may refer 
		to a constant value.
	*/
	virtual bool isWritable(void)
	{
		return false;
	};
	/// (Use after type inference) If writable, this method returns the type that can be written (e.g. the dictionaries value type).
	virtual Type *getWritableType(void)
	{
		return NULL;
	};

	/// (Use after type inference) Generate the address that can be written to, if this expression is writable else null.
	virtual cgvalue getWriteAddress(llvm::IRBuilder<> &,CodeGenEnvironment *)
	{
		return cgvalue(nullptr,nullptr);
	};

	/// Evaluation returns a value as a void pointer, use getType to determine the exact type of the value.
//	virtual void *evaluate(Environment &env)=0;

	/// Virtual function to print the expression to the given stream.
	virtual void print(std::wostream &s)=0;

	/// Collect the free variables in this expression into the given set.
	//virtual void getFreeVariables(VariableSet &set)=0;
	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)=0;
	
	/// Generate the code implementing the expression's semantics
	virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env)=0;
	/// generate the code to provide the expressions run time type
	//virtual llvm::Value *getRTT(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env)=0;
	/// This is an utility method used during testing.
	virtual void collectFunctions(std::list<intro::Function*> &funcs)=0;
};

/// A Type expression is a representation of a type as an AST.
class TypeExpression
{
	// Holds the expression's type.
	Type *type;

	int line,pos;
	ErrorType *error;
protected:
	/// Purely virtual fnction that is used to create an expressions type.
	virtual Type *makeType(Environment *env)=0;
	/// Derived classes use this method to create a new error type with the appropriate message
	inline ErrorType *getError(std::wstring msg)
	{
		if (error!=NULL) delete error; // should not happen
		error= new ErrorType(line,pos,msg);
		return error;
	};

public:
	// Initialize base class object members
	TypeExpression(int l,int p) : type(NULL), line(l), pos(p)
	{
		error=NULL;
	};
	/// Get the line where the token associated with this expression was found.
	inline int getLine(void) { return line; };
	/// Get the position on the line where the token associated with this expression was found.
	inline int getPosition(void) { return pos; };
	/// Destroy an expression - deletes an error if created.
	virtual ~TypeExpression()
	{
		if (error!=NULL) delete error;
		//if (type!=NULL && type->getKind()!=Type::Variable) delete type;
	};
	/// Front end for type inference virtual function, buffing result.
	inline Type *getType(Environment *env)
	{
		if (type==NULL) 
			type=makeType(env);
		return type;
	};
	/// Front end for type inference virtual function, caching result.
	inline Type *getType(void)
	{
		return type->find();
	};

	/// Virtual function to print the expression to the given stream.
	void print(std::wostream &s)
	{
		if (type==NULL)
		{
			s << "??";
		}
		else
		{
			type->find()->print(s);
		};
	};

	/// This method provides access to the exposed type for a module interface.
	/** Type Expression only occur in a modules interface definition, and when
		exporting this definition to the rest of the environment use this function,
		so that the exposed opaque type is used instead of it's internal implementation.
		IMPORTANT: The returned pointer is owned by the calling function, which must make sure it gets
		deleted (i.e. with deleteCopy).
	*/
	virtual Type *getExposedType(void)=0;
};

/// Function application is a fundatmental operation of programming languages
/** This class servers as the base class for all syntactic forms that resemble 
	function application, e.g. arithmetic and other operators or range generators.

	The type inference is the same in all cases, only the function called differs.
	The type of the function called is constructed in the virtual method getCalledFunctionType().
	This class initializes getCalledFunctionType() to return a copy of a function type from the
	expression representing the function (e.g. an identifier bound to a function value).

	Derived functions override only getCalledFunctionType() to create a custom function type
	representing the operation.
	
	In addition, application is the only (wide) case that must handle polymorphic input,
	as well as coercion. 
*/
class Application : public Expression
{
public:
	/// Type of a callback that generates code for an operation with a specific type (e.g. addition, compare or stringification)
	//typedef llvm::Value *(*codegen_cb)(llvm::IRBuilder<> &builder,std::vector<llvm::Value*>&);
	typedef std::function<llvm::Value*(llvm::IRBuilder<>&,std::vector<llvm::Value*>&)> codegen_cb;
	struct cgcb_elem
	{
		intro::rtt::RTType type; ///< the type of interest
		codegen_cb callback; ///< the code generating callback
	};
protected:

	/// Type of the function called for this context (i.e. may specialize functions actual type).
	FunctionType *myFuncType;
	/// Buffer in case we construct a function type for an incoming type variable (we need to clean up later...)
	FunctionType *funvar;
	/// Function type we loaded from the environment.
	Type *calledType;
	/// If we found a non-variable function type, we copy it to leave the environment intact.
	bool deleteCalledType; // may work without due to having a type variable that is not deleted. But that is cryptic.
	/// The list of expressions that evaluate to the function call's parameters
	std::vector<Expression*> params;
	/// The epression which represents the function to be called.
	Expression *func;
	/// List of types instantiated during type inference, remembered for deletion?!
	std::vector<Type*> sourceTypes;
	/// RTT of the value that the function called has returned, if any.
	llvm::Value *returntype;

	/// Allow expressions derived from application to define their operations function type as they like.
	virtual Type *getCalledFunctionType(Environment *env);

	virtual Type *makeType(Environment *env);
	
	/// struct paires types with callbacks generateing the code for that type

	// / Generate code that depends on type, direct for fixed type, switch statement when more than one type possible
	/* * Might move this into CodeGenEnrvironment, and keep type variable instance management there...

		Here is an example how the callback array can be defined:
			static const cgcb_elem division[]=
			{
				{rtt::Integer,gen_div_int},
				{rtt::Real,gen_div_real},
				{rtt::Top,NULL}
			};

		This was originally in Expression, but only applications should be able to be polymorphic,
		as they interact with poymorphic functiony, while othe rexpressions are just literals.
		Hoperfully, this stays true when we get to the complex literals
		@param callbacks array of cgcb_elem structs that generate code for a type, reminated with {rtt::Top,NULL} element
		@param builder the LLVM IR Builder object currently in use
		@param env the current code generation environment, for rttypes, 
		@param bound the type that represents the upper bound for applicable types required
		@returns the LLVM value representing the result of the operation
	*/
	//llvm::Value *generateTypedCode(const cgcb_elem callbacks[],llvm::IRBuilder<> &builder,CodeGenEnvironment *env,
	//	std::vector<llvm::Value*> &param_values,Type *bound);

	llvm::Value *generateTypedOperator(const cgcb_elem callbacks[],llvm::IRBuilder<> &builder,CodeGenEnvironment *env,Type *bound);

public:

	typedef std::vector<Expression*>::iterator iterator;
	Application(int l,int p) 
		: Expression(l,p)
		, myFuncType(NULL)
		, funvar(NULL)
		, calledType(NULL)
		, deleteCalledType(false)
		, func(NULL)
		, returntype(nullptr)
	{ 
	};

	virtual ~Application(void)
	{
		iterator iter;
		for (iter=params.begin();iter!=params.end();iter++)
			delete *iter;
		if (func!=NULL) delete func;
		if (deleteCalledType) deleteCopy(calledType);
		if (myFuncType!=NULL) delete myFuncType;
		if (funvar!=NULL) delete funvar;
		for (size_t i =0;i<sourceTypes.size();++i)
		{
			deleteCopy(sourceTypes[i]);
		}
	};

	//	virtual void *evaluate(Environment &env)=0;
	inline void appendParam(Expression *p) { params.push_back(p); };
	inline void setFunction(Expression *f) { func=f;};

	virtual void print(std::wostream &s)
	{
		if (func->getType()==NULL) return;
		func->print(s);
		// || func->getType()->getKind()==Type::Error) return;
		s <<"(";
		iterator iter=params.begin();
		if (iter!=params.end())
		{
			(*iter)->print(s);
			iter++;
			while (iter!=params.end())
			{
				s << ",";
				(*iter)->print(s);
				iter++;
			};
		}
		s <<") : ";
		getType()->find()->print(s);
	};
	
	virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	//virtual llvm::Value *getRTT(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);

	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		if (func!=NULL) func->getFreeVariables(free,bound);
		iterator iter=params.begin();
		for (iter=params.begin();iter!=params.end();iter++)
		{
			(*iter)->getFreeVariables(free,bound);
		}
	};

	virtual void collectFunctions(std::list<intro::Function*> &funcs);
};

}

/// This namespace contains the parser generated by COCO/R.
namespace parse {} // Dummy namespace for doxygen

#endif