#ifndef TYPEEXPRESSION_H
#define TYPEEXPRESSION_H

#include "Type.h"
#include "Intro.h"

namespace intro {

class TypeVariableExpression : public TypeExpression
{
	std::wstring name;
	TypeExpression *super;
	Type::pointer_t mytype;
	Type::pointer_t mytop;
protected:
	virtual Type::pointer_t makeType(Environment *env, ErrorLocation *errors)
	{
		mytype = env->get(name);
		if (mytype.get() == nullptr)
		{
			//mytype=env->fresh(name,super->copy());
			//Type *mysuper = &mytop;
			Type::pointer_t mysuper;
			if (super != nullptr) mysuper = super->getType(env, errors);
			else mysuper.reset(new Type(Type::Top));
			mytype = env->fresh(name, mysuper);
			env->put(name, mytype);
		}
		return mytype;
	}

public:
	TypeVariableExpression(int l, int p, const wchar_t *varname)
		: TypeExpression(l, p)
		, name(varname)
		, super(nullptr)
		, mytype()
		//, mytop(Type::Top)
	{
	}

	TypeVariableExpression(int l, int p, const std::wstring &varname)
		: TypeExpression(l, p)
		, name(varname)
		, super(nullptr)
		, mytype()
		//, mytop(Type::Top)
	{
	}

	~TypeVariableExpression(void)
	{
		delete super;
	};

	inline void setSuper(TypeExpression *expr)
	{
		super = expr;
	};

	virtual Type::pointer_t getExposedType(void)
	{
		return mytype;
	};
};

class TypeSimpleExpression : public TypeExpression
{
	Type::Types type;
	Type::pointer_t mytype;
protected:
	virtual Type::pointer_t makeType(Environment *,ErrorLocation *)
	{
		mytype= Type::pointer_t (new Type(type));
		return mytype;
	};

public:
	TypeSimpleExpression(int l,int p,Type::Types type_) 
		: TypeExpression(l,p)
		, type(type_)
		, mytype(nullptr)
	{ }

	~TypeSimpleExpression(void)
	{
		//delete mytype;
	};

	virtual Type::pointer_t getExposedType(void)
	{
		return mytype;
	};
};


class TypeListExpression : public TypeExpression
{
	TypeExpression *elemtype;
	Type::pointer_t  myexptype;
	Type::pointer_t  mytype;
protected:
	virtual Type::pointer_t makeType(Environment *env, ErrorLocation *errors)
	{
		ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"list  type parameter"));
		Type::pointer_t inner = elemtype->getType(env,logger);
		if (inner->getKind() == Type::Error)
		{
			errors->addError(logger);
			mytype = Type::pointer_t(new ErrorType(getLine(), getColumn(), L"error in list type"));
		}
		else
		{
			delete logger;
			mytype = Type::pointer_t(new Type(Type::List, inner));
		}
		return mytype;
	};

public:
	TypeListExpression(int l,int p,TypeExpression *elems) 
		: TypeExpression(l,p)
		, elemtype(elems)
		//, myexptype(nullptr)
		, mytype(nullptr)
	{ };

	~TypeListExpression(void)
	{
		delete elemtype;
		//delete mytype;
		//delete myexptype;
	};

	virtual Type::pointer_t getExposedType(void)
	{ 
		if (myexptype.get() == nullptr) myexptype = Type::pointer_t(new Type(Type::List, elemtype->getExposedType()));
		return myexptype; 
	};
};

