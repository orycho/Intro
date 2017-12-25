#include "stdafx.h"
#include "Runtime.h"
#include <cstddef>
#include <cwchar>

#include <string.h>
#include <assert.h>
#include <utility>
#include <iostream>
#include "city.h"
#include <unordered_map>

using namespace intro::rtt;


std::unordered_map<std::wstring,size_t> label2tag;
std::unordered_map<size_t,std::wstring> tag2label;

size_t getTag(const std::wstring &label)
{
	if (label2tag.size()==0) // make sure builtin variants have constant tags...
	{
		tag2label.insert(std::make_pair(label2tag.size(),L"None"));
		label2tag.insert(std::make_pair(L"None",0ull));
		tag2label.insert(std::make_pair(label2tag.size(),L"Some"));
		label2tag.insert(std::make_pair(L"Some",1ull));
		
	}
	std::unordered_map<std::wstring,size_t>::iterator iter=label2tag.find(label);
	if (iter==label2tag.end())
	{
		size_t newtag=label2tag.size();
		tag2label.insert(std::make_pair(newtag,label));
		iter=label2tag.insert(std::make_pair(label,newtag)).first;
	}
	return iter->second;
}

std::wstring getLabel(size_t tag)
{
	// Label must exist!!!
	if (tag2label.size()==0) getTag(L"None");
	return tag2label.find(tag)->second;
}

