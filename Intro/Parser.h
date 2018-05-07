

#if !defined(parse_COCO_PARSER_H__)
#define parse_COCO_PARSER_H__

#include <string>
#include <map>
#include <vector>
#include <stack>
#include <sstream>
#include <iostream>
#include "Expression.h"
#include "TypeExpression.h"
#include "Statement.h"
#include "Type.h"
#include "CodeGen.h"
#include "Environment.h"
#include "Module.h"


#include "Scanner.h"

namespace parse {


class Errors {
public:
	int count;			// number of errors detected

	Errors();
	void SynErr(int line, int col, int n);
	void Error(int line, int col, const wchar_t *s);
	void Warning(int line, int col, const wchar_t *s);
	void Warning(const wchar_t *s);
	void Exception(const wchar_t *s);

}; // Errors

class Parser {
private:
	enum {
		_EOF=0,
		_ident=1,
		_typevar=2,
		_integer=3,
		_real=4,
		_string=5,
		_assign=6
	};
	int maxT;

	Token *dummyToken;
	int errDist;
	int minErrDist;

	void SynErr(int n);
	void Get();
	void Expect(int n);
	bool StartOf(int s);
	void ExpectWeak(int n, int follow);
	bool WeakSeparator(int n, int syFol, int repFol);

public:
	Scanner *scanner;
	Errors  *errors;

	Token *t;			// last recognized token
	Token *la;			// lookahead token

std::vector<intro::Expression*> exprstack;
	/// Push an expression onto the expression stack (asserts non-NULL statement
	void pushExpr(intro::Expression *expr)
	{
		if (expr==NULL)
		{
			SemErr(L"NULL pushed on expression stack! Out of Memory?");
		};
		exprstack.push_back(expr);
	};
	/// Pop an expression from the expression stack (returns NULL if empty)
	intro::Expression *popExpr(void)
	{
		if (exprstack.empty())
			return NULL;
		intro::Expression *retval=exprstack.back();
		exprstack.pop_back();
		return retval;
	};
	/// Get the top expression from the stack without popping it (returns NULL if empty).
	intro::Expression *topExpr(void)
	{
		if (exprstack.empty())
			return NULL;
		return exprstack.back();
	};

	/// This vector is treated as a stack, passing statements between grammar rule functions.
	std::vector<intro::Statement*> statementstack;
	/// Push a statement onto the statement stack (returns NULL if empty).
	void pushStatement(intro::Statement *statement)
	{
		if (statement==NULL)
		{
			SemErr(L"NULL pushed on statement stack! Out of Memory?");
		};
		statementstack.push_back(statement);
	};
	/// Pop a statement from the statement stack (returns NULL if empty)
	intro::Statement *popStatement(void)
	{
		if (statementstack.empty())
			return NULL;
		intro::Statement *retval=statementstack.back();
		statementstack.pop_back();
		return retval;
	};
	/// Return the top statement from the statement stack without popping it (returns NULL if empty).
	intro::Statement *topStatement(void)
	{
		if (statementstack.empty())
			return NULL;
		return statementstack.back();
	};
	
	/// The Global environment (todo: move out of here to where evaluation heppens)
	intro::Environment global;
	/// Member granting access to environment
	inline intro::Environment *getEnv(void) {return &global; };
	/// List contiaining the ASTs of the parsed program.
	std::list<intro::Statement*> parseResult;
	/// Iterator Type for result list.
	typedef std::list<intro::Statement*>::iterator iterator;
	/// This stack remembers which function scope were in (for return and yield statements)
	std::stack<intro::Function*> functions;
	/// This stack remembers which loop scope were in (for break and continue statements)
	std::stack<intro::LoopStatement*> loops;
	/// Stack used for constructing types
	std::stack<intro::TypeExpression*> types;
	/// Stack used for constructing Modules
	std::stack<intro::ModuleStatement*> modules;
	/// This map is used during a type expression to assign the same variable to the same name - must be cleared after the type is complete
	std::map<std::wstring,intro::TypeVariable*> tvarmap;
	/// Get a type variable with the given name, create if not existant yet.
	intro::TypeVariable *getTypeVariable(const std::wstring &name)
	{
		std::map<std::wstring,intro::TypeVariable*>::iterator iter=tvarmap.find(name);
		if (iter==tvarmap.end())
		{
			iter=tvarmap.insert(make_pair(name,intro::Environment::fresh(name))).first;
		}
		return iter->second;
	};
	
