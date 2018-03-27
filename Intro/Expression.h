#ifndef EXPRESSION_H
#define EXPRESSION_H

#include <list>
#include <string>
#include <ios>
#include <memory>
#include <iostream>
//#include "Intro.h"
#include "Generators.h"
#include <regex>

namespace intro
{

/// An Integer constant, represented using a 64 Bit integer.
/**	Every type of constant has a node representing it. The node also stores
	a representation of the constant value.
*/
class IntegerConstant : public Expression
{
	int64_t value;
	Type *myType;
protected:
	virtual Type *makeType(Environment *, ErrorLocation *);
public:
	IntegerConstant(int l,int p,wchar_t *token,int base=10) : Expression(l,p)
	{
		myType=new Type(Type::Integer);
#ifdef WIN32
		value=_wtoi64(token);
#else
		wchar_t *end;
		value=wcstoll(token,&end,base);
#endif
	};
	virtual ~IntegerConstant()
	{
		delete myType;
	};
	virtual void print(std::wostream &s);
	virtual void getFreeVariables(VariableSet &,VariableSet &)
	{};

	virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	//virtual llvm::Value *getRTT(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs) { /*NOP*/ };
};

/// A Boolean constant.
/**	Every type of constant has a node representing it. The node also stores
	a representation of the constant value.
*/
class BooleanConstant : public Expression
{
	bool value;
	Type *myType;
protected:
	virtual Type *makeType(Environment *, ErrorLocation *);
public:
	BooleanConstant(int l,int p,bool val) : Expression(l,p),value(val)
	{
		myType=new Type(Type::Boolean);
	};
	~BooleanConstant()
	{
		delete myType;
	};
	virtual void print(std::wostream &s);
	virtual void getFreeVariables(VariableSet &,VariableSet &)
	{};
	virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	//virtual llvm::Value *getRTT(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs){ /*NOP*/ };
};

/// A real constant, represented using a double.
/**	Every type of constant has a node representing it. The node also stores
	a representation of the constant value.
*/
class RealConstant : public Expression
{
	double value;
	Type *myType;
protected:
	virtual Type *makeType(Environment *, ErrorLocation *);
public:
	RealConstant(int l,int p,wchar_t *token) : Expression(l,p)
	{
		wchar_t *ptr; 
		value=wcstod(token,&ptr); 
		myType=new Type(Type::Real);
	};
	~RealConstant()
	{
		delete myType;
	};
	virtual void print(std::wostream &s);
	virtual void getFreeVariables(VariableSet &,VariableSet &)
	{};
	virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	//virtual llvm::Value *getRTT(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs){ /*NOP*/ };
};

/// A string constant, represented using std::wstring (16 bit).
/**	Every type of constant has a node representing it. The node also stores
	a representation of the constant value.
*/
class StringConstant : public Expression
{
	std::wstring value;
	Type *myType;

	/// A stringpart is either a constant string literal, or a variable interpolation (which may be another string).
	struct part
	{
		part(const std::wstring &s_,intro::Type *t_=NULL)
		{ s=s_; t=t_; };
		/// the constant string (if t==NULL) or a variable name if t != NULL (the variable's type).
		std::wstring s;
		/// The type of the variable, or NULL if a constant string
		intro::Type *t;
	};
	/// The parts from which the string is interpolated.
	std::list<part> parts;
	typedef std::list<part>::iterator iterator;
protected:
	virtual Type *makeType(Environment *, ErrorLocation *);

	Type *original;
public:
	StringConstant(int l,int p,wchar_t *token) : Expression(l,p)
	{
		value.assign(token);
		myType=new Type(Type::String);
		original = myType;
	};
	~StringConstant()
	{
		for (iterator i=parts.begin();i!=parts.end();i++)
			if (i->t!=NULL) deleteCopy(i->t);
		delete myType;
	};
	virtual void print(std::wostream &s);
	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		for (iterator i=parts.begin();i!=parts.end();i++)
			if (i->t!=NULL)
				if (bound.find(i->s)==bound.end()) free.insert(i->s);
	};
	virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	//virtual llvm::Value *getRTT(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs){ /*NOP*/ };
};

