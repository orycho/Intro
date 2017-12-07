#ifndef TYPE_H
#define TYPE_H

#include <map>
#include <set>
#include <list>
#include <stack>
#include <string>
#include <vector>
#include <memory>
#include <iostream>

//#include "ProductList.h"
#include "RTType.h"

namespace intro
{

class Type;
class TypeVariable;
class OpaqueType;

// Collect all types in a type (e.g. type parameters and labels in records have types)
void collectAllTypes(Type *t, std::set<Type*> &items);
/// safely delete a type
void deleteCopy(Type *type);
void deleteExcept(Type *t,std::set<Type*> exclude);

typedef std::set<TypeVariable*> TypeVariableSet;

//typedef ::std::tr1::shared_ptr<Type> pType;

enum PartialOrder
{
	ERROR=-2,
	LESS =-1,
	EQUAL=0,
	GREATER=1
};

std::wostream& operator<<(std::wostream& os, PartialOrder po);

inline bool isLessOrEqual(PartialOrder value)
{
	return value==LESS || value==EQUAL;
}

/// Base class for classes representing types available in intro.
/** <h2>Overview</h2>
	<p>The Type object and it's subclasses implement the type system. Types are essentially represented
	as directed acyclic graphs. The base class encodes all basic, n-ary types. 
	All types may have a supertype, which defaults to the type instance itself in most cases. 
	Only type variables default to super type Top, which means they can be any type.</p>

	<p>To facilitate unification, the type objects implement a union-find agorithm, which makes use of the
	parent member. To retrieve the actual type after unification that a type represents, 
	the find method is used.</p>

	<p>See the documentation for class Environment to learn how the union find algorithm plays
	an important role of providing safe polymorphic type inference for an imperative language.</p>

	<h2>Main Algorithms and derived classes</h2>
	<p>Type inference is implemented by a set of interoperating methods in Type, along with
	derived classes knowledge about their internal structure. The latter is important when
	inductively infer the types.</p>

	<p>The fundamental operation is unification of type terms in unify(). The corresponding
	virtual method is internalUnify(), which in the base class handles simple types (i.e. those 
	without any parameterse or otherwise nested types like Integer and String).
	unify() is responsible for calling makeunion on all nested types, if any.</p>

	<p>The result of unification is computed and represented with the union/find algorithm. Since
	union is a reserved work in C/C++, the method is called makeunion(), and it has no internal variant.
	Substitutions of variables may be subject to "upper bound) constraints, in the form of subtype
	restrictions on the substitutions. The Top type is used as the upper bound if any type
	is permitted. These checks are performed by checkSubtype, and they control if substitution
	is legal and which type (the less general) will be the substitute (i.e. appear in the result).</p>

	<p>The checkSubtype method provides a partial order for all types. Substitutions
	are only legal between types that are comparable in the partial order. More visually,
	it defines the subtype relations between the various types, like Integer&lt;Real or
	List(?A)&lt;:Generator(?A). This Job is again split up, this time into three components,
	the driver method checkSubType() which handles Top and Error cases, the virtual method 
	internalCheckSubType() for methods specific to the concrete type, and use of the TypeGraph
	to determine how sub- and supertype are related.</p>

	<h2>Memory Management (Type Inference)</h2>
	<p>The types and type variables in the type inference process are built with an intricate weave of pointers.
	But important are the two phases using types: as mentioned, type inference makes things tricky, so
	it is simplest to define that all type instances are owned by the AST nodes that creted them,
	and must be deleted by them when the AST is discarded.
	<p>The only exception is function application, since applying the union/find algorithm
	would reduce the function type in the environment to a specific instantiation. Thus,
	application creates a copy of the function type that it can unify with the actual paramters,
	and for which it becomes responsible, according to the rule in above paragraph,</p>
	<p> Variables are tricky on their own, since their repeated occurences imply
	constraints accross several parameter types. It may be possible to simply have the AST node
	be responsible for the variables it creates as well, but for now these are handled by
	putting each in a static list on creation, and deleting them when the 
	program exits...
	</p>
	@see Environment::fresh()
	<p>Since the AST nodes are responsible for the them, all intermediary types are safe as long as the AST 
	exists. Also, as described Application needs to operate on a copy to keep the environment unmodified.
	Thus, the environment itself never needs to own types, and just holds pointers to the types
	owned by the AST.</p>
	<h2>Memory Management (Code Generation)</h2>
	<p>During code generation, the only types that matter are the results of the type inference process,
	that is, the final types associated with each AST node (respecting union/find). These are
	used to avoid runtime checks obviated by type safety, and to generate the correct code
	for the statically inferred types.</p>
	<p>The only cases where types must be handled by the code generation are when 
	polymorphic operations require multiple instances of the same function. This
	however is localized to a single operation of the code generation infrastructure
	in that the types are not returned from it.</p>

	@see Environment
	@see TypeGraph
*/
class Type
{
public:
	/// TypeVariableMaps are used in several operations that need to preserve a types internal structure, e.g. copy.
	typedef std::map<Type*,Type*> TypeVariableMap;
	typedef std::vector<Type*>::iterator iterator;
	typedef std::vector<Type*>::const_iterator const_iterator;

