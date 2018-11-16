#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <string>
#include <stack>
#include <map>
#include <sstream>

#include "Type.h"

namespace intro
{

/// An Environment manages type information.
/**	The Enfironment predominantly manages type definitions and scope, but also manages Modules.
	It is also responsible for managing TypeVariables which are potentially shared
	across the whole type landscape.

	The Environment implementation is also central to supporting type inference in a language with updates.
	The canonical example why this is tricky is a creation of an empty list assigned type List(?a).
	The list element is later used as an integer [ok because of the variable in List(?a)]. 
	Then an element is used in an if condition, requiring it to be boolean. Thus the list
	elements can be accessed both as integers and booleans, without the type clash becoming 
	apparent to the type inference mechanism.

	It seems reasonable that the issue stems from the fact that a polymorphic type is deduced for
	a value, and that a concrete usage of the same value does not propagate the information into
	the operands type. This is alleviated in functional languages by having immutable values.
	Circularly, functional language's immutable values implement type inference programs where
	the types are immutable: once an expression is typed, that type is fixed.

	The way Intro uses the Environment to hold all types for a scope operates then in conjunction
	with the union-find algorithm used to compute the union of types. The unionpart of the algorithm 
	operates directly (and imperatively, that is updating) on the types in the environment. In effect,
	all expressions using a value impose additional constraints on the type.

	The result is that the above list example, the list element would first be instantiated as a general
	variable, but then the constraint "is an integer" would be added. The next use would add a constraint
	"is a boolean" to the value's type, but by this time the type is already integer, which cannot be 
	unified with type boolean.

	For records, there is however a rule of the type system, used inside the union-find algorithm,
	which merges two record types into the supertype (assuming all shared labels have a common supertype).

	Important: when the program exits, 
	 - call clearTypeVariables() to deallocate all variables that have been created during a run.
	 - call deleteAllModules() to deallocate all Modules and their data.

	Note that types are only created at startup, when the source code is typed.
	No typing or type checks are performed during execution of the code (except where LLVM IR typing
	is applied).
*/
class Environment
{
public:
	/// A Module in the Environments stores the exported interface's data for code generation
	/**	Despite being an internal class, the Module is central component of the language
		holding the (exported) results built by module statements.
		It is used during code generation.
	*/
	class Module
	{
		std::wstring name; ///< of the module
		struct entry
		{
			Type::pointer_t type;	/// Internal type for code generation
			bool owned;
			//Value value;

			entry(Type::pointer_t t,bool owned_=true) : type(t), owned(owned_)
			{};
		};
		
		std::map<std::wstring,entry> exports;
		std::map<std::wstring,Module*> submodules;
		Module* parent; ///< accessible inside a module during creation
	protected:
		void setParent(Module *m)
		{
			parent=m;
		};

		inline Module *getCreateSubmodule(const std::wstring &name)
		{
			std::map<std::wstring,Module*>::iterator iter=submodules.find(name);
			if (iter==submodules.end())
			{
				Module *buf=new Module(name);
				buf->setParent(this);
				iter=submodules.insert(std::make_pair(name,buf)).first;
			}
			return iter->second;
			
		};

		inline Module *findSubModule(const std::wstring &name)
		{
			std::map<std::wstring,Module*>::iterator iter=submodules.find(name);
			if (iter==submodules.end())
			{
				return NULL;
			}
			return iter->second;
		};
	public:

		typedef std::map<std::wstring,entry>::iterator export_iter;

		Module()
		{
			parent=NULL;
		};
		Module(const std::wstring &name)
		{
			parent=NULL;
			this->name=name;
		};

		~Module()
		{};

		inline std::wstring getName() { return name; };

		inline void addExport(std::wstring name, Type::pointer_t type,bool owned=true)
		{
			exports.insert(make_pair(name,entry(type, owned)));
		};

		inline void addSubModule(Module *m)
		{
			m->setParent(this);
			submodules.insert(make_pair(m->getName(),m));
		};