/// List constants are either a literal list of the elements initialized to, or a generator based expression
class ListConstant : public Expression
{
	/// The elements, in case of generators there must be one entry here
	std::list<intro::Expression*> elements; 
	GeneratorStatement *generators;
	Type *myType;
protected:
	virtual Type *makeType(Environment *env, ErrorLocation *errors);
public:
	ListConstant(int l,int p,std::list<intro::Expression*> contents) : Expression(l,p),elements(contents), generators(NULL)
	{
	};
	~ListConstant()
	{
		delete myType;
		if (generators!=NULL) delete generators;
		std::list<intro::Expression*>::iterator iter;
		for (iter=elements.begin();iter!=elements.end();iter++)
			delete *iter;
	};
	inline void setGenerators(GeneratorStatement *gen) { generators=gen; };
	/// Virtual function to print the expression to the given stream.
	virtual void print(std::wostream &s);
	/// Collect the free variables in this expression into the given set.
	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		if (generators!=nullptr) generators->getFreeVariables(free,bound);
		std::list<intro::Expression*>::iterator iter;
		for (iter=elements.begin();iter!=elements.end();iter++)
			(*iter)->getFreeVariables(free,bound);
	};
	virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	//virtual llvm::Value *getRTT(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs);
};

/// Dictionary constants are either a literal list of the key-vaue pairs initialized to, or a generator based key-value expression.
class DictionaryConstant : public Expression
{
public:
	typedef std::list<std::pair<intro::Expression*,intro::Expression*> > memberlist;
private:
	/// The elements, in case of generators there must be one entry here
	memberlist elements; 
	GeneratorStatement *generators;
	Type *myType;
protected:
	/// Purely virtual function that is used to create an expressions type.
	virtual Type *makeType(Environment *env, ErrorLocation *errors);
public:
	DictionaryConstant (int l,int p,memberlist contents) : Expression(l,p),elements(contents), generators(NULL), myType(NULL)
	{
	};
	~DictionaryConstant()
	{
		delete myType;
		if (generators!=NULL) delete generators;
		memberlist::iterator iter;
		for (iter=elements.begin();iter!=elements.end();iter++)
		{
			delete iter->first;
			delete iter->second;
		}
	};
	inline void setGenerators(GeneratorStatement *gen) { generators=gen; };
	/// Virtual function to print the expression to the given stream.
	virtual void print(std::wostream &s);
	/// Collect the free variables in this expression into the given set.
	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		if (generators!=NULL) generators->getFreeVariables(free,bound);;
		memberlist::iterator iter;
		for (iter=elements.begin();iter!=elements.end();iter++)
		{
			iter->first->getFreeVariables(free,bound);
			iter->second->getFreeVariables(free,bound);
		}
	};

	virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	//virtual llvm::Value *getRTT(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs);
};

/// A record expression defines record values. 
/**	Since variants are tagged records, it also does a lot of the grunt work for the variant class.
	The variant class just makes sure space for a tag is created then fills the tag. 
*/
class RecordExpression : public Expression
{
protected:
	/// The members of the record are label-expression pairs.
	std::list<std::pair<std::wstring,Expression*> > members;
	RecordType *myType;
	virtual Type *makeType(Environment *env, ErrorLocation *errors);
	bool isVariant;
public:

	typedef std::list<std::pair<std::wstring,Expression*> >::iterator iterator;

	RecordExpression(int l,int p) : Expression(l,p), myType(nullptr), isVariant(false)
	{};

	~RecordExpression()
	{
		delete myType;
		std::list<std::pair<std::wstring,Expression*> >::iterator iter;
		for (iter=members.begin();iter!=members.end();iter++)
			delete iter->second;
	};
	inline void addMember(std::wstring &label,Expression *expr)
	{
		members.push_back(make_pair(label,expr));
	};

	void setToVariant() { isVariant=true; };

	inline iterator begin() {return members.begin(); };
	inline iterator end() {return members.end(); };

	virtual void print(std::wostream &s);
	virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	//virtual llvm::Value *getRTT(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);

	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		std::list<std::pair<std::wstring,Expression*> >::iterator iter;
		for (iter=members.begin();iter!=members.end();iter++)
			iter->second->getFreeVariables(free,bound);
	};
	virtual void collectFunctions(std::list<intro::Function*> &funcs);
};

