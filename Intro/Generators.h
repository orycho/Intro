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

		Generator(int l, int p, std::wstring &varname)
			: Statement(l, p)
			, variable(varname)
			, loop(nullptr)
			, exit(nullptr)
		{};

		std::wstring getVariableName(void) { return variable; };

		// Call only after type inference...
		virtual Type::pointer_t getVariableType(void) = 0;

		/// Virtual function to print the expression to the given stream.
		virtual void print(std::wostream &s) = 0;

		/// Collect the free variables in this expression into the given set.
		//virtual void getFreeVariables(VariableSet &all,VariableSet &bound)=0;
		/// Generators are also "special" in that they generate blocks after the loop body
		virtual void codeGenExitBlock(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env) = 0;

	};

	/// Range generators, for use in C/C++ like for loops over indices.
	/** Implementation is simple, as we can drectly model this as an application.
	*/
	class RangeGen : public Generator
	{
	protected:
		Type::pointer_t myInt;
		Expression *from, *to, *by;
		bool hasIncrement;

	public:

		/// construct a range generator, only parameter by may be NULL.
		RangeGen(int l, int p, std::wstring &varname, Expression *from_, Expression *to_, Expression *by_ = nullptr)
			: Generator(l, p, varname)
			, myInt(std::make_shared<Type>(Type::Integer))
			, from(from_)
			, to(to_)
			, by(by_)
			, hasIncrement(by_ != nullptr)
		{
		}

		virtual ~RangeGen(void)
		{
			delete from;
			delete to;
			if (by != nullptr) delete by;
		}

		virtual Type::pointer_t getVariableType(void)
		{
			return myInt;
		};

		virtual bool makeType(Environment *env, ErrorLocation *errors)
		{

			ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"'from' value of ") + variable);
			Type::pointer_t t = from->getType(env, logger)->find();
			if (t->getKind() == Type::Error)
			{
				errors->addError(logger);
				return false;
			}
			else delete logger;
			if (!t->unify(myInt)) // Make sure we can view the child expr type as a generator type
			{
				errors->addError(new ErrorDescription(getLine(), getColumn(), std::wstring(L"Expected the 'from' value of ") + variable + L" to be an integer"));
				return false;
			}
			logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"'to' value of ") + variable);
			t = to->getType(env, logger)->find();
			if (t->getKind() == Type::Error)
			{
				errors->addError(logger);
				return false;
			}
			else delete logger;
			if (!t->unify(myInt)) // Make sure we can view the child expr type as a generator type
			{
				errors->addError(new ErrorDescription(getLine(), getColumn(), std::wstring(L"Expected the 'to' value of ") + variable + L" to be an integer"));
				return false;
			}
			if (hasIncrement)
			{
				logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"'by' value of ") + variable);
				t = by->getType(env, logger)->find();
				if (t->getKind() == Type::Error)
				{
					errors->addError(logger);
					return false;
				}
				else delete logger;
				if (!t->unify(myInt)) // Make sure we can view the child expr type as a generator type
				{
					errors->addError(new ErrorDescription(getLine(), getColumn(), std::wstring(L"Expected the 'by' value of ") + variable + L" to be an integer"));
					return false;
				}
			}
			env->put(getVariableName(), myInt);
			//return t->find()->getSupertype()->getParameter(0)->find();
			return true;
		}
		/// Virtual function to print the expression to the given stream.
		virtual void print(std::wostream &s)
		{
			s << getVariableName() << L" from ";
			from->print(s);
			s << L" to ";
			to->print(s);
			if (hasIncrement)
			{
				s << " by ";
				by->print(s);
			}
		};

		/// Collect the free variables in this expression into the given set.
		virtual void getFreeVariables(VariableSet &free, VariableSet &bound)
		{
			from->getFreeVariables(free, bound);
			to->getFreeVariables(free, bound);
			if (hasIncrement)
			{
				by->getFreeVariables(free, bound);
			}
			bound.insert(std::make_pair(getVariableName(), getVariableType()));
			// TODO: Capture this variable?!
		};

		virtual bool codeGen(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);
		virtual void codeGenExitBlock(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);
		virtual void collectFunctions(std::vector<intro::Function *> &funcs)
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
		Type::pointer_t mytype;
		Expression::cgvalue rawcontval;
		Expression::cgvalue generator;
	public:

		ContainerGen(int l, int p, std::wstring &varname, Expression *cont)
			: Generator(l, p, varname)
			, container(cont)
		{
		};

		~ContainerGen()
		{
			delete container;
		};

		virtual Type::pointer_t getVariableType(void)
		{
			//return mytype->find()->getSupertype()->find()->getFirstParameter()->find();
			return mytype->find();
		};

		virtual bool makeType(Environment *env, ErrorLocation *errors)
		{
			ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"container expression for ") + variable);
			Type::pointer_t t = container->getType(env, logger)->find();
			if (t->getKind() == Type::Error)
			{
				errors->addError(logger);
				return false;
			}
			else delete logger;
			// The variable name in the next line is for an intermediate, and must not clash with previous one?!
			Type::pointer_t gentype = std::make_shared<Type>(Type::Generator);
			gentype->addParameter(env->put(getVariableName()));
			mytype = Environment::fresh(gentype);
			if (!t->unify(mytype)) // Make sure we can view the child expr type as a generator type
			{
				std::wstringstream strs;
				strs << L"exptected generator type but got a\n";
				t->find()->print(strs);
				strs << "instead\n";
				errors->addError(new ErrorDescription(getLine(), getColumn(), strs.str()));
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
		virtual void getFreeVariables(VariableSet &free, VariableSet &bound)
		{
			bound.insert(std::make_pair(getVariableName(), getVariableType()));
			container->getFreeVariables(free, bound);
		};

		virtual bool codeGen(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);
		virtual void codeGenExitBlock(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);

		virtual void collectFunctions(std::vector<intro::Function *> &funcs)
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

		/// Callback provided by surrounding statement/expressoin to generate the body
		std::function<bool(llvm::IRBuilder<> &, CodeGenEnvironment *)> cgbody;

	public:
		GeneratorStatement(int l, int p) : Statement(l, p)
		{
			cgbody = [](llvm::IRBuilder<> &, CodeGenEnvironment *) {
				std::wcout << L"Error: body callback in GeneratorStatement not set!";
				exit(1);
				return false;
			};
		}

		~GeneratorStatement()
		{
			std::vector<GenCond>::iterator iter;
			for (iter = generators.begin(); iter != generators.end(); iter++)
				if (iter->iscondition) delete iter->condition;
				else delete iter->generator;
		}

		void setBodyCallback(std::function<bool(llvm::IRBuilder<> &, CodeGenEnvironment *)> cgbody_)
		{
			cgbody = cgbody_;
		}

		virtual bool makeType(Environment *env, ErrorLocation *errors)
		{
			std::vector<GenCond>::iterator iter;
			for (iter = generators.begin(); iter != generators.end(); iter++)
			{
				if (iter->iscondition)
				{
					ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"generator statement (condition)"));
					Type::pointer_t t = iter->condition->getType(env, logger);

					if (t->getKind() == Type::Error)
					{
						errors->addError(logger);
						return false;
					}
					else delete logger;
					if (!t->unify(std::make_shared<Type>(Type::Boolean)))
					{
						std::wstringstream strs;
						strs << L"expcted the condition to be a boolean but got ";
						t->find()->print(strs);
						strs << " instead";
						errors->addError(new ErrorDescription(getLine(), getColumn(), strs.str()));
						return false;
					}
				}
				else
				{
					Generator *gen = iter->generator;
					ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"generator statement (binding)"));
					if (gen->makeType(env, logger))
						delete logger;
					else
					{
						errors->addError(logger);
						return false;
					}

					// TODO@ENV: take care of who remembers variable name...
					//env->put(gen->getVariableName(),gen->getVariableType());
				}
			}
			// should not reach
			return true;
		}

		/// Virtual function to print the expression to the given stream.
		virtual void print(std::wostream &s)
		{
			std::vector<GenCond>::iterator iter = generators.begin();
			if (iter == generators.end()) return;
			iter->generator->print(s);
			for (iter++; iter != generators.end(); iter++)
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
			gc.iscondition = true;
			gc.condition = cond;
			generators.push_back(gc);
		}

		void addGenerator(Generator *gen)
		{
			GenCond gc;
			gc.iscondition = false;
			gc.generator = gen;
			generators.push_back(gc);
		}

		virtual void getFreeVariables(VariableSet &free, VariableSet &bound)
		{
			std::vector<GenCond>::iterator iter = generators.begin();
			for (; iter != generators.end(); iter++)
			{
				if (iter->iscondition) iter->condition->getFreeVariables(free, bound);
				else iter->generator->getFreeVariables(free, bound);
			}
		}

		virtual bool codeGen(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);
		virtual void collectFunctions(std::vector<intro::Function *> &funcs)
		{
			std::vector<GenCond>::iterator iter = generators.begin();
			for (; iter != generators.end(); iter++)
			{
				if (iter->iscondition) iter->condition->collectFunctions(funcs);
				else iter->generator->collectFunctions(funcs);
			}
		}
		virtual size_t countVariableStmts(void)
		{
			size_t retval = 0;
			std::vector<GenCond>::iterator iter = generators.begin();
			for (; iter != generators.end(); iter++)
			{
				if (!iter->iscondition) retval += iter->generator->countVariableStmts();
			}
			return retval;
		};

	};

}

#endif