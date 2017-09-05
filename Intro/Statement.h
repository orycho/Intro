#ifndef STATEMENT_H
#define STATEMENT_H

#include <iostream>
#include "Intro.h"
#include "Generators.h"

namespace intro 
{

class ValueStatement;
	
/// A block statement is used to create a local sub-environement.
class BlockStatement : public Statement
{
	std::list<Statement*> body;

public:

	BlockStatement(int l,int p) : Statement(l,p)
	{};

	~BlockStatement()
	{
		std::list<Statement*>::iterator iter;
		for (iter=body.begin();iter!=body.end();iter++)
			delete *iter;
	}

	inline void appendBody(Statement *s) { body.push_back(s); }

	virtual bool makeType(Environment *env)
	{
		Environment localenv(env);
		bool success=true;
		std::list<Statement*>::iterator stmts;
		for (stmts=body.begin();stmts!=body.end();stmts++)
			success &= (*stmts)->makeType(&localenv);
		return success;
	}

	virtual void print(std::wostream &s)
	{
		std::list<Statement*>::iterator stmts;
		for (stmts=body.begin();stmts!=body.end();stmts++)
		{
			(*stmts)->print(s);
			s << ";\n";
		}
	}
	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		std::list<Statement*>::iterator stmts;
		// TODO: Capture this variable?!
		for (stmts=body.begin();stmts!=body.end();stmts++)
			(*stmts)->getFreeVariables(free,bound);
	}

	virtual bool codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs)
	{
		std::list<Statement*>::iterator stmts;
		for (stmts=body.begin();stmts!=body.end();stmts++)
			(*stmts)->collectFunctions(funcs);
	}

	virtual bool isReturnLike(void) 
	{ 
		if (body.empty()) return false;
		return body.back()->isReturnLike(); 
	}
	
	virtual size_t countVariableStmts(void)
	{
		size_t retval=0;
		std::list<Statement*>::iterator stmts;
		for (stmts=body.begin();stmts!=body.end();stmts++)
			retval+=(*stmts)->countVariableStmts();
		return retval;
	}
	
};

/// A function is actually an expression. Created either using value statements, or anonymously using the function keyword.
class Function : public Expression
{
public:
	struct Parameter
	{
		std::wstring name;
		Type *type;

		Parameter(const std::wstring &n,Type *t) : name(n), type(t) {};

		void print(std::wostream &s)
		{
			//s << name << ":" ;
			//type->find()->print(s);
			type->find()->print(s << name << ":" );
		}
	};

	typedef std::list<Parameter> ParameterList;
private:
	ParameterList parameters; /// The function's parameters, for which types are inferred.
	BlockStatement *body; /// The function body.
	Type *type; ///< The return type of the function.
	bool isGenerator; ///< True if this is a generator using yield statements.
	FunctionType *myType;
	ValueStatement *parentvar; ///< If not, this points to the var statement holding the function expressnion.

protected:
	virtual Type *makeType(Environment *env)
	{
		Environment localenv(env);
		// Add return and paramter types to local environment
		//Type *rettype=localenv.put(L"!return");
		// TODO@ENV: Handle return type
		localenv.put(L"!return");
		ParameterList::iterator iter;
		for (iter=parameters.begin();iter!=parameters.end();iter++)
		{
			iter->type=localenv.put(iter->name);
			iter->type->setAccessFlags(Type::Readable);
		}
		if (!body->makeType(&localenv))
		{
			return getError(L"Error in function body");
		}
		std::list<Type*> p;
		for (iter=parameters.begin();iter!=parameters.end();iter++)
		{
			p.push_back(iter->type);
		}

		myType=new FunctionType(p,localenv.get(L"!return"));
		if (myType->getReturnType()->getKind()!=Type::Unit
			&& !body->isReturnLike())
			return getError(L"Function returning a value does not end on return-like satement!");
		
		return myType;
	};