/// A Variant expression defines values of one variant type.
/**	The actual handling shares a lot of tasks with the record contained in the variant.
*/
class VariantExpression : public Expression
{
	
protected:
	/// The members of the record are label-expression pairs.
	//std::list<std::pair<std::wstring,RecordExpression*> > members;
	std::wstring tag;
	RecordExpression *record;
	VariantType *myType;
	virtual Type *makeType(Environment *env, ErrorLocation *errors);
public:


	VariantExpression(int l,int p) : Expression(l,p), record(nullptr), myType(nullptr)
	{};
	~VariantExpression()
	{
		delete myType;
		delete record;
	};
	inline void setVariant(const std::wstring &tag,RecordExpression *expr)
	{
		this->tag=tag;
		record=expr;
		record->setToVariant();
	};
	virtual void print(std::wostream &s);
	virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	//virtual llvm::Value *getRTT(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);

	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		record->getFreeVariables(free,bound);
	};
	virtual void collectFunctions(std::list<intro::Function*> &funcs);
};

/// A Variable access, by identifier used.
/**	Every type of constant has a node representing it. The node also stores
	a representation o the constant value.
*/
class Variable : public Expression
{
public:
	enum Modifier
	{
		Unrestricted=0,
		ReadOnly = 1,
		WriteOnly = 2
	};
private:
	std::list<std::wstring> path;
	std::wstring name;
	int modifiers;
	bool relative;
	//Type *original; ///< Type loaded from env can differ from type after unification (subtyping), necessitating coercion

protected:
	virtual Type *makeType(Environment *env, ErrorLocation *errors);
public:
	Variable(int l,int p,bool rel,std::wstring name_,const std::list<std::wstring> &path_,int mod=0) 
		: Expression(l,p)
		, path(path_)
		, name(name_)
		, modifiers(mod)
		, relative(rel)
		//, original(nullptr)
	{
	};

	~Variable()
	{
		//if (original != nullptr) deleteCopy(original);
	}
	const std::wstring &getName(void) { return name; };
	virtual void print(std::wostream &s);
	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		// If it's in some module or the global scope, then we take it from there.
		if (relative&&path.empty())
		{
			if (bound.find(name) == bound.end()) free.insert(name);
		}
	};

	virtual bool isWritable(void)
	{
		return getType()->isWritable();
	};
	/// (Use after type inference) If writable, this method returns the type that can be written (e.g. the dictionaries value type).
	virtual Type *getWritableType(void)
	{
		return getType();
	};

	/// (Use after type inference) Generate the address that can be written to, if this expression is writable;
	virtual Expression::cgvalue getWriteAddress(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);

	virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	//virtual llvm::Value *getRTT(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs){ /*NOP*/ };
};

/// A record element access expression: givena record and a label, extract the approriate element.
/**	Every type of constant has a node representing it. The node also stores
	a representation of the constant value.
*/
class RecordAccess: public Expression
{
	Expression *record; ///< The record to be accessed
	std::wstring label;	///< The label that is requested
	RecordType *myRecord;
protected:
	virtual Type *makeType(Environment *env, ErrorLocation *errors);
public:
	RecordAccess(int l,int p,Expression *rec,std::wstring &lab) 
		: Expression(l,p)
		, record(rec)
		, label(lab)
		, myRecord(nullptr)
	{ };
	virtual ~RecordAccess()
	{
		delete record;
		if (myRecord != nullptr) delete myRecord;
	};

	inline const std::wstring &getLabel() { return label; };
	virtual void print(std::wostream &s);
	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		record->getFreeVariables(free,bound);
	};

	virtual bool isWritable(void)
	{
		return getType()->isWritable();
	};
	/// (Use after type inference) If writable, this method returns the type that can be written (e.g. the dictionaries value type).
	virtual Type *getWritableType(void)
	{
		RecordType *rt=(RecordType*)record->getType();
		return rt->findMember(label)->second;
	};

	/// (Use after type inference) Generate the address that can be written to, if this expression is writable;
	virtual Expression::cgvalue getWriteAddress(llvm::IRBuilder<> &,CodeGenEnvironment *);

	virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	//virtual llvm::Value *getRTT(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs);
};

