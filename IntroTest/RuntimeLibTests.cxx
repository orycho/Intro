#include "stdafx.h"

#include "../Intro/Runtime.h"

#include <malloc.h>
#include <string.h>
#include <unordered_map>

static const int SOME_TAG=1; // See VariantExpression 

TEST(PerfHashTests, NoCollisionsSmall) 
{
	std::set<std::wstring> keys;
	std::set<std::wstring>::iterator i;
	keys.insert(L"eins");
	keys.insert(L"one");
	keys.insert(L"apple");
	keys.insert(L"ei");
	keys.insert(L"tuna");
	keys.insert(L"iter");
	keys.insert(L"first");
	keys.insert(L"second");
	std::int32_t *offsets=new std::int32_t[keys.size()];
	util::mkPerfectHash(keys,offsets);
	//util::PerfectHash ph(keys);
	std::vector<bool> occupied(keys.size(),false);
	
	for (i=keys.begin();i!=keys.end();i++)
	{
		std::int32_t slot=util::find(i->c_str(),i->size(),offsets,keys.size());
		EXPECT_FALSE(occupied[slot]);
		occupied[slot]=true;
	}
	
	delete [] offsets;
	//exit(0);
}

TEST(PerfHashTests, NoCollisionsLessSmall) 
{
	std::set<std::wstring> keys;
	std::set<std::wstring>::iterator i;
	keys.insert(L"eins");
	keys.insert(L"one");
	keys.insert(L"apple");
	keys.insert(L"ei");
	keys.insert(L"tuna");
	keys.insert(L"iter");
	keys.insert(L"quagmire");
	keys.insert(L"wlan");
	keys.insert(L"3gpo");
	keys.insert(L"night-lite");
	keys.insert(L"quadruple");
	keys.insert(L"guten tag");
	keys.insert(L"l33tsp3@kcr@p");
	std::int32_t *offsets=new std::int32_t[keys.size()];
	util::mkPerfectHash(keys,offsets);
	//util::PerfectHash ph(keys);
	std::vector<bool> occupied(keys.size(),false);
	
	for (i=keys.begin();i!=keys.end();i++)
	{
		std::int32_t slot=util::find(i->c_str(),i->size(),offsets,keys.size());
		EXPECT_FALSE(occupied[slot]);
		occupied[slot]=true;
	}
	
	delete [] offsets;
}

TEST(SimpleRuntimeTests, IntegerAndReal) 
{

	rtdata int_a,int_b;
	rtt_t int_rtt=intro::rtt::Integer;
	int_a.integer=1;
	int_b.integer=2;

	rtdata real_a,real_b,real_c;
	real_a.real=2.26;
	real_b.real=3.84;
	real_c.real=2.0;
	rtt_t real_rtt=intro::rtt::Real;
	
	/// Subtype relation is reflexive
	EXPECT_FALSE(equalPoly(int_a,int_rtt,int_b,int_rtt));
	EXPECT_TRUE(equalPoly(int_a,int_rtt,int_a,int_rtt));
	EXPECT_FALSE(equalPoly(real_a,real_rtt,real_b,real_rtt));
	EXPECT_TRUE(equalPoly(real_a,real_rtt,real_a,real_rtt));
	
	EXPECT_FALSE(equalPoly(int_b,int_rtt,real_b,real_rtt));
	EXPECT_TRUE(equalPoly(int_b,int_rtt,real_c,real_rtt));
	EXPECT_TRUE(equalPoly(real_c,real_rtt,int_b,int_rtt));
	
	//EXPECT_EQ(integer.checkSubtype(&real),intro::GREATER);
}

TEST(RTStringTests, CreationTestEmpty) 
{

	rtdata str_a;
	rtt_t str_rtt=intro::rtt::String;

	str_a.ptr=(gcdata*)allocString(0);
	
	EXPECT_EQ(sizeString((rtstring*)str_a.ptr),0);
	
	decrement(str_a,str_rtt);
}