	/// This method constructs the actual function body. Called by codeGen to have that decluttered.
	/** We pass in the free vars as an array in order to guarantee identical ordering with
		closure initialization.
	*/
	llvm::Function *buildFunction( CodeGenEnvironment *parent,const std::vector<std::wstring> &variables);
public:
	Function(int l,int p): Expression(l,p), myType(NULL)
	{ parentvar=NULL; };

	~Function()
	{
		std::list<Statement*>::iterator sit;
		delete body;
		ParameterList::iterator pit;
		for (pit=parameters.begin();pit!=parameters.end();pit++)
			deleteCopy(pit->type);
		if (myType!=NULL) delete myType;
	};

	void setParentVar(ValueStatement *pv) { parentvar=pv; };
	inline ParameterList &getParameters(void) { return parameters; };
	
	inline void setBody(BlockStatement *s) { body=s; };
	inline Type *getType(void) { return type; };
	inline void setType(Type *t) { type=t; };

	virtual void print(std::wostream &s)
	{
		ParameterList::iterator iter;
		if (parentvar==NULL) s<<"fun";
		s << "(";
		iter=parameters.begin();
		if (iter!=parameters.end())
		{
			iter->print(s);
			iter++;
			while (iter!=parameters.end()) 
			{
				s << ",";
				iter->print(s);
				iter++;
			}
		};
		s << ") -> ";
		body->print(s);
		s << "end";
	};
	
	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		// bind parameters
		ParameterList::iterator iter;
		VariableSet bound2(bound.begin(),bound.end());
		for (iter=parameters.begin();iter!=parameters.end();iter++)
			bound2.insert(iter->name);
		body->getFreeVariables(free,bound2);
	};

	virtual cgvalue codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	//virtual llvm::Value *getRTT(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);

	virtual void collectFunctions(std::list<intro::Function*> &funcs)
	{
		funcs.push_back(this);
		std::list<Statement*>::iterator bodyiter;
		body->collectFunctions(funcs);
	};

	virtual size_t countVariableStmts(void)
	{
		size_t retval=0;
		//std::list<Statement*>::iterator stmts;
		//for (stmts=body.begin();stmts!=body.end();stmts++)
		//	retval+=(*stmts)->countVariableStmts();
		return retval;
	}

};

/// Value statements create a new variable and assigne a value to it.
class ValueStatement : public Statement
{
	std::wstring name;
	Expression *value;
	bool constant;
	bool isInteractive;
public:
	ValueStatement(int l,int p,const std::wstring &n,Expression *e,bool isconst,bool isInteractive_) 
		: Statement(l,p)
		, name(n)
		, value(e)
		, constant(isconst)
		, isInteractive(isInteractive_)
	{
		Function *f=dynamic_cast<Function*>(value);
		if (f!=NULL) f->setParentVar(this);
	};

	~ValueStatement()
	{
		delete value;
	};

	inline std::wstring getName(void) { return  name; };

	inline Expression *getValue(void) { return value; };

	virtual bool makeType(Environment *env)
	{
		// This part needs to account for preexisting name in case
		// it is created through exporting from a module (must make sure of that, hence clumsy?!)
		Type *t=env->put(name);
		if (t==nullptr)
		{
			std::wcout << L"The identifier \"" << name.c_str() << "\" already names a variable in this scope!\n";
			return false;
		}
		// @TODO: When adding constants in the future, so it here...
		if (!constant) t->setAccessFlags(Type::Readable|Type::Writable);
		Type *et=value->getType(env);
		et->setAccessFlags(t->getAccessFlags());
		if (et->getKind()==Type::Error) return false;
		if (!et->unify(t))
		{
				std::wcout << L"The identifier \"" << name.c_str() << "\" could not be assigned a type. Could not unifiy\n\t";
				t->find()->print(std::wcout);
				std::wcout << "\n\twith\n\t";
				et->find()->print(std::wcout);
				std::wcout << "\n";
				env->put(name,new ErrorType(getLine(),getPosition(),std::wstring(L"Type Error Occured defining ")+name+L", check definition."));
				return false;
		}
		return true;
	};

