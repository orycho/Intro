#ifndef RUNTIME_H
#define RUNTIME_H

#include <cstdint>
#include <functional>

#include "PerfHash.h"
#include "RTType.h"


// MSVC Needs this in order to have the symbols in the output binary
// Much like -rdynamic does for GCC toolchain.
#ifdef _MSC_VER
#define FORCE_EXPORT __declspec(dllexport)
#else
#define FORCE_EXPORT
#endif

/// Structure holding raference counting data
struct gcdata
{
	/// Coloring is used during cycle detection
	/** see "Concurrent Cycle Collection in Reference Counted Systems" by Bacon and Rajan
	*/
	enum gcColors 
	{
		Black,		///< 0: In use or free (can it be used to indicate free in roots buffer?!)
		Gray,		///< 1: Possible member of cycle
		White,		///< 2: Member of garbage cycle 
		Purple,		///< 3: Possible root of cycle 
		Green,		///< 4: Acyclic
		Red,		///< 5: Candidate cycle under going Sigma Computation
		Orange,		///< 6: Candidate cycle awaiting epoch boundary
		Octarine	///< 7: exempt - for types like Unit and {[None:]} we can have a global, static value, don't delete
	};

	std::uint64_t buffered:1;	///< Flag indicates if this item is bufferen in roots
	std::uint64_t color:3;		///< Current color of node aaccording to the above enum
	std::uint64_t count:60;		///< Count of references - probaly overkill, but we want to kep it all in 64 bits...
};

/// The C way not to care about input types, tag is external in rtt_t parameters.
/** All referenced types begin witha gcdata struct inside, allows uniform handling during garbage collection.
*/
union rtdata
{
	std::uint64_t integer;
	double real;
	bool boolean;
	gcdata *ptr;
};

/// Number of elements to statically allocate for roots buffer
#define MAX_ROOTS (16*1024)

/// Run time type encoding ("simplified" compared to an old approach)
typedef intro::rtt::tag_t rtt_t;

inline void initGC(gcdata *gc)
{
	gc->buffered=0;
	gc->color=gcdata::Black;
	gc->count=1;
}

inline bool isReferenced(rtt_t type)
{
	//intro::rtt::tag_t t=intro::rtt::getType(*type);
	//return t!=intro::rtt::Integer && t!=intro::rtt::Real && t!=intro::rtt::Boolean;
	return type!=intro::rtt::Integer && type!=intro::rtt::Real && type!=intro::rtt::Boolean && type!=intro::rtt::Undefined;
}

inline std::uint64_t combineHash(std::uint64_t h1,std::uint64_t h2)
{
	return (h1<<1) ^ h2;
}

inline std::int32_t hash(const wchar_t *str,std::uint64_t length,std::int32_t d)
{
	d = d==0?0x9e3779b9:d;
	for (size_t i=0;i<length;i++) d=((d*0x01000193)^(std::int32_t)str[i])&0xFFFFFFFF;
	return d;
}

size_t getTag(const std::wstring &label);