	void clearErrors()
	{
		if (errors->count == 0) return;
		errors->count=0;
		while (!statementstack.empty())
		{
			delete statementstack.back();
			statementstack.pop_back();
		}
		std::list<intro::Statement*>::iterator iter;
		for (iter=parseResult.begin();iter!=parseResult.end();++iter)
			delete *iter;
		parseResult.clear();
	}
	/// The type variable map must be cleared before each type definition (module exports section only).
	void clearTypeVariables(void)
	{ tvarmap.clear(); };
	/// Indicates we are in interactive mode
	bool isInteractive;
	
	bool inferTypes(intro::Environment *env)
	{
		iterator iter;
		if (errors->count>0)
		{
			std::cout << "Found " << errors->count << " errors while parsing!\n";
			errors->count=0;
			return false;
		}
		intro::ErrorLocation *logger = new intro::ErrorLocation(0, 0, std::wstring(L"test"));
		for (iter=parseResult.begin();iter!=parseResult.end();iter++)
		{
			(*iter)->makeType(env,logger);
			(*iter)->print(std::wcout);
			std::wcout << std::endl << std::endl;
		}
		if (logger->hasErrors())
		{
			std::cout << "Type errors:\n";
			logger->print(std::wcout,0);
		}
		delete logger;
		return true;
	}

	void deleteStatements(void)
	{
		iterator iter;
		for (iter=parseResult.begin();iter!=parseResult.end();iter++)
			delete *iter; 
	}
	
	void executeStmt(intro::Statement *stmt)
	{
		if (errors->count>0)
		{
			std::wcout << L"Found " << errors->count << L" errors while parsing!\n";
			delete stmt;
			errors->count=0;
			return;
		}
		intro::ErrorLocation *logger = new intro::ErrorLocation(0, 0, std::wstring(L"interactive session"));
		
		if (stmt->makeType(getEnv(),logger)) 
		{
			intro::executeStatement(stmt);
			// remember statements for the types they own
			parseResult.push_back(stmt);
		}
		else
		{
			std::wcout << L"Type errors detected!\n";
			logger->print(std::wcout,0);
			delete stmt;
		}
		delete logger;
	}
	
	void Init(void)
	{
		isInteractive=false;
	}


	Parser(Scanner *scanner);
	~Parser();
	void SemErr(const wchar_t* msg);

	void Identifier(std::wstring &val);
	void Integer();
	void Boolean();
	void Real();
	void String();
	void Generator();
	void Expression();
	void GeneratorStatement();
	void ContainerElement(intro::Expression *&value, intro::Expression *&key,bool &isConstant);
	void Container();
	void RecordField(intro::RecordExpression *record);
	void Parameter(intro::Function::ParameterList &params);
	void Block();
	void Record();
	void Function();
	void Atom();
	void Expr7();
	void Expr6();
	void Expr5();
	void Expr4();
	void Expr3();
	void Expr2();
	void Expr1();
	void Expr0();
	void Variable();
	void Conditional();
	void ForIter();
	void WhileIter();
	void Return();
	void NonReturnStatement();
	void Scope();
	void Yield();
	void FlowControl();
	void Source();
	void Import();
	void CaseBranch(intro::CaseStatement *parent);
	void Case();
	void TypeVariable();
	void NonVariableType();
	void TypeList();
	void Type();
	void TypeGenerator();
	void TypeDictionary();
	void TypeRecord();
	void TypeVariantTag(intro::TypeVariantExpression *variant);
	void TypeVariant();
	void TypeOpaque();
	void TypeFunction();
	void ExportedName();
	void AbstractType();
	void Module();
	void Intro();

	void Parse();

}; // end Parser

} // namespace


#endif