		export_iter beginExport(void)
		{ return exports.begin(); };

		export_iter endExport(void)
		{ return exports.end(); };

		export_iter findExport(std::wstring s)
		{ return exports.find(s); };

		Module *getCreatePath(const std::vector<std::wstring> &path)
		{
			std::vector<std::wstring>::const_iterator iter;
			Module *mod=this;
			for (iter=path.begin();iter!=path.end();iter++)
			{
				mod=mod->getCreateSubmodule(*iter);
			};
			return mod;
		};

		Module *followPath(const std::vector<std::wstring> &path)
		{
			std::vector<std::wstring>::const_iterator iter;
			Module *mod=this;
			for (iter=path.begin();iter!=path.end();iter++)
			{
				if (mod==NULL) return NULL;
				mod=mod->findSubModule(*iter);
			};
			return mod;
		};

		void importInto(Environment *env)
		{
			std::map<std::wstring,entry>::iterator iter;
			for (iter=exports.begin();iter!=exports.end();iter++)
			{
				//env->put(iter->first,copy(iter->second.type));
				env->put(iter->first,iter->second.type);
			}
		};

		void deleteAll(void)
		{
			for (std::map<std::wstring,entry>::iterator iter=exports.begin();iter!=exports.end();iter++)
			{
				//if (iter->second.owned) deleteCopy(iter->second.type);
			}
			for(std::map<std::wstring,Module*>::iterator iter=submodules.begin();iter!=submodules.end();iter++)
			{
				iter->second->deleteAll();
				delete iter->second;
				iter->second=NULL;
			}

		};
		void print()
		{
			std::wcout <<"'"<<name<<"'\n";
			std::map<std::wstring,entry>::iterator iter;
			for (iter=exports.begin();iter!=exports.end();iter++)
			{
				std::wcout << iter->first << " : " ;
				iter->second.type->print(std::wcout);
				std::wcout << std::endl;
			};
		};
	};

protected:

	static Module *root; ///< The module holding the :: namespace
	static std::stack<Module*> current; ///< The stack of the current module, used for relative access

	Environment *parent;
	std::map<std::wstring, Type::pointer_t > members;

	typedef std::map<std::wstring, Type::pointer_t >::iterator iterator;

	/// Unique names for intermediate type variables
	static long _fresh;
	/// Env owns all type vars, deletes on shutdown
	//static std::set<TypeVariable*> _typevars;
	/// Type inference can add intermediates without sensible owner here...
	//static std::set<Type*> _intermediates;

public:
	//
	static void popModule()
	{
		if (current.top()==getRootModule())
		{
			printf("\n\tERROR! Tried to pop root module!\n");
			return;
		}
		current.pop();
	}

	static Module* getCurrentModule()
	{
		if (current.empty()) current.push(getRootModule());
		return current.top();
	}

	static void pushModule(Module *m)
	{
		if (current.empty()) current.push(getRootModule());
		current.push(m);
	}

	static Module *getRootModule(void) 
	{
		if (root == nullptr)
			root = new Module;
		return root;
	}

	static void deleteRootModule(void)
	{
		delete root;
		root = nullptr;
	}

	//static void addIntermediate(Type::pointer_t t)
	//{
	//	_intermediates.insert(t); 
	//};

	static void clearTypeVariables(void)
	{
		/*
		for (std::set<TypeVariable*>::iterator iter = _typevars.begin();iter != _typevars.end();iter++)
		{
			(*iter)->replaceSupertype(nullptr);
		}
		for (std::set<TypeVariable*>::iterator iter = _typevars.begin();iter != _typevars.end();iter++)
		{
			delete *iter;
		}
		for (std::set<Type*>::iterator iter = _intermediates.begin();iter != _intermediates.end();iter++)
		{
			delete *iter;
		}
		*/
	}

	static void deleteAllModules(void)
	{
		getRootModule()->deleteAll();
	}

