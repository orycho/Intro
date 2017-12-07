#ifndef MODULE_H
#define MODULE_H

#include "Type.h"
#include "Environment.h"

namespace intro
{


/// A module statement defines a module
class ModuleStatement : public Statement
{
	/// Base class for export declarations in a module's interface definition
	class ExportDeclaration
	{
	protected:
		/// Position where the module starts
		int line, pos;
		std::wstring name;
	public:

		ExportDeclaration(int l,int p,const std::wstring &n)
			: line(l)
			, pos(p)
			, name(n)
		{};

		virtual ~ExportDeclaration(void)
		{};

		virtual bool isExport(void) { return true; };

		std::wstring getName(void) { return name; };

		virtual Type *getType(Environment *env)=0;

		virtual void print(std::wostream &s)=0;
	};

	/// Class representing an opaque type definition.
	class OpaqueTypeDeclaration : public ExportDeclaration
	{
		std::list<TypeExpression*> parameters;
		OpaqueType *ot;
	public:
		OpaqueTypeDeclaration(int l,int p,const std::wstring &n,std::list<TypeExpression*> params)
			: ExportDeclaration(l,p,n)
			, parameters(params)
			, ot(NULL)
		{};

		~OpaqueTypeDeclaration(void)
		{
			std::list<TypeExpression*>::iterator it;
			for (it=parameters.begin();it!=parameters.end();it++)
				delete *it;
			if (ot!=NULL) delete ot;
		};

		virtual bool isExport(void) { return false; };

		virtual Type *getType(Environment *env)
		{
			Environment localenv(env);
			ot=new OpaqueType(name);
			std::list<TypeExpression*>::iterator it;
			// Grammar guarantees that only type variables can occur here...
			for (it=parameters.begin();it!=parameters.end();it++)
				ot->addParameter(dynamic_cast<TypeVariable*>((*it)->getType(&localenv)));
			env->put(name,ot);
			return ot;
		};

		virtual void print(std::wostream &s)
		{
			s << name;
			if (parameters.empty()) return;
			s << "(";
			std::list<TypeExpression*>::iterator it=parameters.begin();
			(*it)->print(s);
			it++;
			while (it!=parameters.end())
			{
				s << ",";
				(*it)->print(s);
				it++;
			}
			s << ")" << std::endl;;
			
		};
	};

	/// Class representing a member value export declaration
	class MemberTypeDeclaration : public ExportDeclaration
	{
		TypeExpression *expr;
		Type *type;
	public:
		MemberTypeDeclaration(int l,int p,const std::wstring &n,TypeExpression *te)
			: ExportDeclaration(l,p,n), expr(te), type(NULL)
		{};

		~MemberTypeDeclaration(void)
		{
			delete expr;
		};

		virtual Type *getType(Environment *env)
		{
			Environment localenv(env);
			type=expr->getType(env);
			env->put(name,type);
			return type;
		};

		Type *getExposedType(void)
		{
			return expr->getExposedType();
		};

		virtual void print(std::wostream &s)
		{
			s << name << " : ";
			expr->getExposedType()->print(s);
			s << std::endl;
		};
	};

	/// The name of this module
	std::wstring name;
	/// Is the path to the module relative?
	bool relative; 
	/// The path to the Module
	std::list<std::wstring> path; 

	/// Map names of submodules to their module.
	std::map<std::wstring,ModuleStatement*> submodules;
	/// Exported types of the module
	std::list<ExportDeclaration*> exports;
	/// The content of the module is executed to create the values therein.
	std::list<Statement*> contents;

	std::map<std::wstring,Type*> expout;

public:
	ModuleStatement(int l,int p,bool rel,const std::list<std::wstring> &pa) 
		: Statement(l,p)
		, relative(rel)
		, path(pa)
	{};

	virtual ~ModuleStatement(void)
	{
		std::list<ExportDeclaration*>::iterator eit;
		for (eit=exports.begin();eit!=exports.end();eit++)
			delete *eit;
		std::list<Statement*>::iterator cit; 
		for (cit=contents.begin();cit!=contents.end();cit++)
			delete *cit;
		std::map<std::wstring,Type*>::iterator expit;
		for (expit=expout.begin();expit!=expout.end();expit++)
			deleteCopy(expit->second);
	};

	inline std::wstring getName(void) { return name; };

	inline void addExport(int line,int pos,const std::wstring &n,TypeExpression *t)
	{
		MemberTypeDeclaration *otd=new MemberTypeDeclaration(line,pos,n,t);
		exports.push_back(otd);
	};

