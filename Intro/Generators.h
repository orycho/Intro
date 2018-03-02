#ifndef GENERATORS_H
#define GENERATORS_H

#include "Intro.h"

namespace intro 
{

/// A generator expression binds a variable name to a generator.
/** This class needs to	know the variable that will hold the generated values.
	It also holds some data relevant to the loop's code generation,
	and provides a method codeGenExitBlock() to finalize the loop after the body.
	
	Classes derived of Generator that provide their own Type::makeType() should make sure
	to add the variable to the environment, uding code like
	<tt>mytype=new GeneratorType(env->put(getVariableName()));</tt>.
*/
class Generator : public Statement // Application
{
protected:

	/// Name of the variable this generator instance fills with values.
	std::wstring variable;
public:
	/// These must be set by codeGen() for later use
	llvm::BasicBlock *loop;	///< block to which the body should jump to continue the loop.
	llvm::BasicBlock *exit;	///< block after the loop
	
	Generator(int l,int p,std::wstring &varname) 
		: Statement(l,p)
		, variable(varname)
		, loop(nullptr)
		, exit(nullptr)
	{};

	std::wstring getVariableName(void) { return variable; };
	
	// Call only after type inference...
	virtual Type *getVariableType(void) =0;
	
	/// Virtual function to print the expression to the given stream.
	virtual void print(std::wostream &s) =0;

	/// Collect the free variables in this expression into the given set.
	//virtual void getFreeVariables(VariableSet &all,VariableSet &bound)=0;
	/// Generators are also "special" in that they generate blocks after the loop body
	virtual void codeGenExitBlock(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env)=0;

};

/// Range generators, for use in C/C++ like for loops over indices.
/** Implementation is simple, as we can drectly model this as an application.
*/
class RangeGen : public Generator
{
protected:
	Type myInt;
	Expression *from, *to, *by;
	bool hasIncrement;

public:

	/// construct a range generator, only parameter by may be NULL.
	RangeGen(int l,int p,std::wstring &varname,Expression *from_,Expression *to_,Expression *by_=nullptr) 
		: Generator(l,p,varname)
		, myInt(Type::Integer)
		, from(from_)
		, to(to_)
		, by(by_)
		, hasIncrement(by_!=nullptr)
	{
	}
	
	virtual ~RangeGen(void)
	{
		delete from;
		delete to;
		if (by != nullptr) delete by;
	}

	virtual Type *getVariableType(void) 
	{ 
		return &myInt;
	};
	
	virtual bool makeType(Environment *env)
	{
		Type *t=from->getType(env)->find();
		if (!t->unify(&myInt)) // Make sure we can view the child expr type as a generator type
		{
			//return getError(L"Range generator 'from' was passed a non-integer value.");
			return false;
		}
		t=to->getType(env)->find();
		if (!t->unify(&myInt)) // Make sure we can view the child expr type as a generator type
		{
			//return getError(L"Range generator 'to' was passed a non-integer value.");
			return false;
		}
		if (hasIncrement)
		{
			t=by->getType(env)->find();
			if (!t->unify(&myInt)) // Make sure we can view the child expr type as a generator type
			{
				//return getError(L"Range generator 'from' was passed a non-integer value.");
				return false;
			}
		}
		//return t->find()->getSupertype()->getParameter(0)->find();
		return true;
	}
	/// Virtual function to print the expression to the given stream.
	virtual void print(std::wostream &s)
	{
		s << getVariableName() << L" from ";
		from->print(s);
		s <<  L" to ";
		to->print(s);
		if (hasIncrement)
		{			
			s << " by ";
			by->print(s);
		}
	};

	/// Collect the free variables in this expression into the given set.
	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		from->getFreeVariables(free,bound);
		to->getFreeVariables(free,bound);
		if (hasIncrement)
		{			
			by->getFreeVariables(free,bound);
		}
		bound.insert(getVariableName());
		// TODO: Capture this variable?!
	};

	virtual bool codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void codeGenExitBlock(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs)
	{
	};
	virtual size_t countVariableStmts(void)
	{
		return 1; // variable being iterated over is allocated!
	};
};

/// A container may be anything derived of generator (i.e. also Lists and Strings).
/** Normal generators are also always ok. That is why we do not just use the
	Application class type inference here, but a different makeType().
	Happily, it is much simpler than application :)
*/
class ContainerGen : public Generator
{
	Expression *container;
	Type *mytype;
	Type *source_type;
	Type gentype;
	Expression::cgvalue rawcontval;
	Expression::cgvalue generator;
public:

	ContainerGen(int l,int p,std::wstring &varname,Expression *cont) 
		: Generator(l,p,varname)
		, container(cont)
		, mytype(nullptr)
		, source_type(nullptr)
		, gentype(Type::Generator)
	{
	};

	~ContainerGen()
	{
		deleteCopy(source_type);
		delete container;
	};
	
	virtual Type *getVariableType(void) 
	{ 
		return mytype->find()->getSupertype()->find()->getFirstParameter()->find();
	};