TEST(RTStringTests, AppendStrings) 
{

	rtdata str_a;
	rtt_t str_rtt=intro::rtt::String;
	const wchar_t *cs1=L"Hello";
	const wchar_t *cs2=L" World";

	str_a.ptr=(gcdata*)allocString(0);
	
	EXPECT_EQ(sizeString((rtstring*)str_a.ptr),0);
	
	// Append a constant and check
	appendString((rtstring*)str_a.ptr,cs1,wcslen(cs1));
	EXPECT_EQ(sizeString((rtstring*)str_a.ptr),wcslen(cs1));
	EXPECT_EQ(wcscmp(((rtstring*)str_a.ptr)->data,cs1),0);
	// repeat
	appendString((rtstring*)str_a.ptr,cs2,wcslen(cs2));
	EXPECT_EQ(sizeString((rtstring*)str_a.ptr),wcslen(cs1)+wcslen(cs2));
	EXPECT_EQ(wcscmp(((rtstring*)str_a.ptr)->data,L"Hello World"),0);
	
	decrement(str_a,str_rtt);
}

TEST(RTStringTests, toStringPoly) 
{

	rtdata str_a,buf;
	rtt_t buf_rtt=intro::rtt::String;
	const wchar_t *cs1=L"Hello";

	str_a.ptr=(gcdata*)allocString(0);
	buf.ptr=(gcdata*)allocString(0);
	
	appendString((rtstring*)buf.ptr,cs1,wcslen(cs1));
	EXPECT_EQ(sizeString((rtstring*)buf.ptr),wcslen(cs1));
	EXPECT_EQ(wcscmp(((rtstring*)buf.ptr)->data,cs1),0);
	
	toStringPoly((rtstring*)str_a.ptr,buf,buf_rtt);
	EXPECT_EQ(wcscmp(((rtstring*)str_a.ptr)->data,cs1),0);
	decrement(buf,buf_rtt);
	decrement(str_a,buf_rtt);

	str_a.ptr=(gcdata*)allocString(0);
	buf.integer=123;
	buf_rtt=intro::rtt::Integer;
	toStringPoly((rtstring*)str_a.ptr,buf,buf_rtt);
	EXPECT_EQ(wcscmp(((rtstring*)str_a.ptr)->data,L"123"),0);
	decrement(str_a,intro::rtt::String);
}

TEST(RTStringTests, StringGenerator)
{
	rtdata str;
	
	str.ptr=(gcdata*)allocString(0);

	const wchar_t *cs=L"Hello World";
	appendString((rtstring*)str.ptr,cs,wcslen(cs));
	
	rtdata gen;
	gen.ptr = (gcdata*)allocStringGenerator((rtstring*)str.ptr);
	
	for (size_t i=0;i<wcslen(cs);++i)
	{
		EXPECT_TRUE(callGenerator((rtgenerator*)gen.ptr));
		EXPECT_EQ(getResultTypeGenerator((rtgenerator*)gen.ptr),intro::rtt::String);
		rtstring *str2=(rtstring*)getResultGenerator((rtgenerator*)gen.ptr).ptr;
		EXPECT_EQ(str2->used,1);
		EXPECT_EQ(str2->data[0],cs[i]);
	}
	EXPECT_FALSE(callGenerator((rtgenerator*)gen.ptr));
	EXPECT_EQ(getResultTypeGenerator((rtgenerator*)gen.ptr),intro::rtt::Undefined);
		
	decrement(gen,intro::rtt::Generator);
	decrement(str,intro::rtt::String);
}

TEST(RTListTests, CreationAndEmpty) 
{
	rtdata list;
	list.ptr=(gcdata*)allocList(0,intro::rtt::Integer);
	EXPECT_EQ(sizeList((rtlist*)list.ptr),0);
	decrement(list,intro::rtt::List);
}

TEST(RTListTests, AppendIntegers)
{
	rtdata list;
	rtdata int_a,int_b;
	
	list.ptr=(gcdata*)allocList(0,intro::rtt::Integer);
	int_a.integer=1;
	int_b.integer=3;

	EXPECT_EQ(sizeList((rtlist*)list.ptr),0);
	
	appendList((rtlist*)list.ptr,int_a);
	EXPECT_EQ(sizeList((rtlist*)list.ptr),1);
	EXPECT_EQ(itemList((rtlist*)list.ptr,0).integer,int_a.integer);
	
	appendList((rtlist*)list.ptr,int_b);
	EXPECT_EQ(sizeList((rtlist*)list.ptr),2);
	EXPECT_EQ(itemList((rtlist*)list.ptr,0).integer,int_a.integer);
	EXPECT_EQ(itemList((rtlist*)list.ptr,1).integer,int_b.integer);
	
	decrement(list,intro::rtt::List);
}