	inline void addOpaque(int line,int pos,const std::wstring &n,std::list<TypeExpression*> params)
	{
		OpaqueTypeDeclaration *otd=new OpaqueTypeDeclaration(line,pos,n,params);
		exports.push_back(otd);
	};

	inline void addContent(intro::Statement *s)
	{
		contents.push_back(s);
	};

	inline void addModule(ModuleStatement *m)
	{
		submodules.insert(make_pair(m->getName(),m));
	};

	bool makeType(Environment *env)
	{
		Environment::Module *module;
		if (relative) module=Environment::getCurrentModule()->getCreatePath(path);
		else module=Environment::getRootModule()->getCreatePath(path);

		Environment localenv(env);
		// Iterate over the statements comprising the module body and infer their types.
		std::list<Statement*>::iterator cit; 
		bool success=true;
		for (cit=contents.begin();cit!=contents.end();cit++)
		{
			success &= (*cit)->makeType(&localenv);
			if (success) (*cit)->print(std::wcout);
		}
		if (!success) return false;
		Environment::pushModule(module);
		// For each opaque type declaration in the exposed interface,
		// find the corresponding constructor
		// The following section also verifies that the interface of the module 
		// corresponds to the implementation. This is done by verifying the exposed type is 
		// a subtype (less or equal) of the internal type.
		// E.g., note that internal type first([first:?A])->?A is a subtype of 
		// exposed type first(Pair(?A,?B))->?A = first([first:?A,second:?B])->?A.
		// However, this part also exports the constructors, as it picks them out
		// anyways, knowing it is a constructor and which type it is for.
		std::list<ExportDeclaration*>::iterator exported;
		Environment exportenv(env);

		for (exported= exports.begin();exported!=exports.end() && success;exported++)
		{
			Type *exptype=(*exported)->getType(&exportenv);
			if ((*exported)->isExport()) 
			{
				Type *inside=localenv.get((*exported)->getName());
				if (inside==NULL)
				{
					// some form of error treatment/notification
					continue;
				}
				success&=isLessOrEqual(inside->checkSubtype(exptype));
				MemberTypeDeclaration *mt=dynamic_cast<MemberTypeDeclaration*>(*exported);
				module->addExport((*exported)->getName(),mt->getExposedType());
			}
			else	// Opaque Type declaration
			{
				// Look for variable of same name in environment
				OpaqueType *ot=dynamic_cast<OpaqueType*>(exptype);
				Type *inside=localenv.get(ot->getName());
				if (inside==NULL)
				{
					// Need error message? Or is it ok if there is no "standard"-ctor?
					success=false;
					continue;
				}
				if (inside->getKind()!=Type::Function)
				{
					// Need error message!
					success=false;
					continue;
				}
				FunctionType *ft=dynamic_cast<FunctionType*>(inside);
				success&=ot->setTypeMapping(ft);
				// We build up a function that has the same paramters as the constructor,
				// and assigns those same parameters to the opaque type returned
				OpaqueType *returned=new OpaqueType(*ot);
				FunctionType *ctor=new FunctionType(returned);
				OpaqueType::iterator iter;
				for (iter=returned->begin();iter!=returned->end();iter++)
				{
					ctor->addParameter(iter->first);
				}
				module->addExport((*exported)->getName(),ctor);
			}
		}
		Environment::popModule();
		return success;
	};

	inline void printPath(std::wostream &s)
	{
		if (!relative) s << "::";
		std::list<std::wstring>::iterator pit = path.begin();
		s << *pit;
		for (pit++;pit != path.end();pit++) s << "::" << *pit;
	}

	void print(std::wostream &s)
	{
		s << "module ";
		/*if (!relative) s << "::";
		std::list<std::wstring>::iterator pit=path.begin();
		s << *pit;
		for	(pit++;pit!=path.end();pit++) s << "::" << *pit;
		*/
		printPath(s);
		s << " exports\n";
		std::list<ExportDeclaration*>::iterator eit;	
		for (eit=exports.begin();eit!=exports.end();eit++)
		{
			(*eit)->print(s);
		}
		s << "from\n";
		std::list<Statement*>::iterator cit; 
		for (cit=contents.begin();cit!=contents.end();cit++)
		{
			(*cit)->print(s);
			s << ";\n";
		}
		s << "end.\n";
	};

	virtual void getFreeVariables(VariableSet &free,VariableSet &bound)
	{
		// to be done
	};

	virtual bool codeGen(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env);

	virtual void collectFunctions(std::list<intro::Function*> &funcs)
	{
		// to be done!!!
	};

	virtual size_t countVariableStmts(void)
	{
		return 0;
	}

};

}

#endif