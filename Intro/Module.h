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
		size_t line, col;
		std::wstring name;
	public:

		ExportDeclaration(size_t l, size_t p,const std::wstring &n)
			: line(l)
			, col(p)
			, name(n)
		{};

		virtual ~ExportDeclaration(void)
		{};

		virtual bool isExport(void) { return true; };

		std::wstring getName(void) { return name; };

		virtual Type::pointer_t getType(Environment *env, ErrorLocation *errors)=0;

		virtual void print(std::wostream &s)=0;
	};

	/// Class representing an opaque type definition.
	class OpaqueTypeDeclaration : public ExportDeclaration
	{
		std::vector<TypeExpression*> parameters;
		Type::pointer_t ot;
	public:
		OpaqueTypeDeclaration(size_t l, size_t p,const std::wstring &n,std::vector<TypeExpression*> params)
			: ExportDeclaration(l,p,n)
			, parameters(params)
			, ot(NULL)
		{};

		virtual ~OpaqueTypeDeclaration(void)
		{
			std::vector<TypeExpression*>::iterator it;
			for (it=parameters.begin();it!=parameters.end();it++)
				delete *it;
			//if (ot!=NULL) delete ot;
		};

		virtual bool isExport(void) { return false; };

		virtual Type::pointer_t getType(Environment *env, ErrorLocation *errors)
		{
			Environment localenv(env);
			ot= Type::pointer_t (new OpaqueType(name));
			std::vector<TypeExpression*>::iterator it;
			// Grammar guarantees that only type variables can occur here...
			for (it = parameters.begin();it != parameters.end();it++)
			{
				((OpaqueType*)ot.get())->addParameter((*it)->getType(&localenv, errors));
			}
			env->put(name,ot);
			return ot;
		};

		virtual void print(std::wostream &s)
		{
			s << name;
			if (parameters.empty()) return;
			s << "(";
			std::vector<TypeExpression*>::iterator it=parameters.begin();
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
		Type::pointer_t type;
	public:
		MemberTypeDeclaration(int l,int p,const std::wstring &n,TypeExpression *te)
			: ExportDeclaration(l,p,n), expr(te), type(NULL)
		{};

		virtual ~MemberTypeDeclaration(void)
		{
			delete expr;
		};

		virtual Type::pointer_t getType(Environment *env, ErrorLocation *errors)
		{
			Environment localenv(env);
			type=expr->getType(env,errors);
			env->put(name,type);
			return type;
		};

		Type::pointer_t getExposedType(void)
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
	std::vector<std::wstring> path; 

	/// Map names of submodules to their module.
	std::map<std::wstring,ModuleStatement*> submodules;
	/// Exported types of the module
	std::vector<ExportDeclaration*> exports;
	/// The content of the module is executed to create the values therein.
	std::vector<Statement*> contents;

	std::map<std::wstring, Type::pointer_t> expout;

public:
	ModuleStatement(int l,int p,bool rel,const std::vector<std::wstring> &pa) 
		: Statement(l,p)
		, relative(rel)
		, path(pa)
	{};

	virtual ~ModuleStatement(void)
	{
		std::vector<ExportDeclaration*>::iterator eit;
		for (eit=exports.begin();eit!=exports.end();eit++)
			delete *eit;
		std::vector<Statement*>::iterator cit; 
		for (cit=contents.begin();cit!=contents.end();cit++)
			delete *cit;
	};

	inline std::wstring getName(void) { return name; };

	inline void addExport(size_t line, size_t col,const std::wstring &n,TypeExpression *t)
	{
		MemberTypeDeclaration *otd=new MemberTypeDeclaration(line,col,n,t);
		exports.push_back(otd);
	};

	inline void addOpaque(size_t line, size_t col,const std::wstring &n,std::vector<TypeExpression*> params)
	{
		OpaqueTypeDeclaration *otd=new OpaqueTypeDeclaration(line,col,n,params);
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

	bool makeType(Environment *env, ErrorLocation *errors)
	{
		Environment::Module *module;
		if (relative) module=Environment::getCurrentModule()->getCreatePath(path);
		else module=Environment::getRootModule()->getCreatePath(path);

		Environment localenv(env);
		// Iterate over the statements comprising the module body and infer their types.
		std::vector<Statement*>::iterator cit; 
		bool success=true;
		std::wstringstream strs;
		strs << L"module ";
		printPath(strs);
		ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), strs.str()+L" contents");
		for (cit=contents.begin();cit!=contents.end();cit++)
		{
			success &= (*cit)->makeType(&localenv,logger);
			//if (success) (*cit)->print(std::wcout);
		}
		if (success) delete logger;
		else return false;
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
		std::vector<ExportDeclaration*>::iterator exported;
		Environment exportenv(env);
		logger = new ErrorLocation(getLine(), getColumn(), strs.str()+L" interface");
		for (exported= exports.begin();exported!=exports.end() && success;exported++)
		{
			Type::pointer_t exptype=(*exported)->getType(&exportenv,logger);
			if ((*exported)->isExport()) 
			{
				Type::pointer_t inside=localenv.get((*exported)->getName());
				if (inside==NULL)
				{
					// some form of error treatment/notification
					continue;
				}
				success&=isLessOrEqual(inside->checkSubtype(exptype));
				MemberTypeDeclaration *mt=dynamic_cast<MemberTypeDeclaration*>(*exported);
				module->addExport((*exported)->getName(),mt->getExposedType(),false);
			}
			else	// Opaque Type declaration
			{
				// Look for variable of same name in environment
				OpaqueType *ot = dynamic_cast<OpaqueType*>(exptype.get());
				Type::pointer_t inside = localenv.get(ot->getName());
				if (inside == NULL)
				{
					// Need error message? Or is it ok if there is no "standard"-ctor?
					success = false;
					continue;
				}
				if (inside->getKind() != Type::Function)
				{
					// Need error message!
					success = false;
					continue;
				}
				FunctionType *ft = dynamic_cast<FunctionType*>(inside.get());
				success &= ot->setTypeMapping(ft);
				// We build up a function type for the constructor,
				// with the opaque type's parameter types going
				// into the functions parameters' types.
				// The type is constructed completely on the heap,
				// so that ownership can be passed to the module, 
				// which will delete it eventually.
				//OpaqueType *returned=new OpaqueType(*ot);
				//FunctionType *ctor=new FunctionType(returned);
				Type::pointer_t returned = Type::pointer_t (new OpaqueType(*ot));
				Type::pointer_t ctor = Type::pointer_t (new FunctionType(returned));

				OpaqueType::iterator iter;
				for (iter = ((OpaqueType*)returned.get())->begin();iter != ((OpaqueType*)returned.get())->end();iter++)
				{
					ctor->addParameter(iter->first);
				}
				module->addExport((*exported)->getName(), ctor);
			}
		}
		Environment::popModule();
		if (success) delete logger;
		return success;
	};

	inline void printPath(std::wostream &s)
	{
		if (!relative) s << "::";
		std::vector<std::wstring>::iterator pit = path.begin();
		s << *pit;
		for (pit++;pit != path.end();pit++) s << "::" << *pit;
	}

	void print(std::wostream &s)
	{
		s << "module ";
		/*if (!relative) s << "::";
		std::vector<std::wstring>::iterator pit=path.begin();
		s << *pit;
		for	(pit++;pit!=path.end();pit++) s << "::" << *pit;
		*/
		printPath(s);
		s << " exports\n";
		std::vector<ExportDeclaration*>::iterator eit;	
		for (eit=exports.begin();eit!=exports.end();eit++)
		{
			(*eit)->print(s);
		}
		s << "from\n";
		std::vector<Statement*>::iterator cit; 
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

	virtual void collectFunctions(std::vector<intro::Function*> &funcs)
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