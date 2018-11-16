#include "stdafx.h"
#include "Expression.h"
#include "Statement.h"
#include "CodeGenEnvironment.h"

using namespace std;

namespace intro {

	const std::wregex tokens(
		L"("
		L"\\\\(?:u[[:xdigit:]]{4}|x[[:xdigit:]]+|[[:graph:]])"
		L"|[[:space:]]+"
		L"|\\$\\{[a-zA-Z_][a-zA-Z0-9_]*\\}"
		L"|\\\\\\$"
		L"|[^\\\\\\$\\\"]+"
		L")");
//		L"|[^\\\\$]+"

	Type::pointer_t Expression::getType(CodeGenEnvironment *env)
	{
		if (type==NULL) return NULL; // Nothing to see here
		//if (type->find()->getKind()!=Type::Variable) return type->find(); // give what we got
		//return env->getSpecialization(type->find()); // Variables get mapped using env
		return type->find();
	}

	/////////////////////////////////////////////////////////////////////
	// Expression Type Inference methods - note several expressions are specializations of application
	//		and thus only need to define the function type describing them (i.e. operators).
	
	Type::pointer_t IntegerConstant::makeType(Environment *, ErrorLocation *)
	{
		return std::make_shared<Type>(Type::Integer);
	}

	Type::pointer_t BooleanConstant::makeType(Environment *, ErrorLocation *)
	{
		return std::make_shared<Type>(Type::Boolean);
	}

	Type::pointer_t RealConstant::makeType(Environment *, ErrorLocation *)
	{
		return std::make_shared<Type>(Type::Real);
	}
	
	Type::pointer_t StringConstant::makeType(Environment *env, ErrorLocation *errors)
	{
		// Trim quotation marks read from source from string
		std::wstring trimmed=value.substr(1,value.size()-2);
		// Prepare tokenization of string based on regular expressions (regex does efficint preprocessing here...)
		std::wsregex_token_iterator i(trimmed.begin(),trimmed.end(), tokens, 1);
		std::wsregex_token_iterator eot; // represents end of token stream
		std::wstring curbuf; // Collect a sequence of consecutive tokens that are not interpolations (constant)
		while(i != eot)
		{
			if (i->str().size()==1)
			{
				curbuf.append(i->str());
			} 
			else switch(i->str()[0])
			{
				case L'\\': // Escapes
					switch(i->str()[1])
					{
					case L'n': curbuf.append(L"\n"); break;
					case L'r': curbuf.append(L"\r"); break;
					case L't': curbuf.append(L"\t"); break;
					case L'0': curbuf.append(L"\0"); break;
					default: curbuf.append(i->str().substr(1)); // If it has no special meaning, copy as is (e.g. backslash and dollar sign).
					}
					
					break;
				case L'$': // Interpolation (neither single $, nor escaped...)
					if (curbuf.size()!=0)
					{
						parts.push_back(part(curbuf, Type::pointer_t()));
						curbuf.clear();
					}
					{
						// Extract variable name, and the variable
						std::wstring varname=i->str().substr(2,i->str().size()-3);
						Type::pointer_t vartype=env->get(varname);
						if (vartype == NULL)
						{
							std::wstring msg = L"Variable '" + varname + L"' not found for string interpolation.";
							errors->addError(new ErrorDescription(getLine(), getColumn(), msg));
							return getError(msg);
						}
						//parts.push_back(part(varname,vartype->copy()));
						parts.push_back(part(varname, vartype));
						// convert to string
						// store intermediate string in env for removal
					}
					break;
				default: // Just Text...
					curbuf.append(i->str());
			}
			i++;
		}
		if (curbuf.size()!=0)
		{
			parts.push_back(part(curbuf, Type::pointer_t()));
			curbuf.clear();
		};


		return std::make_shared<Type>(Type::String);
		//return new Type(Type::Boolean);
	}

