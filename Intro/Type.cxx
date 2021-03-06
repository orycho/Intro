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

/// This function is used to collect the covariant relations of contained types into the relation in accu
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
}

bool Type::isAbstract(void)
{
	return theGraph.isAbtractType(shared_from_this());
}

void Type::print(std::wostream &s)
{
	switch (type)
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
	iterator iter = begin();
	if (iter != end())
	{
		s << "(";
		(*iter)->find()->print(s);
		iter++;
		while (iter != end())
		{
			s << ",";
			(*iter)->find()->print(s);
			iter++;
		}
		s << ")";
	}

}

PartialOrder Type::checkSubtype(pointer_t other)
{
	pointer_t ta = find();
	pointer_t tb = other->find();

	// Error and Top are simpe
	if (ta->getKind() == Type::Error)
		return LESS; // propagate errors (bottom type)
	else if (tb->getKind() == Type::Error)
		return GREATER; // propagate errors (bottom type)

	// This is not an optimization: type variables are special because we must check supertype relation.
	if (ta->getKind() == Type::Variable && tb->getKind() == Type::Variable)
		return ta->getSupertype()->checkSubtype(tb->getSupertype());
	else if (ta->getKind() == Type::Variable)
		return ta->getSupertype()->checkSubtype(tb);
	else if (tb->getKind() == Type::Variable)
		return ta->checkSubtype(tb->getSupertype());

	if (ta->getKind() == Type::Top && tb->getKind() == Type::Top)
		return EQUAL;
	else if (ta->getKind() == Type::Top)
		return GREATER;		// All types are subtypes of Top
	else if (tb->getKind() == Type::Top)
		return LESS;		// All types are subtypes of Top

	return internalCheckSubtype(tb);
}

PartialOrder Type::internalCheckSubtype(pointer_t other)
{
	std::vector<Type::pointer_t> current;
	PartialOrder retval=theGraph.findSupertype(shared_from_this(),other,current);
	return retval;
}

PartialOrder FunctionType::internalCheckSubtype(pointer_t other)
{
	if (other->getKind() != Type::Function) return ERROR;
	FunctionType *fb = dynamic_cast<FunctionType*>(other.get());
	if (parameterCount() != fb->parameterCount()) return ERROR;
	FunctionType::iterator ai, bi;
	PartialOrder val = EQUAL; // will be mutated via reference by combineOrders
	for (ai = begin(), bi = fb->begin();ai != end() && val != ERROR;ai++, bi++)
	{
		combinePartialOrders(val, (*bi)->checkSubtype(*ai));
	}
	if (val == ERROR)
		return ERROR;
	// Note the source functions for the return types are 
	// switched (compared to loop), the call to checkSubtype 
	// is thus contravariant. Same for the return statement
	PartialOrder po = fb->getReturnType()->checkSubtype(getReturnType());
	if (po != ERROR)// && po != val)
		return val;
	else
		return ERROR;
	return val;
}