/// Extraction represetns the [] operator for dictionaries
class Extraction: public Application
{
	FunctionType* myType;
	Type* dict;
	RecordType none,some;
	VariantType maybe;

protected:
	virtual Type *getCalledFunctionType(Environment *env, ErrorLocation *errors);
	virtual std::wstring getOperationDescription(void)
	{
		return L"dictionary lookup operation";
	};
public:
	Extraction(int l,int p,Expression *source,Expression *key) 
		: Application(l,p)
		, myType(NULL)
		, dict(NULL)
	{
		params.push_back(source);
		params.push_back(key);
	};
	virtual ~Extraction()
	{
		if (myType!=NULL) delete myType;
		if (dict!=NULL) delete dict;
	};
	virtual void print(std::wostream &s);
	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		for (iterator iter=params.begin();iter!=params.end();++iter)
			(*iter)->getFreeVariables(free,bound);
	};

	virtual bool isWritable(void)
	{
		Type *t=params.front()->getType();
		if (t->getKind()!=Type::Dictionary) return false;
		return t->isWritable();
	};
	/// (Use after type inference) If writable, this method returns the type that can be written (e.g. the dictionaries value type).
	virtual Type *getWritableType(void)
	{
		Type *t=params.front()->getType();
		if (t->getKind()!=Type::Dictionary) return NULL;
		return t->getParameter(1);
	};

	/// (Use after type inference) Generate the address that can be written to, if this expression is writable;
	virtual cgvalue getWriteAddress(llvm::IRBuilder<> &,CodeGenEnvironment *);
	virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs);
};

/// Dictionary Erase represeted b '\'
class DictionaryErase: public Application
{
	FunctionType* myType;
	Type* dict;
	RecordType none,some;
	VariantType maybe;

protected:
	virtual Type *getCalledFunctionType(Environment *env, ErrorLocation *errors);
	virtual std::wstring getOperationDescription(void)
	{
		return L"erase from dictionary operation";
	};
public:
	DictionaryErase(int l,int p,Expression *source,Expression *key) 
		: Application(l,p)
		, myType(NULL)
		, dict(NULL)
	{
		params.push_back(source);
		params.push_back(key);
	};
	virtual ~DictionaryErase()
	{
		if (myType!=NULL) delete myType;
		if (dict!=NULL) delete dict;
	};
	virtual void print(std::wostream &s);
	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		for (iterator iter=params.begin();iter!=params.end();++iter)
			(*iter)->getFreeVariables(free,bound);
	};

	virtual bool isWritable(void)
	{
		Type *t=params.front()->getType();
		if (t->getKind()!=Type::Dictionary) return false;
		return t->isWritable();
	};
	/// (Use after type inference) If writable, this method returns the type that can be written (e.g. the dictionaries value type).
	virtual Type *getWritableType(void)
	{
		Type *t=params.front()->getType();
		if (t->getKind()!=Type::Dictionary) return NULL;
		return t->getParameter(1);
	};

	/// (Use after type inference) Generate the address that can be written to, if this expression is writable;
	//virtual cgvalue getWriteAddress(llvm::IRBuilder<> &,CodeGenEnvironment *);
	virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs);
};

/// Splice a list or string, ternary []- operator (sequence, first, last)
class Splice : public Application
{
	FunctionType* myType;
	Type counter;
	Type *myTypeParam[2];
	Type *sequence;

protected:
	virtual Type *getCalledFunctionType(Environment *env, ErrorLocation *errors);
	virtual Type *makeType(Environment *env, ErrorLocation *errors);
	virtual std::wstring getOperationDescription(void)
	{
		return L"splice operator";
	};
public:
	Splice(int l,int p,Expression *source,Expression *start,Expression *end=NULL) 
		: Application(l,p)
		, myType(NULL)
		, counter(Type::Integer)
		, sequence(nullptr)
	{
		myTypeParam[0]=myTypeParam[1]=NULL;
		params.push_back(source);
		params.push_back(start);
		if (end!=NULL) params.push_back(end);
	};
	virtual ~Splice()
	{
		if (myType!= nullptr) delete myType;
		if (myTypeParam[0]!= nullptr) delete myTypeParam[0];
		if (myTypeParam[1]!= nullptr) delete myTypeParam[1];
		if (sequence != nullptr) delete sequence;
	};
	virtual void print(std::wostream &s);
	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		for (iterator iter=params.begin();iter!=params.end();++iter)
			(*iter)->getFreeVariables(free,bound);
	};

	virtual bool isWritable(void)
	{
		Type *t=params.front()->getType();
		return t->isWritable();
	};
	/// (Use after type inference) If writable, this method returns the type that can be written (e.g. the dictionaries value type).
	virtual Type *getWritableType(void)
	{
		Type *t=params.front()->getType();
		return t->getParameter(1);
	};

	/// (Use after type inference) Generate the address that can be written to, if this expression is writable;
	virtual Expression::cgvalue getWriteAddress(llvm::IRBuilder<> &,CodeGenEnvironment *)
	{
		return Expression::cgvalue(nullptr,nullptr);
	};

	virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	//virtual llvm::Value *getRTT(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs);
};

