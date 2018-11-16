#include "stdafx.h"
#include "TypeGraph.h"

namespace intro 
{

void TypeGraph::node::print(std::wostream &os)
{
	os << name;
	if (!my_params.empty())
	{
		os << "(";
		my_params[0]->print(os);
		for (size_t i=1;i<my_params.size();i++)
		{
			os << ",";
			my_params[i]->print(os);
		}
		os << ")";
	}
}

Type::pointer_t TypeGraph::fresh(const std::wstring &name)
{
	//TypeVariable *retval=new TypeVariable(name);
	//variables.push_back(retval);
	//return retval;
	return std::make_shared<TypeVariable>(name);
}

static Type string_t(Type::String);

void TypeGraph::createBuiltins(void)
{
	TypeGraph::node::edge *edge;
	// Set up vector, Top kind is highest by convention
	builtins.resize(Type::Top+1,NULL);
	// The top node
	node *top=new node(L"Top",Type::Top,true);
	//top->super=top; // Top node is special...it's rank is 0
	top->addSuper(top);
	// Unit is for function return tpes when nothing is returned
	node *unit=new node(L"Top",Type::Unit,true);
	//top->super=top; // Top node is special...it's rank is 0
	unit->addSuper(top);
	// Boolean is a direct descendant of Top
	node *boolean=new node(L"Boolean",Type::Boolean);
	boolean->addSuper(top);
	// Comparable is a direct descendant of Top
	node *comparable=new node(L"Comparable",Type::Comparable,true);
	comparable->addSuper(top);
	// Integer <: Real <: Number
	node *number=new node(L"Number",Type::Number,true);
	edge=number->addSuper(comparable);
	edge->parentTemplate.reset(new Type(Type::Comparable));
	node *real=new node(L"Real",Type::Real);
	node *integer=new node(L"Integer",Type::Integer);
	//real->setSuper(number);
	//real->parentTemplate=new Type(Type::Number);
	//integer->setSuper(real);
	//integer->parentTemplate=new Type(Type::Real);
	edge=integer->addSuper(number);
	edge->parentTemplate.reset(new Type(Type::Number));
	//edge=real->addSuper(integer);
	//edge->parentTemplate=new Type(Type::Integer);
	edge=real->addSuper(number);
	edge->parentTemplate.reset(new Type(Type::Number));
	// Generators
	node *generator=new node(L"Generator",Type::Generator);
	edge=generator->addSuper(top);
	edge->parentTemplate.reset(new Type(Type::Top));
	generator->my_params.push_back(Type::pointer_t(fresh(L"?A")));
	// Dictionary(Key,Value)<:Generator([key:Key;value:Value;])
	Type::pointer_t key_t=fresh(L"?Key"),
		value_t=fresh(L"?Value");
	Type::pointer_t dict_to_gen=std::make_shared<RecordType>();
	RecordType *dict_rec = (RecordType*)dict_to_gen.get();
	dict_rec->addMember(L"key",key_t);
	dict_rec->addMember(L"value",value_t);
	node *dictionary=new node(L"Dictionary",Type::Dictionary);
	dictionary->my_params.push_back(key_t);
	dictionary->my_params.push_back(value_t);
	edge=dictionary->addSuper(generator);
	edge->super_params.push_back(dict_to_gen);
	edge->parentTemplate.reset(new Type(Type::Generator, dict_to_gen));
	// Sequences (lists and strings)
	node *sequence=new node(L"Sequence",Type::Sequence,true);
	edge=sequence->addSuper(generator);
	sequence->my_params.push_back(fresh(L"?A"));
	edge->super_params.push_back(sequence->my_params.front());
	edge->parentTemplate.reset(new Type(Type::Generator,sequence->my_params.front()));
	// Lists
	node *list=new node(L"List",Type::List);
	edge=list->addSuper(sequence);
	list->my_params.push_back(fresh(L"?A"));
	edge->super_params.push_back(list->my_params.front());
	edge->parentTemplate.reset(new Type(Type::Generator,list->my_params.front()));
	// String
	node *string=new node(L"String",Type::String);
	edge=string->addSuper(sequence);
	//edge->super_params.push_back(&string_t); // LEAK
	edge->super_params.push_back(Type::pointer_t(new Type(Type::String))); // LEAK
	edge->parentTemplate.reset(new Type(Type::Sequence,edge->super_params.front())); // LEAK
	//edge->parentTemplate = new Type(Type::Sequence, &string_t);
	string->addSuper(comparable)->parentTemplate.reset(new Type(Type::Comparable));
	node *record=new node(L"Record",Type::Record);
	record->addSuper(top);
	edge->parentTemplate.reset(new Type(Type::Top));
	node *variant=new node(L"Variant",Type::Variant);
	edge=variant->addSuper(top);
	edge->parentTemplate.reset(new Type(Type::Top));
	node *function=new node(L"Function",Type::Function);
	edge=function->addSuper(top);
	edge->parentTemplate.reset(new Type(Type::Top));
	// Put them all into the map
	builtins[Type::Top]=top;
	builtins[Type::Unit]=unit;
	builtins[Type::Boolean]=boolean;
	builtins[Type::Comparable]=comparable;
	builtins[Type::Number]=number;
	builtins[Type::Real]=real;
	builtins[Type::Integer]=integer;
	builtins[Type::Generator]=generator;
	builtins[Type::Dictionary]=dictionary;
	builtins[Type::Sequence]=sequence;
	builtins[Type::List]=list;
	builtins[Type::String]=string;
	builtins[Type::Record]=record;
	builtins[Type::Function]=function;
	builtins[Type::Variant]=variant;
}

// Find the node that represents the passed type's position in the type hierarchy, returns NULL on failure.
TypeGraph::node *TypeGraph::getNode(Type::pointer_t type)
{
	if (type->getKind()==Type::UserDef)
	{
		OpaqueType *ot=dynamic_cast<OpaqueType *>(type.get());
		std::map<std::wstring,node*>::iterator oit=opaques.find(ot->getName());
		if (oit==opaques.end()) return NULL;
		else return oit->second;
	}
	else
	{
		// Builtins are just indexed by kind in the array, but be defensive
		if (type->getKind()>builtins.size()) return NULL;
		else return builtins[type->getKind()];
	}
}

void TypeGraph::addOpaque(/*const std::wstring &name,*/Type::pointer_t opaque,Type::pointer_t super)
{
	// Super may be opaque or builtin
	// Super may be top (or abstract?)
}

TypeGraph::TypeGraph()
{
	createBuiltins();
}

TypeGraph::~TypeGraph()
{
	std::map<std::wstring,node*>::iterator oit;
	std::vector<node*>::iterator bit;
	for (bit=builtins.begin();bit!=builtins.end();bit++)
		delete *bit;
	for (oit=opaques.begin();oit!=opaques.end();oit++)
		delete oit->second;
	std::list<TypeVariable*>::iterator vit;
	for (vit=variables.begin();vit!=variables.end();vit++)
		delete *vit;
}

struct state
{
	std::vector<Type::pointer_t > current;
	TypeGraph::node *traverse;
	TypeGraph::node::iterator supers;

