#include "stdafx.h"
#include "Expression.h"
#include "Statement.h"
#include "CodeGenEnvironment.h"

using namespace std;

namespace intro {

#ifdef WIN32
	const std::tr1::wregex tokens(L"(\\\\(?:u[[:xdigit:]]{4}|x[[:xdigit:]]+|[[:graph:]])|\\$\\{[[:graph:]]+\\}|[[:space:]]+|[^\\\\$ ]+|\\$(?!\\{))");
#else
	const std::wregex tokens(L"(\\\\(?:u[[:xdigit:]]{4}|x[[:xdigit:]]+|[[:graph:]])|\\$\\{[[:graph:]]+\\}|[[:space:]]+|[^\\\\$ ]+|\\$(?!\\{))");
#endif

	Type *Expression::getType(CodeGenEnvironment *env)
	{
		if (type==NULL) return NULL; // Nothing to see here
		//if (type->find()->getKind()!=Type::Variable) return type->find(); // give what we got
		//return env->getSpecialization(type->find()); // Variables get mapped using env
		return type->find();
	}

	/////////////////////////////////////////////////////////////////////
	// Expression Type Inference methods - note several expressions are specializations of application
	//		and thus only need to define the function type describing them (i.e. operators).
	
	Type *IntegerConstant::makeType(Environment *)
	{
		return myType;
	}

	Type *BooleanConstant::makeType(Environment *)
	{
		return myType;
		//return new Type(Type::Boolean);
	}

	Type *RealConstant::makeType(Environment *)
	{
		return myType;
		//return new Type(Type::Boolean);
	}
	
	Type *StringConstant::makeType(Environment *env)
	{
		// Trim quotation marks read from source from string
		std::wstring trimmed=value.substr(1,value.size()-2);
		// Prepare tokenization of string based on regular expressions (regex does efficint preprocessing here...)
#ifdef WIN32
		//std::tr1::wsregex_token_iterator i(value.begin(),value.end(), tokens, 1);
		std::tr1::wsregex_token_iterator i(trimmed.begin(),trimmed.end(), tokens, 1);
		std::tr1::wsregex_token_iterator j; // represents end of token stream
#else 
		//std::wsregex_token_iterator i(value.begin(),value.end(), tokens, 1);
		std::wsregex_token_iterator i(trimmed.begin(),trimmed.end(), tokens, 1);
		std::wsregex_token_iterator j; // represents end of token stream
#endif
		std::wstring curbuf; // Collect a sequence of consecutive tokens that are not interpolations (constant)
		while(i != j)
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
					default: curbuf.append(1,i->str()[1]); // If it has no special meaning, copy as is (e.g. backslash and dollar sign).
					}
					
					break;
				case L'$': // Interpolation (neither single $, nor escaped...)
					if (curbuf.size()!=0)
					{
						parts.push_back(part(curbuf));
						curbuf.clear();
					}
					{
						// Extract variable name, and the variable
						std::wstring varname=i->str().substr(2,i->str().size()-3);
						intro::Type *vartype=env->get(varname);
						if (vartype==NULL)
							return getError(L"Variable '"+varname+L"' not found for string interpolation.");
						parts.push_back(part(varname,vartype->copy()));
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
			parts.push_back(part(curbuf));
			curbuf.clear();
		};