	virtual void print(std::wostream &s)
	{
		s << "var " << name;
		if (value->getType()!=NULL && value->getType()->find()->getKind()!=Type::Function) 
			s << " <- ";
		value->print(s);
		if (value->getType()!=NULL) 
		{
			s << ":";
			value->getType()->find()->print(s);
		}
	};

	virtual bool codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);

	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		bound.insert(name);
		value->getFreeVariables(free,bound);
	};

	virtual void collectFunctions(std::list<intro::Function*> &funcs)
	{
		value->collectFunctions(funcs);
	};
	
	virtual size_t countVariableStmts(void)
	{
		return 1;
	}

};

/// The if statement is a conditional.
class IfStatement : public Statement
{
public:
	typedef std::pair<Expression*,Statement*> Condition;
	typedef std::list<Condition>::iterator iterator;


private:
	Type boolean;
	std::list<Condition> conditions;
	Statement *otherwise;
public:

	IfStatement(int l,int p) 
		: Statement(l,p)
		, boolean(Type::Boolean)
		, otherwise(NULL)
	{};
	
	~IfStatement()
	{
		iterator iter;
		for (iter=conditions.begin();iter!=conditions.end();iter++)
		{
			delete iter->first;
			delete iter->second;
		};
		if (otherwise!=NULL) delete otherwise;
	};

	void appendCondition(Expression *cond,Statement *action)
	{
		conditions.push_back(std::make_pair(cond,action));
	};

	inline void setOtherwise(Statement *s) { otherwise=s; };
	inline Statement *getOtherwise(void) { return otherwise; };

	virtual bool makeType(Environment *env)
	{
		iterator iter;
		bool success=true;
		for (iter=conditions.begin();iter!=conditions.end();iter++)
		{
			Environment localenv(env);
			success&=iter->first->getType(env)->unify(&boolean);
			success&=iter->second->makeType(&localenv);
		}
		if (otherwise!=NULL)
		{
			Environment localenv(env);
			success &= otherwise->makeType(&localenv);
		}
		return success;
	};

	virtual void print(std::wostream &s)
	{
		iterator iter=conditions.begin();
		s << "if ";
		iter->first->print(s);
		s << "then\n";
		iter->second->print(s);
		iter++;
		for (;iter!=conditions.end();iter++)
		{
			s << "elseif";
			iter->first->print(s);
			s << "then\n";
			iter->second->print(s);
		}
		if (otherwise!=NULL)
		{
			s << "else\n";
			otherwise->print(s);
		}
		s << "end\n";
	};

	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		iterator iter;
		for (iter=conditions.begin();iter!=conditions.end();iter++)
		{
			iter->first->getFreeVariables(free,bound);
			iter->second->getFreeVariables(free,bound);
		}
		if (otherwise!=NULL) otherwise->getFreeVariables(free,bound);
	};

	virtual bool codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs)
	{
		iterator iter;
		for (iter=conditions.begin();iter!=conditions.end();iter++)
		{
			iter->first->collectFunctions(funcs);
			iter->second->collectFunctions(funcs);
		}
		if (otherwise!=NULL) otherwise->collectFunctions(funcs);
	};

	virtual bool isReturnLike(void) 
	{ 
		bool retval=true;
		iterator iter;
		for (iter=conditions.begin();iter!=conditions.end();iter++)
			retval&=iter->second->isReturnLike();
		if (retval && otherwise!=NULL) retval&=otherwise->isReturnLike();
		else retval=false; // no else that is return like: we can fall through!
		return retval; 
	};
	
	virtual size_t countVariableStmts(void)
	{
		size_t retval=0;
		iterator iter;
		for (iter=conditions.begin();iter!=conditions.end();iter++)
			retval+=iter->second->countVariableStmts();
		if (otherwise!=NULL) retval+=otherwise->countVariableStmts();;
		return retval;
	}

};

/// The case statement assigns statements to variant types.
class CaseStatement : public Statement
{
public:

	struct Case
	{
		std::wstring tag; ///< The tag of the variant wanted
		std::list<std::wstring> params; ///< The list of labels that are extracted from the variant
		BlockStatement *body;

		RecordType *myRecord;

		Case(void)
		{
			myRecord=NULL;
		};

		~Case()
		{
			if (myRecord!=NULL) delete myRecord;
			std::list<Statement*>::iterator iter;
			delete body;
		};

		bool makeType(VariantType *variant,Environment *env)
		{
			std::wstring q=std::wstring(L"?");
			myRecord=new RecordType;
			Environment local(env);
			for (std::list<std::wstring>::iterator iter=params.begin();iter!=params.end();iter++)
			{
				TypeVariable *tvar=local.fresh(q+*iter);
				local.put(*iter,tvar);
				myRecord->addMember(*iter,tvar);
			}
			variant->addTag(tag,myRecord);
			bool success=true;
			success&=body->makeType(&local);
			return success;
		};

		void print(std::wostream &s)
		{
			s << tag << " ";
			std::list<std::wstring>::iterator param_iter; 
			std::list<Statement*>::iterator body_iter;
			for (param_iter=params.begin();param_iter!=params.end();param_iter++)
				s << *param_iter << " ";
			s << "then\n";
			body->print(s);
		};

		void getFreeVariables(VariableSet &free,VariableSet &bound)
		{
			std::list<std::wstring>::iterator iter;
			VariableSet bound2(bound.begin(),bound.end());
			for (iter=params.begin();iter!=params.end();iter++)
			{
				bound2.insert(*iter);
			}
			body->getFreeVariables(free,bound2);
		};

		//virtual bool codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
		void collectFunctions(std::list<intro::Function*> &funcs)
		{
			std::list<Statement*>::iterator iter;
			body->collectFunctions(funcs);
		};
	};

	typedef std::list<Case*> caselist;
	typedef std::list<Case*>::iterator iterator;

private:

	Expression *caseof;
	caselist cases;
	//VariantType handled;

public:

	CaseStatement(int l,int p) : Statement(l,p), caseof(NULL)
	{};
	
	~CaseStatement()
	{
		delete caseof;
		iterator iter;
		for (iter=cases.begin();iter!=cases.end();iter++)
		{
			delete *iter;
		};
	};

	void setCaseOfExpr(Expression *expr) { caseof=expr; };

	void appendCase(Case *case_)
	{
		cases.push_back(case_);
	};

	virtual bool makeType(Environment *env)
	{
		VariantType *handled=new VariantType;
		iterator iter;
		bool success=true;
		for (iter=cases.begin();iter!=cases.end();iter++)
			(*iter)->makeType(handled,env);
		if (!success) return false;
		Type *variable=env->fresh(handled);
		Type *exprtype=caseof->getType(env);
		exprtype->unify(variable);
		return success;
	};

	virtual void print(std::wostream &s)
	{
		s << "case ";
		caseof->print(s);
		s << " of\n";
		for (iterator iter=cases.begin();iter!=cases.end();iter++)
		{
			if (iter!=cases.begin()) s << "| ";
			(*iter)->print(s);
			s << "\n";
		}
		s << "end";
	};

	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		iterator iter;
		for (iter=cases.begin();iter!=cases.end();iter++)
		{
			(*iter)->getFreeVariables(free,bound);
		}
	};

	virtual bool codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs)
	{
		iterator iter;
		for (iter=cases.begin();iter!=cases.end();iter++)
		{
			(*iter)->collectFunctions(funcs);
		}
	};

	virtual bool isReturnLike(void) 
	{ 
		bool retval=true;
		for (iterator iter=cases.begin();retval && iter!=cases.end();iter++)
			retval&=(*iter)->body->isReturnLike();
		return retval; 
	};
	
	virtual size_t countVariableStmts(void)
	{
		size_t retval=0;
		iterator iter;
		for (iter=cases.begin();iter!=cases.end();iter++)
		{
			retval+=(*iter)->params.size();
			retval+=(*iter)->body->countVariableStmts();
		}
		return retval;
	}
};