/// Assignment a new value to a variable.
class Assignment : public Expression
{
	Type *myType;
protected:
	virtual Type *makeType(Environment *env, ErrorLocation *errors);
	/// The list of expressions that evaluate to the function call's parameters
	Expression *destination;
	/// The epression which must evaluate to a vaue of the correct function type
	Expression *value;
public:
	Assignment(int l,int p,Expression *dest,Expression *val) 
		: Expression(l,p)
		, myType(NULL)
		, destination(dest)
		, value(val)
	{ };
	~Assignment(void)
	{
		delete destination;
		delete value;
		if (myType!=NULL) delete myType;
	};
	virtual void print(std::wostream &s);
	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		destination->getFreeVariables(free,bound);
		value->getFreeVariables(free,bound);
	};
	virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	//virtual llvm::Value *getRTT(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs);
};

/// This is the parent class for all binary operations, which are applictions of functions with two parameters.
class BinaryOperation : public Application
{
protected:
	//Expression *operand1,*operand2;
	BinaryOperation(int l,int p,Expression *op1,Expression *op2) : Application(l,p)
	{
		params.push_back(op1);
		params.push_back(op2);
	};
	virtual ~BinaryOperation()
	{
	};
};

/// Unary operations are a kind of function application
class UnaryOperation : public Application
{
public:
	enum OpType
	{
		Not,
		Negate
	};
protected:
	OpType op;
	//Type* valtype;
	Type boolean,number;
	FunctionType* myType;
	virtual Type *getCalledFunctionType(Environment *, ErrorLocation *);
	virtual std::wstring getOperationDescription(void)
	{
		if (op == Not) return L"not operator";
		else return L"negate operator";
	};
public:
	UnaryOperation(int l,int p, OpType type,Expression *operand) 
		: Application(l,p)
		, op(type)
		, boolean(Type::Boolean)
		, number(Type::Number)
		, myType(NULL)
	{
		params.push_back(operand);
	};
	virtual ~UnaryOperation(void)
	{
		//if (valtype!=NULL) delete valtype;
		if (myType!=NULL) delete myType;
	};
	inline OpType getOperation(void) { return op; };
	virtual void print(std::wostream &s);
	virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	//virtual llvm::Value *getRTT(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
};

/// Boolean binary operations take two booleans and return a boolean.
class BooleanBinary : public BinaryOperation
{
public:
	enum OpType
	{
		And,
		Or,
		Xor
	};
protected:
	OpType op;
	FunctionType *myType;
	Type boolean;
	virtual Type *getCalledFunctionType(Environment *, ErrorLocation *);
	virtual std::wstring getOperationDescription(void)
	{
		switch (op)
		{
		case And: return L"not operator";
		case Or: return L"or operator";
		case Xor: return L"xor operator";
		}
		return L"";
	};
public:
	BooleanBinary(int l,int p,OpType optype,Expression *op1,Expression *op2) 
		: BinaryOperation(l,p,op1,op2)
		, op(optype)
		, myType(NULL)
		, boolean(Type::Boolean)
	{};

	virtual ~BooleanBinary()
	{
		if (myType!=NULL) delete myType; 
	};
	inline OpType getOperation(void) { return op; };
	virtual void print(std::wostream &s);
	virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	//virtual llvm::Value *getRTT(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
};

/// Comparisons are polymorphic as (A,A)->Boolean
class CompareOperation : public BinaryOperation
{
public:
	enum OpType
	{
		Equal,
		Different,
		Greater,
		GreaterEqual,
		Less,
		LessEqual
	};
protected:
	OpType op;
	FunctionType *myType;
	Type *myTypeParam;
	Type comparable;
	virtual Type *getCalledFunctionType(Environment *, ErrorLocation *);
	virtual std::wstring getOperationDescription(void)
	{
		switch (op)
		{
		case Equal: return L"equal comparison";
		case Different: return L"unequal comparison";
		case Greater: return L"greater comparison";
		case GreaterEqual: return L"greater-or-equal comparison";
		case Less: return L"less comparison";
		case LessEqual: return L"less-or-equal comparison";
		}
		return L"";
	};

public:
	CompareOperation(int l,int p,OpType optype,Expression *op1,Expression *op2) 
		: BinaryOperation(l,p,op1,op2)
		, op(optype)
		, myType(NULL)
		, myTypeParam(NULL)
		, comparable(Type::Comparable)
	{};