		return myType;
		//return new Type(Type::Boolean);
	}

	Type *ListConstant::makeType(Environment *env)
	{
		Type *param=NULL;
		if (elements.empty()) param=Environment::fresh();
		else if (generators==NULL)
		{
			std::list<intro::Expression*>::iterator iter=elements.begin();
			if (iter==elements.end())
			{
				param=Environment::fresh();
			} 
			else
			{
				param=(*iter)->getType(env)->find();
				iter++;
				// check that all other expressions in the element list are subtypes of the first one.
				// Note: in the implementation, extra information will be list
				for (;iter!=elements.end();iter++)
				{
					PartialOrder po=param->checkSubtype((*iter)->getType(env));
					if (po!=LESS && po!=EQUAL ) 
					{
						return getError(L"Subtyping mismatch in listelements: all items must be subtypes or of the first item.");
					}
				}
			}
		}
		else
		{
			if (elements.size()==0) return getError(L"List definition using generators require an expression.");
			else if (elements.size()>1) return getError(L"List definition using generators expects one unique expression.");
			Environment localenv(env);
			generators->makeType(&localenv);
			param=elements.front()->getType(&localenv);
		}
		myType=new Type(Type::List,param);
		return myType;
	}

	Type *DictionaryConstant::makeType(Environment *env)
	{
		Type *tkey=NULL;
		Type *tval=NULL;
		if (elements.empty()) 
		{
			tkey=Environment::fresh();
			tval=Environment::fresh();
		}
		else if (generators==NULL)
		{
			memberlist::iterator iter=elements.begin();
			tkey=iter->first->getType(env)->find();
			tval=iter->second->getType(env)->find();
			iter++;
			// check that all other expressions in the element list are subtypes of the first one.
			// Note: in the implementation, extra information will be list
			for (;iter!=elements.end();iter++)
			{
				PartialOrder po=tkey->checkSubtype(iter->first->getType(env));
				if (po!=LESS && po!=EQUAL ) 
				{
					return getError(L"Subtyping mismatch in dictionary key: all keys must be subtypes or of the first item.");
				}
				po=tval->checkSubtype(iter->second->getType(env));
				if (po!=LESS && po!=EQUAL ) 
				{
					return getError(L"Subtyping mismatch in dictionaly value: all values must be subtypes or of the first item.");
				}
			}
		}
		else
		{
			if (elements.size()==0) return getError(L"Dictionary definition using generators require an expression.");
			else if (elements.size()>1) return getError(L"Dictionary definition using generators expects one unique expression for key ad value.");
			Environment localenv(env);
			generators->makeType(&localenv);
			tkey=elements.front().first->getType(&localenv);
			tval=elements.front().second->getType(&localenv);
		}
		myType=new Type(Type::Dictionary,tkey);
		myType->addParameter(tval);
		//DictionaryType buffer(tkey,tval);
		//myType=copy(&buffer);
		return myType;
	}

	//RecordExpression
	Type *RecordExpression::makeType(Environment *env)
	{
		std::list<std::pair<std::wstring,Expression*> >::iterator iter;
		myType=new RecordType();
		std::wstring errmsg;
		Environment localenv(env);
		// For functions defined inside, put all record members in localenv.
		for (iter=members.begin();iter!=members.end();iter++)
		{
			Type *t=localenv.put(iter->first)->find();
			Type *et=iter->second->getType(env)->find();
			et->setAccessFlags(myType->getAccessFlags());
			if (!et->unify(t))
			{
				delete myType;
				myType=NULL;
				return getError(std::wstring(L"Error typing record with respect to label '")+iter->first+L"'.");
			}
			myType->addMember(iter->first,et);
		}
		return myType;
	}

	//VariantExpression
	Type *VariantExpression::makeType(Environment *env)
	{
		//std::list<std::pair<std::wstring,Expression*> >::iterator iter;
		RecordType *rec=(RecordType*)record->getType(env);
		if (rec->getKind()==Type::Error) return rec;
		myType=new VariantType();
		myType->addTag(tag,rec);
		return myType;
	}

	Type *Variable::makeType(Environment *env)
	{
		Type *t=NULL;
		// Get fresh type variable with same name
		if (relative&&path.empty())
		{
			t=env->get(name);
			//if (t!=NULL) original=t->copy();
		}
		else
		{
			Environment::Module *module;
			if (relative) module=Environment::getCurrentModule()->followPath(path);
			else module=Environment::getRootModule()->followPath(path);
			if (module!=NULL)
			{
				Environment::Module::export_iter iter=module->findExport(name);
				if (iter!=module->endExport()) t=iter->second.type;
				else return getError(std::wstring(L"No variable with the name '")+name+L"' found in the module.");
			}
			else
			{
				return getError(std::wstring(L"Invalid qualified path to unknown Module"));
			}
		}
		if (t!=NULL) return t;
		else return getError(std::wstring(L"No variable with the name '")+name+L"' exists.");

	}

	Type *Application::getCalledFunctionType(Environment *env)
	{
		Type *ft=func->getType(env); // Get the type of the (assumed) function to be applied
		// Here, returning a copy is very important unless we are dealing with a type variable.
		// The copy ensures the following unification  does not mess up generic types of the 
		// source function type sitself.
		// However when type is a variable, we need to infer a function type for the variable,
		// so we return the varibale itself so it will be restricted to a function.
		// In  this case, the application will make sure that the variable has previously been
		// unified with a function template of the correct arity.
		// That is also true for applications of recursive functions
		if (ft->getKind()==Type::Variable) calledType=ft;
		else 
		{
			calledType=ft->copy();
			deleteCalledType=true;
		}
		return calledType;
	}
	
	Type *Application::makeType(Environment *env)
	{
		// intermediate should be a function type for correct unification.
		// But if getCalledFunctionType returns a top-bound type variable
		// this can be unified with a function type!
		// The variable can actually also be unified with a function if it's bound
		// is a function type that is a supertype of the called function...
		//FunctionType *intermediate;//=(FunctionType*)getCalledFunctionType(env);
		Type *intermediate;//=(FunctionType*)getCalledFunctionType(env);
		Type *called=getCalledFunctionType(env);
		int flags = Type::Readable | Type::Writable;
		if (called->getKind()==Type::Error)
		{
			return called;
		}
		else if (called->getKind()==Type::Function)
		{
			intermediate = called;
			flags = ((FunctionType*)called)->getReturnType()->find()->getAccessFlags();
		}
		else if (called->getKind()==Type::Variable)
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
			intermediate = called;
		}
		else
		{
			return getError(L"Application expects a function value.");
		};
		std::list<Type*> paramtypes;
		Type *retval=Environment::fresh();
		//retval->setAccessFlags(intermediate->getReturnType()->find()->getAccessFlags());
		retval->setAccessFlags(flags);
		// Get parameter types from application expression for function type to unify against.
		sourceTypes.resize(params.size(),nullptr);
		for (size_t i=0;i<params.size();++i)
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
			Type *pt=params[i]->getType(env);
			if (pt->getKind()==Type::Error) 
			{
				// The very special error type, we should probably check for more and build a more complete mesaage...
				// also cleanup ;)
				return pt;
			}
			TypeVariableSet vars;
			pt->getVariables(vars);
			if (vars.empty() && pt->getKind()!=Type::UserDef)
			{
				sourceTypes[i]=pt->copy();
				pt=sourceTypes[i];
			}
			paramtypes.push_back(pt);
		}
		myFuncType=new FunctionType(paramtypes,retval);
		Type *funvar = myFuncType;
		//if (called->getKind() == Type::Variable) 
			funvar = Environment::fresh(myFuncType);