	/// All kinds of type known to the language
	enum Types : unsigned int
	{
		// Mono Types
		Error, ///< a.k.a Bottom, contains error message
		Unit,
		Variable, ///< not really a type, but a (possibly constrained) placeholder for a type
		Comparable,
		Number,
		Boolean,
		Integer,
		Real,
		String,
		// polytypes
		UserDef,	///< represents all user defined types via class OpaqueType supported by Typegraph
		Sequence,
		Function,
		List,
		Dictionary,
		Record,
		Generator,
		Variant,
		/** top type, actually mono but supertype of everything including polytypes. 
			(Only possible supertype is itself). Must always be the last entry in this enumeration.
		*/
		Top
	};

	/// The access control flags
	enum Flags
	{
		Readable	= 1 << 0,
		Writable	= 1 << 1,
	};

private:
	Types type; ///< The instances kind of type, actually
	unsigned int flags; ///< this instances access restriction

	//static Type _error;

	Type *parent; ///< For union/find algorithm, holds representatve if not this.
	int rank;	///< For union/find algorithm.

	std::vector<Type*> params;

protected:
	/// Derived type specific computation of the unification of two types, always called via public driver
	virtual bool internalUnify(Type *other,bool specialize);
	/// Derived type specific copy, recurse to contained types via "substitute" to retain existing conv mapping.
	virtual Type *internalCopy(TypeVariableMap &conv);
	/// Derived type specific check for subtype relation between two types - always called via public driver.
	virtual PartialOrder internalCheckSubtype(Type *other);
	/// This types supertype. For concrete types like int and list, it's this. For variables, it's anything.
	Type *super;

	void setParent(Type *p) { parent=p; };

	// TypeGraph traversal mappings for parameters... memory management!
	std::vector<Type*> currentMapping;
	std::set<Type*> excludeMapping;


public:


	Type(Types t,unsigned int flags_=Readable) : type(t), flags(flags_)
	{
		parent=this;
		super=this;
		rank=0; 
	};

	Type(Types t,Type *param0,unsigned int flags_=Readable) : type(t), flags(flags_)
	{
		parent=this;
		super=this;
		rank = 0;
		addParameter(param0);
	};

	Type(Types t,iterator &paramstart,iterator &paramend,unsigned int flags_=Readable) : type(t), flags(flags_)
	{
		params.insert(params.begin(),paramstart,paramend);
		parent=this;
		super=this;
		rank=0; 
	};

	virtual ~Type();

	inline iterator begin(void){ return params.begin(); };
	inline iterator end(void) { return params.end(); };
	inline void addParameter(Type *p) { params.push_back(p); };
	inline size_t parameterCount(void) { return params.size(); };
	inline Type *getFirstParameter(void) { return params.front(); };
	inline Type *getParameter(size_t index) { return params[index]; };
	bool isAbstract(void);