TEST(RTListTests, AppendStrings) 
{
	rtdata list;
	rtdata str_a,str_b;
	const wchar_t *cs1=L"Hello";
	const wchar_t *cs2=L" World";
	
	list.ptr=(gcdata*)allocList(0,intro::rtt::String);

	str_a.ptr=(gcdata*)allocString(0);
	appendString((rtstring*)str_a.ptr,cs1,wcslen(cs1));
	str_b.ptr=(gcdata*)allocString(0);
	appendString((rtstring*)str_b.ptr,cs2,wcslen(cs2));
	
	EXPECT_EQ(sizeList((rtlist*)list.ptr),0);
	
	appendList((rtlist*)list.ptr,str_a);
	decrement(str_a,intro::rtt::String);
	EXPECT_EQ(sizeList((rtlist*)list.ptr),1);
	EXPECT_EQ(itemList((rtlist*)list.ptr,0).ptr,str_a.ptr);
	
	appendList((rtlist*)list.ptr,str_b);
	decrement(str_b,intro::rtt::String);
	EXPECT_EQ(sizeList((rtlist*)list.ptr),2);
	EXPECT_EQ(itemList((rtlist*)list.ptr,0).ptr,str_a.ptr);
	EXPECT_EQ(itemList((rtlist*)list.ptr,1).ptr,str_b.ptr);
	
	decrement(list,intro::rtt::List);
}

TEST(RTListTests, ListGenerator)
{
	std::int64_t i;
	rtdata list;
	rtdata int_a;
	int_a.integer=1;

	list.ptr=(gcdata*)allocList(0,intro::rtt::Integer);
	for (i=0;i<16;++i)
	{
		int_a.integer=i;
		appendList((rtlist*)list.ptr,int_a);
		EXPECT_EQ(sizeList((rtlist*)list.ptr),i+1);
	}

	rtdata gen;
	gen.ptr = (gcdata*)allocListGenerator((rtlist*)list.ptr);
	for (i=0;i<16;++i)
	{
		int_a.integer=i;
		EXPECT_EQ(itemList((rtlist*)list.ptr,i).integer,int_a.integer);
		EXPECT_TRUE(callGenerator((rtgenerator*)gen.ptr));
		EXPECT_EQ(getResultTypeGenerator((rtgenerator*)gen.ptr),intro::rtt::Integer);
		EXPECT_EQ(getResultGenerator((rtgenerator*)gen.ptr).integer,int_a.integer);
	}
	EXPECT_FALSE(callGenerator((rtgenerator*)gen.ptr));
	EXPECT_EQ(getResultTypeGenerator((rtgenerator*)gen.ptr),intro::rtt::Undefined);

	decrement(gen,intro::rtt::Generator);
	decrement(list,intro::rtt::List);
}

TEST(RTDictionaryTests, CreationTestEmpty)
{

	rtdata dict;
	dict.ptr=(gcdata*)allocDict(intro::rtt::Integer,intro::rtt::Integer);

	EXPECT_EQ(sizeDict((rtdict*)dict.ptr),0);

	decrement(dict,intro::rtt::Dictionary);
	
}