	Environment(Environment *par=NULL)
	{
		parent=par;
	}

	~Environment()
	{
		//iterator iter;
		//for (iter=members.begin();iter!=members.end();iter++)
		//	if (iter->second->getKind()!=Type::Variable)deleteCopy(iter->second);
	}

#ifdef _TEST
	static void resetFreshCounter(void)
	{
		_fresh=0;
	};
#endif

	/// Add a fresh type variable to the environment, which is bound by the given super-type.
	static Type::pointer_t fresh(const std::wstring &name, Type::pointer_t super)
	{
		return Type::pointer_t(new TypeVariable(name,super));
		//_typevars.insert(t);
		//members.insert(make_pair(str,t));
		//return t;
	}

	/// Add a fresh type variable to the environment, which is bound by the given super-type.
	static Type::pointer_t fresh(const std::wstring &name, Type::pointer_t super, Type::TypeVariableMap &conv)
	{
		return Type::pointer_t(new TypeVariable(name, super,conv));
		//_typevars.insert(t);
		//members.insert(make_pair(str,t));
		//return t;
	}

	/// Add a fresh type variable to the environment, bound by the top type.
	static Type::pointer_t fresh(const std::wstring &name)
	{
		return Type::pointer_t(new TypeVariable(name));
	}

	/// Add a fresh (anonymous/intermediate)) type variable to the environment, which is bound by the given super-type.
	static Type::pointer_t fresh(Type::pointer_t super)
	{
		std::wstringstream bufstream;
		bufstream <<  L"?T"<< _fresh;
		_fresh++;
		return fresh(bufstream.str(),super);
	}

	/// Add a fresh anonymous/intermediate type variable to the environment, bound by the top type.
	static Type::pointer_t fresh(void)
	{
		std::wstringstream bufstream;
		bufstream <<  L"?T"<< _fresh;
		_fresh++;
		return fresh(bufstream.str());
	}

	/// Return the type of the given identifier, or NULL if it does not exist (checks parent environments).
	Type::pointer_t get(const std::wstring &ident)
	{
		iterator iter=members.find(ident);
		
		if (iter==members.end()) 
		{
			if (parent==NULL) return Type::pointer_t();
			else return parent->get(ident);
		}
		else
		{
			return iter->second->find();
		}
	}

	/// Add a new name to the environment, with the given type.
	/** Returns true if the variable was created, false if it already existed.
	*/
	bool put(const std::wstring &ident, Type::pointer_t type)
	{
		iterator iter=members.find(ident);
		if (iter!=members.end()) return false;

		//members.insert(make_pair(ident,type->copy()));
		members.insert(make_pair(ident,type));
		return true;
	}

	/// Add a new name to the environment, with the given type.
	/** Returns a pointer to the variable's new type variable if it was created, nullptr if it already existed.
	*/
	Type::pointer_t put(const std::wstring &ident)
	{
		iterator iter=members.find(ident);
		//if (iter!=members.end()) return iter->second;
		if (iter!=members.end()) return nullptr;
		//Type *t=fresh(std::wstring(L"?")+ident);
		Type::pointer_t t=fresh();
		if (t.get()!=NULL) members.insert(make_pair(ident,t));
		return t;
	}

		/// Replace type of identifier given, environmentgains ownership of type.
	bool replace(const std::wstring &ident, Type::pointer_t type)
	{
		iterator iter=members.find(ident);
		if (iter==members.end()) return false;
		
		//delete iter->second; // TODO: Use deleteCopy?!
		iter->second=type;
		//members.insert(make_pair(ident,type));
		return true;
	}
	
	inline void remove(const std::wstring &ident)
	{
		members.erase(ident);
	}

	bool importInto(Environment *env)
	{
		for (auto member : members)
		{
			if (!env->put(member.first, member.second))
			{
				return false;

			}
		}
		return true;
	}

};

}
#endif