// Parts of this function look like what combinePartialOrders(), but are more restrictive
PartialOrder RecordType::internalCheckSubtype(pointer_t other)
{
	if (other->getKind() != Type::Record) return ERROR;
	RecordType *rb = dynamic_cast<RecordType*>(other.get());
	RecordType::member_iter ai, bi;
	if (size() < rb->size()) // rb<:ra or error
	{
		PartialOrder val = GREATER;
		for (ai = begin();ai != end();ai++)
		{
			bi = rb->findMember(ai->first);
			if (bi == rb->end()) return ERROR;
			switch (ai->second->checkSubtype(bi->second))
			{
			case GREATER:
				if (val == EQUAL || val == GREATER) val = GREATER;
				else return ERROR;
				break;
			case EQUAL: break;
			case LESS:
			case ERROR: return ERROR;
			}
		}
		return val;

	}
	else if (size() > rb->size()) // ra<:rb or error
	{
		PartialOrder val = LESS;
		for (bi = rb->begin();bi != rb->end();bi++)
		{
			ai = findMember(bi->first);
			if (ai == end()) return ERROR;
			switch (ai->second->checkSubtype(bi->second))
			{
			case LESS:
				if (val == EQUAL || val == LESS) val = LESS;
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
		PartialOrder val = EQUAL; // again, mutated by combinePartialOrders via reference.
		for (ai = begin();ai != end();ai++)
		{
			bi = rb->findMember(ai->first);
			if (bi == rb->end()) return ERROR;
			combinePartialOrders(val, ai->second->checkSubtype(bi->second));
		}
		return val;
	}
}

// Parts of this function look like what combinePartialOrders(), but are more restrictive
PartialOrder VariantType::internalCheckSubtype(pointer_t other)
{
	if (other->getKind() != Type::Variant) return ERROR;
	VariantType *rb = dynamic_cast<VariantType*>(other.get());
	VariantType::iterator ai, bi;
	if (size() < rb->size()) // rb<:ra or error
	{
		PartialOrder val = LESS;
		for (ai = beginTag();ai != endTag();ai++)
		{
			bi = rb->findTag(ai->first);
			if (bi == rb->endTag()) return ERROR;
			switch (ai->second->checkSubtype(bi->second))
			{
			case EQUAL: break;
			case LESS:
				if (val == EQUAL || val == LESS) val = LESS;
				else return ERROR;
				break;
			case GREATER:
			case ERROR:
				return ERROR;
			}
		}
		return val;

	}
	else if (size() > rb->size()) // ra<:rb or error
	{
		PartialOrder val = GREATER;
		for (bi = rb->beginTag();bi != rb->endTag();bi++)
		{
			ai = findTag(bi->first);
			if (ai == endTag()) return ERROR;
			switch (ai->second->checkSubtype(bi->second))
			{
				break;
			case EQUAL: break;
			case GREATER:
				if (val == EQUAL || val == GREATER) val = GREATER;
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
		PartialOrder val = EQUAL; // again, mutated by combinePartialOrders via reference.
		for (ai = beginTag();ai != endTag();ai++)
		{
			bi = rb->findTag(ai->first);
			if (bi == rb->endTag()) return ERROR;
			combinePartialOrders(val, ai->second->checkSubtype(bi->second));
		}
		return val;
	}
}
/*
PartialOrder OpaqueType::internalCheckSubtype(pointer_t other)
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
Type::pointer_t mergeRecordTypes(RecordType *a, RecordType *b)
{
	RecordType *result = new RecordType(); // needs to be deleted if 
	std::wstring errmsg = std::wstring();	// Collect all errors here
	//std::wstring errnbuff=std::wstring();
	//std::wstringstream ws=std::wstringstream(errmsg);
	std::wstringstream ws;
	RecordType::member_iter iter, found;
	// For each label in a: if it is not in b, keep it, if it is in b,
	// check subtyping and take the more restrictive one
	for (iter = a->begin();iter != a->end();iter++)
	{
		found = b->findMember(iter->first);
		if (found == b->end())
		{
			result->addMember(iter->first, iter->second);
		}
		else
		{
			switch (iter->second->checkSubtype(found->second))
			{
			case LESS:
				// Same, just fall through to keeping a's label
			case EQUAL:
				// keep label in record a
				result->addMember(iter->first, iter->second->copy());
				break;
			case GREATER:
				// keep label in rectord b
				result->addMember(found->first, found->second->copy());
				break;
			case ERROR:
				ws << "The label '" << iter->first << "' refers to incompatible types in the two records.\n";

			}
		}
	}
	// Skip if errors occured above
	if (!ws.str().empty())
	{
		delete result;
		return std::make_shared<ErrorType>(1, 1, ws.str());
	}
	// For each label in b: if it is not in a already, it can be kept as we checked the intersection labels.
	for (iter = b->begin();iter != b->end();iter++)
	{
		// skip what is already in a
		if (a->findMember(iter->first) != a->end()) continue;
		// add the rest
		result->addMember(iter->first, iter->second);
	}
	return Type::pointer_t(result);
}

Type::pointer_t intersectVariantTypes(VariantType *a, VariantType *b, bool specialize)
{
	Type::pointer_t result = Type::pointer_t(new VariantType()); // needs to be deleted if 
	//Environment::addIntermediate(result);
	std::wstring errmsg = std::wstring();	// Collect all errors here
	std::wstringstream ws;
	VariantType::iterator iter, found;
	for (iter = a->beginTag();iter != a->endTag();iter++)
	{
		found = b->findTag(iter->first);
		if (found != b->endTag())
		{
			if (iter->second->unify(found->second, specialize))
				((VariantType*)result.get())->addTag(iter);
			else
			{
				ws << "The label '" << iter->first << "' refers to incompatible types in the two variants.\n";
			}
		}
	}
	// Skip if errors occured above
	if (!ws.str().empty())
	{
		return Type::pointer_t(new ErrorType(1, 1, ws.str()));
	}
	return result;
}

bool Type::unify(Type::pointer_t b, bool specialize)
{
	Type::pointer_t mroot = find();
	Type::pointer_t oroot = b->find();
	if (mroot.get() == oroot.get()) return true;
	// The preference for non-Variables (i.e. specialization)
	// is fundamental for the workings of the unification algorithm,
	// see Purple Dragon Book, Algorithm 6.19.
	if (mroot->type == Error)	// Error trumps all others
	{
		oroot->parent = mroot;
		return false;
	}
	else if (oroot->type == Error) // Error trumps all others
	{
		mroot->parent = oroot;
		return false;
	}
	if (mroot->type == Top)	// Everything is more specific than Top
	{
		mroot->parent = oroot;
		return true;
	}
	else if (oroot->type == Top) // Everything is more specific than Top
	{
		oroot->parent = mroot;
		return true;
	}
	if (mroot->type == Variable && oroot->type == Variable)
	{
		Type::pointer_t oroot_sup = oroot->getSupertype()->find();
		Type::pointer_t mroot_sup = mroot->getSupertype()->find();
		switch (oroot_sup->checkSubtype(mroot_sup->find()))
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
			if (oroot_sup->getKind() == Type::Record && mroot_sup->getKind() == Type::Record)
			{
				RecordType *orec = dynamic_cast<RecordType*>(oroot_sup.get()),
					*mrec = dynamic_cast<RecordType*>(mroot_sup.get());
				Type::pointer_t merged = mergeRecordTypes(orec, mrec);
				if (merged->getKind() != Type::Error) merged = Environment::fresh(merged);
				oroot->parent = merged;
				mroot->parent = merged;
				return merged->getKind() != Type::Error;
			}
			if (oroot_sup->getKind() == Type::Variant && mroot_sup->getKind() == Type::Variant)
			{
				Type::pointer_t intersect = intersectVariantTypes(dynamic_cast<VariantType*>(oroot_sup.get())
					, dynamic_cast<VariantType*>(mroot_sup.get()), specialize);
				if (intersect->getKind() != Type::Error) intersect = Environment::fresh(intersect);
				oroot->parent = intersect;
				mroot->parent = intersect;
				return intersect->getKind() != Type::Error;
			}
			// if all of the above failed, the types may still be unifiable
			if (!oroot_sup->unify(mroot_sup, specialize))
				return false;
			oroot->parent = mroot->find();
			return true;
		case LESS:
			if (!oroot_sup->unify(mroot_sup, specialize))
				return false;
			mroot->parent = oroot->find();
			return true;
		case EQUAL:
		case GREATER:
			if (!oroot_sup->unify(mroot_sup, specialize))
				return false;
			oroot->parent = mroot->find();
			return true;
		}
	}
	else if (mroot->type == Variable && oroot->type != Variable)
	{
		Type::pointer_t mroot_sup = mroot->getSupertype()->find();
		if (!mroot_sup->unify(oroot, false /*specialize*/))
			return false;
		mroot->parent = oroot->find();
		return true;
	}
	else if (oroot->type == Variable && mroot->type != Variable)
	{
		Type::pointer_t oroot_sup = oroot->getSupertype()->find();
		if (!oroot_sup->unify(mroot, false /*specialize*/))
			return false;
		oroot->parent = mroot->find();
		return true;
	}
	return oroot->internalUnify(mroot, specialize);
}