class TypeDictionaryExpression : public TypeExpression
{
	TypeExpression *keytype;
	TypeExpression *valtype;
	Type::pointer_t myval;
	Type::pointer_t mykey;
	Type::pointer_t mytype;
	Type::pointer_t myexptype;


protected:
	virtual Type::pointer_t makeType(Environment *env, ErrorLocation *errors)
	{
		ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"dictionary key type"));
		Type::pointer_t kt = keytype->getType(env,logger);
		if (kt->getKind() == Type::Error)
		{
			errors->addError(logger);
			mytype = Type::pointer_t (new ErrorType(getLine(), getColumn(), L"error in dictionary key type"));
			return mytype;
		}
		else delete logger;
		logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"dictionary value type"));
		Type::pointer_t vt = valtype->getType(env,logger);
		if (vt->getKind() == Type::Error)
		{
			errors->addError(logger);
			mytype = Type::pointer_t(new ErrorType(getLine(), getColumn(), L"error in dictionary value type"));
			return mytype;
		}
		else delete logger;
		mytype = Type::pointer_t (new Type(Type::Dictionary, kt));
		mytype->addParameter(vt);
		return mytype;
	};

public:
	TypeDictionaryExpression(int l,int p,TypeExpression *key,TypeExpression *val) 
		: TypeExpression(l,p)
		, keytype(key)
		, valtype(val)
		, myval(nullptr)
		, mykey(nullptr)
		, myexptype(nullptr)
	{ };

	~TypeDictionaryExpression(void)
	{
		delete keytype;
		delete valtype;
		//delete myval;
		//delete mykey;
		//delete myexptype;
	};

	virtual Type::pointer_t getExposedType(void)
	{
		if (myexptype.get() == nullptr)
		{
			myexptype = Type::pointer_t (new Type(Type::Dictionary, keytype->getExposedType()));
			myexptype->addParameter(valtype->getExposedType());
		}
		return myexptype;
	};

};

class TypeRecordExpression : public TypeExpression
{
	std::vector<std::pair<std::wstring,TypeExpression*> > members;
	Type::pointer_t myrec;
	Type::pointer_t myexptype;
protected:
	virtual Type::pointer_t makeType(Environment *env, ErrorLocation *errors)
	{
		RecordType *retval=new RecordType();
		std::vector<std::pair<std::wstring,TypeExpression*> >::iterator it;
		
		for (it = members.begin();it != members.end();it++)
		{
			ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"record member type for ")+it->first);
			Type::pointer_t rmt = it->second->getType(env, logger);
			if (rmt->getKind() == Type::Error)
				errors->addError(logger);
			else
				delete logger;
			retval->addMember(it->first, rmt);
		}
		if (errors->hasErrors())
		{
			delete retval;
			myrec = Type::pointer_t (new ErrorType(getLine(), getColumn(), L"error in dictionary type"));
		}
		else myrec = Type::pointer_t (retval);
		return myrec;
	};

public:
	TypeRecordExpression(int l, int p)
		: TypeExpression(l, p)
		, myrec(nullptr)
		, myexptype(nullptr)
	{
	};

	~TypeRecordExpression(void)
	{
		std::vector<std::pair<std::wstring,TypeExpression*> >::iterator it;
		for (it=members.begin();it!=members.end();it++)
			delete it->second;
		//delete myrec;
		//delete myexptype;
	};

	void addMember(const std::wstring &name,TypeExpression *te)
	{
		members.push_back(std::make_pair(name,te));
	};

	virtual Type::pointer_t getExposedType(void)
	{
		if (myexptype == nullptr)
		{
			RecordType *rec = new RecordType();
			std::vector<std::pair<std::wstring, TypeExpression*> >::iterator it;
			for (it = members.begin();it != members.end();it++)
				rec->addMember(it->first, it->second->getExposedType());
			myexptype = Type::pointer_t(rec);
		}
		return myexptype;
	};
};

class TypeVariantExpression : public TypeExpression
{
	std::vector<std::pair<std::wstring, TypeRecordExpression*> > tags;
	typedef std::vector<std::pair<std::wstring, TypeRecordExpression*> >::iterator iterator;
	Type::pointer_t myvariant;
	Type::pointer_t myexptype;
protected:
	virtual Type::pointer_t makeType(Environment *env, ErrorLocation *errors)
	{
		iterator it;
		VariantType *retval = new VariantType();
		for (it = tags.begin();it != tags.end();it++)
		{
			ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"variant type tag ") + it->first);
			Type::pointer_t rmt = it->second->getType(env, logger);
			if (rmt->getKind() == Type::Error)
				errors->addError(logger);
			else
				delete logger;
			retval->addTag(it->first, rmt);
		}
		if (errors->hasErrors())
		{
			delete retval;
			myvariant = Type::pointer_t(new ErrorType(getLine(), getColumn(), L"error in dictionary type"));
		}
		else myvariant = Type::pointer_t(retval);
		return myvariant;
	};