	/// The kind of a type, as defined by the Types enumeration, is used to determine subclass, quickly compare inequality for types, and more...
	inline Types getKind(void) { return type; };
	/// Returns a pointer to this types supertype (constraint).To convert a type to it's supertype @see TypeGraph::getParentType().
	inline Type *getSupertype(void) { return super->find(); };
	/// Assign a new (usually more restrictive) supertype to the type.
	inline void replaceSupertype(Type *t)
	{
		//if (super != this) deleteCopy(super);
		//if (t != nullptr) super = t->copy();
		//else super = nullptr;
		super = t;
	}
	/// set the access flags (Readable and Writable, defined as enum in class Type). Use bitwise- or to combine flags.
	virtual void setAccessFlags(int f) { flags=f; };
	/// Get the access flags as an integer
	inline int getAccessFlags(void) { return flags; };
	/// Is the type readable?
	inline bool isReadable(void) { return (flags&Readable)==Readable; };
	/// Is the type writeable?
	inline bool isWritable(void) { return (flags&Writable)==Writable; };
	/// Fill a vector with any parameter type this type has, including non-variables!
	
	inline void getParameterTypes(std::vector<Type*> &parameters)
	{
		parameters=params;
	}
	
	inline void getParameterTypes(std::set<Type*> &parameters)
	{
		parameters.insert(params.begin(),params.end());
	}

	/// Unify this type with anther, return false if not possible
	bool unify(Type *b,bool specialize=false);
	/// Return a copy of the type
	inline Type *copy()
	{
		TypeVariableMap conv;
		return find()->internalCopy(conv);
	};
	/// Return a copy where type variables occuring in conv are mapped to those passed.
	/** Variables not covered by conv will be mapped to fresh variables and added to the mapping.
		(Otherwise, the types do not stay consistend.)
	*/
	inline Type *substitute(TypeVariableMap &conv) 
	{
		return find()->internalCopy(conv); 
	};
	/// Check the subtype relation between this type and another
	PartialOrder checkSubtype(Type *other);

	/// Find the representative for this type.
	/** Used during unification, the representative is the most general type that fullfills
		all accumulated constraints on the use of the type.
		Also propagates access flags to parent.
	*/
	inline Type *find()
	{
		if (parent==this) return this;
		else 
		{
			Type *r=parent=parent->find();
			r->setAccessFlags(getAccessFlags());
			return r;
		}
	}

	/// Returns the kind of the type as it's rtt value (the value in the opening tag of the rtt)
	virtual rtt::RTType getRTKind(void) 
	{
		switch(getKind())
		{
		case Generator:
			return rtt::Generator;
		case String:
			return rtt::String;
		case List:
			return rtt::List;
		case Dictionary:
			return rtt::Dictionary;
		case Integer:
			return rtt::Integer;
		case Real:
			return rtt::Real;
		case Boolean:
			return rtt::Boolean;
		default:
			return rtt::Undefined;
		}
		return rtt::Undefined;
	}
	
	/// Print the type to the stream, overloaded by polytypes.
	virtual void print(std::wostream &s);

	virtual void getVariables(TypeVariableSet &result)
	{ 
		for (iterator iter=begin();iter!=end();iter++)
		{
			(*iter)->getVariables(result);
		};
	}
};


/// Variables are in general universially quantified
class TypeVariable : public Type
{
	std::wstring name;
	Type top;
	std::vector<Type*> ownedtypes;
protected:
	virtual Type *internalCopy(TypeVariableMap &conv);
public:
	TypeVariable(std::wstring n)
		: Type(Type::Variable)
		, name(n)
		, top(Type::Top)
	{ 
		super = &top;
	};

	TypeVariable(std::wstring n, Type *sup)
		: Type(Type::Variable)
		, name(n+L"sup")
		, top(Type::Top)
	{
		super = sup;
	};