	Type::pointer_t ListConstant::makeType(Environment *env, ErrorLocation *errors)
	{
		Type::pointer_t param=NULL;
		if (elements.empty()) param=Environment::fresh();
		else if (generators==NULL)
		{
			std::vector<intro::Expression*>::iterator iter=elements.begin();
			if (iter==elements.end())
			{
				param=Environment::fresh();
			} 
			else
			{
				ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"list of constants"));
				param=(*iter)->getType(env,logger)->find();
				if (param->getKind() == Type::Error)
				{
					errors->addError(logger);
					return getError(L"error in list element.");
				}
				iter++;
				// check that all other expressions in the element list are subtypes of the first one.
				// Note: in the implementation, extra information will be list
				for (;iter!=elements.end();iter++)
				{
					Type::pointer_t ct = (*iter)->getType(env,logger);
					if (ct->getKind() == Type::Error)
					{
						errors->addError(logger);
						return getError(L"error in list element.");
					}
					//PartialOrder po=param->checkSubtype(ct);
					//if (po!=LESS && po!=EQUAL ) 
					if (!param->unify(ct))
					{
						errors->addError(new ErrorDescription(getLine(), getColumn(), L"Subtyping mismatch in list elements: all items must be subtypes or of the first item."));
						return getError(L"type mismatch in list constants");
					}
				}
				delete logger;
			}
		}
		else
		{
			if (elements.size() == 0)
			{
				errors->addError(new ErrorDescription(getLine(), getColumn(), L"List definition using generators require an expression."));
				return getError(L"List definition using generators require an expression.");
			}
			else if (elements.size() > 1)
			{
				errors->addError(new ErrorDescription(getLine(), getColumn(), L"List definition using generators expects one unique expression."));
				return getError(L"List definition using generators expects one unique expression.");
			}
			Environment localenv(env);
			ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"list generators"));
			if (generators->makeType(&localenv, logger))
				delete logger;
			else
			{
				errors->addError(logger);
				return getError(L"Error in list generators.");
			}
			logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"list element expression"));
			param=elements.front()->getType(&localenv,logger);
			if (param->getKind()!=Type::Error)
				delete logger;
			else
			{
				errors->addError(logger);
				return getError(L"Error in list element expression.");
			}
		}
		return std::make_shared<Type>(Type::List, param);
	}

	Type::pointer_t DictionaryConstant::makeType(Environment *env, ErrorLocation *errors)
	{
		Type::pointer_t tkey;
		Type::pointer_t tval;
		if (elements.empty()) 
		{
			tkey=Environment::fresh();
			tval=Environment::fresh();
		}
		else if (generators==NULL)
		{
			memberlist::iterator iter=elements.begin();
			ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"dictionary element key"));
			tkey = iter->first->getType(env,logger)->find();
			if (tkey->getKind() == Type::Error)
			{
				errors->addError(logger);
				return getError(L"error in dictionary element key.");
			}
			else delete logger;
			logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"dictionary element value"));
			tval=iter->second->getType(env,logger)->find();
			if (tval->getKind() == Type::Error)
			{
				errors->addError(logger);
				return getError(L"error in dictionary element value.");
			}
			else delete logger;
			iter++;
			// check that all other expressions in the element list are subtypes of the first one.
			// Note: in the implementation, extra information will be list
			for (;iter!=elements.end();iter++)
			{
				ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"dictionary element key"));
				Type::pointer_t ct = iter->first->getType(env,logger);
				if (ct->getKind() == Type::Error)
				{
					errors->addError(logger);
					return getError(L"error in dictionary element key.");
				}
				else delete logger;
				//PartialOrder po=tkey->checkSubtype(ct);
				//if (po!=LESS && po!=EQUAL )
				if (!tkey->unify(ct))
				{
					errors->addError(new ErrorDescription(getLine(), getColumn(), L"Subtyping mismatch in dictionary key: all keys must be subtypes of the first element's key."));
					return getError(L"Subtyping mismatch in dictionary key: all keys must be subtypes or of the first item.");
				}

				logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"dictionary element key"));
				ct = iter->second->getType(env, logger);
				if (ct->getKind() == Type::Error)
				{
					errors->addError(logger);
					return getError(L"error in dictionary element key.");
				}
				else delete logger;
				//po=tval->checkSubtype(ct);
				//if (po!=LESS && po!=EQUAL)
				// We will generalize the dictionary values, so that they
				// can be different and we accept the topmost supertype only.
				if (!tval->unify(ct,true))
				{
					errors->addError(new ErrorDescription(getLine(), getColumn(), L"Subtyping mismatch in dictionary value: all vauesmust be subtypes or of the first element's value."));
					return getError(L"Subtyping mismatch in dictionary value: all values must be subtypes or of the first item.");
				}
			}
		}
		else
		{
			if (elements.size() == 0)
			{
				errors->addError(new ErrorDescription(getLine(), getColumn(), L"Dictionary definition using generators require an expression."));
				return getError(L"Dictionary definition using generators require an expression.");
			}
			else if (elements.size() > 1)
			{
				errors->addError(new ErrorDescription(getLine(), getColumn(), L"Dictionary definition using generators expects one unique expression for key ad value."));
				return getError(L"Dictionary definition using generators expects one unique expression for key ad value.");
			}
			Environment localenv(env);
			ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"dictionary generators"));
			if (generators->makeType(&localenv, logger))
				delete logger;
			else
			{
				errors->addError(logger);
				return getError(L"Error in dictionary generators.");
			}
			logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"dictionary generated element key"));
			tkey = elements.front().first->getType(&localenv, logger);
			if (tkey->getKind() != Type::Error)
				delete logger;
			else
			{
				errors->addError(logger);
				return getError(L"Error in dictionary key.");
			}
			logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"dictionary generated element value"));
			tval = elements.front().second->getType(&localenv, logger);
			if (tval->getKind() != Type::Error)
				delete logger;
			else
			{
				errors->addError(logger);
				return getError(L"Error in dictionary value.");
			}
		}
		return make_shared<Type>(Type::Dictionary,tkey,tval);
	}

	//RecordExpression
	Type::pointer_t RecordExpression::makeType(Environment *env, ErrorLocation *errors)
	{
		//std::list<std::pair<std::wstring,Expression*> >::iterator iter;
		iterator iter;
		Type::pointer_t myType= std::make_shared<RecordType>();
		std::wstring errmsg;
		Environment localenv(env);
		// For functions defined inside, put all record members in localenv.
		for (iter=members.begin();iter!=members.end();iter++)
		{
			ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"record member ")+iter->first);
			Type::pointer_t t=localenv.put(iter->first)->find();
			Type::pointer_t et=iter->second->getType(env,logger)->find();
			if (et->getKind() == Type::Error)
			{
				errors->addError(logger);
				return getError(L"error in record member.");
			}
			else delete logger;
			et->setAccessFlags(myType->getAccessFlags());
			if (!et->unify(t))
			{
				myType.reset();
				std::wstring msg = L"Error typing record with respect to label '" + iter->first + L"'.";
				errors->addError(new ErrorDescription(getLine(), getColumn(), msg));
				return getError(msg);
			}
			((RecordType*)myType.get())->addMember(iter->first,et);
		}
		return myType;
	}

	//VariantExpression
	Type::pointer_t VariantExpression::makeType(Environment *env, ErrorLocation *errors)
	{
		//std::list<std::pair<std::wstring,Expression*> >::iterator iter;
		ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"members of variant with tag ") + tag);
		Type::pointer_t rec=record->getType(env,logger);
		if (rec->getKind() == Type::Error)
		{
			errors->addError(logger);
			return rec;
		}
		delete logger;
		Type::pointer_t myType = std::make_shared<VariantType>();
		((VariantType*)myType.get())->addTag(tag,rec);
		return myType;
	}

	Type::pointer_t Variable::makeType(Environment *env, ErrorLocation *errors)
	{
		Type::pointer_t t;
		// Get fresh type variable with same name
		if (relative&&path.empty())
		{
			t=env->get(name);
			//if (t!=NULL) original=t->copy();
		}
		else
		{
			Environment::Module *module;
			if (relative) module = Environment::getCurrentModule()->followPath(path);
			else module = Environment::getRootModule()->followPath(path);
			if (module != NULL)
			{
				Environment::Module::export_iter iter = module->findExport(name);
				if (iter != module->endExport()) t = iter->second.type;
				else
				{
					std::wstring msg = L"No variable with the name '" + name + L"' found in the module.";
					errors->addError(new ErrorDescription(getLine(), getColumn(), msg));
					return getError(msg);
				}
			}
			else
			{
				std::wstring msg = L"Invalid qualified path to unknown Module";
				errors->addError(new ErrorDescription(getLine(), getColumn(), msg));
				return getError(msg);
			}
		}
		if (t!=NULL) return t;
		else
		{
			std::wstring msg=L"No variable with the name '" + name + L"' exists.";
			errors->addError(new ErrorDescription(getLine(), getColumn(), msg));
			return getError(msg);
		}

	}

	Type::pointer_t Application::getCalledFunctionType(Environment *env, ErrorLocation *errors)
	{
		ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"function being called"));
		Type::pointer_t ft=func->getType(env,logger); // Get the type of the (assumed) function to be applied
		// Here, returning a copy is very important unless we are dealing with a type variable.
		// The copy ensures the following unification  does not mess up generic types of the 
		// source function type sitself.
		// However when type is a variable, we need to infer a function type for the variable,
		// so we return the varibale itself so it will be restricted to a function.
		// In  this case, the application will make sure that the variable has previously been
		// unified with a function template of the correct arity.
		// That is also true for applications of recursive functions
		if (ft->getKind() == Type::Error)
		{
			errors->addError(logger);
			return ft;
		}
		else if (ft->find()->getKind() == Type::Variable) calledType = ft;
		else 
		{
			calledType=ft->find()->copy();
			deleteCalledType=true;
		}
		delete logger;
		return calledType;
	}
	
	Type::pointer_t Application::makeType(Environment *env, ErrorLocation *errors)
	{
		// intermediate should be a function type for correct unification.
		// But if getCalledFunctionType returns a top-bound type variable
		// this can be unified with a function type!
		// The variable can actually also be unified with a function if it's bound
		// is a function type that is a supertype of the called function...
		//FunctionType *intermediate;//=(FunctionType*)getCalledFunctionType(env);
		//Type *intermediate;//=(FunctionType*)getCalledFunctionType(env);
		Type::pointer_t called = getCalledFunctionType(env, errors);
		int flags = Type::Readable | Type::Writable;
		if (called->getKind() == Type::Error)
		{
			return called;
		}
		else if (called->getKind() == Type::Function)
		{
			//intermediate = called;
			flags = ((FunctionType*)called.get())->getReturnType()->find()->getAccessFlags();
		}
		else if (called->getKind() == Type::Variable)
		{
			// Turn the incoming type variable inro a compatible function type:
			// same number of parameters, all fresh top bound variables.
			/*
			std::list<Type*> p;
			Type *retval=Environment::fresh();
			//retval->setAccessFlags(intermediate->getReturnType()->find()->getAccessFlags());
			for (size_t i=0;i<params.size();i++)
			{
				p.push_back(Environment::fresh());
			}
			TypeVariable *tv=(TypeVariable*)called;
			funvar=new FunctionType(p,retval);
			// Unify supertype instead?? Then intermediate=called
			tv->getSupertype()->unify(funvar);
			//intermediate=(FunctionType*)tv->find();
			*/
			//intermediate = called;
		}
		else
		{
			// Function operators return hard coded function types,
			// so cannot trigger this. Hence no need to improve error message.g
			errors->addError(new ErrorDescription(getLine(), getColumn(), L"Application expects a function value."));
			return getError(L"Application expects a function value.");
		};
		std::vector<Type::pointer_t> paramtypes;
		Type::pointer_t retval = Environment::fresh();
		//retval->setAccessFlags(intermediate->getReturnType()->find()->getAccessFlags());
		retval->setAccessFlags(flags);
		// Get parameter types from application expression for function type to unify against.
		//sourceTypes.resize(params.size(),nullptr);
		for (size_t i = 0;i < params.size();++i)
		{
			// Putting the pointers to type variables in the function type here
			// causes potential specializations to be communicated back into the environement,
			// so that future uses of the value have to unify with the result as well.
			// This propagation of resulting types through all dependant expressions
			// is important to have type safety despite being a mutable language.
			// When not dealing with variables however, without subtyping the result would be fixed.
			// Since subtyping is supported, we may have to add coercions where we cannot cheat the differences
			// (e.g. for records we cheat in order to not have to care about the labels present as long as 
			// those we need are there.)
			// It makes sense therefore to remember non-variable types the way they where before unification,
			// so that an integer operation is not later specialized to real.
			// I.e. a function (?a<:Number,?a<:Number)->?a<:Number expects two parameters with the same type,
			// so if applying to one integer and a real f(1,2.33) the integer parameter will be turned into a real.
			// The union-find algorithm then "backtypes" all those integers to reals, affecting performance and precision.
			// Anf ultimately, it feels more natural to make the conversion only once, at the latest possible time.
			// Yet again, if we don't have to explicitely coerce a type, like a record, we need not copy?!
			std::wstringstream strs;
			strs << L"parameter " << i << L" for " << getOperationDescription();
			ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), strs.str());
			Type::pointer_t pt = params[i]->getType(env, logger);
			if (pt->getKind() == Type::Error)
			{
				// The very special error type, we should probably check for more and build a more complete mesaage...
				// also cleanup ;)
				errors->addError(logger);
				return pt;
			}
			delete logger;
			/*
			TypeVariableSet vars;
			pt->getVariables(vars);
			if (vars.empty() && pt->getKind()!=Type::UserDef)
			{
				sourceTypes[i]=pt->copy();
				pt=sourceTypes[i];
			}
			*/
			paramtypes.push_back(pt);
		}
		myFuncType = Type::pointer_t(new FunctionType(paramtypes, retval));
		Type::pointer_t funvar = Environment::fresh(myFuncType);
		/*
		std::wcout << L"source type: ";
		funvar->find()->print(std::wcout);
		std::wcout << L"\ncalled: ";
		called->find()->print(std::wcout);
		std::wcout << L"\n";
		*/
		// Check all is well...
		//if (!called->unify(myFuncType)) 
		if (!called->unify(funvar))
		{
			std::wstringstream strs;
			strs << L"the " << getOperationDescription() << " called has type\n\t";
			called->find()->print(strs);
			strs << "\n\tbut the values passed result in type\n\t";
			funvar->find()->print(strs);
			strs << "\n\tinstead";
			errors->addError(new ErrorDescription(getLine(), getColumn(), strs.str()));
			Type::pointer_t et = getError(strs.str());
			retval->unify(et); // makes sure later uses of this return value get the error
			return et;
		}
		return retval;
	}

	Type::pointer_t RecordAccess::makeType(Environment *env, ErrorLocation *errors)
	{
		ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"source record expression"));
		Type::pointer_t myType=record->getType(env,logger)->find();
		if (myType->getKind() == Type::Error)
		{
			errors->addError(logger);
			return getError(L"error in record expression.");
		}
		else delete logger;

		if (myType->getKind()==Type::Record)
		{
			RecordType *rt=dynamic_cast<RecordType*>(myType.get());
			RecordType::member_iter iter;
			iter=rt->findMember(label);
			// Maybe the label does not exist?
			if (iter == rt->end())
			{
				std::wstring msg = L"This record does not contain a member called '" + label + L"'.";
				errors->addError(new ErrorDescription(getLine(), getColumn(), msg));
				return getError(msg);
			}
			// All is ok, return the label's type
			return iter->second;
		}
		else if (myType->getKind()==Type::Variable)
		{
			// Subtype polymorphism! We can add new labels with variable type for later unification
			// Or we have to make sure a existing label has a legal type.
			TypeVariable *vt=dynamic_cast<TypeVariable *>(myType.get());
			Type::pointer_t super=vt->getSupertype();
			if (super->find()->getKind()==Type::Top)
			{
				// Replace supertype with record containings new label
				Type::pointer_t myRecord= std::make_shared<RecordType>();
				Type::pointer_t buf=Environment::fresh();
				((RecordType*)myRecord.get())->addMember(label,buf);
				vt->setSupertype(myRecord);
				return buf;
			}
			else if (super->find()->getKind()==Type::Record)
			{
				RecordType *rt=dynamic_cast<RecordType*>(super->find().get());
				RecordType::member_iter iter;
				iter=rt->findMember(label);
				// Maybe the label does not exist?
				if (iter==rt->end()) 
				{	// Add label with new variable type, return variable type.
					Type::pointer_t buf=Environment::fresh();
					//Type *buf=new TypeVariable(std::wstring(L"?")+label);
					rt->addMember(label,buf);
					return buf;
				}
				else
				{	// return existing type
					return iter->second->find();
				}
			}
			else
			{
				std::wstring msg = L"The value to the left of the '.' is restricted to type other than a record type.";
				errors->addError(new ErrorDescription(getLine(), getColumn(), msg));
				return getError(msg);
			}
		}
		else
		{
			std::wstring msg = L"The type of the value to the left of the '.' is not a record type.";
			errors->addError(new ErrorDescription(getLine(), getColumn(), msg));
			return getError(msg);
		}
	}

	Type::pointer_t Extraction::getCalledFunctionType(Environment *env, ErrorLocation *errors)
	{
		std::vector<Type::pointer_t> pin;
		Type::pointer_t dict = std::make_shared<Type>(
			Type::Dictionary,
			Environment::fresh(L"?key"),
			Environment::fresh(L"?value"),
			Type::Readable | Type::Writable);
		//dict->addParameter();
		//dict->addParameter();
		Type::pointer_t maybe(new VariantType);
		VariantType *var = (VariantType*)maybe.get();
		var->addTag(std::wstring(L"None"), Type::pointer_t(new RecordType()));
		RecordType *some = new RecordType();
		some->addMember(std::wstring(L"value"), dict->getParameter(1));
		var->addTag(std::wstring(L"Some"), Type::pointer_t(some));
		pin.push_back(dict);
		pin.push_back(dict->getParameter(0));
		maybe->setAccessFlags(Type::Readable|Type::Writable);
		return std::make_shared<FunctionType>(pin, maybe);
	}
	
	Type::pointer_t DictionaryErase::getCalledFunctionType(Environment *env, ErrorLocation *errors)
	{
		std::vector<Type::pointer_t> pin;
		Type::pointer_t dict = std::make_shared<Type>(
			Type::Dictionary,
			Environment::fresh(L"?key"),
			Environment::fresh(L"?value"),
			Type::Readable | Type::Writable);
		pin.push_back(dict);
		pin.push_back(dict->getParameter(0));
		return std::make_shared<FunctionType>(pin, dict);
	}

	Type::pointer_t Splice::getCalledFunctionType(Environment *env, ErrorLocation *errors)
	{
		std::vector<Type::pointer_t> pin;
		Type::pointer_t sequence = Type::pointer_t(new Type(Type::Sequence));
		sequence->addParameter(Environment::fresh());
		Type::pointer_t tv=Environment::fresh(sequence);
		pin.push_back(tv);
		pin.push_back(counter);
		pin.push_back(counter);
		return std::make_shared<FunctionType>(pin,tv);
	}
	
	Type::pointer_t Splice::makeType(Environment *env, ErrorLocation *errors)
	{
		// Splice defines an internal value last to help talk about the end of the sequence.
		// It is added here to a local environment to make sure it is defined only inside.
		// strictly speaking, we override a last that could be used for the first parameter...
		// have to think about that.
		Environment local(env);
		local.put(L"last",counter);
		ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"splice operation"));
		Type::pointer_t retval = Application::makeType(&local, logger);
		if (retval->getKind()==Type::Error)
		{
			errors->addError(logger);
		}
		else delete logger;
		return retval;
	}

	Type::pointer_t Assignment::makeType(Environment *env, ErrorLocation *errors)
	{
		ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"assignment source value"));
		Type::pointer_t vt=value->getType(env,logger)->find();
		if (vt->getKind()==Type::Error) 
		{
			errors->addError(logger);
			return value->getType();
		}
		delete logger;
		logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"assignment destination value"));
		Type::pointer_t dt=destination->getType(env,logger)->find();
		if (dt->getKind()==Type::Error) 
		{
			errors->addError(logger);
			return destination->getType();
		}
		delete logger;
		if (!destination->isWritable())
		{
			errors->addError(new ErrorDescription(getLine(), getColumn(), L"Assignment attempts to write to a constant."));
			return getError(L"Assignment has constant on the left side!");
		}
		if (!destination->getWritableType()->unify(vt)) 
		{
			std::wstringstream strs;
			strs << L"cannot assign a value with type\n";
			vt->find()->print(strs);
			strs << "\n\tto a variable with type\n\t";
			destination->getWritableType()->find()->print(strs);
			errors->addError(new ErrorDescription(getLine(), getColumn(), strs.str()));
			return getError(strs.str());
		}
		return vt;
	}

	Type::pointer_t UnaryOperation::getCalledFunctionType(Environment *env, ErrorLocation *errors)
	{
		Type::pointer_t valtype;
		switch (op)
		{
		case Not:
			valtype.reset(new Type(Type::Boolean));
			break;
		case Negate:
			//valtype=new TypeVariable(L"?neg",new Type(Type::Number));
			valtype = env->fresh(L"?neg", Type::pointer_t(new Type(Type::Number)));
			break;
		};
		std::vector<Type::pointer_t> pin;
		pin.push_back(valtype);
		return std::make_shared<FunctionType>(pin, valtype);
	}

	Type::pointer_t BooleanBinary::getCalledFunctionType(Environment *, ErrorLocation *errors)
	{
		// Get the type of the funtion valled
		Type::pointer_t boolean = Type::pointer_t(new Type(Type::Boolean));
		std::vector<Type::pointer_t> pin;
		pin.push_back(Type::pointer_t(new Type(Type::Boolean)));
		pin.push_back(Type::pointer_t(new Type(Type::Boolean)));
		return std::make_shared<FunctionType>(pin, std::make_shared<Type>(Type::Boolean));
	}

	Type::pointer_t CompareOperation::getCalledFunctionType(Environment *, ErrorLocation *errors)
	{
		// Get the type of the funtion valled
		Type::pointer_t oper;
		if (op==Equal || op==Different) oper=Environment::fresh(L"?a");
		else oper=Environment::fresh(L"?a", std::make_shared<Type>(Type::Comparable));
		std::vector<Type::pointer_t> pin;
		pin.push_back(oper);
		pin.push_back(oper);
		return std::make_shared<FunctionType>(pin, std::make_shared<Type>(Type::Boolean));
	}

	Type::pointer_t ArithmeticBinary::getCalledFunctionType(Environment *, ErrorLocation *errors)
	{
		// Get the type of the funtion valled
		std::vector<Type::pointer_t> pin;
		// Variables will be handled elsewhere, deletion wise.
		//Type number(Type::Number);
		Type::pointer_t oper = Environment::fresh(L"?a", std::make_shared<Type>(Type::Number));
		pin.push_back(oper);
		pin.push_back(oper);
		return std::make_shared<FunctionType>(pin, oper);
	}

	/////////////////////////////////////////////////////////////////////
	// Expression printout methods

	void IntegerConstant::print(std::wostream &s)
	{
		//char *buffer=new char[mpz_sizeinbase(value, 10) + 2];
		//s << mpz_get_str(buffer, 10, value);
		s << value;
		Type::pointer_t t=getType();
		if (t!=NULL)
		{
			s << ":";
			t->print(s);
		}
		//delete buffer;
	}

	void BooleanConstant::print(std::wostream &s)
	{
		if (value) s << "true";
		else s << "false";
	}

	void RealConstant::print(std::wostream &s)
	{
		s << std::showpoint << value;
	}

	void StringConstant::print(std::wostream &s)
	{
		s << "\"";
		if (parts.empty()) s << value;
		else 
		{
			for (iterator i=parts.begin();i!=parts.end();i++)
				if (i->t!=NULL) { 
					s << "${" << i->s <<':';
					i->t->print(s);
					s << "}";

				}
				else s << i->s;

		}
		s << "\"";
	}

	void ListConstant::print(std::wostream &s)
	{
		s << "{ ";
		if (!elements.empty())
		{
			std::vector<Expression*>::iterator iter;
			for (iter=elements.begin();iter!=elements.end();iter++)
			{
				if (iter!=elements.begin()) s << ", ";
				(*iter)->print(s);
			}
		}
		if (generators!=NULL)
		{
			s << " | ";
			generators->print(s);
		}
		s << " }";
	}

	void DictionaryConstant::print(std::wostream &s)
	{
		s << "{ ";
		if (!elements.empty())
		{
			memberlist::iterator iter;
			for (iter=elements.begin();iter!=elements.end();iter++)
			{
				if (iter!=elements.begin()) s << ", ";
				iter->first->print(s);
				s << " => ";
				iter->second->print(s);
			}
			if (generators!=NULL)
			{
				s << " | ";
				generators->print(s);
			}
		} 
		else 
		{
			s << " => ";
			
		}
		s << " }";
	}

	void RecordExpression::print(std::wostream &s)
	{
		std::vector<std::pair<std::wstring,Expression*> >::iterator iter;
		s << "[\n";
		for (iter=members.begin();iter!=members.end();iter++)
		{
			s << "\t" << iter->first;
			if (iter->second->getType()!=NULL && iter->second->getType()->find()->getKind()!=Type::Function) 
				s << " <- ";
			iter->second->print(s);
			if (iter->second->getType()!=NULL) 
			{
				s << ":";
				iter->second->getType()->find()->print(s);
			}
			s << ";\n";
			
		}
		s << "]";
	}

	void VariantExpression::print(std::wostream &s)
	{
		RecordExpression::iterator iter;
		s << "[ :"<<tag<<"\n";
		for (iter=record->begin();iter!=record->end();iter++)
		{
			s << "\t" << iter->first;
			if (iter->second->getType()!=NULL && iter->second->getType()->find()->getKind()!=Type::Function) 
			s << " <- ";
			iter->second->print(s);
			if (iter->second->getType()!=NULL) 
			{
				s << ":";
				iter->second->getType()->find()->print(s);
			}
			s << ";\n";
			
		}
		s << "]";
	}
	
	void Variable::print(std::wostream &s)
	{
		if (!relative) s << "::";
		for (std::vector<std::wstring>::iterator pit = path.begin();pit != path.end();pit++) 
			s <<  *pit << "::";
		s << name ;
		if (getType()!=NULL) 
		{
			s << ":";
			/*if (original!=NULL) 
			{
				original->find()->print(s);
				s << "-->";
			}*/
			getType()->find()->print(s);
		}
	}

	void RecordAccess::print(std::wostream &s)
	{
		record->print(s);
		s << "." << label << ":";
		getType()->find()->print(s);
	}

	void Extraction::print(std::wostream &s)
	{
		params.front()->print(s);
		s << "[";
		params.back()->print(s);
		s << "] :";
		getType()->print(s);
	}

	void DictionaryErase::print(std::wostream &s)
	{
		s << "(";
		params.front()->print(s);
		s << "\\";
		params.back()->print(s);
		s << ") : ";
		getType()->print(s);
	}
		
	void Splice::print(std::wostream &s)
	{
		iterator iter=params.begin();
		(*iter)->print(s);
		s << "[";
		iter++;
		(*iter)->print(s);
		s << ":";
		iter++;
		if (iter!=params.end()) (*iter)->print(s);
		s << "] :";
		getType()->print(s);
	}

	void Assignment::print(std::wostream &s)
	{
		destination->print(s);
		s << " <- ";
		value->print(s);
		if (getType()!=NULL) 
		{
			s << ":";
			getType()->find()->print(s);
		}
	}

	void UnaryOperation::print(std::wostream &s)
	{
		if (op==Not) s << "not ";
		else s << "-";
		params.front()->print(s);
	}

	void BooleanBinary::print(std::wostream &s)
	{
		s << "(";
		params.front()->print(s);
		switch(op)
		{
		case And: s << " and "; break;
		case Or: s << " or "; break;
		case Xor: s << " xor "; break;
		}
		params.back()->print(s);
		s << ") : ";
		getType()->find()->print(s);
	}

	void CompareOperation::print(std::wostream &s)
	{
		s << "(";
		params.front()->print(s);
		if (params.front()->getType())
		{
			s << ":";
			params.front()->getType()->print(s);
		}
		switch(op)
		{
		case Equal:			s << " == "; break;
		case Different:		s << " != "; break;
		case Greater:		s << " > "; break;
		case GreaterEqual:	s << " >= "; break;
		case Less:			s << " < "; break;
		case LessEqual:		s << " <= "; break;
		}
		params.back()->print(s);
		if (params.back()->getType())
		{
			s << ":";
			params.back()->getType()->print(s);
		}
		if (getType())
		{
			s << ") : ";
			getType()->find()->print(s);
		}
	}

	//ArithmeticBinary
	void ArithmeticBinary::print(std::wostream &s)
	{
		s << "(";
		params.front()->print(s);
		switch(op)
		{
		case Add: s << " + "; break;
		case Sub: s << " - "; break;
		case Mul: s << " * "; break;
		case Div: s << " / "; break;
		case Mod: s << " % "; break;
		}
		params.back()->print(s);
		s << ")";
	}

	void ListConstant::collectFunctions(std::vector<intro::Function*> &funcs)
	{	
		if (!elements.empty())
		{
			std::vector<Expression*>::iterator iter;
			for (iter=elements.begin();iter!=elements.end();iter++)
			{
				(*iter)->collectFunctions(funcs);
			}
		}
		if (generators!=NULL)
		{
			generators->collectFunctions(funcs);
		}
	}

	void DictionaryConstant::collectFunctions(std::vector<intro::Function*> &funcs)
	{
		if (!elements.empty())
		{
			memberlist::iterator iter;
			for (iter=elements.begin();iter!=elements.end();iter++)
			{
				iter->first->collectFunctions(funcs);
				iter->second->collectFunctions(funcs);
			}
			
		}
		if (generators!=NULL)
		{
			generators->collectFunctions(funcs);
		}
	}
	
	void Application::collectFunctions(std::vector<intro::Function*> &funcs)
	{
		if (func!=NULL) func->collectFunctions(funcs);
		for (iterator iter=params.begin();iter!=params.end();iter++)
			(*iter)->collectFunctions(funcs);
	}

	void Assignment::collectFunctions(std::vector<intro::Function*> &funcs)
	{
		destination->collectFunctions(funcs);
		value->collectFunctions(funcs);
	}

	void RecordExpression::collectFunctions(std::vector<intro::Function*> &funcs)
	{
		std::vector<std::pair<std::wstring,Expression*> >::iterator iter;
		for (iter=members.begin();iter!=members.end();iter++)
		{
			iter->second->collectFunctions(funcs);
		}
	}

	void VariantExpression::collectFunctions(std::vector<intro::Function*> &funcs)
	{
		record->collectFunctions(funcs);
	}

	void RecordAccess::collectFunctions(std::vector<intro::Function*> &funcs)
	{ 
		record->collectFunctions(funcs);
	}

	void Extraction::collectFunctions(std::vector<intro::Function*> &funcs)
	{
		for (iterator iter=params.begin();iter!=params.end();iter++)
			(*iter)->collectFunctions(funcs);
	}

	void DictionaryErase::collectFunctions(std::vector<intro::Function*> &funcs)
	{
		for (iterator iter=params.begin();iter!=params.end();iter++)
			(*iter)->collectFunctions(funcs);
	}

	void Splice::collectFunctions(std::vector<intro::Function*> &funcs)
	{
		for (iterator iter=params.begin();iter!=params.end();iter++)
			(*iter)->collectFunctions(funcs);
	}

}