// Simple types
bool Type::internalUnify(Type::pointer_t other, bool specialize)
{
	//SubtypeTraversal traverse(theGraph,this,other);
	std::vector<Type::pointer_t >::iterator iter;
	std::vector<Type::pointer_t >::iterator oit;
	//currentMapping.clear(); // TODO: Possible leak
	std::vector<Type::pointer_t > currentMapping;
	// First, check the type graph if a legal path actually exists

	PartialOrder order = theGraph.findSupertype(find(), other->find(), currentMapping);

	if (order == ERROR) return false;
	Type::pointer_t goal = find();
	Type::pointer_t source = other->find();
	if (order == LESS) std::swap(goal, source); // order indepenent code hereafter ;)
	goal->parent = source;
	// Graph traversal may have mutated the parameters, here we check they match up with source type
	// by unifing the result with the supertype's type
	// The goal is to manage the relations between the types parameters, a good example is
	// Dictionary(String,Integer) <: Generator([key:String; value:Integer;])
	// where when expecting a generator, we have to infer the record and it's field types
	// from the dictionary and it's type parameters
	bool retval = true;
	if (source->getKind() != goal->getKind())
	{
		for (iter = currentMapping.begin(), oit = goal->begin();iter != currentMapping.end() && retval;iter++, oit++)
		{
			retval &= (*iter)->unify(*oit, specialize);
		}
	}
	else
	{
		// If the types are the same, then there was no traversal - we get the identity mapping.
		for (iter = source->begin(), oit = goal->begin();iter != source->end() && retval;iter++, oit++)
		{
			retval &= (*iter)->unify(*oit, specialize);
		}
	}
	return retval;
}