	virtual ~CompareOperation(void)
	{
		if (myType!=NULL) delete myType; 
		if (myTypeParam!=NULL) delete myTypeParam;
	};
	inline OpType getOperation(void) { return op; };
	virtual void print(std::wostream &s);
	virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	//virtual llvm::Value *getRTT(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
};

/// Arithmetic is (Integer,Integer)->Integer or (Real,Real)->Real. With mixed parameters, Integer is coerced to Real.
class ArithmeticBinary : public BinaryOperation
{
	/// @name Real artihmetic codegen callbacks
	static llvm::Value *gen_add_real(llvm::IRBuilder<> &builder,std::vector<llvm::Value*> &args)
	{ return builder.CreateFAdd(args[0],args[1],"addtmp"); };
	static llvm::Value *gen_sub_real(llvm::IRBuilder<> &builder,std::vector<llvm::Value*> &args)
	{ return builder.CreateFSub(args[0],args[1],"subtmp"); };
	static llvm::Value *gen_mul_real(llvm::IRBuilder<> &builder,std::vector<llvm::Value*> &args)
	{ return builder.CreateFMul(args[0],args[1],"multmp"); };
	static llvm::Value *gen_div_real(llvm::IRBuilder<> &builder,std::vector<llvm::Value*> &args)
	{ return builder.CreateFDiv(args[0],args[1],"divtmp"); };
	static llvm::Value *gen_rem_real(llvm::IRBuilder<> &builder,std::vector<llvm::Value*> &args)
	{ return builder.CreateFRem(args[0],args[1],"modtmp"); };
	/// @name Integer artihmetic codegen callbacks
	static llvm::Value *gen_add_int(llvm::IRBuilder<> &builder,std::vector<llvm::Value*> &args)
	{ return builder.CreateAdd(args[0],args[1],"addtmp"); };
	static llvm::Value *gen_sub_int(llvm::IRBuilder<> &builder,std::vector<llvm::Value*> &args)
	{ return builder.CreateSub(args[0],args[1],"subtmp"); };
	static llvm::Value *gen_mul_int(llvm::IRBuilder<> &builder,std::vector<llvm::Value*> &args)
	{ return builder.CreateMul(args[0],args[1],"multmp"); };
	static llvm::Value *gen_div_int(llvm::IRBuilder<> &builder,std::vector<llvm::Value*> &args)
	{ return builder.CreateSDiv(args[0],args[1],"divtmp"); };
	static llvm::Value *gen_rem_int(llvm::IRBuilder<> &builder,std::vector<llvm::Value*> &args)
	{ return builder.CreateSRem(args[0],args[1],"modtmp"); };

public:
	enum OpType
	{
		Add,
		Sub,
		Mul,
		Div,
		Mod
	};
protected:
	OpType op;
	FunctionType *myType;
	Type *myTypeParam;
	Type number;
	virtual Type *getCalledFunctionType(Environment *, ErrorLocation *);
	virtual std::wstring getOperationDescription(void)
	{
		switch (op)
		{
		case Add: return L"addition operator";
		case Sub: return L"subtraction operator";
		case Mul: return L"multiplication operator";
		case Div: return L"division operator";
		case Mod: return L"modulo operator";
		}
		return L"";
	};

public:
	ArithmeticBinary(int l,int p,OpType optype,Expression *op1,Expression *op2) 
		: BinaryOperation(l,p,op1,op2)
		, op(optype)
		, myType(NULL)
		, myTypeParam(NULL)
		, number(Type::Number)
	{ };

	virtual ~ArithmeticBinary(void)
	{
		if (myType!=NULL) delete myType; 
		if (myTypeParam!=NULL) delete myTypeParam;
	};

	inline OpType getOperation(void) { return op; };
	virtual void print(std::wostream &s);
	virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	//virtual llvm::Value *getRTT(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
};

}

#endif