TEST(RTDictionaryTests, InsertLookup) 
{
	rtdata dict;
	dict.ptr=(gcdata*)allocDict(intro::rtt::Integer,intro::rtt::Integer);
	rtdata variant;

	rtdata int_a,int_b;
	//rtt_t int_rtt=intro::rtt::Integer;

	size_t i;
	for (i=0;i<8;++i)
	{
		int_a.integer=i;
		int_b.integer=i*i;
		insertDict((rtdict*)dict.ptr,int_a,int_b);
		EXPECT_EQ(sizeDict((rtdict*)dict.ptr),i+1);
		variant.ptr=(gcdata*)findDict((rtdict*)dict.ptr,int_a).ptr;
		EXPECT_EQ(getTagVariant((rtvariant*)variant.ptr),SOME_TAG);
		std::int32_t slot=getSlotVariant((rtvariant*)variant.ptr,L"value",5);
		rtdata *field=getFieldVariant((rtvariant*)variant.ptr,slot);
		EXPECT_EQ((i*i),(*field).integer);
		decrement(variant,intro::rtt::Variant);
	}

	for (i=8;i<16;++i)
	{
		int_a.integer=i;
		int_b.integer=i*i;
		insertDict((rtdict*)dict.ptr,int_a,int_b);
		EXPECT_EQ(i+1,sizeDict((rtdict*)dict.ptr));
	}
	
	for (i=0;i<16;++i)
	{
		int_a.integer=i;
		variant.ptr=(gcdata*)findDict((rtdict*)dict.ptr,int_a).ptr;
		EXPECT_EQ(getTagVariant((rtvariant*)variant.ptr),SOME_TAG);
		std::int32_t slot=getSlotVariant((rtvariant*)variant.ptr,L"value",5);
		rtdata *field=getFieldVariant((rtvariant*)variant.ptr,slot);
		EXPECT_EQ((i*i),(*field).integer);
		decrement(variant,intro::rtt::Variant);
	}

	decrement(dict,intro::rtt::Dictionary);
}

TEST(RTDictionaryTests, InsertErase) 
{
	rtdata variant;
	rtdata dict;
	dict.ptr=(gcdata*)allocDict(intro::rtt::Integer,intro::rtt::Integer);

	rtdata int_a,int_b;
	//rtt_t int_rtt=intro::rtt::Integer;

	size_t i;
	for (i=0;i<16;++i)
	{
		int_a.integer=i;
		int_b.integer=i*i;
		insertDict((rtdict*)dict.ptr,int_a,int_b);
		EXPECT_EQ(i+1,sizeDict((rtdict*)dict.ptr));

		variant.ptr=(gcdata*)findDict((rtdict*)dict.ptr,int_a).ptr;
		EXPECT_EQ(getTagVariant((rtvariant*)variant.ptr),SOME_TAG);
		std::int32_t slot=getSlotVariant((rtvariant*)variant.ptr,L"value",5);
		rtdata *field=getFieldVariant((rtvariant*)variant.ptr,slot);
		EXPECT_EQ((i*i),(*field).integer);
		decrement(variant,intro::rtt::Variant);
	}

	size_t expected_size=sizeDict((rtdict*)dict.ptr);
	for (i=0;i<8;++i)
	{
		int_a.integer=i;
		eraseDict((rtdict*)dict.ptr,int_a);
		--expected_size;
		EXPECT_EQ(expected_size,sizeDict((rtdict*)dict.ptr));
	}
	
	for (i=8;i<16;++i)
	{
		int_a.integer=i;
		variant.ptr=(gcdata*)findDict((rtdict*)dict.ptr,int_a).ptr;
		EXPECT_EQ(getTagVariant((rtvariant*)variant.ptr),SOME_TAG);
		std::int32_t slot=getSlotVariant((rtvariant*)variant.ptr,L"value",5);
		rtdata *field=getFieldVariant((rtvariant*)variant.ptr,slot);
		EXPECT_EQ((i*i),(*field).integer);
		decrement(variant,intro::rtt::Variant);
	}

	decrement(dict,intro::rtt::Dictionary);
}