bool FunctionType::internalUnify(Type::pointer_t other, bool specialize)
{
	FunctionType *fb = dynamic_cast<FunctionType*>(other.get());
	if (fb == NULL) return false;
	if (parameterCount() != fb->parameterCount()) return false;
	FunctionType::iterator ai, bi;
	for (ai = begin(), bi = fb->begin();ai != end();ai++, bi++)
	{
		if (!(*ai)->unify(*bi, !specialize)) return false;
	}
	return getReturnType()->unify(fb->getReturnType(), specialize);
}

bool RecordType::internalUnify(Type::pointer_t other, bool specialize)
{
	Type::pointer_t ra = shared_from_this();
	Type::pointer_t rb = other;
	if (rb == NULL) return false;
	RecordType::member_iter ai, bi;
	// Unification with subtypes
	// result type must be the lesser type, i.e the subtype with more labels.
	// The larger record must contain all labels that occur in the smaller one,
	// and these labels' types must be unified.
	//PartialOrder po = ra->checkSubtype(rb);
	//if (po == ERROR) return false;
	//else if (po == GREATER) std::swap(ra, rb);
	if (((RecordType*)rb.get())->members.size() > ((RecordType*)ra.get())->members.size())
		std::swap(ra, rb);
	// rb should now have less or equal many labels
	for (bi = ((RecordType*)rb.get())->begin();bi != ((RecordType*)rb.get())->end();++bi)
	{
		ai = ((RecordType*)ra.get())->findMember(bi->first);
		if (ai == ((RecordType*)ra.get())->end())
			return false;
		if (!ai->second->unify(bi->second, specialize))
			return false;
	}
	return true;
}

bool VariantType::internalUnify(Type::pointer_t other, bool specialize)
{
	VariantType *vb = dynamic_cast<VariantType*>(other.get());
	if (vb == NULL) return false;
	VariantType::iterator ai, bi;
	if (specialize) // excpect [:A ...] with ?a<:{[:A ...] + ...}
	//if (!specialize)
	{
		// Tags present in both : 
		VariantType *va = this;
		if (vb->size() > va->size())
			std::swap(va, vb);
		for (ai = va->beginTag();ai != va->endTag();ai++)
		{
			bi = vb->findTag(ai->first);
			if (bi != vb->endTag())
			{
				if (!ai->second->unify(bi->second, specialize))
					return false;
			}
		}

		for (bi = vb->beginTag();bi != vb->endTag();bi++)
		{
			ai = findTag(bi->first);
			if (ai == endTag())
				addTag(bi);
		}
		vb->setParent(va->shared_from_this());
	}
	else
	{
		VariantType *va = this;
		if (tags.size() > vb->tags.size())
		{
			va = vb;
			vb = this;
		}
		for (ai = va->tags.begin();ai != va->tags.end();++ai)
		{
			bi = vb->findTag(ai->first);
			if (bi == vb->endTag())
				return false;
			if (!ai->second->unify(bi->second, specialize))
				return false;
		}
		vb->setParent(va->shared_from_this());
	}
	return true;
}

bool OpaqueType::internalUnify(Type::pointer_t other, bool specialize)
{
	OpaqueType *ob = dynamic_cast<OpaqueType*>(other.get());
	if (ob == NULL) return false;
	if (getName() != ob->getName()) return false;
	OpaqueType::iterator itera, iterb;
	for (itera = begin(), iterb = ob->begin();itera != end();itera++, iterb++)
	{
		if (!itera->first->unify(iterb->first, specialize)) return false;
	}
	return true;
}

/// Global variable used by type copy operation to name variables.

Type::pointer_t Type::internalCopy(TypeVariableMap &conv)
{
	Type::pointer_t thecopy = std::make_shared<Type>(getKind(), getAccessFlags());

	for (iterator iter = begin();iter != end();++iter)
	{
		thecopy->addParameter((*iter)->substitute(conv));
	}
	return thecopy;
}

Type::pointer_t TypeVariable::internalCopy(TypeVariableMap &conv)
{
	TypeVariableMap::iterator iter = conv.find(shared_from_this());
	if (iter == conv.end())
	{
		//TypeVariable *buf=Environment::fresh(numToString(variableCounter++),getSupertype()->substitute(conv));
		Type::pointer_t copy = getSupertype()->substitute(conv);
		//if (copy->getKind() != Type::Variable)
		//	ownedtypes.push_back(copy);
		Type::pointer_t buf = Environment::fresh(getName(), copy);
		//TypeVariable *buf = Environment::fresh(getName(), getSupertype(),conv);
		buf->setAccessFlags(getAccessFlags());
		// Hopefully this works, otherwise we have to get tricky comparing by embedded pointer...
		iter = conv.insert(std::make_pair(shared_from_this(), buf)).first;
	}
	return iter->second;
}