	state(TypeGraph::node *traverse_) 
		: traverse(traverse_)
	{
		supers=traverse->supers.begin();
	};

	void clearCurrent(std::set<Type::pointer_t > &exclude)
	{
		//for (std::vector<Type::pointer_t>::iterator iter=current.begin();iter!=current.end();++iter)
		//	deleteExcept(*iter,exclude);
		current.clear();
	}

	state &operator =(const state &other)
	{
		current=other.current;
		traverse=other.traverse;
		//exclude=other.exclude;
		supers=traverse->supers.begin();
		return *this;
	}
};

PartialOrder TypeGraph::findSupertype(Type::pointer_t ta_, Type::pointer_t  tb_,std::vector<Type::pointer_t> &cur,Type::set_t &exclude)
{
	std::vector<state> stack;
	Type::pointer_t ta = ta_->find();
	Type::pointer_t tb = tb_->find();
	TypeGraph::node *start=getNode(ta);
	TypeGraph::node *dest=getNode(tb);
	Type::pointer_t param_source;

	PartialOrder retval=EQUAL;
	// Same rank must be same node
	if (start->rank==dest->rank) 
	{
		if (start!=dest) return ERROR;
		Type::iterator tit,oit;
		for (tit=ta->begin(),oit=tb->begin();tit!=ta->end() && retval!=ERROR;tit++,oit++)
		{
			PartialOrder po=(*tit)->checkSubtype(*oit);
			if (retval==EQUAL) retval=po;
			else if (retval==GREATER)
			{
				if (po!=GREATER && po!=EQUAL) return ERROR;
			}
			else if (retval==LESS)
			{
				if (po!=LESS && po!=EQUAL) return ERROR;
			}
		}

	}
	else if (start->rank > dest->rank) retval=LESS;
	else retval=GREATER;

	// We know the direction we have to check, set up iteration
	if (retval==LESS) param_source=ta;
	else // Must be greater at this point...
	{
		std::swap(start,dest);
		param_source=tb;
	}

	// Set up the mapping for the type's parameters
	//std::vector<Type*> init;
	std::vector<Type::pointer_t>::iterator iter;
	stack.push_back(state(start));
	param_source->getParameterTypes(stack.back().current);
	// Make sure incoming types are not marked for deletion! We do not own them.
	for (iter= stack.back().current.begin();iter!=stack.back().current.end();++iter)
	{
		Type::set_t innertypes;
		collectAllTypes(*iter, innertypes);
		exclude.insert(innertypes.begin(),innertypes.end());
	}
	//stack.push_back(state(start,exclude));
	Type::TypeVariableMap mapping;
	std::vector<Type::pointer_t>::iterator npit;
	std::set<Type*>::iterator siter;
	node *found=NULL;

	
	while (!stack.empty())
	{
		state s=stack.back();
		stack.pop_back();

		if (s.traverse->rank <= dest->rank) // Same or failure along this path
		{
			if (s.traverse==dest)
			{
				//swap(s.current,cur);
				//cur.insert(s.current.begin(), s.current.end());
				// Type variables can end up in here. These are maintained by the Environment,
				// so we remove them from here
				//siter = cur.begin();
				for (iter=s.current.begin();iter!= s.current.end();++iter)
				{
					if ((*iter)->getKind()!=Type::Variable)
						cur.push_back(*iter);
				};
				s.current.clear();
				found=s.traverse;
				break; // Found the common supertype
			}
		}
		else
		{
			for (TypeGraph::node::iterator supers=s.traverse->supers.begin();supers!=s.traverse->supers.end();supers++)
			{
				node *next=(*supers)->super;
				//std::vector<Type*> next_params;
				//next_params.reserve((*supers)->super_params.size());
				Type::TypeVariableMap mapping;
				for (iter=s.current.begin(),npit=s.traverse->my_params.begin();iter!=s.current.end();++iter,++npit)
				{
					// If we substitute non-variables for the variables, the types may be distributed all over
					// That can lead to multiple deletes, so we remember what must remain - 
					// since the type is also owned by the typegraph node it's contained in,
					// we need not care abut deletion (Y)
					mapping.insert(std::make_pair(*npit,*iter));
					if ((*iter)->getKind()!=Type::Variable)
					{
						Type::set_t innertypes;
						collectAllTypes(*iter, innertypes);
						exclude.insert(innertypes.begin(),innertypes.end());
					}
				}
				//stack.push_back(state(next,exclude));
				stack.push_back(state(next));
				state &buf=stack.back();
				for (iter=(*supers)->super_params.begin();iter!=(*supers)->super_params.end();++iter)
					buf.current.push_back((*iter)->substitute(mapping));
			}
		}
		//s.clearCurrent(exclude);
	}
	// Cleanup anything left on the stack
	while (!stack.empty()) 
	{
		//stack.back().clearCurrent(exclude);
		stack.pop_back();
	}
	if (found!=dest) return ERROR;
	return retval;
}

}