TEST(RTDictionaryTests, DictGenerator) 
{
	rtdata dict;
	dict.ptr=(gcdata*)allocDict(intro::rtt::String,intro::rtt::Integer);

	rtdata str_a,int_b;
	//rtt_t int_rtt=intro::rtt::Integer;
	const wchar_t *teststr[] =
	{
		L"hash",
		L"map",
		L"foo",
		L"bar",
		L"testemenow",
		L"93-418",
		L"truck-stop",
		L"mirror",
		L"@$$h01e",
		L";-)",
		L"To be or not",
		L"It cannot be!",
		L"Hail to the king",
		L"the end!",
		nullptr
	};
	std::unordered_map<std::wstring,std::int64_t> check;
	std::int64_t i,count=0;
	for (i=0;teststr[i]!=nullptr;++i)
	{
		str_a.ptr=(gcdata*)allocString(0);
		appendString((rtstring*)str_a.ptr,teststr[i],wcslen(teststr[i]));
		int_b.integer=i;
		insertDict((rtdict*)dict.ptr,str_a,int_b);
		check.insert(std::make_pair(teststr[i],i));
		count++;
	}

	EXPECT_EQ(count,14);
	rtdata gen;
	gen.ptr = (gcdata*)allocDictGenerator((rtdict*)dict.ptr);
	
	for (i=0;i<count;++i)
	{
		ASSERT_TRUE(callGenerator((rtgenerator*)gen.ptr));
		EXPECT_EQ(getResultTypeGenerator((rtgenerator*)gen.ptr),intro::rtt::Record);
		rtdata record=getResultGenerator((rtgenerator*)gen.ptr);
		std::int32_t key_slot=getSlotRecord((rtrecord*)record.ptr,L"key",3);
		std::int32_t value_slot=1-key_slot;
		EXPECT_EQ(*getFieldRTTRecord((rtrecord*)record.ptr,key_slot),intro::rtt::String);
		EXPECT_EQ(*getFieldRTTRecord((rtrecord*)record.ptr,value_slot),intro::rtt::Integer);
		rtstring *str=(rtstring*)(getFieldRecord((rtrecord*)record.ptr,key_slot)->ptr);
		std::unordered_map<std::wstring,std::int64_t>::iterator iter=check.find(std::wstring(str->data,str->used));
		ASSERT_NE(iter,check.end());
		EXPECT_EQ(getFieldRecord((rtrecord*)record.ptr,value_slot)->integer,iter->second);
		check.erase(iter);
	}
	
	EXPECT_TRUE(check.empty());
	EXPECT_FALSE(callGenerator((rtgenerator*)gen.ptr));
	EXPECT_EQ(getResultTypeGenerator((rtgenerator*)gen.ptr),intro::rtt::Undefined);
	
	decrement(gen,intro::rtt::Generator);
	decrement(dict,intro::rtt::Dictionary);
}

class RTRecordTests : public ::testing::Test
{
public:
	
	RTRecordTests()
	{
		
	}
	
	virtual void SetUp() 
	{
	}

	virtual void TearDown() 
	{
	}
	
	struct field_def
	{
		std::wstring label;
		rtt_t type;
		rtdata value;
	};
	
	// this is what we do every test
	void buildRecordParts(field_def *def,size_t size,innerrecord &result)
	{
		size_t i;
		std::set<std::wstring> keys;
		// Initalize 
		result.size=size;
		if (size==0)
		{
			result.offsets=nullptr;
			result.fieldrtt=nullptr;
			result.fields=nullptr;
			return;
		}
		result.offsets=(std::int32_t*)malloc(sizeof(std::int32_t)*size);
		result.fieldrtt=(rtt_t*)malloc(sizeof(rtt_t)*size);
		result.fields=(rtdata*)malloc(sizeof(rtdata)*size);
		for (i=0;i<size;++i)
		{
			keys.insert(def[i].label);
		}
		// create offsets
		util::mkPerfectHash(keys,result.offsets);
		// populate values and their types: done
		for (i=0;i<size;++i)
		{
			std::int32_t slot=util::find(def[i].label.c_str(),def[i].label.size(),result.offsets,result.size);
			result.fields[slot]=def[i].value;
			result.fieldrtt[slot]=def[i].type;
		}
	};
	
	// Real runtime uses global constants allocated via LLVM code
	void freeInner(innerrecord &result)
	{
		delete [] result.offsets;
		delete [] result.fieldrtt;
		delete [] result.fields;
	}
};