	TypeVariable(std::wstring n,Type *sup, TypeVariableMap &conv)
		: Type(Type::Variable)
		, name(n+L"subst")
		, top(Type::Top)
	{ 
		super = sup;
	};

	TypeVariable(TypeVariable *other) 
		: Type(Type::Variable)
		, name(other->getName() + L"copy")
		, top(Type::Top)
	{
		Type *copy=other->super->copy();
		//if (copy->getKind() != Type::Variable)
			ownedtypes.push_back(copy);
		super = copy;
	};

	~TypeVariable()
	{
		for (auto iter = ownedtypes.begin();iter != ownedtypes.end();++iter)
			deleteCopy(*iter);
	}

	const std::wstring getName(void) { return name; };

	virtual void print(std::wostream &s)
	{
		s << name << "<:";
		super->find()->print(s);
	};

	virtual void getVariables(TypeVariableSet &set)
	{
		TypeVariable *buf=dynamic_cast<TypeVariable*>(find());
		if (buf!=NULL) set.insert(buf);
	};

	virtual rtt::RTType getRTKind(void) 
	{
		return rtt::Undefined; // Should be specialized for code generation
	}
};

/// Error Propagation. May not be the best place to log error messages...
class ErrorType : public Type
{
	std::wstring message;
	size_t line, pos;
protected:
	/// Internal variant of Return a copy of this type, calls during recursion
	virtual Type *internalCopy(TypeVariableMap &conv);
	/// Check the subtype relation between two types - always called via public driver
	//virtual PartialOrder internalCheckSubtype(Type *other)0;

public:
	ErrorType(size_t l, size_t p, std::wstring msg) : Type(Type::Error), message(msg), line(l), pos(p)
	{
	};

	inline int getLine(void) { return line; };
	inline int getPosition(void) { return pos; };
	inline std::wstring getMessage(void) { return message; };

	virtual void print(std::wostream &s)
	{
		s << "Type Error ("<< line << ", "<< pos <<"): " << message << '!' << std::endl;
	};

	virtual void getVariables(TypeVariableSet &)
	{
	};
};
/// Function types internally pretend that the parameters are (formally) a tuple.
class FunctionType : public Type
{
	//std::list<Type*> params;
	Type *rettype;
protected:
	/// Compute the unification of two types, always called via public driver. This one handles simple (mono) types.
	virtual bool internalUnify(Type *other,bool specialize);
	/// Internal variant of Return a copy of this type, calls during recursion
	virtual Type *internalCopy(TypeVariableMap &conv);
	/// Check the subtype relation between two types - always called via public driver
	virtual PartialOrder internalCheckSubtype(Type *other);

public: 
	FunctionType(const std::list<Type*> &p,Type *rt) : Type(Type::Function)
	{
		std::list<Type*>::const_iterator iter;
		for (iter=p.begin();iter!=p.end();iter++)
			addParameter((*iter)->find());
		rettype=rt->find();
	};

	FunctionType(Type *rt) : Type(Type::Function)
	{
		rettype=rt->find();
	};

	FunctionType() : Type(Type::Function)
	{
		rettype=NULL;
	};

	FunctionType(FunctionType &other) : Type(Type::Function)
	{
		rettype=other.getReturnType()->find();
		const_iterator iter;
		for (iter=other.begin();iter!=other.end();iter++)
			addParameter((*iter)->find());
	};

	virtual ~FunctionType()
	{
	};

	inline void setReturnType(Type *r) { rettype=r; };
	inline Type *getReturnType(void) { return rettype; };

	virtual void print(std::wostream &s)
	{
		s << "(";
		iterator iter=begin();
		if (iter!=end())
		{
			(*iter)->find()->print(s);
			iter++;
			while (iter!=end())
			{
				s << ",";
				(*iter)->find()->print(s);
				iter++;
			}
		}
		s << ")";
		s << " -> ";
		rettype->find()->print(s);
		
	};

