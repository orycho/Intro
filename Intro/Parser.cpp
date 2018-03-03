

#include "stdafx.h"
#include <wchar.h>
#include "Parser.h"
#include "Scanner.h"


namespace parse {


void Parser::SynErr(int n) {
	if (errDist >= minErrDist) errors->SynErr(la->line, la->col, n);
	errDist = 0;
}

void Parser::SemErr(const wchar_t* msg) {
	if (errDist >= minErrDist) errors->Error(t->line, t->col, msg);
	errDist = 0;
}

void Parser::Get() {
	for (;;) {
		t = la;
		la = scanner->Scan();
		if (la->kind <= maxT) { ++errDist; break; }

		if (dummyToken != t) {
			dummyToken->kind = t->kind;
			dummyToken->pos = t->pos;
			dummyToken->col = t->col;
			dummyToken->line = t->line;
			dummyToken->next = NULL;
			coco_string_delete(dummyToken->val);
			dummyToken->val = coco_string_create(t->val);
			t = dummyToken;
		}
		la = t;
	}
}

void Parser::Expect(int n) {
	if (la->kind==n) Get(); else { SynErr(n); }
}

void Parser::ExpectWeak(int n, int follow) {
	if (la->kind == n) Get();
	else {
		SynErr(n);
		while (!StartOf(follow)) Get();
	}
}

bool Parser::WeakSeparator(int n, int syFol, int repFol) {
	if (la->kind == n) {Get(); return true;}
	else if (StartOf(repFol)) {return false;}
	else {
		SynErr(n);
		while (!(StartOf(syFol) || StartOf(repFol) || StartOf(0))) {
			Get();
		}
		return StartOf(syFol);
	}
}

void Parser::Identifier(std::wstring &val) {
		Expect(_ident);
		val.assign(t->val); 
}

void Parser::Integer() {
		Expect(_integer);
		pushExpr(new intro::IntegerConstant(t->line,t->pos,t->val));
		
}

void Parser::Boolean() {
		if (la->kind == 7 /* "true" */) {
			Get();
			pushExpr(new intro::BooleanConstant(t->line,t->pos,true)); 
		} else if (la->kind == 8 /* "false" */) {
			Get();
			pushExpr(new intro::BooleanConstant(t->line,t->pos,false)); 
		} else SynErr(80);
}

void Parser::Real() {
		Expect(_real);
		pushExpr(new intro::RealConstant(t->line,t->pos,t->val));
		
}

void Parser::String() {
		Expect(_string);
		pushExpr(new intro::StringConstant(t->line,t->pos,t->val));
		
}

void Parser::Generator() {
		std::wstring ident;
		
		Identifier(ident);
		if (la->kind == 9 /* "in" */) {
			Get();
			Expression();
			pushStatement(new intro::ContainerGen(t->line,t->pos,ident,popExpr()));
			
		} else if (la->kind == 10 /* "from" */) {
			intro::Expression *from=NULL, *to=NULL, *by=NULL;
			
			Get();
			Expression();
			from=popExpr(); 
			Expect(11 /* "to" */);
			Expression();
			to=popExpr(); 
			if (la->kind == 12 /* "by" */) {
				Get();
				Expression();
				by=popExpr(); 
			}
			pushStatement(new intro::RangeGen(t->line,t->pos,ident,from,to,by));
			
		} else SynErr(81);
}

void Parser::Expression() {
		Expr0();
		if (la->kind == _assign) {
			intro::Expression *expr1=popExpr(); 
			Get();
			Expr0();
			intro::Expression *expr2=popExpr();
			pushExpr(new intro::Assignment(t->line,t->pos,expr1,expr2));
			
		}
}

void Parser::GeneratorStatement() {
		intro::GeneratorStatement *gen=new intro::GeneratorStatement(t->line,t->pos);
		
		Generator();
		gen->addGenerator((intro::Generator*) popStatement());
		
		while (la->kind == 13 /* "&&" */ || la->kind == 14 /* "??" */) {
			if (la->kind == 13 /* "&&" */) {
				Get();
				Generator();
				gen->addGenerator((intro::Generator*) popStatement());
				
			} else {
				Get();
				Expression();
				gen->addCondition(popExpr());
				
			}
		}
		pushStatement(gen);
		
}

void Parser::ContainerElement(intro::Expression *&value, intro::Expression *&key,bool &isConstant) {
		key=NULL; value=NULL; isConstant=false; 
		Expression();
		value=popExpr(); 
		if (la->kind == 15 /* "=>" */) {
			Get();
			isConstant=true; 
			Expression();
			key=value; 
			value=popExpr();
			
		}
}

void Parser::Container() {
		intro::Expression *value=NULL, *key=NULL;
		intro::GeneratorStatement *gen=NULL;
		std::list<std::pair<intro::Expression*,intro::Expression*> > dict_elem; 
		std::list<intro::Expression*> list_elem; 
		bool isDictionary=false;
		
		Expect(16 /* "{" */);
		if (StartOf(1)) {
			ContainerElement(value,key,isDictionary);
			if (isDictionary) 
			{
			if (key!=NULL)// SemErr(L"Expected dictionary entry as indicated by first entry in element list");
				dict_elem.push_back(std::make_pair(key,value));
			//else 
			}
			else list_elem.push_back(value);
			
			if (la->kind == 17 /* "," */ || la->kind == 19 /* "}" */) {
				while (la->kind == 17 /* "," */) {
					Get();
					ContainerElement(value,key,isDictionary);
					if (isDictionary) 
					{
					if (key==NULL) SemErr(L"Expected dictionary entry as indicated by first entry in element list");
					else dict_elem.push_back(std::make_pair(key,value));
					}
					else list_elem.push_back(value);
					
				}
			} else if (la->kind == 18 /* "|" */) {
				Get();
				GeneratorStatement();
				gen=dynamic_cast<intro::GeneratorStatement*>(popStatement());
				
			} else SynErr(82);
		} else if (la->kind == 15 /* "=>" */) {
			Get();
			isDictionary=true; 
		} else if (la->kind == 19 /* "}" */) {
		} else SynErr(83);
		Expect(19 /* "}" */);
		if (isDictionary)
		{
		intro::DictionaryConstant *dict=new intro::DictionaryConstant(t->line,t->pos,dict_elem);
		if (gen!=NULL) dict->setGenerators(gen);
		pushExpr(dict);
		}
		else
		{
		intro::ListConstant *list=new intro::ListConstant(t->line,t->pos,list_elem);
		if (gen!=NULL) list->setGenerators(gen);
		pushExpr(list);
		}
		
}

void Parser::RecordField(intro::RecordExpression *record) {
		std::wstring str; 
		
		Identifier(str);
		if (la->kind == _assign) {
			Get();
			Expression();
			intro::Expression *expr=popExpr();
			record->addMember(str,expr);
			
		} else if (la->kind == 20 /* "(" */) {
			Get();
			intro::Function *fun=new intro::Function(t->line,t->pos);
			
			if (StartOf(2)) {
				Parameter((*fun).getParameters());
				while (la->kind == 17 /* "," */) {
					Get();
					Parameter((*fun).getParameters());
				}
			}
			Expect(21 /* ")" */);
			Expect(22 /* "->" */);
			Block();
			fun->setBody((intro::BlockStatement*)popStatement()); 
			Expect(23 /* "end" */);
			record->addMember(str,fun);
			
		} else SynErr(84);
}

void Parser::Parameter(intro::Function::ParameterList &params) {
		std::wstring str; 
		intro::Variable::Modifier mod=intro::Variable::ReadOnly;
		
		if (la->kind == 9 /* "in" */ || la->kind == 47 /* "inout" */ || la->kind == 48 /* "out" */) {
			if (la->kind == 9 /* "in" */) {
				Get();
			} else if (la->kind == 47 /* "inout" */) {
				Get();
				mod=intro::Variable::Unrestricted; 
			} else {
				Get();
				mod=intro::Variable::WriteOnly; 
			}
		}
		Identifier(str);
		params.push_back(intro::Function::Parameter(str,NULL));
		
}

void Parser::Block() {
		intro::BlockStatement *bs=new intro::BlockStatement(t->line,t->pos);
		intro::Statement *stmt;
		
		if (StartOf(3)) {
			if (la->kind == 60 /* "return" */) {
				Return();
				Expect(26 /* ";" */);
				stmt=popStatement(); bs->appendBody(stmt); 
			} else {
				NonReturnStatement();
				Expect(26 /* ";" */);
				stmt=popStatement(); bs->appendBody(stmt); 
				while (StartOf(4)) {
					NonReturnStatement();
					Expect(26 /* ";" */);
					stmt=popStatement(); bs->appendBody(stmt); 
				}
				if (la->kind == 60 /* "return" */) {
					Return();
					Expect(26 /* ";" */);
					stmt=popStatement(); bs->appendBody(stmt); 
				}
			}
		}
		pushStatement(bs); 
}

void Parser::Record() {
		int l=t->line,p=t->pos;
		intro::RecordExpression *expr=new  intro::RecordExpression(t->line,t->pos);
		std::wstring tag; 
		
		Expect(24 /* "[" */);
		if (la->kind == 25 /* ":" */) {
			Get();
			Identifier(tag);
		}
		while (la->kind == _ident) {
			RecordField(expr);
			Expect(26 /* ";" */);
		}
		Expect(27 /* "]" */);
		if (!tag.empty())
		{
		intro::VariantExpression *variant=new intro::VariantExpression(l,p);
		variant->setVariant(tag,expr);
		pushExpr(variant);
		}
		else pushExpr(expr);
		
}

void Parser::Function() {
		Expect(28 /* "fun" */);
		Expect(20 /* "(" */);
		intro::Function *fun=new intro::Function(t->line,t->pos);
		functions.push(fun);
		
		if (StartOf(2)) {
			Parameter((*fun).getParameters());
			while (la->kind == 17 /* "," */) {
				Get();
				Parameter((*fun).getParameters());
			}
		}
		Expect(21 /* ")" */);
		Expect(22 /* "->" */);
		Block();
		fun->setBody((intro::BlockStatement*)popStatement()); 
		Expect(23 /* "end" */);
		functions.pop();
		pushExpr(fun);
		
}

void Parser::Atom() {
		std::wstring str; 
		switch (la->kind) {
		case 7 /* "true" */: case 8 /* "false" */: {
			Boolean();
			break;
		}
		case _integer: {
			Integer();
			break;
		}
		case _real: {
			Real();
			break;
		}
		case _string: {
			String();
			break;
		}
		case _ident: case 29 /* "::" */: {
			bool rel=true; std::list<std::wstring> path; 
			if (la->kind == 29 /* "::" */) {
				Get();
				rel=false; 
			}
			Identifier(str);
			path.push_back(str); 
			while (la->kind == 29 /* "::" */) {
				Get();
				Identifier(str);
				path.push_back(str); 
			}
			str=path.back();
			path.pop_back();
			
			{
			pushExpr(new intro::Variable(t->line,t->pos,rel,str,path));
			}
			
			break;
		}
		case 20 /* "(" */: {
			Get();
			Expression();
			Expect(21 /* ")" */);
			break;
		}
		case 16 /* "{" */: {
			Container();
			break;
		}
		case 24 /* "[" */: {
			Record();
			break;
		}
		case 28 /* "fun" */: {
			Function();
			break;
		}
		default: SynErr(85); break;
		}
}

void Parser::Expr7() {
		std::wstring str; 
		intro::Expression *expr;
		bool isSplice=false;
		
		Atom();
		while (la->kind == 30 /* "." */) {
			Get();
			Identifier(str);
			expr=popExpr();
			intro::RecordAccess *ra=new intro::RecordAccess(t->line,t->pos,expr,str);
			pushExpr(ra);
			
		}
		while (la->kind == 31 /* "\\" */) {
			expr=popExpr(); // The dictionary on the lhs
			
			Get();
			Atom();
			intro::Expression *keyexpr=popExpr();
			intro::DictionaryErase *de=new intro::DictionaryErase(t->line,t->pos,expr,keyexpr);
			pushExpr(de);
			
		}
		if (la->kind == 24 /* "[" */) {
			Get();
			expr=popExpr();
			intro::Expression *op,*op2=NULL;
			
			Expression();
			op=popExpr();
			
			if (la->kind == 25 /* ":" */) {
				Get();
				Expression();
				op2=popExpr();
				
				isSplice=true; 
			}
			intro::Expression *result;
			if (isSplice) result=new intro::Splice(t->line,t->pos,expr,op,op2);
			else result=new intro::Extraction(t->line,t->pos,expr,op);
			pushExpr(result);
			
			Expect(27 /* "]" */);
		}
}

void Parser::Expr6() {
		bool negate=false; 
		if (la->kind == 32 /* "-" */) {
			Get();
			negate=true; 
		}
		Expr7();
		while (la->kind == 20 /* "(" */) {
			Get();
			intro::Application *app=new intro::Application(t->line,t->pos);
			app->setFunction(popExpr());
			
			if (StartOf(1)) {
				Expression();
				app->appendParam(popExpr()); 
				while (la->kind == 17 /* "," */) {
					Get();
					Expression();
					app->appendParam(popExpr()); 
				}
			}
			Expect(21 /* ")" */);
			pushExpr(app);
			
		}
		if (negate) 
		{
		intro::Expression *expr=popExpr(); 
		pushExpr(new intro::UnaryOperation(t->line,t->pos,intro::UnaryOperation::Negate,expr));
		}
		
}

void Parser::Expr5() {
		Expr6();
		while (la->kind == 33 /* "*" */ || la->kind == 34 /* "/" */ || la->kind == 35 /* "%" */) {
			intro::ArithmeticBinary::OpType op=intro::ArithmeticBinary::Mul;
			intro::Expression *expr1=popExpr(); 
			
			if (la->kind == 33 /* "*" */) {
				Get();
				op=intro::ArithmeticBinary::Mul; 
			} else if (la->kind == 34 /* "/" */) {
				Get();
				op=intro::ArithmeticBinary::Div; 
			} else {
				Get();
				op=intro::ArithmeticBinary::Mod; 
			}
			Expr6();
			intro::Expression *expr2=popExpr();
			pushExpr(new intro::ArithmeticBinary(t->line,t->pos,op,expr1,expr2));
			
		}
}

void Parser::Expr4() {
		Expr5();
		while (la->kind == 32 /* "-" */ || la->kind == 36 /* "+" */) {
			intro::ArithmeticBinary::OpType op=intro::ArithmeticBinary::Add;
			intro::Expression *expr1=popExpr(); 
			
			if (la->kind == 36 /* "+" */) {
				Get();
				op=intro::ArithmeticBinary::Add; 
			} else {
				Get();
				op=intro::ArithmeticBinary::Sub; 
			}
			Expr5();
			intro::Expression *expr2=popExpr();
			pushExpr(new intro::ArithmeticBinary(t->line,t->pos,op,expr1,expr2));
			
		}
}

void Parser::Expr3() {
		Expr4();
		while (StartOf(5)) {
			intro::Expression *expr1=popExpr(); 
			intro::CompareOperation::OpType op=intro::CompareOperation::Equal; 
			
			switch (la->kind) {
			case 37 /* ">=" */: {
				Get();
				op=intro::CompareOperation::GreaterEqual; 
				break;
			}
			case 38 /* "<=" */: {
				Get();
				op=intro::CompareOperation::LessEqual; 
				break;
			}
			case 39 /* ">" */: {
				Get();
				op=intro::CompareOperation::Greater; 
				break;
			}
			case 40 /* "<" */: {
				Get();
				op=intro::CompareOperation::Less; 
				break;
			}
			case 41 /* "!=" */: {
				Get();
				op=intro::CompareOperation::Different; 
				break;
			}
			case 42 /* "==" */: {
				Get();
				op=intro::CompareOperation::Equal; 
				break;
			}
			}
			Expr4();
			intro::Expression *expr2=popExpr();
			pushExpr(new intro::CompareOperation(t->line,t->pos,op,expr1,expr2));
			
		}
}

void Parser::Expr2() {
		bool negate=false; 
		if (la->kind == 43 /* "not" */) {
			Get();
			negate=true; 
		}
		Expr3();
		if (negate) 
		{
		intro::Expression *expr=popExpr(); 
		pushExpr(new intro::UnaryOperation(t->line,t->pos,intro::UnaryOperation::Not,expr));
		}
		
}

void Parser::Expr1() {
		Expr2();
		while (la->kind == 44 /* "and" */) {
			intro::Expression *expr1=popExpr(); 
			Get();
			Expr2();
			intro::Expression *expr2=popExpr();
			pushExpr(new intro::BooleanBinary(t->line,t->pos,intro::BooleanBinary::And,expr1,expr2));
			
		}
}

void Parser::Expr0() {
		intro::BooleanBinary::OpType op; 
		Expr1();
		while (la->kind == 45 /* "or" */ || la->kind == 46 /* "xor" */) {
			intro::Expression *expr1=popExpr(); 
			if (la->kind == 45 /* "or" */) {
				Get();
				op=intro::BooleanBinary::Or; 
			} else {
				Get();
				op=intro::BooleanBinary::Xor; 
			}
			Expr1();
			intro::Expression *expr2=popExpr();
			pushExpr(new intro::BooleanBinary(t->line,t->pos,op,expr1,expr2));
			
		}
}

void Parser::Variable() {
		std::wstring str; bool isconst=false;
		
		if (la->kind == 49 /* "var" */) {
			Get();
		} else if (la->kind == 50 /* "const" */) {
			Get();
			isconst=true; 
		} else SynErr(86);
		Identifier(str);
		
		if (la->kind == _assign) {
			Get();
			Expression();
			intro::Expression *expr=popExpr();
			pushStatement(new intro::ValueStatement(t->line,t->pos,str,expr,isconst,isInteractive));
			
		} else if (la->kind == 20 /* "(" */) {
			Get();
			intro::Function *fun=new intro::Function(t->line,t->pos);
			functions.push(fun);
			
			if (StartOf(2)) {
				Parameter((*fun).getParameters());
				while (la->kind == 17 /* "," */) {
					Get();
					Parameter((*fun).getParameters());
				}
			}
			Expect(21 /* ")" */);
			Expect(22 /* "->" */);
			Block();
			fun->setBody((intro::BlockStatement*)popStatement()); 
			Expect(23 /* "end" */);
			functions.pop();
			pushStatement(new intro::ValueStatement(t->line,t->pos,str,fun,isconst,isInteractive));
			
		} else SynErr(87);
}

void Parser::Conditional() {
		intro::Expression *cond;
		intro::Statement *stmt;
		intro::IfStatement *cs = new intro::IfStatement(t->line,t->pos);
		
		Expect(51 /* "if" */);
		Expression();
		cond=popExpr(); 
		Expect(52 /* "then" */);
		Block();
		stmt=popStatement(); cs->appendCondition(cond,stmt); 
		while (la->kind == 53 /* "elsif" */) {
			Get();
			Expression();
			cond=popExpr(); 
			Expect(52 /* "then" */);
			Block();
			stmt=popStatement(); cs->appendCondition(cond,stmt); 
		}
		if (la->kind == 54 /* "else" */) {
			Get();
			Block();
			stmt=popStatement(); cs->setOtherwise(stmt); 
		}
		Expect(23 /* "end" */);
		pushStatement(cs);
		
}

void Parser::ForIter() {
		intro::ForStatement *fs = new intro::ForStatement(t->line,t->pos);
		loops.push(fs);
		
		Expect(55 /* "for" */);
		GeneratorStatement();
		fs->setGenerators((intro::GeneratorStatement*)popStatement()); 
		Expect(56 /* "do" */);
		Block();
		fs->setBody((intro::BlockStatement*)popStatement()); 
		Expect(57 /* "done" */);
		loops.pop();
		pushStatement(fs);
		
}

void Parser::WhileIter() {
		intro::WhileStatement *ws = new intro::WhileStatement(t->line,t->pos);
		loops.push(ws);
		
		Expect(58 /* "while" */);
		Expression();
		ws->setCondition(popExpr()); 
		Expect(56 /* "do" */);
		Block();
		ws->setBody((intro::BlockStatement*)popStatement()); 
		Expect(57 /* "done" */);
		loops.pop();
		pushStatement(ws);
		
}

void Parser::Return() {
		Expect(60 /* "return" */);
		intro::Expression *expr=NULL;
		if (functions.empty()) SemErr(L"Found 'return' statement outside of function body!");
		
		if (StartOf(1)) {
			Expression();
			expr=popExpr(); 
		}
		pushStatement(new intro::ReturnStatement(t->line,t->pos,expr));
		
}

void Parser::NonReturnStatement() {
		switch (la->kind) {
		case 49 /* "var" */: case 50 /* "const" */: {
			Variable();
			break;
		}
		case 51 /* "if" */: {
			Conditional();
			break;
		}
		case 59 /* "begin" */: {
			Scope();
			break;
		}
		case 55 /* "for" */: {
			ForIter();
			break;
		}
		case 58 /* "while" */: {
			WhileIter();
			break;
		}
		case 61 /* "yield" */: {
			Yield();
			break;
		}
		case 62 /* "break" */: case 63 /* "continue" */: {
			FlowControl();
			break;
		}
		case 64 /* "source" */: {
			Source();
			break;
		}
		case 65 /* "import" */: {
			Import();
			break;
		}
		case 66 /* "case" */: {
			Case();
			break;
		}
		case _ident: case _integer: case _real: case _string: case 7 /* "true" */: case 8 /* "false" */: case 16 /* "{" */: case 20 /* "(" */: case 24 /* "[" */: case 28 /* "fun" */: case 29 /* "::" */: case 32 /* "-" */: case 43 /* "not" */: {
			Expression();
			intro::Statement *stmt;
			intro::Expression *expr;
			expr=popExpr(); 
			stmt=new intro::ExpressionStatement(t->line,t->pos,expr,isInteractive); 
			pushStatement(stmt); 
			
			break;
		}
		default: SynErr(88); break;
		}
}

void Parser::Scope() {
		Expect(59 /* "begin" */);
		Block();
		Expect(23 /* "end" */);
}

void Parser::Yield() {
		Expect(61 /* "yield" */);
		intro::Expression *expr=NULL;
		if (functions.empty()) SemErr(L"Found 'return' statement outside of function body.");
		
		if (StartOf(1)) {
			Expression();
			expr=popExpr(); 
		} else if (la->kind == 57 /* "done" */) {
			Get();
		} else SynErr(89);
		pushStatement(new intro::YieldStatement(t->line,t->pos,expr));
		
}

void Parser::FlowControl() {
		bool isContinue=false;
		
		if (la->kind == 62 /* "break" */) {
			Get();
		} else if (la->kind == 63 /* "continue" */) {
			Get();
			isContinue=true; 
		} else SynErr(90);
		if (loops.empty()) 
		{
		if (isContinue) SemErr(L"Found 'continue' statement outside of loop body.");
		else SemErr(L"Found 'break' statement outside of loop body.");
		}
		pushStatement(new intro::FlowCtrlStatement(t->line,t->pos,isContinue
		? intro::FlowCtrlStatement::Continue
		: intro::FlowCtrlStatement::Break));
		
}

void Parser::Source() {
		Expect(64 /* "source" */);
		Expect(_string);
		pushStatement(new intro::SourceStatement(t->line,t->pos,t->val));
		
}

void Parser::Import() {
		bool rel=true; 
		std::list<std::wstring> path; 
		//std::wstring val;
		
		
		Expect(65 /* "import" */);
		if (la->kind == 29 /* "::" */) {
			Get();
			rel=false; 
		}
		Expect(_ident);
		path.push_back(t->val); /*val.assign(t->val);*/ 
		while (la->kind == 29 /* "::" */) {
			Get();
			Expect(_ident);
			path.push_back(t->val); 
		}
		pushStatement(new intro::ImportStatement(t->line,t->pos,rel,path));
		
}

void Parser::CaseBranch(intro::CaseStatement *parent) {
		intro::CaseStatement::Case *branch= new intro::CaseStatement::Case();
		std::wstring str;
		
		Identifier(str);
		branch->tag=str;
		
		while (la->kind == _ident) {
			Identifier(str);
			branch->params.push_back(str);
			
		}
		Expect(52 /* "then" */);
		Block();
		branch->body=(intro::BlockStatement*)popStatement();
		//branch->body.push_back(popStatement());
		
		parent->appendCase(branch);
		
}

void Parser::Case() {
		intro::CaseStatement *casestmt=new intro::CaseStatement(t->line,t->pos);
		
		Expect(66 /* "case" */);
		Expression();
		casestmt->setCaseOfExpr(popExpr());
		
		Expect(67 /* "of" */);
		CaseBranch(casestmt);
		while (la->kind == 18 /* "|" */) {
			Get();
			CaseBranch(casestmt);
		}
		Expect(23 /* "end" */);
		pushStatement(casestmt);
		
}

void Parser::TypeVariable() {
		Expect(_typevar);
		types.push(new intro::TypeVariableExpression(t->line,t->pos,t->val));
		
}

void Parser::TypeList() {
		std::wstring str; intro::TypeExpression *buf; 
		Expect(68 /* "List" */);
		Expect(20 /* "(" */);
		Type();
		Expect(21 /* ")" */);
		buf=types.top();
		types.pop();
		types.push(new intro::TypeListExpression(t->line,t->pos,buf));
		
}

void Parser::Type() {
		std::wstring str; 
		switch (la->kind) {
		case 71 /* "Boolean" */: {
			Get();
			types.push(new intro::TypeSimpleExpression(t->line,t->pos,intro::Type::Boolean)); 
			break;
		}
		case 72 /* "Integer" */: {
			Get();
			types.push(new intro::TypeSimpleExpression(t->line,t->pos,intro::Type::Integer)); 
			break;
		}
		case 73 /* "Real" */: {
			Get();
			types.push(new intro::TypeSimpleExpression(t->line,t->pos,intro::Type::Real)); 
			break;
		}
		case 74 /* "String" */: {
			Get();
			types.push(new intro::TypeSimpleExpression(t->line,t->pos,intro::Type::String)); 
			break;
		}
		case 75 /* "Unit" */: {
			Get();
			types.push(new intro::TypeSimpleExpression(t->line,t->pos,intro::Type::Unit)); 
			break;
		}
		case 68 /* "List" */: {
			TypeList();
			break;
		}
		case 69 /* "Generator" */: {
			TypeGenerator();
			break;
		}
		case 70 /* "Dictionary" */: {
			TypeDictionary();
			break;
		}
		case 24 /* "[" */: {
			TypeRecord();
			break;
		}
		case _ident: {
			TypeOpaque();
			break;
		}
		case _typevar: {
			TypeVariable();
			break;
		}
		case 20 /* "(" */: {
			TypeFunction();
			break;
		}
		default: SynErr(91); break;
		}
}

void Parser::TypeGenerator() {
		std::wstring str; intro::TypeExpression *buf; 
		Expect(69 /* "Generator" */);
		Expect(20 /* "(" */);
		Type();
		Expect(21 /* ")" */);
		buf=types.top();
		types.pop();
		types.push(new intro::TypeGeneratorExpression(t->line,t->pos,buf));
		
}

void Parser::TypeDictionary() {
		std::wstring str; intro::TypeExpression *buf; 
		Expect(70 /* "Dictionary" */);
		Expect(20 /* "(" */);
		Type();
		buf=types.top();
		types.pop();
		
		Expect(17 /* "," */);
		Type();
		Expect(21 /* ")" */);
		intro::TypeExpression *buf2=types.top();
		types.pop();
		types.push(new intro::TypeDictionaryExpression(t->line,t->pos,buf,buf2));
		
}

void Parser::TypeRecord() {
		std::wstring str; intro::TypeExpression *buf; 
		Expect(24 /* "[" */);
		intro::TypeRecordExpression *rec=new intro::TypeRecordExpression(t->line,t->pos);
		
		while (la->kind == _ident) {
			Identifier(str);
			Expect(25 /* ":" */);
			Type();
			Expect(26 /* ";" */);
			buf=types.top();
			types.pop();
			rec->addMember(str,buf);
			
		}
		Expect(27 /* "]" */);
		types.push(rec); 
}

void Parser::TypeOpaque() {
		std::wstring str; 
		Identifier(str);
		intro::TypeOpaqueExpression *opaque=new intro::TypeOpaqueExpression(t->line,t->pos,str);
		
		if (la->kind == 20 /* "(" */) {
			Get();
			Type();
			opaque->addParameter(types.top()); types.pop(); 
			while (la->kind == 17 /* "," */) {
				Get();
				Type();
				opaque->addParameter(types.top()); types.pop(); 
			}
			Expect(21 /* ")" */);
		}
		types.push(opaque);
		
}

void Parser::TypeFunction() {
		std::wstring str;
		intro::TypeFunctionExpression *fun=new intro::TypeFunctionExpression(t->line,t->pos);
		
		Expect(20 /* "(" */);
		if (StartOf(6)) {
			Type();
			fun->addParameter(types.top()); types.pop(); 
			while (la->kind == 17 /* "," */) {
				Get();
				Type();
				fun->addParameter(types.top()); types.pop(); 
			}
		}
		Expect(21 /* ")" */);
		Expect(22 /* "->" */);
		Type();
		fun->setReturnType(types.top()); 
		types.pop(); 
		types.push(fun); 
}

void Parser::ExportedName() {
		std::wstring str;
		
		Identifier(str);
		int line=t->line,pos=t->pos; 
		Expect(25 /* ":" */);
		Type();
		intro::TypeExpression *buf=types.top();
		types.pop();
		modules.top()->addExport(line,pos,str,buf);
		
}

void Parser::AbstractType() {
		std::wstring str,name;
		std::list<intro::TypeExpression*> parameters;
		
		Expect(76 /* "type" */);
		Identifier(name);
		int line=t->line,pos=t->pos;
		
		if (la->kind == 20 /* "(" */) {
			Get();
			TypeVariable();
			parameters.push_back(types.top());
			types.pop();
			
			while (la->kind == 17 /* "," */) {
				Get();
				TypeVariable();
				parameters.push_back(types.top());
				types.pop();
				
			}
			Expect(21 /* ")" */);
		}
		modules.top()->addOpaque(line,pos,name,parameters);
		
}

void Parser::Module() {
		std::wstring name;
		bool rel=true; 
		std::list<std::wstring> path; 
		
		Expect(77 /* "module" */);
		if (la->kind == 29 /* "::" */) {
			Get();
			rel=false; 
		}
		Expect(_ident);
		path.push_back(t->val); 
		while (la->kind == 29 /* "::" */) {
			Get();
			Expect(_ident);
			path.push_back(t->val); 
		}
		intro::ModuleStatement *module=new intro::ModuleStatement(t->line,t->pos,rel,path);
		modules.push(module);
		
		Expect(78 /* "exports" */);
		while (la->kind == _ident || la->kind == 76 /* "type" */) {
			clearTypeVariables(); 
			if (la->kind == _ident) {
				ExportedName();
			} else {
				AbstractType();
			}
			Expect(26 /* ";" */);
		}
		Expect(10 /* "from" */);
		while (StartOf(7)) {
			if (StartOf(4)) {
				NonReturnStatement();
				intro::Statement *s=popStatement();
				module->addContent(s);
				
				Expect(26 /* ";" */);
			} else {
				Module();
				Expect(30 /* "." */);
				module->addModule(modules.top());
				modules.pop();
				
			}
		}
		Expect(23 /* "end" */);
}

void Parser::Intro() {
		exprstack.reserve(30); 
		statementstack.reserve(30); 
		
		if (StartOf(4)) {
			NonReturnStatement();
			intro::Statement *s=popStatement();
			if (s!=NULL)
			{
			// if interactive, execute statement, otherwise queue. 
			if (isInteractive)
				executeStmt(s);
			else parseResult.push_back(s);			
			}
			
			Expect(26 /* ";" */);
		} else if (la->kind == 77 /* "module" */) {
			Module();
			intro::ModuleStatement *mod=modules.top(); modules.pop();
			//if (mod->makeType(getEnv()))
			{
			if (isInteractive)
			{
				// process parse result
				executeStmt(mod);
			}
			else parseResult.push_back(mod);
			}
			/*else
			{
			printf("Error in Module definition!");
			}
			*/
			
			Expect(30 /* "." */);
		} else SynErr(92);
		while (StartOf(7)) {
			if (StartOf(4)) {
				NonReturnStatement();
				intro::Statement *s=popStatement();
				if (s!=NULL)
				{
				// if interactive, execute statement, otherwise queue. 
				if (isInteractive)
					executeStmt(s);
				else parseResult.push_back(s);
				}
				
				Expect(26 /* ";" */);
			} else {
				Module();
				intro::ModuleStatement *mod=modules.top(); modules.pop();
				//if (mod->makeType(getEnv()))
				{
				if (isInteractive)
				{
					// process parse result
					executeStmt(mod);
				}
				else parseResult.push_back(mod);
				}
				/*else
				{
				printf("Error in Module definition!");
				}*/
				
				Expect(30 /* "." */);
			}
		}
}




// If the user declared a method Init and a mehtod Destroy they should
// be called in the contructur and the destructor respctively.
//
// The following templates are used to recognize if the user declared
// the methods Init and Destroy.

template<typename T>
struct ParserInitExistsRecognizer {
	template<typename U, void (U::*)() = &U::Init>
	struct ExistsIfInitIsDefinedMarker{};

