#ifndef TYPEGRAPH_H
#define TYPEGRAPH_H

#include <string>
#include <vector>

#include <map>
#include <list>


#include "Type.h"

namespace intro
{

/// A TypeGraph describes the subtype relations that hold in a a type system 
/** <p>It holds complete information about every types parent type, and any variable
	transformations required. For instance, Dictionary(?V,?K)&lt;:Generator([key:?K;value:?V;]).</p>

	<p>It should be remembered that opaque types provie mechanisms for adding to the type hierarchy,
	even though we can expect most opaque types to be records (and union types, when implemented).</p>

	<p>The TypeGraph owns all types used internally, as it created them itself...</p>
*/
class TypeGraph
{
public:
	/// Nodes represent subtype hierarchy and provide data for converting a given type to it's supertype.
	struct node
	{
		struct edge
		{
			/// Type parameters used to represent this type as a supertype, e.g. Dictionary(A,B)<:Generator([key:A;value:B;])
			/** <p>This vector of types, together with the type variables in my_params, describes how to turn a 
				type's parameters into it's supertype's paramters (which need not be the same number),
				by using my_params to build a variable mapping for a substitution (i.e. which also creates
				a new, "copied" types which must be manually deleted after use).</p>

				<p>To continue the Dictionary(A,B)&lt;:Generator([key:A;value:B;]) example, for this
				my_params would the dictionary node's parameters A' and B', and super_params
				would contain one record type [key:A';value:B';].</p>

				<p> Given this information in the node, and a concrete use of e.g. a Dictionary(Integer,String)
				as a generator, the subtype check would build a mapping {A'=>Integer,B'=>String} which
				subtitutes the record type to [key:Integer;value:String;] which is what we need
				to continue type inference.</p>
			*/
			std::vector<Type*> super_params; ///< Replace with actual type of parent expressed with above variables, so that we can just copy for return value!
			Type *parentTemplate; ///< used with this types paramter to convert a type instance to it's supertype isntance.
			node *super,*sub;
			typedef std::vector<Type*>::iterator iterator;

			inline edge(void) : parentTemplate(NULL), super(NULL), sub(NULL)
			{};

			inline ~edge(void)
			{
				//if (parentTemplate!=NULL) deleteCopy(parentTemplate);
				for (Type *t : super_params)
					if (t->getKind()!=Type::Variable)
						delete t;
				if (parentTemplate != NULL) delete parentTemplate;
				//std::vector<Type*>::iterator it;
				//for (it=super_params.begin();it!=super_params.end();it++)
				//	deleteCopy(*it);
				
			};
		};

		std::wstring name; ///< Name of the type?
		size_t rank; ///< The rank is the distance from the Top type, that is the number of supertypes. Type Error has no meaningfull rank
		std::vector<TypeVariable*> my_params;	///< Type parameters of this type, see member super_params
		bool isAbstract; ///< An abstract type does not have actual values, but has subtypes that do have values.
		Type::Types my_kind; ///< Nodes refer to the type they represent.
		typedef std::vector<edge*> edges; ///< Type for lists of edges (connections between nodes).
		typedef std::vector<TypeVariable*>::iterator param_iter;
		typedef edges::iterator iterator; ///< An nterator tpe for edges.
		edges children; ///< Edges linking to subtypes.
		edges supers; ///< Edges linking to supertypes.
	

		// Convenient, not reliable
		inline node(const std::wstring &name_,Type::Types kind,bool isAbstract_=false) 
			: name(name_)
			, rank(0)
			, isAbstract(isAbstract_)
			, my_kind(kind)
		{ };

		inline ~node(void)
		{
			for (iterator iter=supers.begin();iter!=supers.end();iter++)
				delete (*iter);
		};

		/// Set a nodes supernode and rank
		inline edge *addSuper(node *sup)
		{
			edge *e=new edge;
			e->sub=this;
			e->super=sup;
			if (rank<(sup->rank+1)) rank=sup->rank+1;
			supers.push_back(e);
			if (this!=sup) sup->children.push_back(e);
			return e;
		};

		void print(std::wostream &os);
	};
private:
	/// Manage opaque types by name, they are user defined so the total number os opaque types is dynamic
	std::map<std::wstring,node*> opaques;
	/// Builtins are referenced by numeric value with a static maximum, so we can use a vector (or array) for fast lookup
	std::vector<node*> builtins;

	std::list<TypeVariable*> variables;

	TypeVariable *fresh(const std::wstring &name);
	/// Add the built in types to the type graph
	void createBuiltins(void);

public:

	void addOpaque(OpaqueType *opaque,Type *super);

	TypeGraph();

	~TypeGraph();

	/// Given a type, returns the parent type with type paramters transformations applied
	/** Note the returned type should be treated as a copied type, i.e. deleted with deleteCopy() once
		done with.
	*/
	//Type *getParentType(Type *type);

	PartialOrder findSupertype(Type *ta,Type*tb,std::vector<Type*> &cur,std::set<Type*> &exclude);

	// Find the node that represents the passed type's position in the type hierarchy, returns NULL on failure.
	node *getNode(Type *type); 
	
	inline bool isAbtractType(Type *type)
	{
		node*n=getNode(type);
		return n->isAbstract;
	}

	static inline void clearMapping(std::vector<Type*> &cur, std::set<Type*> &exclusions)
	{
		for (std::vector<Type*>::iterator iter = cur.begin();iter != cur.end();++iter)
			deleteExcept(*iter, exclusions);
	}

};

}

#endif