#ifdef __cplusplus
extern "C"
{
#endif

/// @name GC Public Interface Functions
///@{
FORCE_EXPORT void increment(rtdata rt,rtt_t type);
FORCE_EXPORT void decrement(rtdata rt,rtt_t type);
///@}

// Integer, Real and Bool are native LLVM types.


// Problem: Attaching data to representation seems elegant, but invalidates references on resize.
// So it is not actually a good idea, sadly. INdirection via pointer to data seems necessary.
// In that case, we can use std::wstring for string. For lists, we could use std::vector
// Casting the contents. Unordered_set however must know the has function to use at compile time...
// So it is easier to implement with a hash function that is rtt aware.
// In the end, it's my call and I prefer rolling my own. First of, clang can turn it into LLVM IR,
// Which may one day be linked in with global optimization with LLMV.

// For records and closures, flexible arrays are usable, since the number of fields does not change.

/// @name List intrinsics
///@{

/// A run time list 
struct rtlist
{
	gcdata gc;
	rtt_t elem_type;		///< The run time type tag of the elements
	std::uint64_t size;		///< number of slots allocated
	std::uint64_t used;		///< number of slots actually used
	rtdata *data;			///< Pointer to data storage
};



FORCE_EXPORT rtlist *allocList(std::uint64_t size,rtt_t elem_type);
FORCE_EXPORT rtt_t getElemTypeList(rtlist *list);
FORCE_EXPORT void setElemTypeList(rtlist *list,rtt_t rtt);
FORCE_EXPORT void freeList(rtlist *list);
FORCE_EXPORT std::uint64_t sizeList(rtlist *list);
FORCE_EXPORT void resizeList(rtlist *list,std::uint64_t newsize);
FORCE_EXPORT void appendList(rtlist *list,rtdata elem);
FORCE_EXPORT rtlist *subList(rtlist *list,std::uint64_t from,std::uint64_t to);
FORCE_EXPORT rtdata itemList(rtlist *list,std::uint64_t at);
///@}

/// @name String intrinsics
///@{
struct rtstring
{
	gcdata gc;
	std::uint64_t size;		///< number of slots allocated
	std::uint64_t used;		///< number of slots actually used
	wchar_t *data;			///< Pointer to data storage
};

FORCE_EXPORT rtstring *allocString(std::uint64_t size);
FORCE_EXPORT void freeString(rtstring *str);
FORCE_EXPORT std::uint64_t sizeString(rtstring *str);
FORCE_EXPORT void resizeString(rtstring *str,std::uint64_t newsize);
FORCE_EXPORT void appendString(rtstring *str,const wchar_t *text,std::uint64_t length);
FORCE_EXPORT rtstring *subString(rtstring *str,std::uint64_t begin,std::uint64_t end);
FORCE_EXPORT void print(rtstring *str);

FORCE_EXPORT bool lessString(rtstring *lhs,rtstring *rhs);
FORCE_EXPORT bool lessEqString(rtstring *lhs,rtstring *rhs);
FORCE_EXPORT bool greaterString(rtstring *lhs,rtstring *rhs);
FORCE_EXPORT bool greaterEqString(rtstring *lhs,rtstring *rhs);
///@}

/// @name Dictionary intrinsics
///@{
struct rtdict
{
	gcdata gc;
	rtt_t key_type;		///< The run time type tag of the elements
	rtt_t value_type;		///< The run time type tag of the elements
	std::uint64_t size_pow;	///< number of slots allocated is the largest prime less than 2^size_pow
	std::uint64_t usedcount;		///< number of slots actually used
	bool *usedflag;			///< we do need a flag to see if a slot is used or not.
	std::uint64_t *hashes;	///< Dynamic Array of hash values
	rtdata *keys;			///< Dynamic Array of pointers to keys (may be coerced to non-ptr int, double or bool)
	rtdata *values;			///< Dynamic Array of pointers to values (may be coerced to non-ptr int, double or bool)
};

FORCE_EXPORT rtdict *allocDict(rtt_t key_type,rtt_t value_type);
FORCE_EXPORT rtt_t getKeyTypeDict(rtdict *dict);
FORCE_EXPORT void setKeyTypeDict(rtdict *dict,rtt_t type);
FORCE_EXPORT rtt_t getValueTypeDict(rtdict *dict);
FORCE_EXPORT void setValueTypeDict(rtdict *dict,rtt_t type);
FORCE_EXPORT void freeDict(rtdict *dict);
FORCE_EXPORT std::uint64_t sizeDict(rtdict *dict);
FORCE_EXPORT void resizeDict(rtdict *dict,rtt_t type,std::uint64_t newsize);
FORCE_EXPORT rtdata *getCreateSlotDict(rtdict *dict,rtdata key);
FORCE_EXPORT void insertDict(rtdict *dict,rtdata key,rtdata value);
FORCE_EXPORT rtdata findDict(rtdict *dict,rtdata key);
FORCE_EXPORT void eraseDict(rtdict *dict,rtdata key);
FORCE_EXPORT void clearDict(rtdict *dict);
///@}

/// @name Record and Variant intrinsics
/** Runtime representation of records (and variants) supporting subtyping.
	The thing is, when the rtt is encoded by the calle, the type of the
	record may come from a container type's parameter. That may be a supertype
	of the actual recurd type, i.e. only a subset of the labels may be known,
	and of course those label's types, where known, may also be incomplete.
	Therefore, it seems that the shared data will have to inlcude the rtt.
	Then again, the label hash is type agnostic, so shared data could be reused 
	between different records with different label types if rtt is kept in the record.
*/
///@{
/*
struct rtrecord_shared
{
	rtt_t type; //??
	std::int32_t size;
	std::int32_t hashes[1];
};
*/

struct innerrecord
{
	std::uint32_t size;	///< number of fields in the record
	std::int32_t *offsets;	///< displacements for perfect hashing
	rtt_t *fieldrtt;	///< pointer to global array of field rtts
	rtdata *fields;		///< Pointer to dynamically allocated array of field values
	const wchar_t **labels; ///< Need the labels for printing and comparing
};

struct rtrecord
{
	gcdata gc;
	innerrecord r;
};

struct rtvariant
{
	gcdata gc;
	std::uint64_t tag; 		///< The variants tag
	innerrecord r;
};

FORCE_EXPORT rtrecord *allocRecord(std::int32_t *displacement,const wchar_t **labels,std::uint32_t fieldcount);
FORCE_EXPORT std::uint32_t getSlotRecord(rtrecord *record,const wchar_t *label,size_t label_length);
FORCE_EXPORT void setFieldRecord(rtrecord *record,std::uint32_t slot,rtdata value,rtt_t rtt);
FORCE_EXPORT rtdata *getFieldRecord(rtrecord *record,std::uint32_t slot);
FORCE_EXPORT rtt_t *getFieldRTTRecord(rtrecord *record,std::uint32_t slot);

FORCE_EXPORT rtvariant *allocVariant(std::int32_t *displacement,const wchar_t **labels, std::uint64_t tag, std::uint32_t fieldcount);
FORCE_EXPORT std::uint64_t getTagVariant(rtvariant *variant);
FORCE_EXPORT std::uint32_t getSlotVariant(rtvariant *variant,const wchar_t *label,size_t label_length);
FORCE_EXPORT void setFieldVariant(rtvariant *variant,std::uint32_t slot,rtdata value,rtt_t rtt);
FORCE_EXPORT rtdata *getFieldVariant(rtvariant *variant,std::uint32_t slot);
FORCE_EXPORT rtt_t *getFieldRTTVariant(rtvariant *variant,std::uint32_t slot);

///@}

/// @name Function closure intrinsics
///@{
	
struct rtclosure_field
{
	rtdata field;
	rtt_t type;
};

/// Closures represent a higher order function with bound environment
struct rtclosure
{
    gcdata gc;				///< Always start with garbage collector data
	void *code;
    std::uint64_t size;		///< number of slots allocated
	rtclosure_field fields[1];
};

#define MKCLOSURE(name,codeptr) rtclosure name = { \
	{false, gcdata::Octarine, 1}, \
	(void*)codeptr, 1, \
	{ 0ll, 0} };

