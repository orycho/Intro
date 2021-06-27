#include "stdafx.h"
#include "Statement.h"
#include "Parser.h"
#include "CodeGenEnvironment.h"
#include <unordered_map>

namespace intro
{
	bool BlockStatement::makeType(Environment *env, ErrorLocation *errors)
	{
		Environment localenv(env);
		bool success = true;
		ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), L"block scope");
		std::vector<Statement *>::iterator stmts;
		for (stmts = body.begin(); stmts != body.end(); stmts++)
			success &= (*stmts)->makeType(&localenv, logger);
		if (success) delete logger;
		else errors->addError(logger);
		return success;
	}


	Type::pointer_t Function::makeType(Environment *env, ErrorLocation *errors)
	{
		Environment localenv(env);
		// Add return and paramter types to local environment
		//Type *rettype=localenv.put(L"!return");
		// TODO@ENV: Handle return type
		ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), L"function definition");
		if (body->isReturnLike())
			localenv.put(L"!return");
		else
			localenv.put(L"!return", Type::pointer_t(new Type(Type::Unit)));
		ParameterList::iterator iter;
		for (iter = parameters.begin(); iter != parameters.end(); iter++)
		{
			iter->type = localenv.put(iter->name);
			iter->type->setAccessFlags(Type::Readable);
		}
		if (!body->makeType(&localenv, logger))
		{
			errors->addError(logger);
			return getError(L"Error in function body");
		}
		else delete logger;
		std::vector<Type::pointer_t> p;
		for (iter = parameters.begin(); iter != parameters.end(); iter++)
		{
			p.push_back(iter->type);
		}

		myType = Type::pointer_t(new FunctionType(p, localenv.get(L"!return")));
		if (((FunctionType *)myType.get())->getReturnType()->getKind() != Type::Unit
			&& !body->isReturnLike())
			errors->addError(new ErrorDescription(getLine(), getColumn(), L"Function returns a value but does not end in a return-like statement!"));
		//return getError(L"Function returning a value does not end on return-like satement!");

		return myType;
	}

	bool ValueStatement::makeType(Environment *env, ErrorLocation *errors)
	{
		// This part needs to account for preexisting name in case
		// it is created through exporting from a module (must make sure of that, hence clumsy?!)

		// IMPORTANT: When doing this, the variable name is usable in the body,
		// but since it is not a variable, it cannot be unified there.
		// That is desirable for the outer environment, but not the inner,
		// so we should probabl split that...?
		Type::pointer_t t = env->put(name);
		if (t == nullptr)
		{
			errors->addError(new ErrorDescription(getLine(), getColumn(), std::wstring(L"The variable name'") + name + L"' is already in use!"));
			//std::wcout << L"The identifier \"" << name.c_str() << "\" already names a variable in this scope!\n";
			return false;
		}
		// @TODO: When adding constants in the future, do it here...
		ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"definition of variable '") + name + L"'");
		if (!constant) t->setAccessFlags(Type::Readable | Type::Writable);
		Type::pointer_t et = value->getType(env, logger);
		et->setAccessFlags(t->getAccessFlags());
		if (et->getKind() == Type::Error)
		{
			errors->addError(logger);
			return false;
		}
		else delete logger;
		if (!et->unify(t))
		{
			std::wstringstream strs;
			strs << L"The identifier \"" << name.c_str() << "\" could not be assigned a type. Could not unifiy\n\t";
			t->find()->print(strs);
			strs << "\n\twith\n\t";
			et->find()->print(strs);
			strs << "\n";
			errors->addError(new ErrorDescription(getLine(), getColumn(), strs.str()));
			// Remove name from environment instead?
			env->remove(name);
			//env->put(name,new ErrorType(getLine(),getColumn(),std::wstring(L"Type Error Occured defining ")+name+L", check definition."));
			return false;
		}
		return true;
	}

	bool IfStatement::makeType(Environment *env, ErrorLocation *errors)
	{
		iterator iter;
		//int cond_count = 0;
		ErrorLocation *logger;
		for (iter = conditions.begin(); iter != conditions.end(); iter++)
		{
			logger = new ErrorLocation(getLine(), getColumn(), L"if statement (condition)");
			Type::pointer_t cond_type = iter->first->getType(env, logger);
			if (cond_type->getKind() == Type::Error)
			{
				errors->addError(new ErrorDescription(getLine(), getColumn(), L"Could not determine type of condition expression"));
				return false;
			}
			else delete logger;

			Environment localenv(env);
			if (!iter->first->getType()->unify(Type::pointer_t(new Type(Type::Boolean))))
			{
				std::wstringstream strs;
				strs << L"The type of the condition expression is not Boolean but ";
				iter->first->getType()->print(strs);
				strs << L"!";
				errors->addError(new ErrorDescription(getLine(), getColumn(), strs.str()));
				return false;
			}
			ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), L"if statement (body)");
			if (!iter->second->makeType(&localenv, logger))
			{
				errors->addError(logger);
				return false;
			}
			else delete logger;
		}
		if (otherwise != NULL)
		{
			Environment localenv(env);
			logger = new ErrorLocation(getLine(), getColumn(), L"if statement (else block)");
			if (otherwise->makeType(&localenv, logger)) delete logger;
			else
			{
				errors->addError(logger);
				return false;
			}
		}
		return true;
	}

	bool CaseStatement::Case::makeType(Type::pointer_t variant, Environment *env, ErrorLocation *errors)
	{
		Environment local(env);
		VariantType::iterator iter = ((VariantType *)variant.get())->findTag(tag);
		myRecord = Type::pointer_t(new RecordType);
		if (iter == ((VariantType *)variant.get())->endTag())
		{
			std::wstring q = std::wstring(L"?");
			for (std::vector<std::wstring>::iterator pit = params.begin(); pit != params.end(); pit++)
			{
				Type::pointer_t tvar = local.fresh(q + *pit);
				local.put(*pit, tvar);
				((RecordType *)myRecord.get())->addMember(*pit, tvar);
			}
			((VariantType *)variant.get())->addTag(tag, myRecord);
		}
		else
		{
			RecordType::member_iter members;
			for (members = ((RecordType *)iter->second.get())->begin(); members != ((RecordType *)iter->second.get())->end(); ++members)
			{
				((RecordType *)myRecord.get())->addMember(members->first, members->second);
				local.put(members->first, members->second);
			}
		}
		ErrorLocation *logger = new ErrorLocation(line, col, std::wstring(L"case for tag '") + tag + L"'");
		if (body->makeType(&local, logger))
			delete logger;
		else
		{
			errors->addError(logger);
			return false;
		}
		return true;
	}

	bool CaseStatement::makeType(Environment *env, ErrorLocation *errors)
	{
		// Inferring type here means it is not assumed inside the case?!
		Type::pointer_t exprtype = caseof->getType(env, errors);
		if (exprtype->getKind() == Type::Error)
			return false;
		else if (exprtype->getKind() == Type::Variable)
		{
			// For type variables, introduced b a parameter of the enclosing function
			// (Only way to introduce type vars) we need to communicate the
			// Expected types for each arm to the outside world

			handled = Type::pointer_t(new VariantType);
			bool success = true;
			ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), L"case statement");
			for (iterator iter = cases.begin(); iter != cases.end(); iter++)
				success &= (*iter)->makeType(handled, env, logger);
			if (success) delete logger;
			else
			{
				errors->addError(logger);
				return false;
			}
			Type::pointer_t variable = env->fresh(handled);
			if (!exprtype->unify(variable, exprtype->getKind() == Type::Variable))
			{
				std::wstringstream strs;
				strs << L"case statement expects a value with type\n";
				variable->find()->print(strs);
				strs << "\n\tbut the value has type\n\t";
				exprtype->find()->print(strs);
				strs << "instead\n";
				errors->addError(new ErrorDescription(getLine(), getColumn(), strs.str()));
				return false;
			}

			variable = variable->find();
			exprtype = exprtype->find();
		}
		else if (exprtype->getKind() == Type::Variant)
		{
			VariantType *input = dynamic_cast<VariantType *>(exprtype.get());
			bool success = true;
			if (input->size() > cases.size())
			{
				std::wstringstream strs;
				strs << L"case statement was passed a value with type\n";
				input->find()->print(strs);
				strs << "\n\tbut not all tags are handled.\n";
				errors->addError(new ErrorDescription(getLine(), getColumn(), strs.str()));
				return false;
			}
			ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), L"case statement");
			for (iterator iter = cases.begin(); iter != cases.end(); iter++)
				success &= (*iter)->makeType(exprtype, env, logger);
			if (success) delete logger;
			else
			{
				errors->addError(logger);
				return false;
			}

		}
		else
		{
			errors->addError(new ErrorDescription(getLine(), getColumn(), L"Unexpected input type to case"));
			return false;
		}
		return true;
	}

	bool ForStatement::makeType(Environment *env, ErrorLocation *errors)
	{
		Environment localenv(env);
		ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"for statement (generators)"));
		if (generators->makeType(&localenv, logger))
			delete logger;
		else
		{
			errors->addError(logger);
			return false;
		}
		logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"for statement (body)"));
		if (body->makeType(&localenv, logger))
			delete logger;
		else
		{
			errors->addError(logger);
			return false;
		}
		return true;
	}

	bool WhileStatement::makeType(Environment *env, ErrorLocation *errors)
	{
		Type::pointer_t t = condition->getType(env, errors);
		if (t->find()->getKind() != Type::Boolean)
		{
			errors->addError(new ErrorDescription(getLine(), getColumn(), L"while statement requires boolean condition expression"));
			return false;
		}
		ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"whilse statement (body)"));
		if (body->makeType(env, logger))
			delete logger;
		else
		{
			errors->addError(logger);
			return false;
		}
		return true;
	}

	bool ReturnStatement::makeType(Environment *env, ErrorLocation *errors)
	{
		Type::pointer_t t;
		if (expr != NULL)
		{
			ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"return value expression"));
			t = expr->getType(env, logger);
			if (t->getKind() == Type::Error)
			{
				errors->addError(logger);
				return false;
			}
			delete logger;
		}
		else t = Type::pointer_t(new Type(Type::Unit));
		Type::pointer_t r = env->get(L"!return");
		// Return is contravariant
		if (!t->unify(r, true))
			//if (!t->unify(r))
		{
			std::wstringstream strs;
			strs << L"the return type of the function has already been inferred to be\n";
			r->find()->print(strs);
			strs << "\n\tbut this expression evauates to\n\t";
			t->find()->print(strs);
			strs << "instead\n";
			errors->addError(new ErrorDescription(getLine(), getColumn(), strs.str()));
			return false;
		}
		return true;
	}

	bool YieldStatement::makeType(Environment *env, ErrorLocation *errors)
	{
		Type::pointer_t r = env->get(L"!return");
		if (r->getKind() != Type::Variable && r->getKind() != Type::Generator)
		{
			if (r->getKind() != Type::Error)
				errors->addError(new ErrorDescription(getLine(), getColumn(), L"Unexpected return type for yield statement, mixing in return somewhere? "));
			return false;
		}
		//		std::wcout << L"\nGenerator source Type from env: ";
		//		r->print(std::wcout);

		Type::pointer_t yieldType;
		if (expr == NULL) yieldType = env->fresh();
		else
		{
			ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"yielded value expression"));
			yieldType = expr->getType(env, logger);
			if (yieldType->getKind() == Type::Error)
			{
				errors->addError(logger);
				return false;
			}
			delete logger;
		}
		// "yield done" does not provide additional information
		// But in case that the generator is empty, it is all there is.
		// An empty generator can be concatenated with any other, so top is ok supertype?!
		// But that could lead to an empty list with element type top...
		// that means we cannot do much with those elements... but is it a problem?!
		myType = Type::pointer_t(new Type(Type::Generator, yieldType));
		// What is a fact is that variant types can cause trouble
		// when checking supertypes, because yield needs to specialize, just like return.
		// But two unrelated variant types will cause the two generators
		// to not be supertype-related, so unification of such generators
		// will fail, while the correct action is actually to extend the variant with the new tag(s)
		// Hence, we pick out the parameter if we can.
		bool success = false;
		if (r->getKind() == Type::Generator)
			success = yieldType->unify(r->getFirstParameter(), true);
		else
			success = myType->unify(r, true);
		if (!success)
		{
			std::wstringstream strs;
			strs << L"the yielded type of the generator has already been inferred to be\n";
			r->find()->print(strs);
			strs << "\n\tbut this expression evaluates to\n\t";
			myType->find()->print(strs);
			strs << "instead\n";
			errors->addError(new ErrorDescription(getLine(), getColumn(), strs.str()));
			return false;
		}
		return true;
	}

	bool ExpressionStatement::makeType(Environment *env, ErrorLocation *errors)
	{
		Type::pointer_t t = expr->getType(env, errors);
		return t->getKind() != Type::Error;
	}

	bool ImportStatement::makeType(Environment *env, ErrorLocation *errors)
	{
		// Copy exports of imported module indo current environment
		Environment::Module *module;
		if (relative) module = Environment::getCurrentModule()->followPath(path);
		else module = Environment::getRootModule()->followPath(path);
		if (module == NULL)
		{
			std::wstringstream strs;
			strs << L"The module '";
			printPath(strs);
			strs << "' is not known here! Was the file sourced?";
			errors->addError(new ErrorDescription(getLine(), getColumn(), strs.str()));
			return false;
		}
		module->importInto(env);
		return true;
	}

	struct SourceFile
	{
		enum State
		{
			Loading,
			Done
		};

		std::wstring path; // move to hashmap key
		/// The statements that where parsed from the file
		std::list<intro::Statement *> statements;
		/// The environment used for type inference of the statements
		intro::Environment env;
		/** The CGEnv containing any identifiers the file did not put in modules.
			Allocate this later, as we don't have the parent CGEnv when creating this
		*/
		intro::CodeGenEnvironment *cgenv;
		State state;
		llvm::Module *current;

		SourceFile()
			: cgenv(nullptr)
			, state(Loading)
			, current(nullptr)
		{}

		~SourceFile()
		{
			delete cgenv;
			for (auto stmt : statements)
				delete stmt;
		}
	};

	static std::unordered_map<std::wstring, SourceFile *> files;

	void cleanupSourceFiles()
	{
		for (auto file : files)
		{
			delete file.second;
		}
	}

	/* TODO:
		change behavior to store env created locally when successfully compiled.
		Storage by file name, so multiple source-ing does not cause
		the file to be loaded multiple times.
		instead just import the contents into the current scope
		when sourced multiple times.
		Also adapt codegen appropriately, generate the code only
		if not already stored as CodeGenEnv.
	*/
	bool SourceStatement::makeType(Environment *env, ErrorLocation *errors)
	{
		std::unordered_map<std::wstring, SourceFile *>::iterator iter = files.find(path);
		if (iter == files.end())
		{
			parse::Scanner scanner(path.c_str());
			if (!scanner.isOk())
			{
				//std::wcout << L"Error: Could not load file '"
				//	<< path << "'!\n";
				errors->addError(new ErrorDescription(getLine(), getColumn(), std::wstring(L"Failed to load file")));
				return false;
			}

			SourceFile *file = new SourceFile();
			iter = files.insert(std::make_pair(path, file)).first;
			// make sure we can find files in progress

			parse::Parser parser(&scanner);
			parser.isInteractive = false;
			parser.Parse();

			if (parser.errors->count > 0)
			{
				std::wcout << L"Found " << parser.errors->count << L" errors while parsing file "
					<< path << "!\n";
				delete file;
				files.erase(iter->first);
				return false;
			}
			file->statements.insert(file->statements.end(), parser.parseResult.begin(), parser.parseResult.end());
			parser.parseResult.clear();
			bool isOK = true;
			ErrorLocation *logger = new ErrorLocation(0, 0, std::wstring(L"source file ") + path);
			for (auto iter = file->statements.begin(); isOK && iter != file->statements.end(); iter++)
			{
				isOK &= (*iter)->makeType(&(file->env), logger);
			}
			if (logger->hasErrors())
			{
				//std::wcout << L"Type errors encountered while parsing file "
				//	<< path << "!\n";
				errors->addError(logger);
				files.erase(iter->first);
				delete file;
				return false;
			}
			else
			{
				file->state = SourceFile::Done;
			}
			delete logger;
		}
		else if (iter->second->state != SourceFile::Done)
		{
			std::wstringstream out;
			out << L"Circular dependency for file '"
				<< path << "'(it tries to source itself, possibly indirectly)! \n";
			errors->addError(new ErrorDescription(getLine(), getColumn(), out.str()));
			return false;

		}
		if (!iter->second->env.importInto(env))
		{
			std::wcout << L"Error while importing symbols from file '"
				<< path << "': a symbol defined in it already exists! \n";
			return false;
		}
		return true;
	}

	extern std::unique_ptr<llvm::Module> TheModule;

	bool SourceStatement::codeGen(llvm::IRBuilder<> &TmpB, intro::CodeGenEnvironment *env)
	{
		bool isOK = true;
		std::unordered_map<std::wstring, SourceFile *>::iterator iter = files.find(path);
		if (iter->second->cgenv == nullptr)
		{
			intro::CodeGenEnvironment *cgenv = new intro::CodeGenEnvironment(nullptr, intro::CodeGenEnvironment::GlobalScope);
			iter->second->cgenv = cgenv;
			for (auto inner = iter->second->statements.begin()
				; isOK && inner != iter->second->statements.end()
				; inner++)
				isOK &= (*inner)->codeGen(TmpB, cgenv);
			//TheModule->dump();
		}
		CodeGenEnvironment::iterator eit;
		if (iter->second->current != TheModule.get())
		{
			iter->second->cgenv->addExternalsForGlobals();
			iter->second->current = TheModule.get();
		}
		for (eit = iter->second->cgenv->begin(); eit != iter->second->cgenv->end(); eit++)
		{
			//std::wcout << "Found import: " << eit->first << "!\n";
			env->importElement(eit->first, eit->second);
		}
		return isOK;
	}


}