class LoopStatement : public Statement
{
protected:
	LoopStatement(int l,int p) 
		: Statement(l,p) 
		, body(nullptr)
	{};
	//std::list<Statement*> body;
	//typedef std::list<Statement*>::iterator iterator;
	BlockStatement *body;
public:
	inline void setBody(BlockStatement *b) { body=b; };

	virtual ~LoopStatement(void)
	{
		delete body;
	};

	//virtual void getFreeVariables(VariableSet &all,VariableSet &bound)
	//{
	//	std::list<Statement*>::iterator stmts;
	//	for (stmts=body.begin();stmts!=body.end();stmts++)
	//		(*stmts)->getFreeVariables(all,bound)
	//};

};

/// A for statments repeatedly executes a statement for al values generated by a generator statement.
class ForStatement : public LoopStatement
{
	GeneratorStatement *generators;
public:
	ForStatement(int l,int p) : LoopStatement(l,p)
	{};

	~ForStatement()
	{
		delete generators;
	};

	virtual bool makeType(Environment *env)
	{
		Environment localenv(env);
		if (!generators->makeType(&localenv))
			return false;
		return body->makeType(&localenv);
	};

	inline void setGenerators(GeneratorStatement *gen) { generators=gen; };

	virtual void getFreeVariables(VariableSet &all,VariableSet &bound)
	{
		generators->getFreeVariables(all,bound);
		body->getFreeVariables(all,bound);
	};

	virtual void print(std::wostream &s)
	{
		s << "for ";
		generators->print(s);
		s << " do\n";
		body->print(s);
		s << "\ndone";
	};

	virtual bool codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs)
	{
		generators->collectFunctions(funcs);
		body->collectFunctions(funcs);
	};
	
	virtual size_t countVariableStmts(void)
	{
		size_t retval=generators->countVariableStmts();
		retval+=body->countVariableStmts();
		return retval;
	}
};

/// A while statement repeats a block of code until the condition becomes false.
class WhileStatement : public LoopStatement
{
	Expression *condition;
public:
	WhileStatement(int l,int p) : LoopStatement(l,p), condition(NULL)
	{};

	~WhileStatement()
	{
		delete condition;
	};

	virtual bool makeType(Environment *env)
	{
		Type *t=condition->getType(env);
		if (t->find()->getKind()!=Type::Boolean) 
		{
			std::wcout << "Error (" << getLine() << "," << getPosition() <<"): condition in while statement must be boolean!\n";
			return false;
		}
		return body->makeType(env);
	};

	inline void setCondition(Expression *cond) { condition=cond; };

	virtual void print(std::wostream &s)
	{
		s << "while ";
		condition->print(s);
		s << " do\n";
		body->print(s);
		s << "\ndone";
	};

	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		condition->getFreeVariables(free,bound);
		body->getFreeVariables(free,bound);
	};
	virtual bool codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs)
	{
		condition->collectFunctions(funcs);
		body->collectFunctions(funcs);
		//std::list<Statement*>::iterator stmts;
		//for (stmts=body.begin();stmts!=body.end();stmts++)
		//	(*stmts)->collectFunctions(funcs);
	};
	
	virtual size_t countVariableStmts(void)
	{
		size_t retval=body->countVariableStmts();
		return retval;
	}

};

/// Return statements end a function. A value may be passed back.
class ReturnStatement  : public Statement
{
	Expression *expr;
	Type unit;

public:
	ReturnStatement(int l, int p, Expression *e) : Statement(l, p), expr(e), unit(Type::Unit)
	{};

	~ReturnStatement()
	{
		delete expr;
	};

	Expression *getExpression(void) { return expr; };

	virtual bool makeType(Environment *env)
	{
		Type *t;
		if (expr!=NULL) t=expr->getType(env);
		//else t=new Type(Type::Unit);
		else t = &unit;
		Type *r=env->get(L"!return");
		return t->unify(r);
	};