	virtual void getVariables(TypeVariableSet &set)
	{
		for (iterator i=begin();i!=end();i++)
			(*i)->find()->getVariables(set);
		rettype->find()->getVariables(set);
	};

	virtual rtt::RTType getRTKind(void) 
	{ return rtt::Function; }
};

/// record types are composed of the identifiers and types of all fields.
class RecordType : public Type
{
	std::map<std::wstring,Type*> members;
protected:
	/// Compute the unification of two types, always called via public driver
	virtual bool internalUnify(Type *other,bool specialize);
	/// Internal variant of Return a copy of this type, calls during recursion
	virtual Type *internalCopy(TypeVariableMap &conv);
	/// Check the subtype relation between two types - always called via public driver
	virtual PartialOrder internalCheckSubtype(Type *other);
public:

	typedef std::map<std::wstring,Type*> membermap;

	RecordType(const std::map<std::wstring,Type*> &m) : Type(Type::Record)
	{
		members=m;
	};

	RecordType() : Type(Type::Record)
	{
	};

	virtual void setAccessFlags(int f) 
	{ 
		Type::setAccessFlags(f); 
		for (iterator iter=members.begin();iter!=members.end();iter++)
		{
			iter->second->find()->setAccessFlags(f);
		}

	};

	void addMember(const std::wstring &label,Type *type)
	{
		members.insert(make_pair(label,type));
	};

	void addMember(const wchar_t *label,Type *type)
	{
		members.insert(make_pair(std::wstring(label,wcslen(label)),type));
	};

	RecordType(RecordType &other) : Type(Type::Record)
	{
		std::map<std::wstring,Type*>::const_iterator iter;
		for (iter=other.begin();iter!=other.end();iter++)
			members.insert(make_pair(iter->first,iter->second->find()));
	};

	typedef std::map<std::wstring,Type*>::iterator iterator;

	iterator begin(void) { return members.begin(); };
	iterator findMember(const std::wstring &field) { return members.find(field); };
	iterator end(void) { return members.end(); };

	inline size_t size(void) { return members.size(); };

	virtual void print(std::wostream &s)
	{
		s << "[ ";
		for (iterator iter=members.begin();iter!=members.end();iter++)
		{
			s << iter->first << " : " ;
			iter->second->find()->print(s);
			s << "; ";
		}
		s << "]";
	}

	virtual void getVariables(TypeVariableSet &set)
	{
		for (iterator iter=members.begin();iter!=members.end();iter++)
			iter->second->find()->getVariables(set);
	};

	virtual rtt::RTType getRTKind(void) 
	{ return rtt::Record; }
};

/// Variant Types
class VariantType : public Type
{
	std::map<std::wstring,RecordType*> tags;
protected:
	/// Compute the unification of two types, always called via public driver
	virtual bool internalUnify(Type *other,bool specialize);
	/// Internal variant of Return a copy of this type, calls during recursion
	virtual Type *internalCopy(TypeVariableMap &conv);
	/// Check the subtype relation between two types - always called via public driver
	virtual PartialOrder internalCheckSubtype(Type *other);
	Type mytop;
public:

	typedef std::map<std::wstring,RecordType*> tagmap;
	typedef std::map<std::wstring,RecordType*>::iterator iterator;

	VariantType(): Type(Type::Variant), mytop(Type::Top)
	{
		super=&mytop;
	};

	VariantType(const tagmap &m) : VariantType() //Type(Type::Variant), mytop(Type::Top)
	{
		tagmap::const_iterator iter;
		for (iter=tags.begin();iter!=tags.end();iter++)
			tags.insert(make_pair(iter->first,(RecordType*)iter->second->find()->copy()));
	};

	VariantType(VariantType &other) : VariantType() // Type(Type::Variant), mytop(Type::Top)
	{
		tagmap::const_iterator iter;
		for (iter=other.beginTag();iter!=other.endTag();iter++)
			tags.insert(make_pair(iter->first,(RecordType*)iter->second->find()->copy()));
	};