public:
	TypeVariantExpression(int l, int p)
		: TypeExpression(l, p)
		, myvariant(nullptr)
		, myexptype(nullptr)
	{

	}

	~TypeVariantExpression(void)
	{
		iterator it;
		for (it = tags.begin();it != tags.end();it++)
			delete it->second;
		//delete myvariant;
		//delete myexptype;
	};

	void addTag(const std::wstring &tag, TypeRecordExpression *contents)
	{
		tags.push_back(std::make_pair(tag, contents));
	};

	virtual Type::pointer_t getExposedType(void)
	{
		if (myexptype == nullptr)
		{
			VariantType *var = new VariantType();
			iterator it;
			for (it = tags.begin();it != tags.end();it++)
				var->addTag(it->first, it->second->getExposedType());
			myexptype = Type::pointer_t(var);
		}
		return myexptype;
	};
};

class TypeFunctionExpression : public TypeExpression
{
	std::vector<TypeExpression*> parameters;
	TypeExpression *returned;
	Type::pointer_t mytype;
	Type::pointer_t myexptype;
protected:
	virtual Type::pointer_t makeType(Environment *env, ErrorLocation *errors)
	{
		ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"function return type "));
		Type::pointer_t rt = returned->getType(env,logger);
		if (rt->getKind() == Type::Error)
		{
			errors->addError(logger);
			mytype = Type::pointer_t(new ErrorType(getLine(), getColumn(), L"error in function return type"));
			return mytype;
		}
		else delete logger;
		FunctionType *ft=new FunctionType(rt);
		std::vector<TypeExpression*>::iterator it;
		for (it = parameters.begin();it != parameters.end();it++)
		{
			logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"function parameter type "));
			Type::pointer_t pt = (*it)->getType(env,logger);
			if (pt->getKind() == Type::Error)
			{
				delete ft;
				errors->addError(logger);
				mytype = Type::pointer_t(new ErrorType(getLine(), getColumn(), L"error in function parameter type"));
				return mytype;
			}
			else delete logger;
			ft->addParameter(pt);
		}
		mytype = Type::pointer_t(ft);
		return mytype;
	};

public:
	TypeFunctionExpression(int l,int p) 
		: TypeExpression(l,p)
		, returned(nullptr)
		, mytype(NULL)
		, myexptype(NULL)
	{ };

	~TypeFunctionExpression(void)
	{
		delete returned;
		std::vector<TypeExpression*>::iterator it;
		for (it=parameters.begin();it!=parameters.end();it++)
			delete *it;
		//delete mytype;
		//delete myexptype;
	};

	void setReturnType(TypeExpression *ret)	{ returned=ret; };

	void addParameter(TypeExpression *te)
	{
		parameters.push_back(te);
	};

	virtual Type::pointer_t getExposedType(void)
	{ 
		if (myexptype == nullptr)
		{
			myexptype = std::make_shared<FunctionType>(returned->getExposedType());
			FunctionType *func = (FunctionType *)myexptype.get();
			std::vector<TypeExpression*>::iterator it;
			for (it = parameters.begin();it != parameters.end();it++)
				myexptype->addParameter((*it)->getExposedType());
			
		}
		return myexptype;
	};

};