FORCE_EXPORT rtclosure *allocClosure(std::uint32_t freevarcont);

FORCE_EXPORT void sioPrint_(rtclosure *closure, rtdata data, rtt_t rtt);
FORCE_EXPORT rtstring *sioRead_(rtclosure *closure, rtt_t *retvalrtt);

FORCE_EXPORT extern rtclosure sioPrint;
FORCE_EXPORT extern rtclosure sioRead;

///@}

/// @name Generator closure intrinsics
///@{

/// Runtime struct for generators, a special type of functions
struct rtgenerator
{
    gcdata gc;					///< Always with the garbage...
	bool (*code)(rtgenerator*);	///< Generator functino to call, returns true when done (no value available)
	size_t size;	///< number of slots allocated
	rtdata retval;	///< The value generated by a call to code that returend false
	rtt_t retvalrtt;
	size_t state;	///< used by yield statements to set next entry point.
	rtclosure_field fields[1];
};

typedef bool (*genbody)(rtgenerator*);

FORCE_EXPORT rtgenerator *allocGenerator(std::uint32_t varcount,genbody code);
FORCE_EXPORT void freeGenerator(rtgenerator *gen);
//FORCE_EXPORT rtgenerator *allocRangeGenerator(std::int64_t from,std::int64_t to,std::int64_t by);
FORCE_EXPORT rtdata getResultGenerator(rtgenerator *gen);
FORCE_EXPORT rtt_t getResultTypeGenerator(rtgenerator *gen);
FORCE_EXPORT bool callGenerator(rtgenerator *gen);
FORCE_EXPORT void setResultGenerator(rtgenerator *gen,rtdata value,rtt_t rtt);
FORCE_EXPORT std::uint64_t getStateGenerator(rtgenerator *gen);
FORCE_EXPORT void setStateGenerator(rtgenerator *gen,std::uint64_t newstate);

FORCE_EXPORT rtgenerator *allocListGenerator(rtlist *list);
FORCE_EXPORT rtgenerator *allocStringGenerator(rtstring *str);
FORCE_EXPORT rtgenerator *allocDictGenerator(rtdict *dict);



///@}

/// @name Polymorphic intrinsics (Top type)
///@{

FORCE_EXPORT std::uint64_t hashPoly(rtdata data,rtt_t type);
FORCE_EXPORT bool equalPoly(rtdata lhs,rtt_t lhstype,rtdata rhs,rtt_t rhstype);
FORCE_EXPORT void toStringPoly(rtstring *str,rtdata data,rtt_t type);

///@}

#ifdef __cplusplus
}
#endif

#endif