	virtual ~VariantType(void)
	{
	};

	iterator beginTag(void) { return tags.begin(); };
	iterator findTag(const std::wstring &tag) { return tags.find(tag); };
	iterator endTag(void) { return tags.end(); };

	virtual void setAccessFlags(int f) 
	{ 
		Type::setAccessFlags(f); 
		for (iterator iter=tags.begin();iter!=tags.end();iter++)
		{
			iter->second->find()->setAccessFlags(f);
		}

	};

	void addTag(const std::wstring &tag,RecordType *contents)
	{
		tags.insert(make_pair(tag,(RecordType*)contents));
	};

	void addTag(const wchar_t *tag,RecordType *contents)
	{
		addTag(std::wstring(tag,wcslen(tag)),contents);
	};

	void addTag(iterator &iter)
	{
		addTag(iter->first,iter->second);
	};

	inline size_t size(void) { return tags.size(); };

	virtual void print(std::wostream &s)
	{
		iterator iter;
		s << "{ ";
		for (iter=tags.begin();iter!=tags.end();iter++)
		{
			if (iter!=tags.begin()) s << " + ";
			s << "[ :" << iter->first << " ";
			for (RecordType::iterator rec_iter=iter->second->begin();rec_iter!=iter->second->end();rec_iter++)
			{
				s << rec_iter->first << " : " ;
				rec_iter->second->find()->print(s);
				s << "; ";
			}
			s << "]";
		}
		s << "}";
	};

	virtual rtt::RTType getRTKind(void) 
	{ return rtt::Variant; }

	virtual void getVariables(TypeVariableSet &set)
	{
		for (iterator iter=tags.begin();iter!=tags.end();iter++)
			iter->second->find()->getVariables(set);
	}
};

/// Opaque Types are used with modules to hide implementation details of a type.
/** This kind of type is probably the most complex in Intro: It hides the implementation
	Details of some type, associated with a module, from the world outside of the module.
	However, the interpreter must know everything related to types, and it has to verify
	that the module's export declarations match up with the calues in the module body.
	Thus, it requires the ability to "look into" an opaque type.

	The implementation type, which hidden from users outside the module, is indeed
	used by the code generation to know how to implement the opaque type in terms
	of the built-in types it knows about.
	 
	In order to ensure sensible typing, the opaque type exposes free variables in the "hidden"
	type as parameters - just like List(?a) and Dictionary(?key,?val) expose parameters.
	Making all those variables and paramters match up given possible permutations takes
	quite some effort. 
	
	Indeed, to remain within deciadable type inference, the module
	implementor must provide a "constructor" function in the module body, whose name and
	number of parameters match those of the opaque types exposed by the module. Each opaque
	type requires a constructor, but multiple opaque types can be defined in a module.

	The member function setTypeMapping() sets the hidden type and associates the free variables 
	with the parameters. The parameters are expected to be in the same order as in both contructor
	and opaque type (the programmer has control over both, this was the core of the actual problem
	why constructors are necessary).

	An instantiation of an opaque type is a backward mapping from opaque type with parameters to 
	the appropriate internal type. This is done to assure that, given 
		Pair(?a,?b) :: [first:?a;second:?b;]
	not only makes sense when a single Pair occurs, e.g. in a function type. There must be an accounting
	for the popular swap funtions for pairs, exported as
		swap:(Pair(?a,?b))->Pair(?b,?a)
	a type that must match
		swap:([ first:?a;second:?b;])->[first:?b;second:?a;]
	i.e. the permutation of the parameters must be propagates to the 

	This is performed by the instantiate() member.

	The hidden type must also be shared amongst all instances of the same opaque type, which is once again
	done in classic C style, with pointers. In order to tackle this memory management issue, a "static
	strategy" approach, as with expressions and types, cannot keep pace with our needs. The method used
	is the simplest alternative, reference counting - after all, we should not have an paque type occur 
	inside it's hidden type, so there are no cycles possible . This is especially simple, as we have 
	only a few places where reference counting occurs: in the decrease in destructor and increase in 
	the copy constructor! The shared data is initialized by the function setTypeMapping(), the
	reference count starts with one during creation of the shared data together with the hidden data.

	@TODO: Move name to shared data? use std::tr1::shared_ptr fr shared data!
*/
class OpaqueType : public Type
{
	std::list<std::pair<TypeVariable*,TypeVariable*> > params;
	std::wstring name;
	Type *instance;
	Type *error;