	virtual bool makeType(Environment *env)
	{
		Type *t=container->getType(env)->find();
		// The variable name in the next line is for an intermediate, and must not clash with previous one?!
		//Type gentype(Type::Generator, env->put(getVariableName()));
		//gentype.replaceSupertype(env->put(getVariableName()));
		gentype.addParameter(env->put(getVariableName()));
		mytype=Environment::fresh(&gentype);
		//mytype = Environment::fresh(new Type(Type::Generator, env->put(getVariableName())));
		source_type=t->copy();
		if (!t->unify(mytype)) // Make sure we can view the child expr type as a generator type
		{
			return false;
		}
		return true;
	};

	/// Virtual function to print the expression to the given stream.
	virtual void print(std::wostream &s)
	{
		s << getVariableName() << " in ";
		container->print(s);
	};

	/// Collect the free variables in this expression into the given set.
	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		bound.insert(getVariableName());
		container->getFreeVariables(free,bound);
	};

	virtual bool codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void codeGenExitBlock(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);

	virtual void collectFunctions(std::list<intro::Function*> &funcs)
	{
		container->collectFunctions(funcs);
	};
	
	virtual size_t countVariableStmts(void)
	{
		return 1; // variable being iterated over is allocated!
	};

};

/// This class represents a full generator statement for values.
/** A generator expressions is an expression with free variables,
	and a list of generators and conditions. The order of the list is important,
	as it defines how loops and conditions are nested.
	The generators provide "variable binders" for the expression.
	The conditions impose restrictions on the values generated, and they contain free variables which
	are a subset of the variable binders defined previously in the list.
*/
class GeneratorStatement : public Statement
{
private:
	typedef struct
	{
		bool iscondition;
		union
		{
			Generator *generator;
			Expression *condition; ///< A boolean expression with free variables.
		};
	} GenCond;

	std::vector<GenCond> generators;
	Type mybool;
	
	/// Callback provided by surrounding statement/expressoin to generate the body
	std::function<bool(llvm::IRBuilder<>&,CodeGenEnvironment*)> cgbody;

public:
	GeneratorStatement(int l,int p) : Statement(l,p), mybool(Type::Boolean)
	{
		cgbody=[](llvm::IRBuilder<>&,CodeGenEnvironment*){
			std::wcout << L"Error: body callback in GeneratorStatement not set!";
			exit(1);
			return false;
		};
	}

	~GeneratorStatement()
	{
		std::vector<GenCond>::iterator iter;
		for (iter=generators.begin();iter!=generators.end();iter++)
			if (iter->iscondition) delete iter->condition;
			else delete iter->generator;
	}
	
	void setBodyCallback(std::function<bool(llvm::IRBuilder<>&,CodeGenEnvironment*)> cgbody_)
	{ cgbody = cgbody_; }

	virtual bool makeType(Environment *env)
	{
		std::vector<GenCond>::iterator iter;
		for (iter=generators.begin();iter!=generators.end();iter++)
		{
			if (iter->iscondition)
			{
				Type *t=iter->condition->getType(env);
				//if (t->find()->getKind()!=Type::Boolean) // condition must be boolean
				if (!t->unify(&mybool))
					return false;
			}
			else 
			{
				Generator *gen=iter->generator;
				if (!gen->makeType(env))
					return false;
				// TODO@ENV: take care of who remembers variable name...
				env->put(gen->getVariableName(),gen->getVariableType());
			}
		}
		// should not reach
		return true;
	}

	/// Virtual function to print the expression to the given stream.
	virtual void print(std::wostream &s)
	{
		std::vector<GenCond>::iterator iter=generators.begin();
		if (iter==generators.end()) return;
		iter->generator->print(s);
		for (iter++;iter!=generators.end();iter++)
		{
			if (iter->iscondition) 
			{
				s << " ?? ";
				iter->condition->print(s);
			}
			else 
			{
				s << " && ";
				iter->generator->print(s);
			}
		}
	}

	void addCondition(Expression *cond)
	{
		GenCond gc;
		gc.iscondition=true;
		gc.condition=cond;
		generators.push_back(gc);
	}

	void addGenerator(Generator *gen)
	{
		GenCond gc;
		gc.iscondition=false;
		gc.generator=gen;
		generators.push_back(gc);
	}

	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		std::vector<GenCond>::iterator iter=generators.begin();
		for (;iter!=generators.end();iter++)
		{
			if (iter->iscondition) iter->condition->getFreeVariables(free,bound);
			else iter->generator->getFreeVariables(free,bound);
		}
	}

	virtual bool codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs)
	{
		std::vector<GenCond>::iterator iter=generators.begin();
		for (;iter!=generators.end();iter++)
		{
			if (iter->iscondition) iter->condition->collectFunctions(funcs);
			else iter->generator->collectFunctions(funcs);
		}
	}
	virtual size_t countVariableStmts(void)
	{
		size_t retval=0;
		std::vector<GenCond>::iterator iter=generators.begin();
		for (;iter!=generators.end();iter++)
		{
			if (!iter->iscondition) retval+=iter->generator->countVariableStmts();
		}
		return retval;
	};

};

}

#endif