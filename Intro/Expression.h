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
	protected:
		virtual Type::pointer_t makeType(Environment *, ErrorLocation *);
	public:
		IntegerConstant(int l, int p, wchar_t *token, int base = 10) : Expression(l, p)
		{

#ifdef WIN32
			value = _wtoi64(token);
#else
			wchar_t *end;
			value = wcstoll(token, &end, base);
#endif
		};
		virtual ~IntegerConstant()
		{
		};
		virtual void print(std::wostream &s);
		virtual void getFreeVariables(VariableSet &, VariableSet &)
		{};

		virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);
		virtual void collectFunctions(std::vector<intro::Function *> &funcs) { /*NOP*/ };
	};

	/// A Boolean constant.
	/**	Every type of constant has a node representing it. The node also stores
		a representation of the constant value.
	*/
	class BooleanConstant : public Expression
	{
		bool value;
	protected:
		virtual Type::pointer_t makeType(Environment *, ErrorLocation *);
	public:
		BooleanConstant(int l, int p, bool val) : Expression(l, p), value(val)
		{};
		~BooleanConstant()
		{
		};
		virtual void print(std::wostream &s);
		virtual void getFreeVariables(VariableSet &, VariableSet &)
		{};
		virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);
		virtual void collectFunctions(std::vector<intro::Function *> &funcs) { /*NOP*/ };
	};

	/// A real constant, represented using a double.
	/**	Every type of constant has a node representing it. The node also stores
		a representation of the constant value.
	*/
	class RealConstant : public Expression
	{
		double value;
	protected:
		virtual Type::pointer_t makeType(Environment *, ErrorLocation *);
	public:
		RealConstant(int l, int p, wchar_t *token) : Expression(l, p)
		{
			wchar_t *ptr;
			value = wcstod(token, &ptr);
		};
		~RealConstant()
		{
		};
		virtual void print(std::wostream &s);
		virtual void getFreeVariables(VariableSet &, VariableSet &)
		{};
		virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);
		virtual void collectFunctions(std::vector<intro::Function *> &funcs) { /*NOP*/ };
	};

	/// A string constant, represented using std::wstring (16 bit).
	/**	Every type of constant has a node representing it. The node also stores
		a representation of the constant value.
	*/
	class StringConstant : public Expression
	{
		std::wstring value;

		/// A stringpart is either a constant string literal, or a variable interpolation (which may be another string).
		struct part
		{
			part(const std::wstring &s_, intro::Type::pointer_t t_)
			{
				s = s_; t = t_;
			};
			/// the constant string (if t==NULL) or a variable name if t != NULL (the variable's type).
			std::wstring s;
			/// The type of the variable, or NULL if a constant string
			intro::Type::pointer_t t;
		};
		/// The parts from which the string is interpolated.
		std::vector<part> parts;
		typedef std::vector<part>::iterator iterator;
	protected:
		virtual Type::pointer_t makeType(Environment *, ErrorLocation *);
	public:
		StringConstant(int l, int p, wchar_t *token) : Expression(l, p)
		{
			value.assign(token);
		};
		~StringConstant()
		{
		};
		virtual void print(std::wostream &s);
		virtual void getFreeVariables(VariableSet &free, VariableSet &bound)
		{
			for (iterator i = parts.begin(); i != parts.end(); i++)
				if (i->t != NULL)
					if (bound.find(i->s) == bound.end()) free.insert(std::make_pair(i->s, i->t));
		};
		virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);
		virtual void collectFunctions(std::vector<intro::Function *> &funcs) { /*NOP*/ };
	};

	/// List constants are either a literal list of the elements initialized to, or a generator based expression
	class ListConstant : public Expression
	{
		/// The elements, in case of generators there must be one entry here
		std::vector<intro::Expression *> elements;
		GeneratorStatement *generators;
	protected:
		virtual Type::pointer_t makeType(Environment *env, ErrorLocation *errors);
	public:
		ListConstant(int l, int p, std::vector<intro::Expression *> contents)
			: Expression(l, p)
			, elements(contents)
			, generators(NULL)
		{
		};
		~ListConstant()
		{
			if (generators != NULL) delete generators;
			std::vector<intro::Expression *>::iterator iter;
			for (iter = elements.begin(); iter != elements.end(); iter++)
				delete *iter;
		};
		inline void setGenerators(GeneratorStatement *gen) { generators = gen; };
		/// Virtual function to print the expression to the given stream.
		virtual void print(std::wostream &s);
		/// Collect the free variables in this expression into the given set.
		virtual void getFreeVariables(VariableSet &free, VariableSet &bound)
		{
			if (generators != nullptr) generators->getFreeVariables(free, bound);
			std::vector<intro::Expression *>::iterator iter;
			for (iter = elements.begin(); iter != elements.end(); iter++)
				(*iter)->getFreeVariables(free, bound);
		};
		virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);
		virtual void collectFunctions(std::vector<intro::Function *> &funcs);
	};

	/// Dictionary constants are either a literal list of the key-vaue pairs initialized to, or a generator based key-value expression.
	class DictionaryConstant : public Expression
	{
	public:
		typedef std::vector<std::pair<intro::Expression *, intro::Expression *> > memberlist;
	private:
		/// The elements, in case of generators there must be one entry here
		memberlist elements;
		GeneratorStatement *generators;
	protected:
		/// Purely virtual function that is used to create an expressions type.
		virtual Type::pointer_t makeType(Environment *env, ErrorLocation *errors);
	public:
		DictionaryConstant(int l, int p, memberlist contents)
			: Expression(l, p)
			, elements(contents)
			, generators(NULL)
		{
		};
		~DictionaryConstant()
		{
			if (generators != NULL) delete generators;
			memberlist::iterator iter;
			for (iter = elements.begin(); iter != elements.end(); iter++)
			{
				delete iter->first;
				delete iter->second;
			}
		};
		inline void setGenerators(GeneratorStatement *gen) { generators = gen; };
		/// Virtual function to print the expression to the given stream.
		virtual void print(std::wostream &s);
		/// Collect the free variables in this expression into the given set.
		virtual void getFreeVariables(VariableSet &free, VariableSet &bound)
		{
			if (generators != NULL) generators->getFreeVariables(free, bound);;
			memberlist::iterator iter;
			for (iter = elements.begin(); iter != elements.end(); iter++)
			{
				iter->first->getFreeVariables(free, bound);
				iter->second->getFreeVariables(free, bound);
			}
		};

		virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);
		virtual void collectFunctions(std::vector<intro::Function *> &funcs);
	};

	/// A record expression defines record values. 
	/**	Since variants are tagged records, it also does a lot of the grunt work for the variant class.
		The variant class just makes sure space for a tag is created then fills the tag.
	*/
	class RecordExpression : public Expression
	{
	protected:
		/// The members of the record are label-expression pairs.
		std::vector<std::pair<std::wstring, Expression *> > members;

		virtual Type::pointer_t makeType(Environment *env, ErrorLocation *errors);
		bool isVariant;
	public:

		typedef std::vector<std::pair<std::wstring, Expression *> >::iterator iterator;

		RecordExpression(int l, int p)
			: Expression(l, p)
			, isVariant(false)
		{};

		~RecordExpression()
		{
			iterator iter;
			for (iter = members.begin(); iter != members.end(); iter++)
				delete iter->second;
		};
		inline void addMember(std::wstring &label, Expression *expr)
		{
			members.push_back(make_pair(label, expr));
		};

		void setToVariant() { isVariant = true; };

		inline iterator begin() { return members.begin(); };
		inline iterator end() { return members.end(); };

		virtual void print(std::wostream &s);
		virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);

		virtual void getFreeVariables(VariableSet &free, VariableSet &bound)
		{
			iterator iter;
			for (iter = members.begin(); iter != members.end(); iter++)
				iter->second->getFreeVariables(free, bound);
		};
		virtual void collectFunctions(std::vector<intro::Function *> &funcs);
	};

	/// A Variant expression defines values of one variant type.
	/**	The actual handling shares a lot of tasks with the record contained in the variant.
	*/
	class VariantExpression : public Expression
	{

	protected:
		/// The members of the record are label-expression pairs.
		std::wstring tag;
		RecordExpression *record;
		virtual Type::pointer_t makeType(Environment *env, ErrorLocation *errors);
	public:


		VariantExpression(int l, int p)
			: Expression(l, p)
			, record(nullptr)
		{};
		~VariantExpression()
		{
			delete record;
		};
		inline void setVariant(const std::wstring &tag, RecordExpression *expr)
		{
			this->tag = tag;
			record = expr;
			record->setToVariant();
		};
		virtual void print(std::wostream &s);
		virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);

		virtual void getFreeVariables(VariableSet &free, VariableSet &bound)
		{
			record->getFreeVariables(free, bound);
		};
		virtual void collectFunctions(std::vector<intro::Function *> &funcs);
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
			Unrestricted = 0,
			ReadOnly = 1,
			WriteOnly = 2
		};
	private:
		std::vector<std::wstring> path;
		std::wstring name;
		int modifiers;
		bool relative;

	protected:
		virtual Type::pointer_t makeType(Environment *env, ErrorLocation *errors);
	public:
		Variable(int l, int p, bool rel, std::wstring name_, const std::vector<std::wstring> &path_, int mod = 0)
			: Expression(l, p)
			, path(path_)
			, name(name_)
			, modifiers(mod)
			, relative(rel)
		{
		};

		~Variable()
		{
		}
		const std::wstring &getName(void) { return name; };
		virtual void print(std::wostream &s);
		virtual void getFreeVariables(VariableSet &free, VariableSet &bound)
		{
			// If it's in some module or the global scope, then we take it from there.
			if (relative && path.empty())
			{
				if (bound.find(name) == bound.end()) free.insert(std::make_pair(name, getType()));
			}
		};

		virtual bool isWritable(void)
		{
			return getType()->isWritable();
		};
		/// (Use after type inference) If writable, this method returns the type that can be written (e.g. the dictionaries value type).
		virtual Type::pointer_t getWritableType(void)
		{
			return getType();
		};

		/// (Use after type inference) Generate the address that can be written to, if this expression is writable;
		virtual Expression::cgvalue getWriteAddress(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);

		virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);
		virtual void collectFunctions(std::vector<intro::Function *> &funcs) { /*NOP*/ };
	};

	/// A record element access expression: givena record and a label, extract the approriate element.
	/**	Every type of constant has a node representing it. The node also stores
		a representation of the constant value.
	*/
	class RecordAccess : public Expression
	{
		Expression *record; ///< The record to be accessed
		std::wstring label;	///< The label that is requested
	protected:
		virtual Type::pointer_t makeType(Environment *env, ErrorLocation *errors);
	public:
		RecordAccess(int l, int p, Expression *rec, std::wstring &lab)
			: Expression(l, p)
			, record(rec)
			, label(lab)
		{ };
		virtual ~RecordAccess()
		{
			delete record;
		};

		inline const std::wstring &getLabel() { return label; };
		virtual void print(std::wostream &s);
		virtual void getFreeVariables(VariableSet &free, VariableSet &bound)
		{
			record->getFreeVariables(free, bound);
		};

		virtual bool isWritable(void)
		{
			return getType()->isWritable();
		};
		/// (Use after type inference) If writable, this method returns the type that can be written (e.g. the dictionaries value type).
		virtual Type::pointer_t getWritableType(void)
		{
			Type::pointer_t rt = record->getType();
			if (rt->getKind() != Type::Record) return Type::pointer_t();
			RecordType *rect = (RecordType *)rt.get();
			return rect->findMember(label)->second;
		};

		/// (Use after type inference) Generate the address that can be written to, if this expression is writable;
		virtual Expression::cgvalue getWriteAddress(llvm::IRBuilder<> &, CodeGenEnvironment *);

		virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);
		virtual void collectFunctions(std::vector<intro::Function *> &funcs);
	};

	/// Extraction represetns the [] operator for dictionaries
	class Extraction : public Application
	{
	protected:
		virtual Type::pointer_t getCalledFunctionType(Environment *env, ErrorLocation *errors);
		virtual std::wstring getOperationDescription(void)
		{
			return L"dictionary lookup operation";
		};
	public:
		Extraction(int l, int p, Expression *source, Expression *key)
			: Application(l, p)
		{
			params.push_back(source);
			params.push_back(key);
		};
		virtual ~Extraction()
		{
		};
		virtual void print(std::wostream &s);
		virtual void getFreeVariables(VariableSet &free, VariableSet &bound)
		{
			for (iterator iter = params.begin(); iter != params.end(); ++iter)
				(*iter)->getFreeVariables(free, bound);
		};

		virtual bool isWritable(void)
		{
			Type::pointer_t t = params.front()->getType();
			if (t->getKind() != Type::Dictionary) return false;
			return t->isWritable();
		};
		/// (Use after type inference) If writable, this method returns the type that can be written (e.g. the dictionaries value type).
		virtual Type::pointer_t getWritableType(void)
		{
			Type::pointer_t t = params.front()->getType();
			if (t->getKind() != Type::Dictionary) return NULL;
			return t->getParameter(1);
		};

		/// (Use after type inference) Generate the address that can be written to, if this expression is writable;
		virtual cgvalue getWriteAddress(llvm::IRBuilder<> &, CodeGenEnvironment *);
		virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);
		virtual void collectFunctions(std::vector<intro::Function *> &funcs);
	};

	/// Dictionary Erase represeted b '\'
	class DictionaryErase : public Application
	{
	protected:
		virtual Type::pointer_t getCalledFunctionType(Environment *env, ErrorLocation *errors);
		virtual std::wstring getOperationDescription(void)
		{
			return L"erase from dictionary operation";
		};
	public:
		DictionaryErase(int l, int p, Expression *source, Expression *key)
			: Application(l, p)
		{
			params.push_back(source);
			params.push_back(key);
		};
		virtual ~DictionaryErase()
		{
		};
		virtual void print(std::wostream &s);
		virtual void getFreeVariables(VariableSet &free, VariableSet &bound)
		{
			for (iterator iter = params.begin(); iter != params.end(); ++iter)
				(*iter)->getFreeVariables(free, bound);
		};

		virtual bool isWritable(void)
		{
			Type::pointer_t t = params.front()->getType();
			if (t->getKind() != Type::Dictionary) return false;
			return t->isWritable();
		};
		/// (Use after type inference) If writable, this method returns the type that can be written (e.g. the dictionaries value type).
		virtual Type::pointer_t getWritableType(void)
		{
			Type::pointer_t t = params.front()->getType();
			if (t->getKind() != Type::Dictionary) return NULL;
			return t->getParameter(1);
		};

		/// (Use after type inference) Generate the address that can be written to, if this expression is writable;
		virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);
		virtual void collectFunctions(std::vector<intro::Function *> &funcs);
	};

	/// Splice a list or string, ternary []- operator (sequence, first, last)
	class Splice : public Application
	{
		Type::pointer_t counter;
	protected:
		virtual Type::pointer_t getCalledFunctionType(Environment *env, ErrorLocation *errors);
		virtual Type::pointer_t makeType(Environment *env, ErrorLocation *errors);
		virtual std::wstring getOperationDescription(void)
		{
			return L"splice operator";
		};
	public:
		Splice(int l, int p, Expression *source, Expression *start, Expression *end = NULL)
			: Application(l, p)
			, counter(new Type(Type::Integer))
		{
			params.push_back(source);
			params.push_back(start);
			if (end != NULL) params.push_back(end);
		};
		virtual ~Splice()
		{
		};
		virtual void print(std::wostream &s);
		virtual void getFreeVariables(VariableSet &free, VariableSet &bound)
		{
			VariableSet bound2(bound.begin(), bound.end());
			// The variable "last" is defined in here, it is not free
			bound2.insert(std::make_pair(L"last", counter));
			for (iterator iter = params.begin(); iter != params.end(); ++iter)
				(*iter)->getFreeVariables(free, bound2);
		};

		virtual bool isWritable(void)
		{
			Type::pointer_t t = params.front()->getType();
			return t->isWritable();
		};
		/// (Use after type inference) If writable, this method returns the type that can be written (e.g. the dictionaries value type).
		virtual Type::pointer_t getWritableType(void)
		{
			Type::pointer_t t = params.front()->getType();
			return t->getParameter(1);
		};

		/// (Use after type inference) Generate the address that can be written to, if this expression is writable;
		virtual Expression::cgvalue getWriteAddress(llvm::IRBuilder<> &, CodeGenEnvironment *)
		{
			return Expression::cgvalue(nullptr, nullptr);
		};

		virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);
		//virtual llvm::Value *getRTT(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
		virtual void collectFunctions(std::vector<intro::Function *> &funcs);
	};

	/// Assignment a new value to a variable.
	class Assignment : public Expression
	{
	protected:
		virtual Type::pointer_t makeType(Environment *env, ErrorLocation *errors);
		/// The list of expressions that evaluate to the function call's parameters
		Expression *destination;
		/// The epression which must evaluate to a vaue of the correct function type
		Expression *value;
	public:
		Assignment(int l, int p, Expression *dest, Expression *val)
			: Expression(l, p)
			, destination(dest)
			, value(val)
		{ };
		~Assignment(void)
		{
			delete destination;
			delete value;
		};
		virtual void print(std::wostream &s);
		virtual void getFreeVariables(VariableSet &free, VariableSet &bound)
		{
			destination->getFreeVariables(free, bound);
			value->getFreeVariables(free, bound);
		};
		virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);
		virtual void collectFunctions(std::vector<intro::Function *> &funcs);
	};

	/// This is the parent class for all binary operations, which are applictions of functions with two parameters.
	class BinaryOperation : public Application
	{
	protected:
		//Expression *operand1,*operand2;
		BinaryOperation(int l, int p, Expression *op1, Expression *op2) : Application(l, p)
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
		virtual Type::pointer_t getCalledFunctionType(Environment *, ErrorLocation *);
		virtual std::wstring getOperationDescription(void)
		{
			if (op == Not) return L"not operator";
			else return L"negate operator";
		};
	public:
		UnaryOperation(int l, int p, OpType type, Expression *operand)
			: Application(l, p)
			, op(type)
		{
			params.push_back(operand);
		};
		virtual ~UnaryOperation(void)
		{
		};
		inline OpType getOperation(void) { return op; };
		virtual void print(std::wostream &s);
		virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);
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
		virtual Type::pointer_t getCalledFunctionType(Environment *, ErrorLocation *);
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
		BooleanBinary(int l, int p, OpType optype, Expression *op1, Expression *op2)
			: BinaryOperation(l, p, op1, op2)
			, op(optype)
		{};

		virtual ~BooleanBinary()
		{
		};
		inline OpType getOperation(void) { return op; };
		virtual void print(std::wostream &s);
		virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);
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
		virtual Type::pointer_t getCalledFunctionType(Environment *, ErrorLocation *);
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
		CompareOperation(int l, int p, OpType optype, Expression *op1, Expression *op2)
			: BinaryOperation(l, p, op1, op2)
			, op(optype)
		{};

		virtual ~CompareOperation(void)
		{
		};
		inline OpType getOperation(void) { return op; };
		virtual void print(std::wostream &s);
		virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);
	};

	/// Arithmetic is (Integer,Integer)->Integer or (Real,Real)->Real. With mixed parameters, Integer is coerced to Real.
	class ArithmeticBinary : public BinaryOperation
	{
		/// @name Real artihmetic codegen callbacks
		static llvm::Value *gen_add_real(llvm::IRBuilder<> &builder, std::vector<llvm::Value *> &args)
		{
			return builder.CreateFAdd(args[0], args[1], "addtmp");
		};
		static llvm::Value *gen_sub_real(llvm::IRBuilder<> &builder, std::vector<llvm::Value *> &args)
		{
			return builder.CreateFSub(args[0], args[1], "subtmp");
		};
		static llvm::Value *gen_mul_real(llvm::IRBuilder<> &builder, std::vector<llvm::Value *> &args)
		{
			return builder.CreateFMul(args[0], args[1], "multmp");
		};
		static llvm::Value *gen_div_real(llvm::IRBuilder<> &builder, std::vector<llvm::Value *> &args)
		{
			return builder.CreateFDiv(args[0], args[1], "divtmp");
		};
		static llvm::Value *gen_rem_real(llvm::IRBuilder<> &builder, std::vector<llvm::Value *> &args)
		{
			return builder.CreateFRem(args[0], args[1], "modtmp");
		};
		/// @name Integer artihmetic codegen callbacks
		static llvm::Value *gen_add_int(llvm::IRBuilder<> &builder, std::vector<llvm::Value *> &args)
		{
			return builder.CreateAdd(args[0], args[1], "addtmp");
		};
		static llvm::Value *gen_sub_int(llvm::IRBuilder<> &builder, std::vector<llvm::Value *> &args)
		{
			return builder.CreateSub(args[0], args[1], "subtmp");
		};
		static llvm::Value *gen_mul_int(llvm::IRBuilder<> &builder, std::vector<llvm::Value *> &args)
		{
			return builder.CreateMul(args[0], args[1], "multmp");
		};
		static llvm::Value *gen_div_int(llvm::IRBuilder<> &builder, std::vector<llvm::Value *> &args)
		{
			return builder.CreateSDiv(args[0], args[1], "divtmp");
		};
		static llvm::Value *gen_rem_int(llvm::IRBuilder<> &builder, std::vector<llvm::Value *> &args)
		{
			return builder.CreateSRem(args[0], args[1], "modtmp");
		};

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
		virtual Type::pointer_t getCalledFunctionType(Environment *, ErrorLocation *);
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
		ArithmeticBinary(int l, int p, OpType optype, Expression *op1, Expression *op2)
			: BinaryOperation(l, p, op1, op2)
			, op(optype)
		{ };

		virtual ~ArithmeticBinary(void)
		{
		};

		inline OpType getOperation(void) { return op; };
		virtual void print(std::wostream &s);
		virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);
	};

}

#endif