TEST_F(RTRecordTests, TwoLabels) 
{
	// An integer for the record
	rtdata int_a;
	//rtt_t int_rtt=intro::rtt::Integer;
	int_a.integer=1;
	// A string for the record
	rtdata str_a;
	//rtt_t str_rtt=intro::rtt::String;
	const wchar_t *cs=L"Hello World";
	str_a.ptr=(gcdata*)allocString(wcslen(cs));
	appendString((rtstring*)str_a.ptr,cs,wcslen(cs));
	
	// cheat...
	rtrecord *record=(rtrecord*)malloc(sizeof(rtrecord));
	initGC(&(record->gc));
	field_def def[2]=
	{
		{ L"first",intro::rtt::Integer,int_a },
		{ L"second",intro::rtt::String,str_a }
	};
	buildRecordParts(def,2,record->r);
	for (size_t i=0;i<2;++i)
	{
		//std::int32_t slot=util::find(def[i].label.c_str(),def[i].label.size(),result.offsets,result.size);
		std::uint32_t slot=getSlotRecord(record,def[i].label.c_str(),def[i].label.size());
		rtt_t rtt=*getFieldRTTRecord(record,slot);
		EXPECT_EQ(def[i].type,rtt);
		EXPECT_TRUE(equalPoly(def[i].value,rtt,*getFieldRecord(record,slot),rtt));
		
	}
	freeInner(record->r);
	//	decrement(str_a,intro::rtt::String);
}
/*
TEST(RTGeneratorTests, BuiltinRange) {



	rtgenerator *gen=allocRangeGenerator(1,20,2);
	
	for (std::int64_t i=1;i<=20;i+=2)
	{
		EXPECT_TRUE(gen->code(gen));
		EXPECT_EQ(getResultGenerator(gen).integer,i);
	}
	EXPECT_FALSE(gen->code(gen));
	EXPECT_FALSE(gen->code(gen));
	EXPECT_FALSE(gen->code(gen));
}
*/

/*
TEST(RTStringTests, SubStrings) {

	rtdata str_a,str_b;
	rtt_t str_rtt=intro::rtt::String;

	str_a.ptr=(gcdata*)allocString(0);
	const wchar_t *cs1=L"Hello";
	const wchar_t *cs2=L" World";
	
	EXPECT_EQ(sizeString((rtstring*)str_a.ptr),0);
	
	// Append a constant and check
	appendString((rtstring*)str_a.ptr,cs1,wcslen(cs1));
	EXPECT_EQ(sizeString((rtstring*)str_a.ptr),wcslen(cs1));
	EXPECT_EQ(wcscmp(((rtstring*)str_a.ptr)->data,cs1),0);
	// repeat
	appendString((rtstring*)str_a.ptr,cs2,wcslen(cs2));
	EXPECT_EQ(sizeString((rtstring*)str_a.ptr),wcslen(cs1)+wcslen(cs2));
	EXPECT_EQ(wcscmp(((rtstring*)str_a.ptr)->data,L"Hello World"),0);
	
	decrement(str_a,str_rtt);
}

TEST(RTListTests, CreationAndEmpty) {

	rtdata list;
	rtt_t list_rtt=intro::rtt::List;
	rtdata inta,intb;
	rtt_t int_rtt=intro::rtt::Integer;


	
}

TEST(RTListTests, AppendElements) {

	rtdata list;
	rtt_t list_rtt=intro::rtt::List;
	rtdata inta,intb;
	rtt_t int_rtt=intro::rtt::Integer;


	
}

TEST(RTListTests, GetSplice) {

	rtdata list;
	rtt_t list_rtt=intro::rtt::List;
	rtdata inta,intb;
	rtt_t int_rtt=intro::rtt::Integer;


	
}


TEST(RTDictionaryTests, CreationTestEmpty) {

	rtdata dict;
	rtt_t list_rtt=intro::rtt::Dictionary;

	rtdata inta,intb;
	rtt_t int_rtt=intro::rtt::Integer;

	rtdata str_a;
	rtt_t str_rtt=intro::rtt::String;
	
}

TEST(RTDictionaryTests, IntegerAndReal) {

	rtdata dict;
	rtt_t list_rtt=intro::rtt::Dictionary;

	rtdata inta,intb;
	rtt_t int_rtt=intro::rtt::Integer;

	rtdata str_a;
	rtt_t str_rtt=intro::rtt::String;
	
}
*/