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

	virtual bool makeType(Environment *env, ErrorLocation *errors);

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

	virtual bool isTerminatorLike(void)
	{
		if (body.empty()) return false;
		return body.back()->isTerminatorLike();
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

	Type unit_type;

protected:
	virtual Type *makeType(Environment *env, ErrorLocation *errors);

	/// This method constructs the actual function body. Called by codeGen to have that decluttered.
	/** We pass in the free vars as an array in order to guarantee identical ordering with
		closure initialization.
	*/
	llvm::Function *buildFunction(CodeGenEnvironment * parent, const VariableSet & free);
public:
	Function(int l,int p): Expression(l,p), myType(NULL), unit_type(Type::Unit)
	{ parentvar=NULL; };

	~Function()
	{
		std::list<Statement*>::iterator sit;
		delete body;
		ParameterList::iterator pit;
		//for (pit=parameters.begin();pit!=parameters.end();pit++)
		//	deleteCopy(pit->type);
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
			bound2.insert(std::make_pair(iter->name,iter->type));
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
	}

	~ValueStatement()
	{
		delete value;
	}

	inline std::wstring getName(void) { return  name; }

	inline Expression *getValue(void) { return value; }

	virtual bool makeType(Environment *env, ErrorLocation *errors);

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
	}

	virtual bool codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);

	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		bound.insert(std::make_pair(name,value->getType()));
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

	virtual bool makeType(Environment *env, ErrorLocation *errors);

	virtual void print(std::wostream &s)
	{
		iterator iter=conditions.begin();
		s << "if ";
		iter->first->print(s);
		s << " then\n";
		iter->second->print(s);
		iter++;
		for (;iter!=conditions.end();iter++)
		{
			s << "elseif";
			iter->first->print(s);
			s << " then\n";
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
		size_t  line, col;

		RecordType *myRecord;

		Case(size_t l, size_t c)
			: line(l)
			, col(c)
		{
			myRecord=NULL;
		};

		~Case()
		{
			if (myRecord!=NULL) delete myRecord;
			delete body;
		};

		bool makeType(VariantType *input,Environment *env, ErrorLocation *errors);

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
				bound2.insert(std::make_pair(*iter,this->myRecord->findMember(*iter)->second));
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

	typedef std::vector<Case*> caselist;
	typedef std::vector<Case*>::iterator iterator;

private:

	Expression *caseof;
	caselist cases;
	VariantType *handled;

public:

	CaseStatement(int l,int p) : Statement(l,p), caseof(NULL), handled(nullptr)
	{};
	
	~CaseStatement()
	{
		delete caseof;
		if (handled != nullptr) delete handled;
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

	virtual bool makeType(Environment *env, ErrorLocation *errors);

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

	virtual bool makeType(Environment *env, ErrorLocation *errors);

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

	virtual bool makeType(Environment *env, ErrorLocation *errors);

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

/// A while statement repeats a block of code until the condition becomes false.
class FlowCtrlStatement : public Statement
{
public:
	enum FlowType
	{
		Continue,
		Break
	};

	FlowType myflow;
public:
	FlowCtrlStatement(int l, int p, FlowType flowtype) : Statement(l, p), myflow(flowtype)
	{};

	~FlowCtrlStatement()
	{
	};

	virtual bool makeType(Environment *env, ErrorLocation *)
	{
		return true;
	};

	virtual void print(std::wostream &s)
	{
		switch (myflow)
		{
		case Break: 
			s << "break";
			break;
		case Continue:
			s << "continue";
			break;
		}
	};
	virtual bool isTerminatorLike(void) { return true; };
	virtual void getFreeVariables(VariableSet &free, VariableSet &bound)
	{
	};
	virtual bool codeGen(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs)
	{
	};

	virtual size_t countVariableStmts(void)
	{
		return 0;
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

	virtual bool makeType(Environment *env, ErrorLocation *errors);

	virtual void print(std::wostream &s)
	{
		s << "return ";
		if (expr!=NULL) expr->print(s);
	};

	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		if (expr!=nullptr) expr->getFreeVariables(free,bound);
	};
	virtual bool codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs)
	{
		expr->collectFunctions(funcs);
	};

	virtual bool isTerminatorLike(void) { return true; };
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

	virtual bool makeType(Environment *env, ErrorLocation *errors);

	virtual void print(std::wostream &s)
	{
		s << L"yield ";
		if (expr!=NULL) expr->print(s);
		else s << L"done";
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

	virtual bool isTerminatorLike(void) { return expr == NULL; };

	virtual bool isReturnLike(void) 
	{ 
		//return true; // Return true if "yield done" only!
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

	virtual bool makeType(Environment *env, ErrorLocation *errors);

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
	
	void printPath(std::wostream &s)
	{
		if (!relative) s << L"::";
		std::list<std::wstring>::iterator iter = path.begin();
		s << *iter;
		for (iter++;iter != path.end();iter++) s << L"::" << *iter;
	};

public:
	ImportStatement(int l,int p,bool rel,std::list<std::wstring> &path) : Statement(l,p)
	{
		this->relative=rel;
		this->path=path;
	};

	~ImportStatement()
	{
	};

	virtual bool makeType(Environment *env, ErrorLocation *errors);

	virtual void print(std::wostream &s)
	{
		s << L"import ";
		printPath(s);
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

/// Return statements end a function. A value may be passed back.
class SourceStatement : public Statement
{
	std::wstring path;
	//std::list<intro::Statement*> statements;
public:
	SourceStatement(int l, int p, const std::wstring &path_) 
		: Statement(l, p)
		, path(path_.substr(1,path_.size()-2))
	{};

	~SourceStatement()
	{
	};

	virtual bool makeType(Environment *env, ErrorLocation *errors);

	virtual void print(std::wostream &s)
	{
		s << L"source \"" << path <<L"\"";
	};

	virtual void getFreeVariables(VariableSet &free, VariableSet &bound)
	{};
	virtual bool codeGen(llvm::IRBuilder<> &TmpB, CodeGenEnvironment *env);
	virtual void collectFunctions(std::list<intro::Function*> &funcs)
	{ };

	virtual size_t countVariableStmts(void)
	{
		return 0;
	}

};

}
#endif