#ifdef __cplusplus
extern "C"
{
#endif

std::uint64_t getDictCapacity(std::uint64_t pow2);
size_t findSlot(rtdict *dict,std::uint64_t hash,rtdata key);

// Built in type for None
rtvariant rtnone = 
{
	{false,gcdata::Octarine,1}, // gcdata
	0, // tag
	{ 0, nullptr, nullptr, nullptr} // innerrecord
};

/// Polymorphic hash function
std::uint64_t hashPoly(rtdata data,rtt_t type)
{
	//switch(getType(type[0]))
	switch(type)
	{
		case Integer:
		{
			return std::hash<long long>{}((long long)data.integer);
		}
		case Real:
		{
			return std::hash<double>{}(data.real);
		}
		case Boolean:
		{
			return std::hash<bool>{}(data.boolean);
		}
		case String:
		{
			rtstring *str=(rtstring*)data.ptr;
			return CityHash64((const char*)str->data,str->used*sizeof(wchar_t));
		}
		case List:
		{
			//rtt_t elem_type=type+1;
			rtlist *list=(rtlist*)data.ptr;
			std::uint64_t hash=0xDEADBEEF00000000ull; // Just a type specific ini value
			for (std::uint64_t i=0;i<list->used;++i)
				hash=combineHash(hash,hashPoly(list->data[i],list->elem_type));
			return hash;
		}
		case Dictionary:
		{
			rtdict *dict=(rtdict*)data.ptr;
			//rtt_t key_type=dict->key_type; // doesn't have to use key, use precomupted - but have toskip it for value type
			rtt_t value_type=dict->value_type;
			std::uint64_t hash=0x00000000DEADBEEFull; // Just a type specific ini value
			std::uint64_t *hashes=dict->hashes;
			rtdata *values=dict->values;
			std::uint64_t seen=0;
			for (std::uint64_t i=0;seen<dict->usedcount;++i)
			{
				if (!dict->usedflag[i]) continue;
				hash=combineHash(hash,hashes[i]);
				hash=combineHash(hash,hashPoly(values[i],value_type));
				++seen;
			}
			return hash;
		}
		case Function:
		{
			rtclosure *func=(rtclosure*)data.ptr;
			return (std::uint64_t)func; // points in memory should be unique and sparse
		}
		case Record:
		case Variant:
		{
			innerrecord *r;
			std::uint64_t hash=0;
			if (type==Variant) // also differ when tag differs.
			{
				rtvariant *var=(rtvariant*)data.ptr;
				hash=var->tag;
				r=&(var->r);
			}
			else
			{
				rtrecord *rec=(rtrecord*)data.ptr;
				r=&(rec->r);

			}
			for (size_t i=0;i<r->size;++i)
			{
				hash=combineHash(hash,hashPoly(r->fields[i],r->fieldrtt[i]));
			}
			return hash;
		}
		case Generator:
		break;
		case BuiltIn:
		case Label:
		case ParamCount:
		case Undefined:
			// Only so -wpedantic does not complain - maybe assert(false)?
		break;
	}

	return 0;
}

/// Polymorphic equality function
bool equalPoly(rtdata lhs,rtt_t lhstype,rtdata rhs,rtt_t rhstype)
{
	//switch(getType(type[0]))
	switch(lhstype)
	{
		case Integer:
		{
			if (rhstype==Integer)
				return lhs.integer==rhs.integer;
			else
				return ((double)lhs.integer)==rhs.real;
		}
		case Real:
		{
			if (rhstype==Integer)
				return lhs.real==((double)rhs.integer);
			else
				return lhs.real==rhs.real;
			//return memcmp(lhs,rhs,sizeof(double))==0;
		}
		case Boolean:
		{
			return lhs.boolean&rhs.boolean;
		}
		case String:
		{
			rtstring *l=(rtstring*)lhs.ptr;
			rtstring *r=(rtstring*)rhs.ptr;
			if (l->used != r->used) return false;
			return memcmp(l->data,r->data,sizeof(wchar_t)*l->used)==0;
		}
		case List:
		{
			rtlist *l=(rtlist*)lhs.ptr;
			rtlist *r=(rtlist*)rhs.ptr;
			if (l->used != r->used) return false;
			//rtt_t elem_type=type+1;
			for (std::uint64_t i=0;i<l->used;++i)
				if (!equalPoly(l->data[i],l->elem_type,r->data[i],r->elem_type)) return false;
			return true;
		}
		case Dictionary:
		{
			rtdict *l=(rtdict*)lhs.ptr;
			rtdict *r=(rtdict*)rhs.ptr;
			if (l->usedcount != r->usedcount) return false;
			rtt_t lhs_value_type=l->value_type;
			rtt_t rhs_value_type=l->value_type;
			std::uint64_t seen=0;
			std::uint64_t r_capacity=getDictCapacity(r->size_pow);
			for (std::uint64_t i=0;seen<l->usedcount;++i)
			{
				if (!l->usedflag[i]) continue;
				uint64_t slot=findSlot(r,l->hashes[i],l->keys[i]);
				// did we find an empty/unused slot??
				if (slot > r_capacity) return false;
				if (!r->usedflag[slot]) return false;
				// Key and hash was compared during lookup!
				//if (l->hashes[i]!=r->hashes[slot]) return false;
				//if (!equalPoly(l->keys[i],r->keys[slot],key_type)) return false;
				if (!equalPoly(l->values[i],lhs_value_type,r->values[slot],rhs_value_type)) return false;
				++seen;
			}
			return true;
		}
		case Record:	// !! Note that the type system requires records to be structurally identical
		case Variant:	// !! I.e. they have the same type... not supertype!
		{
			// Uniform treatment of fields in innerrecord
			innerrecord *lr,*rr;
			if (lhstype==Variant) // also differ when tag differs.
			{
				rtvariant *lv=(rtvariant*)lhs.ptr,*rv=(rtvariant*)rhs.ptr;
				if (lv->tag!=rv->tag) return false;
				lr=&(lv->r);
				rr=&(rv->r);
			}
			else
			{
				rtrecord *lv=(rtrecord*)lhs.ptr,*rv=(rtrecord*)rhs.ptr;
				lr=&(lv->r);
				rr=&(rv->r);
			}
			if (lr->size!=rr->size) return false;
			// Compare fields
			for (size_t i=0;i<lr->size;++i)
			{
				//if (wcscmp(lr->labels[i],lr->labels[i])!=0) return false;
				if (lr->fieldrtt[i]!=rr->fieldrtt[i]) return false;
				if (!equalPoly(lr->fields[i],lr->fieldrtt[i],rr->fields[i],rr->fieldrtt[i])) return false;;
			}
			return true;
		}
		case Function:
		case Generator:
		{
			return lhs.ptr==rhs.ptr;
		}
		case BuiltIn:
		case Label:
		case ParamCount:
		case Undefined:
			// Only so -wpedantic does not complain - maybe assert(false)?
		break;
	}
	return false;
}

/// Polymorphic string conversion function
void toStringPoly(rtstring *str,rtdata data,rtt_t type)
{
	wchar_t buffer[128];
	switch(type)
	{
		case Integer:
		{
			int length=std::swprintf(buffer, sizeof(buffer),L"%lld", data.integer);
			appendString(str,buffer,(std::uint64_t)length);
			return;
		}
		case Real:
		{
			int length=std::swprintf(buffer, sizeof(buffer),L"%f", data.real);
			appendString(str,buffer,length);
			return;
		}
		case Boolean:
		{
			if (data.boolean) appendString(str,L"true",4);
			else appendString(str,L"false",5);
			return;
		}
		case String:
		{
			rtstring *buf=(rtstring*)data.ptr;
			appendString(str,buf->data,buf->used);
			return;
		}
		case List:
		{
			//rtt_t elem_type=type+1;
			rtlist *list=(rtlist*)data.ptr;
			appendString(str,L"{",1);
			for (std::uint64_t i=0;i<list->used;++i)
			{
				toStringPoly(str,list->data[i],list->elem_type);
				if (i+1<list->used) appendString(str,L", ",2);
			}
			appendString(str,L"}",1);
			return;
		}
		case Dictionary:
		{
			rtdict *dict=(rtdict*)data.ptr;
			rtt_t key_type=dict->key_type; // doesn't have to use key, use precomupted - but have toskip it for value type
			rtt_t value_type=dict->value_type;
			std::uint64_t seen=0;
			appendString(str,L"{",1);
			if (dict->usedcount==0) appendString(str,L"=>",2);
			else for (std::uint64_t i=0;seen<dict->usedcount;++i)
			{
				if (!dict->usedflag[i]) continue;
				toStringPoly(str,dict->keys[i],key_type);
				appendString(str,L"=>",2);
				toStringPoly(str,dict->values[i],value_type);
				++seen;
				if (seen<dict->usedcount) appendString(str,L", ",2);
			}
			appendString(str,L"}",1);
			return;
		}
		case Function:
			appendString(str,L"function",8);
			return;
		case Record:
		case Variant:
		{
			innerrecord *r;
			appendString(str,L"[",1);
			if (type==Variant) // also differ when tag differs.
			{
				rtvariant *var=(rtvariant*)data.ptr;
				std::wstring label=getLabel(var->tag);
				appendString(str,L" :",2);
				appendString(str,label.c_str(),label.size());
				r=&(var->r);
			}
			else
			{
				rtrecord *rec=(rtrecord*)data.ptr;
				r=&(rec->r);
			}
			appendString(str,L" ",1);
			for (size_t i=0;i<r->size;++i)
			{
				appendString(str,r->labels[i],wcslen(r->labels[i]));
				appendString(str,L"<-",2);
				toStringPoly(str,r->fields[i],r->fieldrtt[i]);
				appendString(str,L"; ",2);
			}
			appendString(str,L"]",1);
			return;
		}
		case Generator:
			appendString(str,L"generator",9);
			return;
		case BuiltIn:
		case Label:
		case ParamCount:
		case Undefined:
			// Only so -wpedantic does not complain - maybe assert(false)?
			// Instead put message in result string...
			appendString(str,L"Undefined!",10);
		break;
	}
}

std::int32_t some_offsets[1];
wchar_t const *some_labels[1]={L"value"};

rtvariant *allocSomeVariant(rtdata value,rtt_t rtt)
{
	static bool hasOffsets=false;
	if (!hasOffsets)
	{
		std::set<std::wstring> keys{L"value"};
		util::mkPerfectHash(keys,some_offsets);
		hasOffsets=true;
	};
	size_t tag=getTag(L"Some");
	rtvariant *variant=allocVariant(some_offsets, some_labels, tag, 1);
	setFieldVariant(variant,0,value,rtt);
	return variant;
}

rtvariant *getNoneVariant(void)
{
	return &rtnone;
}

std::int32_t key_value_offsets[2];
wchar_t const *key_value_labels[2];

rtrecord *allocKeyValueRec(rtdata key,rtt_t key_rtt,rtdata value,rtt_t value_rtt)
{
	static bool hasOffsets=false;
	if (!hasOffsets)
	{
		std::set<std::wstring> keys{L"key", L"value"};
		util::mkPerfectHash(keys,key_value_offsets);
		hasOffsets=true;
		std::int32_t keyslot=util::find(L"key",3,key_value_offsets,2);
		key_value_labels[keyslot]=L"key";
		key_value_labels[1-keyslot]=L"value";
	
	};
	rtrecord *record=allocRecord(key_value_offsets, key_value_labels, 2);
	std::int32_t keyslot=util::find(L"key",3,record->r.offsets,record->r.size);
	
	setFieldRecord(record,keyslot,key,key_rtt);
	setFieldRecord(record,1-keyslot,value,value_rtt);
	return record;
}

// List ///////////////
rtlist *allocList(std::uint64_t size,rtt_t type)
{
    rtlist *result=(rtlist*)malloc(sizeof(rtlist));
	initGC(&(result->gc));
	result->elem_type=type;
    result->used=0;//result->size=size;
	result->size=size;
    if (size==0)
        result->data=nullptr;
    else
        result->data=(rtdata*)malloc(sizeof(rtdata)*size);
    return result;
}

rtt_t getElemTypeList(rtlist *list)
{
	return list->elem_type;
}

void setElemTypeList(rtlist *list,rtt_t rtt)
{
	list->elem_type=rtt;
}

// Free up a list
void freeList(rtlist *list)
{
	/// Iterate over contents and decrement, if referenced values.
	if (isReferenced(list->elem_type)) for (std::uint64_t i=0;i<list->used;++i)
		decrement(list->data[i],list->elem_type);
    free(list->data);
	memset(list,0,sizeof(rtlist));
    free(list);
	
}

std::uint64_t sizeList(rtlist *list)
{ return list->used; }

/// Resize a list, new size must hold all used elements (more like std::vector::reserve).
void resizeList(rtlist *list,std::uint64_t newsize)
{
	newsize*=2;
	if (newsize==0) newsize=16;
	rtdata *newmem=(rtdata*)malloc(sizeof(rtdata)*newsize);
	memcpy(newmem,list->data,list->used*sizeof(rtdata));
	free(list->data);
	list->data=newmem;
	list->size=newsize;
}

void appendList(rtlist *list,rtdata elem)
{
	if (list->used==list->size) resizeList(list,list->size*2);
	list->data[list->used].ptr=elem.ptr;
	increment(elem,list->elem_type);
	list->used++;
}

rtlist *subList(rtlist *list,std::uint64_t from,std::uint64_t to)
{
	std::uint64_t begin=from;
	std::uint64_t end=to;
	bool reverse=false;
	if (begin>end)
	{
		std::swap(begin,end);
		reverse=true;
	}
	if (begin > list->used) return allocList(0,list->elem_type);
	if (end > list->used) end=list->used;
	std::uint64_t length=end-begin+1;
	rtlist *result=allocList(length,list->elem_type);
	if (reverse)
	{
		size_t src,dst;
		for (src=end,dst=0;dst<=length;--src,++dst)
			result->data[dst]=list->data[src];
	}
	else memcpy(result->data,&(list->data[begin]),length*sizeof(rtdata));
	result->used=length;
	return result;
}
rtdata itemList(rtlist *list,std::uint64_t at)
{
	// some paranoia here?
	return list->data[at];
}
// String ///////////////
/// Alocate a string string with the given size preallocated.
rtstring *allocString(std::uint64_t size)
{
    rtstring *result=(rtstring*)malloc(sizeof(rtstring));
	initGC(&(result->gc));
	result->gc.color=gcdata::Green;
    result->used=0;
    if (size==0) result->size=8;
	else result->size=size+1; // add nullterm
	result->data=(wchar_t*)malloc(sizeof(wchar_t)*result->size);
	//memset(result->data,0,sizeof(wchar_t)*result->size);
	result->data[0]=0;
	return result;
}


void freeString(rtstring *str)
{
    free(str->data);
 	memset(str,0,sizeof(rtstring));
	free(str);
}

std::uint64_t sizeString(rtstring *str)
{ return str->used; }

// Resize a string, excess characters will be cut off.
void resizeString(rtstring *str,std::uint64_t newsize)
{
    wchar_t *old=str->data;
    if (str->used>newsize) str->used=newsize;
	str->size=newsize;
	str->data=(wchar_t*)malloc(sizeof(wchar_t)*newsize);
	memcpy(str->data,old,sizeof(wchar_t)*str->used);
	free(old);
}

void appendString(rtstring *str,const wchar_t *text,std::uint64_t length)
{
	// Check if we need to increase available space for text + null-terminator
	if (str->size<str->used+length+1)
	{
		size_t newlen=str->size*2;
		while (newlen<str->used+length+1) newlen*=2;
		wchar_t *buf=(wchar_t*)malloc(newlen*sizeof(wchar_t));
		memcpy(buf,str->data,str->used*sizeof(wchar_t));
		free(str->data);
		str->data=buf;
		str->size=newlen;
	}
	memcpy(&str->data[str->used],text,length*sizeof(wchar_t));
	str->used+=length;
	str->data[str->used]=0;
}

rtstring *subString(rtstring *str,std::uint64_t from,std::uint64_t to)
{
	std::uint64_t begin=from;
	std::uint64_t end=to;
	bool reverse=false;
	if (begin>end)
	{
		std::swap(begin,end);
		reverse=true;
	}
	if (begin>str->used) return allocString(0);
	if (end>str->used) end=str->used;
	std::uint64_t length=end-begin+1;
	rtstring *result=allocString(length+1); // reserve space for zero terminator.
	memset(result->data,L'a',length*sizeof(wchar_t));
	if (reverse)
	{
		// Copy the slow way
		size_t src,dst;
		for (src=end,dst=0;dst<=length;--src,++dst)
			result->data[dst]=str->data[src];
	}
	else memcpy(result->data,&(str->data[begin]),length*sizeof(wchar_t));
	result->data[length]=0; // we want to be C compatible ;)
	result->used=length;
	return result;
}

void print(rtstring *str)
{
	std::wcout << L" = " << str->data << std::endl;
}

bool lessString(rtstring *lhs,rtstring *rhs)
{
	int cmp=wcscmp(lhs->data,rhs->data);
	return cmp < 0;
}
bool lessEqString(rtstring *lhs,rtstring *rhs)
{
	int cmp=wcscmp(lhs->data,rhs->data);
	return cmp <= 0;
}
bool greaterString(rtstring *lhs,rtstring *rhs)
{
	int cmp=wcscmp(lhs->data,rhs->data);
	return cmp > 0;
}
bool greaterEqString(rtstring *lhs,rtstring *rhs)
{
	int cmp=wcscmp(lhs->data,rhs->data);
	return cmp >= 0;
}



// Dictionary ///////////////

/// Given the size_pow, return the actual number of slots.
/** pow2 must be between 8 and 64. This returns the largest prime smaller than 2^pow2.
*/
uint64_t getDictCapacity(std::uint64_t pow2)
{
	static const std::uint64_t primeOffset[] =
	{
		5,   3,  3,  9,  3,  1,  3, 19, 15,  1,
		5,   1,  3,  9,  3, 15,  3, 39,  5, 39,
		57,  3, 35,  1,  5,  9, 41, 31,  5, 25,
		45,  7, 87, 21, 11, 57, 17, 55, 21,115,
		59, 81, 27,129, 47,111, 33, 55,  5, 13,
		27, 55, 93,  1, 57, 25, 59
	};
	assert( pow2>=8 );
	return (1ll<<pow2)-primeOffset[pow2-8];
}

/// returns the slot where the key would go - slot is either empty or match or collision
// what if passed key type is not the same as the internal key type (just a subtype relation)
// can happen e.g. during comparison...
size_t findSlot(rtdict *dict,std::uint64_t hash,rtdata key)
{
	// compute hash based on rtt...
	//std::uint64_t hash=hashPoly(key,key_type);
	std::uint64_t capacity=getDictCapacity(dict->size_pow);
	std::uint64_t slot=hash % capacity;
	//unsigned int max_displacement=capacity_pow*2;
	unsigned int i=0;
	// Find slot that has same key or is empty within displacement
	do
	{
		//wprintf(L"Hash %ld -> Slot: %ld (Displacement %ld)?\n",hash,slot,i);
		if (!dict->usedflag[slot]) return slot;
		if (dict->hashes[slot]==hash && equalPoly(dict->keys[slot],dict->key_type,key,dict->key_type)) return slot;
		++slot;
		slot=(slot < capacity)?slot:0;
		++i;
	} while (i<dict->size_pow && slot < capacity);
	return capacity+1;
}

/// increase size of container, each increase roughly doubles the size.
void grow(rtdict *dict,std::uint64_t increase=1)
{
	std::uint64_t old_capacity=getDictCapacity(dict->size_pow);
	dict->size_pow+=increase;
	assert(dict->size_pow<64);
	std::uint64_t new_capacity=getDictCapacity(dict->size_pow);
	
	std::uint64_t *hashes=dict->hashes;
	rtdata *keys=dict->keys;
	rtdata *values=dict->values;
	bool *usedflag=dict->usedflag;
	
	dict->hashes=(uint64_t*)malloc(sizeof(std::uint64_t)*new_capacity);
	dict->keys=(rtdata*)malloc(sizeof(rtdata)*new_capacity);
	dict->values=(rtdata*)malloc(sizeof(rtdata)*new_capacity);
	dict->usedflag=(bool*)malloc(sizeof(bool)*new_capacity);
	
	// Must ensure empty values are recognized!
	memset(dict->hashes,0,sizeof(std::uint64_t)*new_capacity);
	memset(dict->values,0,sizeof(rtdata)*new_capacity);
	memset(dict->keys,0,sizeof(rtdata)*new_capacity);
	memset(dict->usedflag,0,sizeof(bool)*new_capacity);
	
	for (std::uint64_t i=0;i<old_capacity;++i)
	{
		if (!usedflag[i]) continue;
		insertDict(dict,keys[i],values[i]);
	}
	free(hashes);
	free(keys);
	free(values);
	free(usedflag);
}

rtdict *allocDict(rtt_t key_type,rtt_t value_type)
{
    rtdict *dict=(rtdict*)malloc(sizeof(rtdict));
	initGC(&(dict->gc));
	dict->key_type=key_type;
	dict->value_type=value_type;
	dict->usedcount=0;
	dict->size_pow=8;
	std::uint64_t capacity=getDictCapacity(dict->size_pow);
	//wprintf(L"Allocating %ld slots.\n",capacity);
	dict->hashes=(std::uint64_t*)malloc(sizeof(std::uint64_t)*capacity);
	dict->keys=(rtdata*)malloc(sizeof(rtdata)*capacity);
	dict->values=(rtdata*)malloc(sizeof(rtdata)*capacity);
	dict->usedflag=(bool*)malloc(sizeof(bool)*capacity);
	memset(dict->hashes,0,sizeof(std::uint64_t)*capacity);
	memset(dict->values,0,sizeof(rtdata)*capacity);
	memset(dict->keys,0,sizeof(rtdata)*capacity);
	memset(dict->usedflag,0,sizeof(bool)*capacity);
	return dict;
}

rtt_t getKeyTypeDict(rtdict *dict)
{
	return dict->key_type;
}

void setKeyTypeDict(rtdict *dict,rtt_t type)
{
	dict->key_type=type;
}

rtt_t getValueTypeDict(rtdict *dict)
{
	return dict->value_type;
}

void setValueTypeDict(rtdict *dict,rtt_t type)
{
	dict->value_type=type;
}
void freeDict(rtdict *dict)
{
	std::uint64_t capacity=getDictCapacity(dict->size_pow);
	for (std::uint64_t i=0;i<capacity;++i)
	{
		if (!dict->usedflag[i]) continue;
		decrement(dict->keys[i],dict->key_type);
		decrement(dict->values[i],dict->value_type);
	}
	free(dict->hashes);
	free(dict->keys);
	free(dict->values);
	free(dict->usedflag);
	memset(dict,0,sizeof(rtdict));
	free(dict);
}

std::uint64_t sizeDict(rtdict *dict)
{ return dict->usedcount; }

void resizeDict(rtdict *dict,rtt_t type,std::uint64_t newsize)
{
	//uint64_t capacity=getDictCapacity(dict->size_pow);
}

rtdata *getCreateSlotDict(rtdict *dict,rtdata key)
{
	std::uint64_t capacity=getDictCapacity(dict->size_pow);
	std::uint64_t hash=hashPoly(key,dict->key_type);
	size_t slot=findSlot(dict,hash,key);
	if (slot>=capacity)
	{
		grow(dict);
		return getCreateSlotDict(dict,key);;
	}
	if (!dict->usedflag[slot])
	{
		// Only allocate key storage if it's a new key...
		dict->hashes[slot]=hash;
		dict->keys[slot].ptr=key.ptr;
		increment(key,dict->key_type);
		dict->usedflag[slot]=true;
		dict->usedcount++;
	} 
	return &dict->values[slot];
}
void insertDict(rtdict *dict,rtdata key,rtdata value)
{
	rtdata *slot=getCreateSlotDict(dict,key);
	slot->ptr=value.ptr;
	increment(*slot,dict->value_type);
	return;
}

rtdata findDict(rtdict *dict,rtdata key)
{
	std::uint64_t capacity=getDictCapacity(dict->size_pow);
	std::uint64_t hash=hashPoly(key,dict->key_type);
	size_t slot=findSlot(dict,hash,key);
	rtdata noneptr;
	noneptr.ptr=(gcdata*)&rtnone;
	if (slot>=capacity) return noneptr;
	if (!dict->usedflag[slot]) return noneptr;
	rtdata retval;
	retval.ptr=(gcdata*)allocSomeVariant(dict->values[slot],dict->value_type);
	return retval;
}

/// Erase the item with the given key from the dict
void eraseDict(rtdict *dict,rtdata key)
{
	std::uint64_t capacity=getDictCapacity(dict->size_pow);
	std::uint64_t hash=hashPoly(key,dict->key_type);
	std::uint64_t slot=findSlot(dict,hash,key);
	if (slot>=capacity) return;
	if (!dict->usedflag[slot]) return;
	decrement(dict->keys[slot],dict->key_type);
	decrement(dict->values[slot],dict->value_type);
	dict->hashes[slot]=0;
	dict->keys[slot].ptr=nullptr;
	dict->values[slot].ptr=nullptr;
	dict->usedflag[slot]=false;
	size_t oldslot=slot;
	// Need to make sure following items, possibly displaced, will still be found by closing gaps.
	for (size_t i=1;i<dict->size_pow && oldslot+i < capacity && dict->usedflag[oldslot+i];++i)
	{
		size_t slot2=oldslot+i; // this slot we are checking
		size_t slot2f=findSlot(dict,dict->hashes[slot2],dict->keys[slot2]); // this slot is being found with key and hash
		if (slot2!=slot2f)
		{
			//printf("  Expect current slot to fill %ld (%ld) == %ld the slot found\n",slot,slot2,slot2f);
			dict->hashes[slot]=dict->hashes[slot2];
			dict->keys[slot].ptr=dict->keys[slot2].ptr;
			dict->values[slot].ptr=dict->values[slot2].ptr;
			dict->usedflag[slot]=true;
			dict->hashes[slot2]=0;
			dict->keys[slot2].ptr=nullptr;
			dict->values[slot2].ptr=nullptr;
			dict->usedflag[slot2]=false;
			slot=slot2;
		}
	}
	dict->usedcount--;
}

/// Turn the dictionary into an empty one
void clearDict(rtdict *dict)
{
	std::uint64_t capacity=getDictCapacity(dict->size_pow);
	for (std::uint64_t i=0;i<capacity;++i)
	{
		if (!dict->usedflag[i]) continue;
		decrement(dict->keys[i],dict->key_type);
		decrement(dict->values[i],dict->value_type);
	}
	free(dict->hashes);
	free(dict->keys);
	free(dict->values);
	dict->usedcount=0;
	dict->size_pow=8;
	capacity=getDictCapacity(dict->size_pow);
	dict->hashes=(std::uint64_t*)malloc(sizeof(std::uint64_t)*capacity);
	dict->keys=(rtdata*)malloc(sizeof(rtdata)*capacity);
	dict->values=(rtdata*)malloc(sizeof(rtdata)*capacity);
	memset(dict->hashes,0,sizeof(std::uint64_t)*capacity);
	memset(dict->values,0,sizeof(rtdata)*capacity);
	memset(dict->keys,0,sizeof(rtdata)*capacity);
	memset(dict->usedflag,0,sizeof(bool)*capacity);
}

void setInnerRecord(innerrecord *ir,std::int32_t *offsets,const wchar_t **labels,std::uint32_t fieldcount)
{
	ir->size=fieldcount;
	if (fieldcount==0)
	{
		ir->offsets=nullptr;
		ir->fieldrtt=nullptr;
		ir->fields=nullptr;
		ir->labels=nullptr;
		return;
	}
	ir->offsets=offsets;
	ir->fieldrtt=(rtt_t*)malloc(sizeof(rtt_t)*fieldcount);
	ir->fields=(rtdata*)malloc(sizeof(rtdata)*fieldcount);
	ir->labels=labels;
}

void freeInnerrecord(innerrecord *ir)
{
	free(ir->fields);
	free(ir->fieldrtt);
}

rtrecord *allocRecord(std::int32_t *offsets,const wchar_t **labels,std::uint32_t fieldcount)
{
	rtrecord *record=(rtrecord*)malloc(sizeof(rtrecord));
	//rtrecord *record =(rtrecord*)malloc(offsetof(rtrecord, fields[fieldcount]));
	initGC(&(record->gc));
	setInnerRecord(&(record->r),offsets,labels,fieldcount);
	return record;
}

void freeRecord(rtrecord *record)
{
	freeInnerrecord(&(record->r));
	free(record);
}

void setFieldRecord(rtrecord *record,std::uint32_t slot,rtdata value,rtt_t rtt)
{
	record->r.fields[slot]=value;
	record->r.fieldrtt[slot]=rtt;
}

std::uint32_t getSlotRecord(rtrecord *record,const wchar_t *label,size_t label_length)
{
	return (std::uint32_t)util::find(label,label_length,record->r.offsets,record->r.size);
}

// Maybe pass perfect hash displacements instead of rtrecord? Then also good for variant...
// rtdata *getField(std::int32 *displace,wchar_t label,size_t length)
rtdata *getFieldRecord(rtrecord *record,std::uint32_t slot)
{
	return &(record->r.fields[slot]);
}

rtt_t *getFieldRTTRecord(rtrecord *record,std::uint32_t slot)
{
	return &(record->r.fieldrtt[slot]);
}

rtvariant *allocVariant(std::int32_t *offsets,const wchar_t **labels, std::uint64_t tag, std::uint32_t fieldcount)
{
	rtvariant *variant=(rtvariant*)malloc(sizeof(rtvariant));
	//rtrecord *record =(rtrecord*)malloc(offsetof(rtrecord, fields[fieldcount]));
	initGC(&(variant->gc));
	variant->tag=tag;
	setInnerRecord(&(variant->r),offsets,labels,fieldcount);
	return variant;
}

void freeVariant(rtvariant *variant)
{
	freeInnerrecord(&(variant->r));
	free(variant);
}

void setFieldVariant(rtvariant *variant,std::uint32_t slot,rtdata value,rtt_t rtt)
{
	variant->r.fields[slot]=value;
	variant->r.fieldrtt[slot]=rtt;
}

std::uint64_t getTagVariant(rtvariant *variant)
{
	return variant->tag;
}

std::uint32_t getSlotVariant(rtvariant *variant,const wchar_t *label,size_t label_length)
{
	return (std::uint32_t) util::find(label,label_length,variant->r.offsets,variant->r.size);	
}

rtdata *getFieldVariant(rtvariant *variant,std::uint32_t slot)
{
	return &(variant->r.fields[slot]);
}

rtt_t *getFieldRTTVariant(rtvariant *variant,std::uint32_t slot)
{
	return &(variant->r.fieldrtt[slot]);
}

rtclosure *allocClosure(std::uint32_t freevarcount)
{
	rtclosure *closure=(rtclosure*)malloc(offsetof(rtclosure, fields[freevarcount]));
	initGC(&(closure->gc));
	closure->size=(std::uint64_t)freevarcount;
	return closure;
}

void freeClosure(rtclosure *closure)
{
	for (size_t i=0;i<closure->size;++i)
		decrement(closure->fields[i].field,closure->fields[i].type);
	free(closure);
}

bool listGenerator(rtgenerator *gen)
{
	rtlist *list=(rtlist*)gen->fields[0].field.ptr;
	switch (gen->state)
	{
		case 0: 
		{
			gen->fields[1].field.integer=0;
			gen->state=1;
		}
		case 1: 
		{
			if (gen->fields[1].field.integer>=list->used)
			{
				gen->state=2;
				decrement(gen->retval,gen->retvalrtt);
				gen->retval.ptr=nullptr;
				gen->retvalrtt=Undefined;
				return false;
			}
			if (gen->retvalrtt==Undefined)
			{
				gen->retvalrtt=list->elem_type;
			}
			else
			{
				decrement(gen->retval,gen->retvalrtt);
			}
			gen->retval.ptr=list->data[gen->fields[1].field.integer].ptr;
			increment(gen->retval,gen->retvalrtt);
			++gen->fields[1].field.integer;
			return true;
		}
		case 2:
			return false;
	}
	return true;
}

bool stringGenerator(rtgenerator *gen)
{
	rtstring *str=(rtstring*)gen->fields[0].field.ptr;
	switch (gen->state)
	{
		case 0: 
		{
			gen->fields[1].field.integer=0;
			gen->state=1;
		}
		case 1: 
		{
			if (gen->fields[1].field.integer>=str->used)
			{
				gen->state=2;
				decrement(gen->retval,gen->retvalrtt);
				gen->retvalrtt=Undefined;
				return false;
			}
			if (gen->retvalrtt==Undefined)
			{
				gen->retvalrtt=intro::rtt::String;
			}
			else
			{
				decrement(gen->retval,gen->retvalrtt);
			}
			gen->retval.ptr=(gcdata*)allocString(1);
			appendString((rtstring *)gen->retval.ptr,&(str->data[gen->fields[1].field.integer]),1);
			++gen->fields[1].field.integer;
			return true;
		}
		case 2:
			return false;
	}
	return true;
}

bool dictGenerator(rtgenerator *gen)
{
	rtdict *dict=(rtdict*)gen->fields[0].field.ptr;
	switch (gen->state)
	{
		case 0: 
		{
			gen->fields[1].field.integer=0; // Next slot to examine
			gen->fields[2].field.integer=0;	// Number of used slots seen
			gen->state=1;
		}
		case 1: 
		{
			if (gen->fields[2].field.integer>=dict->usedcount)
			{
				gen->state=2;
				decrement(gen->retval,gen->retvalrtt);
				gen->retvalrtt=Undefined;
				return false;
			}
			if (gen->retvalrtt==Undefined)
			{
				gen->retvalrtt=intro::rtt::Record;
			}
			else
			{
				decrement(gen->retval,gen->retvalrtt);
			}
			// find next slot that is in use
			while (!dict->usedflag[gen->fields[1].field.integer]) ++gen->fields[1].field.integer;
			std::uint32_t slot=(std::uint32_t)gen->fields[1].field.integer;
			// build result dictionary
			gen->retval.ptr=(gcdata*)allocKeyValueRec(dict->keys[slot],dict->key_type,dict->values[slot],dict->value_type);
			// advance both the slot and the elements seen count
			++gen->fields[1].field.integer;
			++gen->fields[2].field.integer;
			return true;
		}
		case 2:
			return false;
	}
	return true;
}

rtgenerator *allocGenerator(std::uint32_t varcount,genbody code)
{
	rtgenerator *result=(rtgenerator*)malloc(offsetof(rtgenerator, fields[varcount]));
	initGC(&(result->gc));
	result->code=code;
	result->state=0;
	result->size=varcount;
	result->retvalrtt=Undefined;
	result->retval.ptr=nullptr;
	return result;
}

rtgenerator *allocListGenerator(rtlist *list)
{
	rtgenerator *result=allocGenerator(2,listGenerator);
	result->fields[0].field.ptr=(gcdata*)list;
	result->fields[1].field.integer=0;
	result->fields[0].type=intro::rtt::List;
	result->fields[1].type=intro::rtt::Integer;
	result->retvalrtt = Undefined;
	result->retval.ptr = nullptr;
	increment(result->fields[0].field,intro::rtt::List);
	return result;
}

rtgenerator *allocStringGenerator(rtstring *str)
{
	rtgenerator *result=allocGenerator(2,stringGenerator);
	result->fields[0].field.ptr=(gcdata*)str;
	result->fields[1].field.integer=0;
	result->fields[0].type=intro::rtt::String;
	result->fields[1].type=intro::rtt::Integer;
	result->retvalrtt = Undefined;
	result->retval.ptr = nullptr;
	increment(result->fields[0].field,intro::rtt::String);
	return result;
}

rtgenerator *allocDictGenerator(rtdict *dict)
{
	rtgenerator *result=allocGenerator(3,dictGenerator);
	result->fields[0].field.ptr=(gcdata*)dict;
	result->fields[1].field.integer=0;
	result->fields[2].field.integer=0;
	result->fields[0].type=intro::rtt::Dictionary;
	result->fields[1].type=intro::rtt::Integer;
	result->fields[2].type=intro::rtt::Integer;
	result->retvalrtt = Undefined;
	result->retval.ptr = nullptr;
	rtdata buf;
	buf.ptr=(gcdata*)dict;
	increment(buf,Dictionary);
	return result;
}

bool callGenerator(rtgenerator *gen)
{
	return gen->code(gen);
}

rtdata getResultGenerator(rtgenerator *gen)
{
	return gen->retval;
}

rtt_t getResultTypeGenerator(rtgenerator *gen)
{
	return gen->retvalrtt;
}

std::uint64_t getStateGenerator(rtgenerator *gen)
{
	return gen->state;
}
void setStateGenerator(rtgenerator *gen,std::uint64_t newstate)
{
	gen->state=newstate;
}

void setResultGenerator(rtgenerator *gen,rtdata value,rtt_t rtt)
{
	gen->retval.ptr=value.ptr;
	gen->retvalrtt=rtt;
}

void freeGenerator(rtgenerator *gen)
{
	if (gen->retvalrtt!=Undefined)
		decrement(gen->retval,gen->retvalrtt);
	for (size_t i=0;i<gen->size;++i)
		decrement(gen->fields[i].field,gen->fields[i].type);
	free(gen);
}

//////////////////////////////////////////////////////
// Garbage Collection ///////////////////////////////
struct root_t
{
	gcdata *ptr;	///< Direct access to garbage collection data
	rtt_t type;		///< Type is needed for iterating over children
};

static root_t roots[MAX_ROOTS];	///< Root buffer for cycle collection
static size_t root_count=0;	///< Number of roots in buffer

int compareRoots(const void *a, const void *b)
{
	const root_t *lhs=(const root_t*)a;
	const root_t *rhs=(const root_t*)b;
	if ( *(size_t*)lhs->ptr >  *(size_t*)rhs->ptr ) return -1;
	if ( *(size_t*)lhs->ptr == *(size_t*)rhs->ptr ) return 0;
	//if ( *(size_t*)lhs->ptr <  *(size_t*)rhs->ptr ) 
	return 1;
}

void compactRoots()
{
	qsort(roots,MAX_ROOTS,sizeof(root_t),compareRoots);
}

void release(gcdata *ptr,rtt_t type);
void possibleRoot(gcdata *ptr,rtt_t type);
void markGrey(gcdata *ptr,rtt_t type);
void scan(gcdata *ptr,rtt_t type);
void scanBlack(gcdata *ptr,rtt_t type);
void collectWhite(gcdata *ptr,rtt_t type);
void collectCycles();
void markRoots();
void scanRoots();
void collectRoots();

typedef void (*rtoper)(gcdata *ptr,rtt_t type);

void iterateChildren(gcdata *ptr,rtt_t type,rtoper op)
{
//printf("IterateChildren %d\n",(int)type);
	//switch(getType(type[0]))
	switch(type)
	{
		case Integer:
		case Real:
		case Boolean:
		case String:
			return; // no children!
		case List:
		{
			//rtt_t elem_type=type+1;
			rtlist *list=(rtlist*)ptr;
			if (!isReferenced(list->elem_type)) for (std::uint64_t i=0;i<list->used;++i)
			{
				op(list->data[i].ptr,list->elem_type);
			}
		}
		case Dictionary:
		{
			rtdict *dict=(rtdict*)ptr;
			rtt_t key_type=dict->key_type; // doesn't have to use key, use precomupted - but have toskip it for value type
			rtt_t value_type=dict->value_type;
			std::uint64_t seen=0;
			for (std::uint64_t i=0;seen<dict->usedcount;++i)
			{
				if (!dict->usedflag[i]) continue;
				if (!isReferenced(key_type)) op(dict->keys[i].ptr,key_type);
				if (!isReferenced(value_type)) op(dict->values[i].ptr,value_type);
				++seen;
			}
			break;
		}
		case Function:
		{
			rtclosure *func=(rtclosure*)ptr;
			if (func->size==0) return; // empty closure, nothing to do
			for (size_t i=0;i<func->size;++i)
			{
				op(func->fields[i].field.ptr,func->fields[i].type);
			}
			break;
		}
		case Generator:
		{
			rtgenerator *gen=(rtgenerator*)ptr;
			if (gen->retvalrtt!=Undefined)
				op(gen->retval.ptr,gen->retvalrtt);
			if (gen->size==0) return; // empty closure, nothing to do
			for (size_t i=0;i<gen->size;++i)
			{
				op(gen->fields[i].field.ptr,gen->fields[i].type);
			}
			break;
		}
		case Record:
		case Variant:
		case BuiltIn:
		case Label:
		case ParamCount:
		case Undefined:
			// Only so -wpedantic does not complain - maybe assert(false)?
		break;
	}
}

void increment(rtdata rt,rtt_t type)
{
	if (!isReferenced(type)) return; // value type
	if (rt.ptr->color==gcdata::Octarine) return;
	++rt.ptr->count;
	rt.ptr->color=gcdata::Black;
}

void decrement(rtdata rt,rtt_t type)
{
	if (!isReferenced(type)) return; // value type
	if (rt.ptr->color==gcdata::Octarine) return; // exempt tyype
	--rt.ptr->count;
	if (rt.ptr->count==0) release(rt.ptr,type);
	else if (rt.ptr->color!=gcdata::Green) possibleRoot(rt.ptr,type); // Green are acyclic, never a possible root.
}

void release(gcdata *ptr,rtt_t type)
{
	ptr->color=gcdata::Black;
	
//printf("called release...\n");

	switch(type)
	{
		case Integer:
		case Real:
		case Boolean:
			break;
		case String:
		{
			freeString((rtstring*)ptr);
			break;
		}
		case List:
		{
			freeList((rtlist*)ptr);
			break;
		}
		case Dictionary:
		{
			freeDict((rtdict*)ptr);
			break;
		}
		case Function:
		{
			break;
		}
		case Record:
			freeRecord((rtrecord*)ptr);
			break;
		case Variant:
			freeVariant((rtvariant*)ptr);
			break;
		case Generator:
			freeGenerator((rtgenerator*)ptr);
			break;
		case BuiltIn:
		case Label:
		case ParamCount:
		case Undefined:
			// Only so -wpedantic does not complain - maybe assert(false)?
		break;
	}
	return;
}

void possibleRoot(gcdata *ptr,rtt_t type)
{
//printf("Possible root\n")	;
	if (ptr->color!=gcdata::Purple)
	{
//printf("Make purple\n")	;
		ptr->color=gcdata::Purple;
//printf("Buffered?\n")	;
		if (!ptr->buffered)
		{
//printf("Add to root at %lu\n",root_count)	;
			roots[root_count].ptr=ptr;
			roots[root_count].type=type;
			ptr->buffered=true;
			++root_count;
		}
	}
}

void markGray(gcdata *ptr, rtt_t type)
{
	if (ptr->color!=gcdata::Gray)
	{
		ptr->color=gcdata::Gray;
		// Iterate over children, decrease count for each and recurse on mark gray
		iterateChildren(ptr,type,[](gcdata *cptr, rtt_t ctype){
			--cptr->count;
			markGray(cptr,ctype);
		});
	}
}

void scan(gcdata *ptr,rtt_t type)
{
	if (ptr->color==gcdata::Gray)
	{
		if (ptr->count>0)
			scanBlack(ptr,type);
		else
		{
			ptr->color=gcdata::White;
			// recurse scan over children
			iterateChildren(ptr,type,[](gcdata *cptr, rtt_t ctype){
				scan(cptr,ctype);
			});
		}
	}
}

void scanBlack(gcdata *ptr,rtt_t type)
{
	ptr->color=gcdata::Black;
	// iterate over children, increase count and scan black unless black
	iterateChildren(ptr,type,[](gcdata *cptr, rtt_t ctype){
		++cptr->count;
		if (cptr->color!=gcdata::Black)
			scanBlack(cptr,ctype);
	});
}

void collectWhite(gcdata *ptr,rtt_t type)
{
	if (ptr->color==gcdata::White && !ptr->buffered)
	{
		ptr->color=gcdata::Black;
		iterateChildren(ptr,type,[](gcdata *cptr, rtt_t ctype){
			collectWhite(cptr,ctype);
		});
		release(ptr,type);
	}
}

void collectCycles()
{
	markRoots();
	scanRoots();
	collectRoots();
}

void markRoots()
{
	root_t *root;
	size_t pos;
	size_t released=0;
	for(root=roots,pos=0;pos<root_count;++pos,++root)
	{
		if (root->ptr->color==gcdata::Purple) 
			markGray(root->ptr,root->type);
		else
		{
			root->ptr->buffered=false;
			gcdata *gc=root->ptr;
			rtt_t type=root->type;
			root->ptr=nullptr;
			root->type=intro::rtt::Undefined;
			if (gc->color==gcdata::Black && gc->count==0)
			{
				release(gc,type);
				++released;
			}
		}
	}
	compactRoots();
	root_count-=released;
}

void scanRoots()
{
	root_t *root;
	size_t pos;
	for(root=roots,pos=0;pos<root_count;++pos,++root)
		scan(root->ptr,root->type);
}

void collectRoots()
{
	root_t *root;
	size_t pos;
	// Remove all current entries from roots
	//root_t oldroots[MAX_ROOTS];
	root_t *oldroots= new root_t[MAX_ROOTS];
	size_t old_count=root_count;
	memcpy(oldroots,roots,sizeof(root_t)*MAX_ROOTS);
	memset(roots,0,sizeof(root_t)*MAX_ROOTS);
	root_count=0;
	// Only cyclic garbage in roots, kick it all out.
	for(root=oldroots,pos=0;pos<old_count;++pos,++root)
	{
		root->ptr->buffered=false;
		collectWhite(root->ptr,root->type);
	}
	delete [] oldroots;
}

#ifdef __cplusplus
}
#endif


