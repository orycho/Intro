#ifndef TYPEEXPRESSION_H
#define TYPEEXPRESSION_H

#include "Type.h"
#include "Intro.h"

namespace intro {

class TypeVariableExpression : public TypeExpression
{
	std::wstring name;
	Type *super;
	Type *mytype;

protected:
	virtual Type *makeType(Environment *env)
	{
		mytype=env->get(name);
		if (mytype==NULL) 
		{
			//mytype=env->fresh(name,super->copy());
			mytype = env->fresh(name, super);
			env->put(name,mytype);
		}
		return mytype;
	};

public:
	TypeVariableExpression(int l,int p,const wchar_t *varname) : TypeExpression(l,p), name(varname)
	{
		super=new Type(Type::Top);
	};

	TypeVariableExpression(int l,int p,const std::wstring &varname) : TypeExpression(l,p), name(varname)
	{ 
		super=new Type(Type::Top);
	};

	~TypeVariableExpression(void)
	{
		//delete super;
		deleteCopy(super);
	};

	virtual Type *getExposedType(void)
	{
		return mytype;
	};
};

class TypeSimpleExpression : public TypeExpression
{
	Type::Types type;
	Type *mytype;
protected:
	virtual Type *makeType(Environment *)
	{
		mytype=new Type(type);
		return mytype;
	};

public:
	TypeSimpleExpression(int l,int p,Type::Types mytype) : TypeExpression(l,p), type(mytype)
	{ };

	~TypeSimpleExpression(void)
	{
		delete mytype;
	};

	virtual Type *getExposedType(void)
	{
		return mytype;
	};
};


class TypeListExpression : public TypeExpression
{
	TypeExpression *elemtype;
	Type *myexptype,*mytype;
protected:
	virtual Type *makeType(Environment *env)
	{
		mytype=new Type(Type::List,elemtype->getType(env));
		return mytype;
	};

public:
	TypeListExpression(int l,int p,TypeExpression *elems) : TypeExpression(l,p), elemtype(elems)
	{ myexptype=NULL; };

	~TypeListExpression(void)
	{
		delete elemtype;
		delete mytype;
		delete myexptype;
	};

	virtual Type *getExposedType(void)
	{ 
		if (myexptype == nullptr) myexptype = new Type(Type::List, elemtype->getExposedType());
		return myexptype; 
	};
};

class TypeDictionaryExpression : public TypeExpression
{
	TypeExpression *keytype;
	TypeExpression *valtype;
	Type *myval,*mykey,*mytype,*myexptype;

protected:
	virtual Type *makeType(Environment *env)
	{
		mytype=new Type(Type::Dictionary,keytype->getType(env));
		mytype->addParameter(valtype->getType(env));
		return mytype;
	};

public:
	TypeDictionaryExpression(int l,int p,TypeExpression *key,TypeExpression *val) : TypeExpression(l,p), keytype(key), valtype(val)
	{ myval=NULL; mykey=NULL; };

	~TypeDictionaryExpression(void)
	{
		delete keytype;
		delete valtype;
		delete myval;
		delete mykey;
		delete myexptype;
	};

	virtual Type *getExposedType(void)
	{
		if (myexptype == nullptr)
		{
			myexptype = new Type(Type::Dictionary, keytype->getExposedType());
			myexptype->addParameter(valtype->getExposedType());
		}
		return myexptype;
	};

};

class TypeRecordExpression : public TypeExpression
{
	std::list<std::pair<std::wstring,TypeExpression*> > members;
	RecordType *myrec,*myexptype;
protected:
	virtual Type *makeType(Environment *env)
	{
		RecordType *myrec=new RecordType();
		std::list<std::pair<std::wstring,TypeExpression*> >::iterator it;
		for (it=members.begin();it!=members.end();it++)
			myrec->addMember(it->first,it->second->getType(env));
		return myrec;
	};

public:
	TypeRecordExpression(int l,int p) : TypeExpression(l,p)
	{
		myrec = NULL; 
		myexptype = nullptr;
	};

	~TypeRecordExpression(void)
	{
		std::list<std::pair<std::wstring,TypeExpression*> >::iterator it;
		for (it=members.begin();it!=members.end();it++)
			delete it->second;
		delete myrec;
		delete myexptype;
	};

	void addMember(const std::wstring &name,TypeExpression *te)
	{
		members.push_back(std::make_pair(name,te));
	};