	struct InitIsMissingType {
		char dummy1;
	};
	
	struct InitExistsType {
		char dummy1; char dummy2;
	};

	// exists always
	template<typename U>
	static InitIsMissingType is_here(...);

	// exist only if ExistsIfInitIsDefinedMarker is defined
	template<typename U>
	static InitExistsType is_here(ExistsIfInitIsDefinedMarker<U>*);

	enum { InitExists = (sizeof(is_here<T>(NULL)) == sizeof(InitExistsType)) };
};

template<typename T>
struct ParserDestroyExistsRecognizer {
	template<typename U, void (U::*)() = &U::Destroy>
	struct ExistsIfDestroyIsDefinedMarker{};

	struct DestroyIsMissingType {
		char dummy1;
	};
	
	struct DestroyExistsType {
		char dummy1; char dummy2;
	};

	// exists always
	template<typename U>
	static DestroyIsMissingType is_here(...);

	// exist only if ExistsIfDestroyIsDefinedMarker is defined
	template<typename U>
	static DestroyExistsType is_here(ExistsIfDestroyIsDefinedMarker<U>*);

	enum { DestroyExists = (sizeof(is_here<T>(NULL)) == sizeof(DestroyExistsType)) };
};

// The folloing templates are used to call the Init and Destroy methods if they exist.

// Generic case of the ParserInitCaller, gets used if the Init method is missing
template<typename T, bool = ParserInitExistsRecognizer<T>::InitExists>
struct ParserInitCaller {
	static void CallInit(T *t) {
		// nothing to do
	}
};

// True case of the ParserInitCaller, gets used if the Init method exists
template<typename T>
struct ParserInitCaller<T, true> {
	static void CallInit(T *t) {
		t->Init();
	}
};

// Generic case of the ParserDestroyCaller, gets used if the Destroy method is missing
template<typename T, bool = ParserDestroyExistsRecognizer<T>::DestroyExists>
struct ParserDestroyCaller {
	static void CallDestroy(T *t) {
		// nothing to do
	}
};

// True case of the ParserDestroyCaller, gets used if the Destroy method exists
template<typename T>
struct ParserDestroyCaller<T, true> {
	static void CallDestroy(T *t) {
		t->Destroy();
	}
};

void Parser::Parse() {
	t = NULL;
	la = dummyToken = new Token();
	la->val = coco_string_create(L"Dummy Token");
	Get();
	Intro();
	Expect(0);
}

Parser::Parser(Scanner *scanner) {
	maxT = 79;

	ParserInitCaller<Parser>::CallInit(this);
	dummyToken = NULL;
	t = la = NULL;
	minErrDist = 2;
	errDist = minErrDist;
	this->scanner = scanner;
	errors = new Errors();
}

bool Parser::StartOf(int s) {
	const bool T = true;
	const bool x = false;

	static bool set[8][81] = {
		{T,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x},
		{x,T,x,T, T,T,x,T, T,x,x,x, x,x,x,x, T,x,x,x, T,x,x,x, T,x,x,x, T,T,x,x, T,x,x,x, x,x,x,x, x,x,x,T, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x},
		{x,T,x,x, x,x,x,x, x,T,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,T, T,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x},
		{x,T,x,T, T,T,x,T, T,x,x,x, x,x,x,x, T,x,x,x, T,x,x,x, T,x,x,x, T,T,x,x, T,x,x,x, x,x,x,x, x,x,x,T, x,x,x,x, x,T,T,T, x,x,x,T, x,x,T,T, T,T,T,T, T,T,T,x, x,x,x,x, x,x,x,x, x,x,x,x, x},
		{x,T,x,T, T,T,x,T, T,x,x,x, x,x,x,x, T,x,x,x, T,x,x,x, T,x,x,x, T,T,x,x, T,x,x,x, x,x,x,x, x,x,x,T, x,x,x,x, x,T,T,T, x,x,x,T, x,x,T,T, x,T,T,T, T,T,T,x, x,x,x,x, x,x,x,x, x,x,x,x, x},
		{x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,T,T,T, T,T,T,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x},
		{x,T,T,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, T,x,x,x, T,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, T,T,T,T, T,T,T,T, x,x,x,x, x},
		{x,T,x,T, T,T,x,T, T,x,x,x, x,x,x,x, T,x,x,x, T,x,x,x, T,x,x,x, T,T,x,x, T,x,x,x, x,x,x,x, x,x,x,T, x,x,x,x, x,T,T,T, x,x,x,T, x,x,T,T, x,T,T,T, T,T,T,x, x,x,x,x, x,x,x,x, x,T,x,x, x}
	};



	return set[s][la->kind];
}

Parser::~Parser() {
	ParserDestroyCaller<Parser>::CallDestroy(this);
	delete errors;
	delete dummyToken;
}

Errors::Errors() {
	count = 0;
}

void Errors::SynErr(int line, int col, int n) {
	wchar_t* s;
	switch (n) {
			case 0: s = coco_string_create(L"EOF expected"); break;
			case 1: s = coco_string_create(L"ident expected"); break;
			case 2: s = coco_string_create(L"typevar expected"); break;
			case 3: s = coco_string_create(L"integer expected"); break;
			case 4: s = coco_string_create(L"real expected"); break;
			case 5: s = coco_string_create(L"string expected"); break;
			case 6: s = coco_string_create(L"assign expected"); break;
			case 7: s = coco_string_create(L"\"true\" expected"); break;
			case 8: s = coco_string_create(L"\"false\" expected"); break;
			case 9: s = coco_string_create(L"\"in\" expected"); break;
			case 10: s = coco_string_create(L"\"from\" expected"); break;
			case 11: s = coco_string_create(L"\"to\" expected"); break;
			case 12: s = coco_string_create(L"\"by\" expected"); break;
			case 13: s = coco_string_create(L"\"&&\" expected"); break;
			case 14: s = coco_string_create(L"\"??\" expected"); break;
			case 15: s = coco_string_create(L"\"=>\" expected"); break;
			case 16: s = coco_string_create(L"\"{\" expected"); break;
			case 17: s = coco_string_create(L"\",\" expected"); break;
			case 18: s = coco_string_create(L"\"|\" expected"); break;
			case 19: s = coco_string_create(L"\"}\" expected"); break;
			case 20: s = coco_string_create(L"\"(\" expected"); break;
			case 21: s = coco_string_create(L"\")\" expected"); break;
			case 22: s = coco_string_create(L"\"->\" expected"); break;
			case 23: s = coco_string_create(L"\"end\" expected"); break;
			case 24: s = coco_string_create(L"\"[\" expected"); break;
			case 25: s = coco_string_create(L"\":\" expected"); break;
			case 26: s = coco_string_create(L"\";\" expected"); break;
			case 27: s = coco_string_create(L"\"]\" expected"); break;
			case 28: s = coco_string_create(L"\"fun\" expected"); break;
			case 29: s = coco_string_create(L"\"::\" expected"); break;
			case 30: s = coco_string_create(L"\".\" expected"); break;
			case 31: s = coco_string_create(L"\"\\\\\" expected"); break;
			case 32: s = coco_string_create(L"\"-\" expected"); break;
			case 33: s = coco_string_create(L"\"*\" expected"); break;
			case 34: s = coco_string_create(L"\"/\" expected"); break;
			case 35: s = coco_string_create(L"\"%\" expected"); break;
			case 36: s = coco_string_create(L"\"+\" expected"); break;
			case 37: s = coco_string_create(L"\">=\" expected"); break;
			case 38: s = coco_string_create(L"\"<=\" expected"); break;
			case 39: s = coco_string_create(L"\">\" expected"); break;
			case 40: s = coco_string_create(L"\"<\" expected"); break;
			case 41: s = coco_string_create(L"\"!=\" expected"); break;
			case 42: s = coco_string_create(L"\"==\" expected"); break;
			case 43: s = coco_string_create(L"\"not\" expected"); break;
			case 44: s = coco_string_create(L"\"and\" expected"); break;
			case 45: s = coco_string_create(L"\"or\" expected"); break;
			case 46: s = coco_string_create(L"\"xor\" expected"); break;
			case 47: s = coco_string_create(L"\"inout\" expected"); break;
			case 48: s = coco_string_create(L"\"out\" expected"); break;
			case 49: s = coco_string_create(L"\"var\" expected"); break;
			case 50: s = coco_string_create(L"\"const\" expected"); break;
			case 51: s = coco_string_create(L"\"if\" expected"); break;
			case 52: s = coco_string_create(L"\"then\" expected"); break;
			case 53: s = coco_string_create(L"\"elsif\" expected"); break;
			case 54: s = coco_string_create(L"\"else\" expected"); break;
			case 55: s = coco_string_create(L"\"for\" expected"); break;
			case 56: s = coco_string_create(L"\"do\" expected"); break;
			case 57: s = coco_string_create(L"\"done\" expected"); break;
			case 58: s = coco_string_create(L"\"while\" expected"); break;
			case 59: s = coco_string_create(L"\"begin\" expected"); break;
			case 60: s = coco_string_create(L"\"return\" expected"); break;
			case 61: s = coco_string_create(L"\"yield\" expected"); break;
			case 62: s = coco_string_create(L"\"break\" expected"); break;
			case 63: s = coco_string_create(L"\"continue\" expected"); break;
			case 64: s = coco_string_create(L"\"source\" expected"); break;
			case 65: s = coco_string_create(L"\"import\" expected"); break;
			case 66: s = coco_string_create(L"\"case\" expected"); break;
			case 67: s = coco_string_create(L"\"of\" expected"); break;
			case 68: s = coco_string_create(L"\"List\" expected"); break;
			case 69: s = coco_string_create(L"\"Generator\" expected"); break;
			case 70: s = coco_string_create(L"\"Dictionary\" expected"); break;
			case 71: s = coco_string_create(L"\"Boolean\" expected"); break;
			case 72: s = coco_string_create(L"\"Integer\" expected"); break;
			case 73: s = coco_string_create(L"\"Real\" expected"); break;
			case 74: s = coco_string_create(L"\"String\" expected"); break;
			case 75: s = coco_string_create(L"\"Unit\" expected"); break;
			case 76: s = coco_string_create(L"\"type\" expected"); break;
			case 77: s = coco_string_create(L"\"module\" expected"); break;
			case 78: s = coco_string_create(L"\"exports\" expected"); break;
			case 79: s = coco_string_create(L"??? expected"); break;
			case 80: s = coco_string_create(L"invalid Boolean"); break;
			case 81: s = coco_string_create(L"invalid Generator"); break;
			case 82: s = coco_string_create(L"invalid Container"); break;
			case 83: s = coco_string_create(L"invalid Container"); break;
			case 84: s = coco_string_create(L"invalid RecordField"); break;
			case 85: s = coco_string_create(L"invalid Atom"); break;
			case 86: s = coco_string_create(L"invalid Variable"); break;
			case 87: s = coco_string_create(L"invalid Variable"); break;
			case 88: s = coco_string_create(L"invalid NonReturnStatement"); break;
			case 89: s = coco_string_create(L"invalid Yield"); break;
			case 90: s = coco_string_create(L"invalid FlowControl"); break;
			case 91: s = coco_string_create(L"invalid Type"); break;
			case 92: s = coco_string_create(L"invalid Intro"); break;

		default:
		{
			wchar_t format[20];
			coco_swprintf(format, 20, L"error %d", n);
			s = coco_string_create(format);
		}
		break;
	}
	wprintf(L"-- line %d col %d: %ls\n", line, col, s);
	coco_string_delete(s);
	count++;
}

void Errors::Error(int line, int col, const wchar_t *s) {
	wprintf(L"-- line %d col %d: %ls\n", line, col, s);
	count++;
}

void Errors::Warning(int line, int col, const wchar_t *s) {
	wprintf(L"-- line %d col %d: %ls\n", line, col, s);
}

void Errors::Warning(const wchar_t *s) {
	wprintf(L"%ls\n", s);
}

void Errors::Exception(const wchar_t* s) {
	wprintf(L"%ls", s); 
	exit(1);
}

} // namespace