/*
std::wcout << L"source type: ";
funvar->find()->print(std::wcout);
std::wcout << L"\nintermediate: ";
intermediate->find()->print(std::wcout);
std::wcout << L"\n";
*/
		// Check all is well...
		//if (!intermediate->unify(myFuncType)) 
		if (!intermediate->unify(funvar))
		{
			return getError(L"At least one parameter type does not fit what the function expects.");
		}
		return retval;
	}

	Type *RecordAccess::makeType(Environment *env)
	{
		Type *myType=record->getType(env)->find();
		// are we accessing a record?
		if (myType->getKind()==Type::Record)
		{
			RecordType *rt=dynamic_cast<RecordType*>(myType);
			RecordType::iterator iter;
			iter=rt->findMember(label);
			// Maybe the label does not exist?
			if (iter==rt->end()) return getError(std::wstring(L"This record does not contain a member called '")+label+L"'.");
			// All is ok, return the label's type
			return iter->second;
		}
		else if (myType->getKind()==Type::Variable)
		{
			// Subtype polymorphism! We can add new labels with variable type for later unification
			// Or we have to make sure a existing label has a legal type.
			TypeVariable *vt=dynamic_cast<TypeVariable *>(myType);
			Type *super=vt->getSupertype();
			if (super->find()->getKind()==Type::Top)
			{
				// Replace supertype with record containings new label
				RecordType *rt=new RecordType();
				Type *buf=Environment::fresh();
				rt->addMember(label,buf);
				vt->replaceSupertype(rt);
				return buf;
			}
			else if (super->find()->getKind()==Type::Record)
			{
				RecordType *rt=dynamic_cast<RecordType*>(super);
				RecordType::iterator iter;
				iter=rt->findMember(label);
				// Maybe the label does not exist?
				if (iter==rt->end()) 
				{	// Add label with new variable type, return variable type.
					Type *buf=Environment::fresh();
					//Type *buf=new TypeVariable(std::wstring(L"?")+label);
					rt->addMember(label,buf);
					return buf;
				}
				else
				{	// return existing type
					return iter->second->find();
				}
			}
			else return getError(L"The value to the left of the '.' is restricted to type other than a record type.");
		}
		else return getError(L"The type of the value to the left of the '.' is not a record type.");
	}

	Type *Extraction::getCalledFunctionType(Environment *env)
	{
		//Type *in=params.front()->getType(env)->find();
		Type *ret=NULL;
		std::list<Type*> pin;

		dict=new Type(Type::Dictionary,Type::Readable|Type::Writable);
		//Type *in=new Type(Type::Dictionary,Type::Readable|Type::Writable);
		//Type *in=dict;
		dict->addParameter(Environment::fresh(L"?key"));
		dict->addParameter(Environment::fresh(L"?value"));
		maybe.addTag(std::wstring(L"None"),&none);
		maybe.addTag(std::wstring(L"Some"),&some);
		some.addMember(std::wstring(L"value"), dict->getParameter(1));
		pin.push_back(dict);
		pin.push_back(dict->getParameter(0));
		maybe.setAccessFlags(Type::Readable|Type::Writable);
		ret=&maybe;

		myType=new FunctionType(pin,ret);
		return myType;
	}
	
	Type *DictionaryErase::getCalledFunctionType(Environment *env)
	{
		//Type *in=params.front()->getType(env)->find();
		Type *ret=NULL;
		std::list<Type*> pin;
		// Our expected source type is a dictionary.
		dict=new Type(Type::Dictionary,Type::Readable|Type::Writable);
		dict->addParameter(Environment::fresh(L"?key"));
		dict->addParameter(Environment::fresh(L"?value"));
		pin.push_back(dict);
		pin.push_back(dict->getParameter(0));
		myType=new FunctionType(pin,dict);
		return myType;
	}

	Type *Splice::getCalledFunctionType(Environment *env)
	{
		//Type *in=params.front()->getType(env)->find();
		Type *ret=NULL;
		std::list<Type*> pin;
		VariableSet free,bound;
		// TBD: supertype as member in class
		ret=new Type(Type::Sequence);
		ret->addParameter(Environment::fresh());
		TypeVariable *tv=Environment::fresh(ret);
		pin.push_back(tv);
		pin.push_back(&counter);
		pin.push_back(&counter);
		myType=new FunctionType(pin,tv);
		return myType;
	}
	
	Type *Splice::makeType(Environment *env)
	{
		// Splice defines an internal value last to help talk about the end of the sequence.
		// It is added here to a local environment to make sure it is defined only inside.
		// strictly speaking, we override a last that could be used for the first parameter...
		// have to think about that.
		Environment local(env);
		local.put(L"last",&counter);
		return Application::makeType(&local);
	}

	Type *Assignment::makeType(Environment *env)
	{
		Type *vt=value->getType(env)->find();
		if (vt->getKind()==Type::Error) 
		{
			return value->getType();
		}
		Type *dt=destination->getType(env)->find();
		if (dt->getKind()==Type::Error) 
		{
			return destination->getType();
		}
		if (!destination->isWritable())
		{
			return getError(L"Assignment has constant on the left side!");
		}
		if (!destination->getWritableType()->unify(vt)) {
			return getError(L"Assignment between incompatible variable and expression types.");
		}
		return vt;
	}

	Type *UnaryOperation::getCalledFunctionType(Environment *env)
	{
		Type *valtype=nullptr;
		switch(op)
		{
		case Not:
			valtype=&boolean;
			break;
		case Negate:
			//valtype=new TypeVariable(L"?neg",new Type(Type::Number));
			valtype = env->fresh(L"?neg", &number);
			break;
		};
		std::list<Type*> pin;
		pin.push_back(valtype);
		myType=new FunctionType(pin,valtype);
		return myType;

	}

	Type *BooleanBinary::getCalledFunctionType(Environment *)
	{
		// Get the type of the funtion valled
		std::list<Type*> pin;
		pin.push_back(&boolean);
		pin.push_back(&boolean);
		myType=new FunctionType(pin, &boolean);
		return myType;
	}

	Type *CompareOperation::getCalledFunctionType(Environment *)
	{
		// Get the type of the funtion valled
		std::list<Type*> pin;
		Type *oper;
		if (op==Equal || op==Different) oper=Environment::fresh(L"?a");
		else oper=Environment::fresh(L"?a",&comparable);
		pin.push_back(oper);
		pin.push_back(oper);
		myTypeParam=new Type(Type::Boolean);
		myType=new FunctionType(pin,myTypeParam);
		return myType;
	}

	Type *ArithmeticBinary::getCalledFunctionType(Environment *)
	{
		// Get the type of the funtion valled
		std::list<Type*> pin;
		// Variables will be handled elsewhere, deletion wise.
		//Type number(Type::Number);
		Type *oper=Environment::fresh(L"?a",&number);
		// TBD: supertype as member in class
		//Type *oper = Environment::fresh(L"?a", new Type(Type::Number));
		pin.push_back(oper);
		pin.push_back(oper);
		myType=new FunctionType(pin,oper);
		return myType;

	}

	/////////////////////////////////////////////////////////////////////
	// Expression printout methods

	void IntegerConstant::print(std::wostream &s)
	{
		//char *buffer=new char[mpz_sizeinbase(value, 10) + 2];
		//s << mpz_get_str(buffer, 10, value);
		s << value;
		Type *t=getType();
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
			std::list<Expression*>::iterator iter;
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
		std::list<std::pair<std::wstring,Expression*> >::iterator iter;
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
		s << ":";
		params.front()->getType()->print(s);
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
		s << ":";
		params.back()->getType()->print(s);

		s << ") : ";
		getType()->find()->print(s);
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

	void ListConstant::collectFunctions(std::list<intro::Function*> &funcs)
	{	
		if (!elements.empty())
		{
			std::list<Expression*>::iterator iter;
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

	void DictionaryConstant::collectFunctions(std::list<intro::Function*> &funcs)
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
	
	void Application::collectFunctions(std::list<intro::Function*> &funcs)
	{
		if (func!=NULL) func->collectFunctions(funcs);
		for (iterator iter=params.begin();iter!=params.end();iter++)
			(*iter)->collectFunctions(funcs);
	}

	void Assignment::collectFunctions(std::list<intro::Function*> &funcs)
	{
		destination->collectFunctions(funcs);
		value->collectFunctions(funcs);
	}

	void RecordExpression::collectFunctions(std::list<intro::Function*> &funcs)
	{
		std::list<std::pair<std::wstring,Expression*> >::iterator iter;
		for (iter=members.begin();iter!=members.end();iter++)
		{
			iter->second->collectFunctions(funcs);
		}
	}

	void VariantExpression::collectFunctions(std::list<intro::Function*> &funcs)
	{
		record->collectFunctions(funcs);
	}

	void RecordAccess::collectFunctions(std::list<intro::Function*> &funcs)
	{ 
		record->collectFunctions(funcs);
	}

	void Extraction::collectFunctions(std::list<intro::Function*> &funcs)
	{
		for (iterator iter=params.begin();iter!=params.end();iter++)
			(*iter)->collectFunctions(funcs);
	}

	void DictionaryErase::collectFunctions(std::list<intro::Function*> &funcs)
	{
		for (iterator iter=params.begin();iter!=params.end();iter++)
			(*iter)->collectFunctions(funcs);
	}

	void Splice::collectFunctions(std::list<intro::Function*> &funcs)
	{
		for (iterator iter=params.begin();iter!=params.end();iter++)
			(*iter)->collectFunctions(funcs);
	}

}