	/// The hidden type of an opaque type is shared by several instances, this is handled by reference counting.
	struct shared_data
	{
		Type *hidden;			///< the hidden type, the implementation of the opaque type.
		unsigned int refcount;	///< The reference count for the hidden type.
		
		shared_data() : hidden(nullptr), refcount(1)
		{}
	};
	/// The local pointer to the shared data.
	shared_data *shared;

	/// Remove a reference from the shared data, delete if it was the last reference.
	inline void unshare(void)
	{
		if (shared==NULL) return;
		shared->refcount--;
		if (shared->refcount==0) 
		{
			deleteCopy(shared->hidden);
			delete shared;
		}
		shared=NULL;
	};
protected:
	/// Compute the unification of two types, always called via public driver
	virtual bool internalUnify(Type *other,bool specialize);
	/// Internal variant of Return a copy of this type, calls during recursion
	virtual Type *internalCopy(TypeVariableMap &conv);
	/// Check the subtype relation between two types - always called via public driver
	//virtual PartialOrder internalCheckSubtype(Type *other);

public:
	typedef std::list<std::pair<TypeVariable*,TypeVariable*> >::iterator iterator;
	OpaqueType(const std::wstring &n) 
		: Type(Type::UserDef)
		, name(n)
	{ shared=NULL; instance=NULL; error=NULL; };

	OpaqueType(OpaqueType &other)
		: Type(Type::UserDef)
		, name(other.getName())
	{ 
		shared=other.shared; 
		if (shared!=NULL) shared->refcount++;
		instance=NULL;
		error=NULL;
		OpaqueType::iterator iter;
		for (iter=other.begin();iter!=other.end();iter++)
			addParameter(iter->first);
	};

	~OpaqueType(void)
	{
		if (instance!=NULL) deleteCopy(instance);
		if (error!=NULL) delete error;
		error=NULL;
		unshare();
	};

	/// This provides access to the hidden type for type checking a modules interface.
	Type *getHiddenType(void) { return shared->hidden; };

	/// This function assigns the implementation type to the Opaque type, setting the hidden type.
	/** Use this function to assign a constructor's type (a function) to the Opaque type.
		It creates a mapping from the type variables for the constructor's parameters to the
		type variables in the internal type.
	*/
	bool setTypeMapping(FunctionType *ft);

	std::wstring getName(void) const {return name; };

	inline iterator begin(void) { return params.begin(); };
	inline iterator end(void) { return params.end(); };

	inline void addParameter(TypeVariable *t) { params.push_back(std::make_pair(t,(TypeVariable*)NULL)); };
	inline void addParameter(const std::pair<TypeVariable*,TypeVariable*> &val) { params.push_back(val); };

	Type *instantiate(std::list<TypeVariable*> &params,size_t line,size_t pos);

	virtual void print(std::wostream &s)
	{
		iterator iter;
		s << name;
		if (!params.empty())
		{
			s << "(";
			iter=params.begin();
			iter->first->find()->print(s);
			iter++;
			for (;iter!=params.end();iter++)
			{
				s << ",";
				iter->first->find()->print(s);
			}
			s << ")";
		}
	};

	//virtual void getParameterTypes(std::vector<Type*> &parameters);

	virtual void getVariables(TypeVariableSet &set)
	{
		for (iterator iter=params.begin();iter!=params.end();iter++)
			iter->first->getVariables(set);
	};

};

}

#endif