	virtual Type *getExposedType(void)
	{ 
		if (myexptype == nullptr)
		{
			myexptype = new RecordType();
			std::list<std::pair<std::wstring, TypeExpression*> >::iterator it;
			for (it = members.begin();it != members.end();it++)
				myexptype->addMember(it->first, it->second->getExposedType());
		}
		return myexptype;
	};
};

class TypeFunctionExpression : public TypeExpression
{
	std::list<TypeExpression*> parameters;
	TypeExpression *returned;
	FunctionType *mytype,*myexptype;
protected:
	virtual Type *makeType(Environment *env)
	{
		mytype=new FunctionType(returned->getType(env));
		std::list<TypeExpression*>::iterator it;
		for (it=parameters.begin();it!=parameters.end();it++)
			mytype->addParameter((*it)->getType(env));
		return mytype;
	};

public:
	TypeFunctionExpression(int l,int p) : TypeExpression(l,p), mytype(NULL), myexptype(NULL)
	{ };

	~TypeFunctionExpression(void)
	{
		delete returned;
		std::list<TypeExpression*>::iterator it;
		for (it=parameters.begin();it!=parameters.end();it++)
			delete *it;
		delete mytype;
		delete myexptype;
	};

	void setReturnType(TypeExpression *ret)	{ returned=ret; };

	void addParameter(TypeExpression *te)
	{
		parameters.push_back(te);
	};

	virtual Type *getExposedType(void)
	{ 
		if (myexptype == nullptr)
		{
			myexptype = new FunctionType(returned->getExposedType());
			std::list<TypeExpression*>::iterator it;
			for (it = parameters.begin();it != parameters.end();it++)
				myexptype->addParameter((*it)->getExposedType());
		}
		return myexptype;
	};

};

class TypeOpaqueExpression : public TypeExpression
{
	std::list<TypeExpression*> parameters;
	std::wstring name;
	Type *myhidden;
	OpaqueType *myexptype;
protected:
	virtual Type *makeType(Environment *env)
	{
		myhidden=NULL;
		Type *buf=env->get(name);
		if (buf==NULL)
		{
			myhidden=Environment::fresh();
		}
		else if (buf->getKind()!=Type::UserDef)
		{
			return getError(std::wstring(L"The identifier '")+name+L"' already exists but does not refer to a user defined type.");
		}
		else 
		{
			OpaqueType *other=dynamic_cast<OpaqueType*>(buf);
			std::list<TypeVariable*> paramtypes;
			std::list<TypeExpression*>::iterator pit;
			for (pit=parameters.begin();pit!=parameters.end();pit++)
			{
				Type *paramtype=(*pit)->getType(env);
				if (paramtype->getKind()==Type::Error) return paramtype;
				else if (paramtype->getKind()!=Type::Variable)
				{
					return getError(L"Parameters in opaque types must be variables!");
				};				
				paramtypes.push_back(dynamic_cast<TypeVariable*>(paramtype));
			}
			// Instantiate Opaque type here!
			myhidden=other->instantiate(paramtypes,getLine(),getPosition());
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
		std::list<TypeExpression*>::iterator it;
		for (it=parameters.begin();it!=parameters.end();it++)
			delete *it;
		delete myhidden;
		delete myexptype;
	};

	std::wstring getName(void) { return name; };

	void addParameter(TypeExpression *te)
	{
		parameters.push_back(te);
	};

	virtual Type *getExposedType(void)
	{ 
//		return myexptype;
		//OpaqueType *ret=new OpaqueType(*dynamic_cast<OpaqueType*>(myexptype));
		if (myexptype == nullptr)
		{
			myexptype = new OpaqueType(name);
			std::list<TypeExpression*>::iterator pit;
			for (pit = parameters.begin();pit != parameters.end();pit++)
			{
				Type *paramtype = (*pit)->getExposedType();
				if (paramtype->getKind() == Type::Error) return paramtype;
				else if (paramtype->getKind() != Type::Variable)
				{
					return getError(L"Parameters in opaque types must be variables!");
				};
				myexptype->addParameter(dynamic_cast<TypeVariable*>(paramtype));
			}
		}
		return myexptype;

	};
};

class TypeGeneratorExpression : public TypeExpression
{
	TypeExpression *valtype;
	Type *mytype,*myexptype;
protected:
	virtual Type *makeType(Environment *env)
	{
		mytype=new Type(Type::Generator,valtype->getType(env));
		return mytype;
	};

public:
	TypeGeneratorExpression(int l,int p,TypeExpression *val) : TypeExpression(l,p), valtype(val)
	{ myexptype=NULL; };

	~TypeGeneratorExpression(void)
	{
		delete valtype;
		delete mytype;
		delete myexptype;
	};

	virtual Type *getExposedType(void)
	{ 
		if (myexptype == nullptr) myexptype=new Type(Type::Generator,valtype->getExposedType());
		return myexptype;
	};

};

}
#endif