class TypeOpaqueExpression : public TypeExpression
{
	std::vector<TypeExpression*> parameters;
	std::wstring name;
	Type::pointer_t myhidden;
	Type::pointer_t myexptype;
protected:
	virtual Type::pointer_t makeType(Environment *env, ErrorLocation *errors)
	{
		myhidden = NULL;
		Type::pointer_t buf = env->get(name);
		if (buf == NULL)
		{
			myhidden = Environment::fresh();
		}
		else if (buf->getKind() != Type::UserDef)
		{
			std::wstring str = L"The identifier '" + name + L"' already exists but does not refer to a user defined type.";
			errors->addError(new ErrorDescription(getLine(), getColumn(), str));
			return getError(str);
		}
		else
		{
			OpaqueType *other = dynamic_cast<OpaqueType*>(buf.get());
			std::vector<Type::pointer_t > paramtypes;
			std::vector<TypeExpression*>::iterator pit;
			ErrorLocation *logger = new ErrorLocation(getLine(), getColumn(), std::wstring(L"opaque type parameters"));
			for (pit = parameters.begin();pit != parameters.end();pit++)
			{
				Type::pointer_t paramtype = (*pit)->getType(env,logger);
				if (paramtype->getKind() == Type::Error)
				{
					errors->addError(logger);
					return paramtype;
				}
				else if (paramtype->getKind() != Type::Variable)
				{
					errors->addError(new ErrorDescription(getLine(), getColumn(), L"Parameters in opaque types must be variables!"));
					return getError(L"Parameters in opaque types must be variables!");
				};
				paramtypes.push_back(paramtype);
			}
			delete logger;
			// Instantiate Opaque type here!
			myhidden = other->instantiate(paramtypes, getLine(), getColumn());
		}
		return myhidden;
	};

public:
	TypeOpaqueExpression(int l,int p, const std::wstring &n) 
		: TypeExpression(l,p)
		, name(n)
		, myhidden(NULL)
		, myexptype(NULL)
	{ };

	~TypeOpaqueExpression(void)
	{
		std::vector<TypeExpression*>::iterator it;
		for (it=parameters.begin();it!=parameters.end();it++)
			delete *it;
		//delete myhidden;
		//delete myexptype;
	};

	std::wstring getName(void) { return name; };

	void addParameter(TypeExpression *te)
	{
		parameters.push_back(te);
	};

	virtual Type::pointer_t getExposedType(void)
	{ 
//		return myexptype;
		//OpaqueType *ret=new OpaqueType(*dynamic_cast<OpaqueType*>(myexptype));
		if (myexptype == nullptr)
		{
			OpaqueType *ot = new OpaqueType(name);
			std::vector<TypeExpression*>::iterator pit;
			for (pit = parameters.begin();pit != parameters.end();pit++)
			{
				Type::pointer_t paramtype = (*pit)->getExposedType();
				if (paramtype->getKind() == Type::Error) return paramtype;
				else if (paramtype->getKind() != Type::Variable)
				{
					return getError(L"Parameters in opaque types must be variables!");
				};
				ot->addParameter(paramtype);
			}
			myexptype = Type::pointer_t(Type::pointer_t(ot));
		}
		return myexptype;

	};
};

class TypeGeneratorExpression : public TypeExpression
{
	TypeExpression *valtype;
	Type::pointer_t mytype;
	Type::pointer_t myexptype;
protected:
	virtual Type::pointer_t makeType(Environment *env, ErrorLocation *errors)
	{
		mytype = Type::pointer_t(new Type(Type::Generator, valtype->getType(env, errors)));
		return mytype;
	};

public:
	TypeGeneratorExpression(int l, int p, TypeExpression *val)
		: TypeExpression(l, p)
		, valtype(val)
		//, mytype(nullptr)
		//, myexptype(NULL)
	{ }

	~TypeGeneratorExpression(void)
	{
		delete valtype;
		//delete mytype;
		//delete myexptype;
	}

	virtual Type::pointer_t getExposedType(void)
	{
		if (myexptype == nullptr)
			myexptype = Type::pointer_t(new Type(Type::Generator, valtype->getExposedType()));
		return myexptype;
	}

};

}
#endif