Type::pointer_t ErrorType::internalCopy(TypeVariableMap &conv)
{
	return Type::pointer_t(new ErrorType(getLine(),getColumn(),getMessage()));
}

Type::pointer_t FunctionType::internalCopy(TypeVariableMap &conv)
{
	FunctionType *b = new FunctionType(getReturnType()->substitute(conv));
	b->setAccessFlags(getAccessFlags());
	for (Type::iterator iter = begin();iter != end();iter++)
		b->addParameter((*iter)->substitute(conv));
	return Type::pointer_t(b);
}

Type::pointer_t  RecordType::internalCopy(TypeVariableMap &conv)
{
	RecordType *b = new RecordType();
	for (RecordType::member_iter iter = begin();iter != end();iter++)
		b->addMember(iter->first, iter->second->substitute(conv));
	b->setAccessFlags(getAccessFlags());
	return Type::pointer_t(b);
}

Type::pointer_t VariantType::internalCopy(TypeVariableMap &conv)
{
	VariantType *b = new VariantType();
	for (VariantType::iterator iter = beginTag();iter != endTag();iter++)
		b->addTag(iter->first, iter->second->substitute(conv));
	b->setAccessFlags(getAccessFlags());
	return Type::pointer_t(b);
}

Type::pointer_t OpaqueType::internalCopy(TypeVariableMap &conv)
{
	OpaqueType *cp=new OpaqueType(getName());
	cp->shared=shared; 
	if (shared!=NULL) shared->refcount++;
	//std::wcout << "Copying "<<getName()<<":\n";
	for (OpaqueType::iterator iter=begin();iter!=end();iter++) 
	{
		cp->addParameter(iter->first->substitute(conv));
		//cp->params.back().first->print(std::wcout);
		//std::wcout << "\n";
	}
	cp->setAccessFlags(getAccessFlags());
	return Type::pointer_t (cp);
}

void collectAllTypes(Type::pointer_t t,Type::set_t &items)
{
	if (t->getKind() == Type::Variable)
	{
		TypeVariable *tv = (TypeVariable *)t.get();
		collectAllTypes(tv->getSupertype(), items);
		return;
	}
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
		FunctionType *f = dynamic_cast<FunctionType*>(t.get());
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
		RecordType *ra = dynamic_cast<RecordType*>(t.get());
		RecordType::member_iter iter;
		for (iter = ra->begin();iter != ra->end();iter++)
			collectAllTypes(iter->second, items);
	}
	return;
	case Type::Variant:
		{
			VariantType *va=dynamic_cast<VariantType *>(t.get());
			for (VariantType::iterator iter = va->beginTag();iter != va->endTag();iter++)
				collectAllTypes(iter->second, items);
		}
		return;
	case Type::UserDef:
	{
		OpaqueType *oa = dynamic_cast<OpaqueType*>(t.get());
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

bool OpaqueType::setTypeMapping(FunctionType *ft)
{
	if (shared != NULL) return false; // must be called on hidden-less opaque type
	if (ft->parameterCount() != params.size()) return false;
	FunctionType::iterator iter;
	TypeVariableMap conv;
	iterator param;
	for (param = begin(), iter = ft->begin();iter != ft->end();iter++, param++)
	{
		Type::pointer_t mytype = (*iter)->substitute(conv);
		param->second = mytype;
	};
	shared = new shared_data;
	shared->hidden = ft->getReturnType()->substitute(conv);
	shared->refcount = 1;
	return true;
}

Type::pointer_t OpaqueType::instantiate(std::vector<Type::pointer_t> &instparams, size_t line, size_t col)
{
	if (error != NULL) return error;
	if (shared == NULL)
	{
		error = Type::pointer_t(new ErrorType(line, col, std::wstring(L"Opaque Type '") + name + L"' has not been instantiated yet!"));
		return error;
	}
	TypeVariableMap conv;
	std::vector<Type::pointer_t >::iterator given;
	iterator iter;
	if (params.size() != instparams.size())
	{
		error = Type::pointer_t(new ErrorType(line, col, std::wstring(L"Opaque Type '") + name + L"' is being used with the wrong number of paramters!"));
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