	virtual void print(std::wostream &s)
	{
		s << "return ";
		if (expr!=NULL) expr->print(s);
	};

	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		expr->getFreeVariables(free,bound);
	};
	virtual bool codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs)
	{
		expr->collectFunctions(funcs);
	};

	virtual bool isReturnLike(void) { return true; };

	virtual size_t countVariableStmts(void)
	{
		return 0;
	}

};

/// Yield statements pause the execution of a function, returning a value.
/** Generators are defined as functions using yield statements instead of return.
*/
class YieldStatement : public Statement
{
	Expression *expr;

	Type *myType;

public:
	YieldStatement(int l,int p,Expression *e) : Statement(l,p), expr(e), myType(NULL)
	{};

	~YieldStatement()
	{
		delete expr;
		delete myType;
	};

	Expression *getExpression(void) { return expr; };

	virtual bool makeType(Environment *env)
	{
		Type *r=env->get(L"!return");
//		std::wcout << L"\nGenerator source Type from env: ";
//		r->print(std::wcout);
		
		Type *t;
		if (expr==NULL) t=env->fresh();
		else t=expr->getType(env);
		myType=new Type(Type::Generator,t);
//		std::wcout << L"\nGenerator Type to unify against: ";
//		myType->print(std::wcout);
		if (!myType->unify(r))
		{
			return false;
		}
		return true;
	};

	virtual void print(std::wostream &s)
	{
		s << "yield ";
		if (expr!=NULL) expr->print(s);
		else s << "done";
	};

	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		if (expr!=nullptr) expr->getFreeVariables(free,bound);
	};

	virtual bool codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs)
	{
		if (expr!=nullptr) expr->collectFunctions(funcs);
	};

	virtual bool isReturnLike(void) 
	{ 
		//return true; // Return like if "yield done" only!
		return expr==NULL;
	};
	
	virtual size_t countVariableStmts(void)
	{
		return 0;
	}

};


/// Wrapper class to pretend an expression is a statement...
class ExpressionStatement  : public Statement
{
	Expression *expr;
	bool isInteractive;
public:
	ExpressionStatement(int l,int p,Expression *e,bool isInteractive_) : Statement(l,p), expr(e), isInteractive(isInteractive_)
	{};

	~ExpressionStatement()
	{
		delete expr;
	};

	Expression *getExpression(void) { return expr; };

	virtual bool makeType(Environment *env)
	{
		Type *t=expr->getType(env);
		return t->getKind()!=Type::Error;
	};

	virtual void print(std::wostream &s)
	{
		if (expr!=NULL) expr->print(s);
	};

	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		expr->getFreeVariables(free,bound);
	};

	virtual bool codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs)
	{
		expr->collectFunctions(funcs);
	};
	
	virtual size_t countVariableStmts(void)
	{
		return 0;
	}

};

/// Wrapper class to pretend an expression is a statement...
class ImportStatement  : public Statement
{
	bool relative;
	std::list<std::wstring> path;
	

public:
	ImportStatement(int l,int p,bool rel,std::list<std::wstring> &path) : Statement(l,p)
	{
		this->relative=rel;
		this->path=path;
	};

	~ImportStatement()
	{
	};

	virtual bool makeType(Environment *env)
	{
		// Copy exports of imported module indo current environment
		Environment::Module *module;
		if (relative) module=Environment::getCurrentModule()->followPath(path);
		else module=Environment::getRootModule()->followPath(path);
		if (module==NULL) 
		{
			// Error Message
			return false;
		}
		module->importInto(env);
		return true;
	};

	virtual void print(std::wostream &s)
	{
		s<<"import ";
		if (!relative) s<<"::";
		std::list<std::wstring>::iterator iter=path.begin();
		s<<*iter;
		for (iter++;iter!=path.end();iter++) s<<"::"<<*iter;

	};

	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		// bind names imported from module.
	};

	virtual bool codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs)
	{
	};
	
	virtual size_t countVariableStmts(void)
	{
		return 0;
	}

};
}
#endif