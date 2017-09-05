#include "stdafx.h"
#include "Type.h"
#include <algorithm>
#include "Environment.h"
#include <sstream>
#include <string>
#include <vector>

#include "TypeGraph.h"

namespace intro 
{
static TypeGraph theGraph;

//////////////////////////////////////////////////////////////////////////////////
// PartialOrder support functions
std::wostream& operator<<(std::wostream& os, PartialOrder po)
{
	switch (po)
	{
	case ERROR: os << L"ERROR"; break;
	case LESS: os << L"LESS"; break;
	case EQUAL:  os << L"EQUAL"; break;
	case GREATER:  os << L"GREATER"; break;
	}
	return os;
}

/// This function is used to collect the relations of contained types into the relation in accu
// make this inline (move to header).
void combinePartialOrders(PartialOrder &accu, PartialOrder additional)
{
	switch (additional)
	{
	case LESS:
		if (accu == EQUAL || accu == LESS) accu = LESS;
		else accu = ERROR;
		break;
	case EQUAL: break;
	case GREATER:
		if (accu == EQUAL || accu == GREATER) accu = GREATER;
		else accu = ERROR;
		break;
	case ERROR: accu = ERROR;
	};
}



//////////////////////////////////////////////////////////////////////////////////
// Type Members

Type::~Type()
{
	if (super != this && super != nullptr) deleteCopy(super);
	if (getKind()!=Variable) 
		TypeGraph::clearMapping(currentMapping, excludeMapping);
	params.clear();
}

bool Type::isAbstract(void)
{
	return theGraph.isAbtractType(this);
}

void Type::print(std::wostream &s)
{
	switch(type)
	{
	case Error: 
		s << "Error!"; 
		break;
	case Top: 
		s << "Top"; 
		break;
	case Unit: 
		s << "Unit"; 
		break;
	case Variable:
		s << "Variable"; 
	case Boolean:
		s << "Boolean"; 
		break;
	case Comparable:
		s << "Comparable"; 
		break;
	case Number:
		s << "Number"; 
		break;
	case Sequence:
		s << "Sequence"; 
		break;
	case Integer:
		s << "Integer"; 
		break;
	case Real:
		s << "Real"; 
		break;
	case String:
		s << "String"; 
		break;
	case UserDef:
		s << "UserDefined";
		break;
	case Function:
		s << "Function"; 
		break;
	case List:
		s << "List"; 
		break;
	case Dictionary:
		s << "Dictionary"; 
		break;
	case Record:
		s << "Record"; 
		break;
	case Generator:
		s << "Generator"; 
		break;
	case Variant:
		s << "Variant"; 
		break;
	default: 
		s << "SomeType";
		break;
	}
	iterator iter=begin();
	if (iter!=end())
	{
		s << "(";
		(*iter)->find()->print(s);
		iter++;
		while (iter!=end())
		{
			s << ",";
			(*iter)->find()->print(s);
			iter++;
		}
		s << ")";
	}

}

PartialOrder Type::checkSubtype(Type *other)
{
	Type *ta=find();
	Type *tb=other->find();

	// Error and Top are simpe
	if (ta->getKind()== Type::Error) 
		return LESS; // propagate errors (bottom type)
	else if (tb->getKind()== Type::Error) 
		return GREATER; // propagate errors (bottom type)

	// This is not an optimization: type variables are special because we must check supertype relation.
	if (ta->getKind()==Type::Variable && tb->getKind()==Type::Variable)
		return ta->getSupertype()->checkSubtype(tb->getSupertype());
	else if (ta->getKind()==Type::Variable)
		return ta->getSupertype()->checkSubtype(tb);
	else if (tb->getKind()==Type::Variable)
		return ta->checkSubtype(tb->getSupertype());

	if (ta->getKind()==Type::Top && tb->getKind()==Type::Top) 
		return EQUAL;
	else if (ta->getKind()==Type::Top) 
		return GREATER;		// All types are subtypes of Top
	else if (tb->getKind()==Type::Top) 
		return LESS;		// All types are subtypes of Top
	
	return internalCheckSubtype(tb);
}

PartialOrder Type::internalCheckSubtype(Type *other)
{
	std::set<Type*> current;
	std::set<Type*> exclude;
	PartialOrder retval=theGraph.findSupertype(this,other,current,exclude);
	theGraph.clearMapping(current,exclude);
	return retval;
}

PartialOrder FunctionType::internalCheckSubtype(Type *other)
{
	if (other->getKind()!=Type::Function) return ERROR;
	FunctionType *fb=dynamic_cast<FunctionType*>(other);
	if (parameterCount()!=fb->parameterCount()) return ERROR;
	FunctionType::iterator ai,bi;
	PartialOrder val=EQUAL; // will be mutated via reference by combineOrders
	for (ai=begin(),bi=fb->begin();ai!=end() && val!=ERROR;ai++,bi++)
	{
		combinePartialOrders(val,(*bi)->checkSubtype(*ai));
	}
	if (val==ERROR) return ERROR;
	// Note the order of parameters in this call tocheckSubtype is opposite from above!
	combinePartialOrders(val,getReturnType()->checkSubtype(fb->getReturnType()));
	return val;
}

// Parts of this function look like what combinePartialOrders(), but are more restrictive
PartialOrder RecordType::internalCheckSubtype(Type *other)
{
	if (other->getKind()!=Type::Record) return ERROR;
	RecordType *rb=dynamic_cast<RecordType*>(other);
	RecordType::iterator ai,bi;
	if (size()<rb->size()) // rb<:ra or error
	{
		PartialOrder val=GREATER;
		for (ai=begin();ai!=end();ai++)
		{
			bi=rb->findMember(ai->first);
			if (bi==rb->end()) return ERROR;
			switch(ai->second->checkSubtype(bi->second))
			{
			case GREATER:
				if (val==EQUAL || val==GREATER) val = GREATER;
				else return ERROR;
				break;
			case EQUAL: break;
			case LESS: 
			case ERROR: return ERROR;
			}
		}
		return val;

	}
	else if (size()>rb->size()) // ra<:rb or error
	{
		PartialOrder val=LESS;
		for (bi=rb->begin();bi!=rb->end();bi++)
		{
			ai=findMember(bi->first);
			if (ai==end()) return ERROR;
			switch(ai->second->checkSubtype(bi->second))
			{
			case LESS:
				if (val==EQUAL || val==LESS) val = LESS;
				else return ERROR;
				break;
			case EQUAL: break;
			case GREATER: 
			case ERROR: return ERROR;
			}
		}
		return val;
	}
	else // sizes are equal, normal supertype check wth combineOrders
	{
		PartialOrder val=EQUAL; // again, mutated by combinePartialOrders via reference.
		for (ai=begin();ai!=end();ai++)
		{
			bi=rb->findMember(ai->first);
			if (bi==rb->end()) return ERROR;
			combinePartialOrders(val,ai->second->checkSubtype(bi->second));
		}
		return val;
	}
}

// Parts of this function look like what combinePartialOrders(), but are more restrictive
PartialOrder VariantType::internalCheckSubtype(Type *other)
{
	if (other->getKind()!=Type::Variant) return ERROR;
	VariantType *rb=dynamic_cast<VariantType*>(other);
	VariantType::iterator ai,bi;
	if (size()<rb->size()) // rb<:ra or error
	{
		PartialOrder val=LESS;
		for (ai=beginTag();ai!=endTag();ai++)
		{
			bi=rb->findTag(ai->first);
			if (bi==rb->endTag()) return ERROR;
			switch(ai->second->checkSubtype(bi->second))
			{
/*			case GREATER:
				if (val==EQUAL || val==GREATER) val = GREATER;
				else return ERROR;
				break;
*/
			case EQUAL: break;
			case LESS:
				if (val==EQUAL || val==LESS) val = LESS;
				else return ERROR;
				break;
			case GREATER:				
			case ERROR: 
				return ERROR;
			}
		}
		return val;

	}
	else if (size()>rb->size()) // ra<:rb or error
	{
		PartialOrder val=GREATER;
		for (bi=rb->beginTag();bi!=rb->endTag();bi++)
		{
			ai=findTag(bi->first);
			if (ai==endTag()) return ERROR;
			switch(ai->second->checkSubtype(bi->second))
			{
/*			case LESS:
				if (val==EQUAL || val==LESS) val = LESS;
				else return ERROR;
				break;
*/			case EQUAL: break;
			case GREATER: 
				if (val==EQUAL || val==GREATER) val = GREATER;
				else return ERROR;
				break;
			case LESS:
			case ERROR:
				return ERROR;
			}
		}
		return val;
	}
	else // sizes are equal, normal supertype check wth combineOrders
	{
		PartialOrder val=EQUAL; // again, mutated by combinePartialOrders via reference.
		for (ai=beginTag();ai!=endTag();ai++)
		{
			bi=rb->findTag(ai->first);
			if (bi==rb->endTag()) return ERROR;
			combinePartialOrders(val,ai->second->checkSubtype(bi->second));
		}
		return val;
	}
}
/*
PartialOrder OpaqueType::internalCheckSubtype(Type *other)
{
	// @TODO !!!
	
	// if other dynamic-casts to OpaqueType
	OpaqueType *ot=dynamic_cast<OpaqueType*>(other);
	if (ot!=NULL)
	{
	// if thet opaque types have the same name, they are either
	// of the same type, or potential parameters may be in a subtype relation.
	
	// if the name is different, check the subtype relation between the 
	// opaque types
	} 
	else
	{
		// if other is not another opaque type, it may still be an explicitely stated supertype.
	}
	
	return ERROR;
};
*/
/// Used by Type::unify() to merge two record types (infer a common subtype) if it is legal.
/** Because, in case of record supertypes for two variables, we can merge the records
	and then the result is a common supertype, if the new supertype is a record that 
	has all labels that occur in both record types, and labels that occur in both types 
	must have unifiable (or subtypeable??) types. 
	
	Note that the result must be placed in a type variable as a supertype,	
	which is then the owner (responsible for deleting) of the record type returned.
	
	This function returns an ErrorType with a message in case something is incompatible.
*/
Type *mergeRecordTypes(RecordType *a,RecordType *b)
{
	RecordType *result=new RecordType(); // needs to be deleted if 
	std::wstring errmsg=std::wstring();	// Collect all errors here
	//std::wstring errnbuff=std::wstring();
	//std::wstringstream ws=std::wstringstream(errmsg);
	std::wstringstream ws;
	RecordType::iterator iter,found;
	// For each label in a: if it is not in b, keep it, if it is in b,
	// check subtyping and take the more restrictive one
	for (iter=a->begin();iter!=a->end();iter++)
	{
		found=b->findMember(iter->first);
		if (found==b->end())
		{
			result->addMember(iter->first,iter->second);
		}
		else
		{
			switch(iter->second->checkSubtype(found->second))
			{
			case LESS:
				// Same, just fall through to keeping a's label
			case EQUAL: 
				// keep label in record a
				result->addMember(iter->first,iter->second->copy());
				break;
			case GREATER: 
				// keep label in rectord b
				result->addMember(found->first,found->second->copy());
				break;
			case ERROR: 
				ws << "The label '" << iter->first << "' refers to incompatible types in the two records.\n";

			}
		}
	}
	// Skip if errors occured above
	//if (!errmsg.empty())
	if (!ws.str().empty())
	{
		//deleteCopy(result);
		delete result;
		//return new ErrorType(1,1,errmsg);
		return new ErrorType(1,1,ws.str());
	}
	// For each label in b: if it is not in a already, it can be kept as we checked the intersection labels.
	for (iter=b->begin();iter!=b->end();iter++)
	{
		// skip what is already in a
		if (a->findMember(iter->first)!=a->end()) continue;
		// add the rest
		result->addMember(iter->first,iter->second);
	}
	return result;
}

Type *intersectVariantTypes(VariantType *a,VariantType *b,bool specialize)
{
	VariantType *result=new VariantType(); // needs to be deleted if 
	std::wstring errmsg=std::wstring();	// Collect all errors here
	//std::wstring errnbuff=std::wstring();
	//std::wstringstream ws=std::wstringstream(errmsg);
	std::wstringstream ws;
	VariantType::iterator iter,found;
	for (iter=a->beginTag();iter!=a->endTag();iter++)
	{
		found=b->findTag(iter->first);
		if (found!=b->endTag())
		{
			if (iter->second->unify(found->second,specialize))
				result->addTag(iter);
			else
			{
				ws << "The label '" << iter->first << "' refers to incompatible types in the two variants.\n";
			}
		}
	}
	// Skip if errors occured above
	if (!ws.str().empty())
	{
		//deleteCopy(result);
		delete result;
		return new ErrorType(1,1,ws.str());
	}
	return result;
}

bool Type::unify(Type *b,bool specialize)
{
	Type *mroot=find();
	Type *oroot=b->find();
	if (mroot==oroot) return true;
	// The preference for non-Variables (i.e. specialization)
	// is fundamental for the workings of the unification algorithm,
	// see Purple Dragon Book, Algorithm 6.19.
	if (mroot->type==Error)	// Error trumps all others
	{
		oroot->parent=mroot;
		return false;
	}
	else if (oroot->type==Error) // Error trumps all others
	{
		mroot->parent=oroot;
		return false;
	}
	if (mroot->type==Variable && oroot->type==Variable) 
	{
		switch(oroot->getSupertype()->checkSubtype(mroot->getSupertype()))
		{
		case ERROR:
			// Here, we have detected incompatible supertypes, but the
			// checkSubtype function does not know (or care) that we compared
			// the supertypes of two variables.
			// It may seem unelegant to do this in the error handling section, but:
			// if the records/variants are already in a subtype relation, then we need not create
			// a new type which contains the merged/intersected contents of both, we can take the subtype
			// (the more restrictive type) directly.
			// in other cases, we must stick to the type graph.
			if (oroot->getSupertype()->getKind()==Type::Record && mroot->getSupertype()->getKind()==Type::Record)
			{
				Type *merged=mergeRecordTypes(dynamic_cast<RecordType*>(oroot->getSupertype()),dynamic_cast<RecordType*>(mroot->getSupertype()));
				if (merged->getKind()!=Type::Error) merged=Environment::fresh(merged);
				oroot->parent=merged;
				mroot->parent=merged;
				return merged->getKind()!=Type::Error;
			}
			if (oroot->getSupertype()->getKind()==Type::Variant && mroot->getSupertype()->getKind()==Type::Variant)
			{
				Type *intersect=intersectVariantTypes(dynamic_cast<VariantType*>(oroot->getSupertype()),dynamic_cast<VariantType*>(mroot->getSupertype()),specialize);
				if (intersect->getKind()!=Type::Error) intersect=Environment::fresh(intersect);
				oroot->parent=intersect;
				mroot->parent=intersect;
				return intersect->getKind()!=Type::Error;
			}
			return false;
		case LESS: 
			mroot->parent=oroot;
			if (mroot->getSupertype()->getKind()==Top) return true;
			return isLessOrEqual(oroot->checkSubtype(mroot->getSupertype()));
		case EQUAL: 
		case GREATER:
			oroot->parent=mroot;
			if (oroot->getSupertype()->getKind()==Top) return true;
			return isLessOrEqual(mroot->checkSubtype(oroot->getSupertype()));
		}
	}
	else if (mroot->type==Variable && oroot->type!=Variable)
	{
		switch(oroot->checkSubtype(mroot->getSupertype()))
		{
		case LESS:
		case EQUAL: 
			mroot->parent=oroot; 
			if (mroot->getSupertype()->getKind()==Top) return true;
			if (!isLessOrEqual(oroot->checkSubtype(mroot->getSupertype())))
				return false;
			else return oroot->unify(mroot->getSupertype(),specialize);
		case GREATER: // Yes, it is an error if we expect more than is given
		case ERROR: 
			return false;
		}
	}
	else if (oroot->type==Variable && mroot->type!=Variable)
	{
		switch(mroot->checkSubtype(oroot->getSupertype()))
		{
		case LESS: 
		case EQUAL: 
			oroot->parent=mroot; 
			if (oroot->getSupertype()->getKind()==Top) return true;
			if (!isLessOrEqual(mroot->checkSubtype(oroot->getSupertype())))
				return false;
			else return mroot->unify(oroot->getSupertype(),specialize);
			//return true;
		case GREATER: // Yes, it is an error if we expect more than is given
		case ERROR: 
			return false;
		}
	}
	return oroot->internalUnify(mroot,specialize);
}

// Simple types
bool Type::internalUnify(Type *other,bool specialize)
{
	//SubtypeTraversal traverse(theGraph,this,other);
	std::set<Type*> current;
	std::set<Type*> exclude;
	//std::set<Type*>::iterator iter;
	std::vector<Type*>::iterator iter;
	std::vector<Type*>::iterator oit;
	// First, check the type graph if a legal path actually exists
	PartialOrder order=theGraph.findSupertype(this,other,currentMapping,excludeMapping);

	if (order==ERROR) return false;
	Type *goal=this;
	Type *source=other;
	if (order==LESS) std::swap(goal,source); // order indepenent code hereafter ;)

	// Perform the actual unification by making one type the others parent,
	// depending on wether we're specializing or not
	if (specialize)
	{
		goal->parent=source;
	}
	else
	{
		source->parent=goal;
	}
	// Graph traversal may have provided additional parameters, here we check they match up with source type
	// by realizing the modifications along the path in the graph
	// The goal is to manage the relations between the types parameters, a good example is
	// Dictionary(String,Integer) <: Generator([key:String; value:Integer;])
	// where when expecting a generator, we have to infer the record and it's field types
	// from the dictionary and it's type parameters
	bool retval=true;
	//for (iter=currentMapping.begin(),oit=goal->begin();iter!=currentMapping.end() && retval;iter++,oit++)
	for (iter = begin(), oit = goal->begin();iter != end() && retval;iter++, oit++)
	{
		retval&=(*iter)->unify(*oit);
	}
	return retval;
}

bool FunctionType::internalUnify(Type *other,bool specialize)
{
	FunctionType *fb=dynamic_cast<FunctionType*>(other);
	if (fb==NULL) return false;
	if (parameterCount()!=fb->parameterCount()) return false;
	FunctionType::iterator ai,bi;
	for (ai=begin(),bi=fb->begin();ai!=end();ai++,bi++)
	{
		if (!(*ai)->unify(*bi,!specialize)) return false;
	}
	return getReturnType()->unify(fb->getReturnType(),specialize);
}

bool RecordType::internalUnify(Type *other,bool specialize)
{
	RecordType *rb=dynamic_cast<RecordType*>(other);
	if (rb==NULL) return false;
	RecordType::iterator ai,bi;
	if (size()!=rb->size()) return false;
	for (ai=begin(),bi=rb->begin();ai!=end();ai++,bi++)
	{
		if (ai->first!=bi->first) return false;
		if (!ai->second->unify(bi->second,specialize)) return false;
	}
	return true;
}

bool VariantType::internalUnify(Type *other,bool specialize)
{
	VariantType *vb=dynamic_cast<VariantType*>(other);
	if (vb==NULL) return false;
	VariantType::iterator ai,bi;
	if (specialize) // excpect [:A ...] with ?a<:{[:A ...] + ...}
	{
		//VariantType *bigger=nullptr;
		if (tags.size()==1)
		{
			ai=tags.begin();
			bi=vb->findTag(ai->first);
			//bigger=vb;
		}
		else if (vb->tags.size()==1)
		{
			ai=vb->beginTag();
			bi=tags.find(ai->first);
			//bigger=this;
			
		}
		else return false;
		ai->second->unify(bi->second);
		//if (bigger==vb) this->setParent(ai->second);
		//else vb->setParent(ai->second);

		vb->setParent(this);
	}
	else
	{	
		// Tags present in both : 
		for (ai=beginTag();ai!=endTag();ai++)
		{
			bi=vb->findTag(ai->first);
			if (bi!=vb->endTag())
			{
				if (!ai->second->unify(bi->second,specialize))
					return false;
			}
		}
		for (bi=vb->beginTag();bi!=vb->endTag();bi++)
		{
			ai=findTag(bi->first);
			if (ai==endTag())
				addTag(bi);
		}
		vb->setParent(this);
	}
	return true;
}

bool OpaqueType::internalUnify(Type *other,bool specialize)
{
	OpaqueType *ob=dynamic_cast<OpaqueType*>(other);
	if (ob==NULL) return false;
	if (getName()!=ob->getName()) return false;
	OpaqueType::iterator itera,iterb;
	for (itera=begin(),iterb=ob->begin();itera!=end();itera++,iterb++)
	{
		if (!itera->first->unify(iterb->first,specialize)) return false;
	}
	return true;
}

/// Global variable used by type copy operation to name variables.

Type *Type::internalCopy(TypeVariableMap &conv)
{
	Type *thecopy=new Type(getKind(),getAccessFlags());
	
	for (iterator iter=begin();iter!=end();++iter)
	{
		thecopy->addParameter((*iter)->substitute(conv));
	}
	return thecopy;
}

Type *TypeVariable::internalCopy(TypeVariableMap &conv)
{
	TypeVariableMap::const_iterator iter=conv.find(this);
	if (iter==conv.end()) 
	{
		//TypeVariable *buf=Environment::fresh(numToString(variableCounter++),getSupertype()->substitute(conv));
		//TypeVariable *buf=Environment::fresh(getName(),getSupertype()->substitute(conv));
		TypeVariable *buf = Environment::fresh(getName(), getSupertype(),conv);
		buf->setAccessFlags(getAccessFlags());
		iter=conv.insert(std::make_pair(this,buf)).first;
	}
	return iter->second;
}

Type *ErrorType::internalCopy(TypeVariableMap &conv)
{
	return new ErrorType(getLine(),getPosition(),getMessage());
}

Type *FunctionType::internalCopy(TypeVariableMap &conv)
{
	FunctionType *b=new FunctionType(getReturnType()->substitute(conv));
	b->setAccessFlags(getAccessFlags());
	for (FunctionType::iterator iter=begin();iter!=end();iter++)
		b->addParameter((*iter)->substitute(conv));
	return b;
}

Type *RecordType::internalCopy(TypeVariableMap &conv)
{
	RecordType *b= new RecordType();
	for (RecordType::iterator iter=begin();iter!=end();iter++)
		b->addMember(iter->first,iter->second->substitute(conv));
	b->setAccessFlags(getAccessFlags());
	return b;
}

Type *VariantType::internalCopy(TypeVariableMap &conv)
{
	VariantType *b= new VariantType();
	for (VariantType::iterator iter=beginTag();iter!=endTag();iter++)
		b->addTag(iter->first,(RecordType*)iter->second->substitute(conv));
		//b->addTag(iter); // Record always copied
	b->setAccessFlags(getAccessFlags());
	return b;
}

Type *OpaqueType::internalCopy(TypeVariableMap &conv)
{
	OpaqueType *cp=new OpaqueType(getName());
	cp->shared=shared; 
	if (shared!=NULL) shared->refcount++;
	//std::wcout << "Copying "<<getName()<<":\n";
	for (OpaqueType::iterator iter=begin();iter!=end();iter++) 
	{
		cp->addParameter(dynamic_cast<TypeVariable*>(iter->first->substitute(conv)));
		//cp->params.back().first->print(std::wcout);
		//std::wcout << "\n";
	}
	cp->setAccessFlags(getAccessFlags());
	return cp;
}

void collectAllTypes(Type *t, std::set<Type*> &items)
{
	if (t->getKind() == Type::Variable) return;
	if (!items.insert(t).second) return; // visit things once
	switch (t->getKind())
	{
	case Type::Error:
	case Type::Unit:
	case Type::Boolean:
	case Type::Integer:
	case Type::Real:
	case Type::Number:
	case Type::String:
		return;
	case Type::Function:
	{
		FunctionType *f = dynamic_cast<FunctionType*>(t);
		FunctionType::iterator iter;
		for (iter = f->begin();iter != f->end();iter++)
			collectAllTypes(*iter, items);
		collectAllTypes(f->getReturnType(), items);
		return;
	};
	case Type::Generator:
	case Type::Sequence:
	case Type::List:
	case Type::Dictionary:
	{
		for (Type::iterator iter = t->begin();iter != t->end();++iter)
			collectAllTypes(*iter, items);
		return;
	}
	case Type::Record:
	{
		RecordType *ra = dynamic_cast<RecordType*>(t);
		RecordType::iterator iter;
		for (iter = ra->begin();iter != ra->end();iter++)
			collectAllTypes(iter->second, items);
	}
	return;
	case Type::Variant:
		// Records for tas are owned by variant
/*		{
			VariantType *va=dynamic_cast<VariantType *>(t);
			VariantType::iterator iter;
			for (iter=va->beginTag();iter!=va->endTag();iter++)
				collectAllTypes(iter->second,items);
		}
*/		return;
	case Type::UserDef:
	{
		OpaqueType *oa = dynamic_cast<OpaqueType*>(t);
		OpaqueType::iterator iter;
		for (iter = oa->begin();iter != oa->end();iter++)
			collectAllTypes(iter->first, items);
	}
	return;
	default:
		//Should not happen, variables are not deleted except by static member in Environment class
		return;
	}
}

void deleteCopy(Type *t)
{
	if (t == NULL) return;
	std::set<Type*> nodes;
	std::set<Type*>::iterator iter;
	collectAllTypes(t, nodes);
	for (iter = nodes.begin();iter != nodes.end();iter++)
		if ((*iter)->getKind() != Type::Variable) delete *iter;
}

void deleteExcept(Type *t, std::set<Type*> exclude)
{
	if (t == NULL) return;
	std::set<Type*> nodes;
	std::set<Type*>::iterator iter;
	collectAllTypes(t, nodes);
	for (iter = nodes.begin();iter != nodes.end();iter++)
		if ((*iter)->getKind() != Type::Variable &&
			exclude.find(*iter) == exclude.end())
			delete *iter;
}


bool OpaqueType::setTypeMapping(FunctionType *ft)
{
	if (shared != NULL) return false; // must be called on hidden-less opaque type
	if (ft->parameterCount() != params.size()) return false;
	FunctionType::iterator iter;
	TypeVariableMap conv;
	iterator param;
	for (param = begin(), iter = ft->begin();iter != ft->end();iter++, param++)
	{
		Type *mytype = (*iter)->substitute(conv);
		param->second = dynamic_cast<TypeVariable*>(mytype);
	};
	shared = new shared_data;
	shared->hidden = ft->getReturnType()->substitute(conv);
	shared->refcount = 1;
	return true;
}

Type *OpaqueType::instantiate(std::list<TypeVariable*> &instparams, size_t line, size_t pos)
{
	if (error != NULL) return error;
	if (shared == NULL)
	{
		error = new ErrorType(line, pos, std::wstring(L"Opaque Type '") + name + L"' has not been instantiated yet!");
		return error;
	}
	TypeVariableMap conv;
	std::list<TypeVariable*>::iterator given;
	iterator iter;
	if (params.size() != instparams.size())
	{
		error = new ErrorType(line, pos, std::wstring(L"Opaque Type '") + name + L"' is being used with the wrong number of paramters!");
		return error;
	};
	for (iter = begin(), given = instparams.begin();iter != end();iter++, given++)
	{
		// Here we actually perform the permutation of the parameters
		conv.insert(std::make_pair(iter->second, *given));
	}
	return shared->hidden->substitute(conv);
}


}
