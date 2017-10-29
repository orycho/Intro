#include "stdafx.h"

#include "../Intro/Parser.h"
#include "../Intro/CodeGen.h"

#include <malloc.h>


#ifdef _MSC_VER

__pragma(warning(push))
__pragma(warning(disable:4355))
__pragma(warning(disable:4244))

#include "llvm/IR/Verifier.h"

__pragma(warning(pop))

#else 

#include "llvm/IR/Verifier.h"

#endif


TEST(SubtypeRelationTests, BasicConcreteTypes) {
	intro::Type boolean(intro::Type::Boolean);
	intro::Type string(intro::Type::String);
	intro::Type integer(intro::Type::Integer);
	intro::Type real(intro::Type::Real);
	intro::ErrorType error(0,0,std::wstring(L""));
	/// Subtype relation is reflexive
	EXPECT_EQ(boolean.checkSubtype(&boolean),intro::EQUAL);
	EXPECT_EQ(string.checkSubtype(&string),intro::EQUAL);
	EXPECT_EQ(integer.checkSubtype(&integer),intro::EQUAL);
	EXPECT_EQ(real.checkSubtype(&real),intro::EQUAL);
	// Real <: Integer
	//EXPECT_EQ(real.checkSubtype(&integer),intro::LESS);
	//EXPECT_EQ(integer.checkSubtype(&real),intro::GREATER);
}

TEST(SubtypeRelationTests, TopRules) {
	intro::Type boolean(intro::Type::Boolean);
	intro::Type string(intro::Type::String);
	intro::Type integer(intro::Type::Integer);
	intro::Type real(intro::Type::Real);
	intro::Type top(intro::Type::Top);
	// Top is greater than all
	EXPECT_EQ(top.checkSubtype(&boolean),intro::GREATER);
	EXPECT_EQ(top.checkSubtype(&string),intro::GREATER);
	EXPECT_EQ(top.checkSubtype(&integer),intro::GREATER);
	EXPECT_EQ(top.checkSubtype(&real),intro::GREATER);
	// Symmetry: All is less than top
	EXPECT_EQ(boolean.checkSubtype(&top),intro::LESS);
	EXPECT_EQ(string.checkSubtype(&top),intro::LESS);
	EXPECT_EQ(integer.checkSubtype(&top),intro::LESS);
	EXPECT_EQ(real.checkSubtype(&top),intro::LESS);
}

TEST(SubtypeRelationTests, UnrelatedConcreteTypes) {
	intro::Type boolean(intro::Type::Boolean);
	intro::Type string(intro::Type::String);
	intro::Type integer(intro::Type::Integer);
	intro::Type real(intro::Type::Real);

	/// All incompatible pairs of non-polymorphic basic types
	EXPECT_EQ(boolean.checkSubtype(&string),intro::ERROR);
	EXPECT_EQ(boolean.checkSubtype(&real),intro::ERROR);
	EXPECT_EQ(boolean.checkSubtype(&integer),intro::ERROR);
	EXPECT_EQ(string.checkSubtype(&boolean),intro::ERROR);
	EXPECT_EQ(string.checkSubtype(&real),intro::ERROR);
	EXPECT_EQ(string.checkSubtype(&integer),intro::ERROR);
	EXPECT_EQ(integer.checkSubtype(&boolean),intro::ERROR);
	EXPECT_EQ(integer.checkSubtype(&string),intro::ERROR);
	EXPECT_EQ(real.checkSubtype(&boolean),intro::ERROR);
	EXPECT_EQ(real.checkSubtype(&string),intro::ERROR);
}


TEST(SubtypeRelationTests, ErrorPropagation) {
	intro::Type boolean(intro::Type::Boolean);
	intro::Type string(intro::Type::String);
	intro::Type integer(intro::Type::Integer);
	intro::Type real(intro::Type::Real);
	intro::ErrorType error(0,0,std::wstring(L""));
	intro::RecordType::membermap members;
	intro::RecordType recTop(members);	// [] : Empty record, supertype of all records
	members.insert(std::make_pair(L"a",&integer));
	intro::RecordType recA(members);	// [ a:Integer; ]
	
	// Error is lower than all (hence, always takes precedence because more specific)
	EXPECT_EQ(error.checkSubtype(&boolean),intro::LESS);
	EXPECT_EQ(error.checkSubtype(&string),intro::LESS);
	EXPECT_EQ(error.checkSubtype(&integer),intro::LESS);
	EXPECT_EQ(error.checkSubtype(&real),intro::LESS);
	EXPECT_EQ(error.checkSubtype(&recTop),intro::LESS);
	EXPECT_EQ(error.checkSubtype(&recA),intro::LESS);
	// Symmetry: All is greater than error
	EXPECT_EQ(boolean.checkSubtype(&error),intro::GREATER);
	EXPECT_EQ(string.checkSubtype(&error),intro::GREATER);
	EXPECT_EQ(integer.checkSubtype(&error),intro::GREATER);
	EXPECT_EQ(real.checkSubtype(&error),intro::GREATER);
	EXPECT_EQ(recTop.checkSubtype(&error),intro::GREATER);
	EXPECT_EQ(recA.checkSubtype(&error),intro::GREATER);
}

TEST(SubtypeRelationTests, EmptyRecord) {
	intro::Type real(intro::Type::Real);
	intro::Type integer1(intro::Type::Integer);
	intro::Type integer2(intro::Type::Integer);
	intro::RecordType::membermap members;
	intro::RecordType recTop(members);	// [] : Empty record, supertype of all records
	members.insert(std::make_pair(L"a",&integer1));
	intro::RecordType recA(members);	// [ a:Integer; ]
	members.insert(std::make_pair(L"b",&integer2));
	intro::RecordType recAB(members); // [ a:Integer;  b:Integer; ]
	members.clear();
	members.insert(std::make_pair(L"a",&real));
	intro::RecordType recReal(members);	// [ a:Real; ]
	// Everything <: Top
	EXPECT_EQ(recA.checkSubtype(&recTop),intro::LESS);
	EXPECT_EQ(recAB.checkSubtype(&recTop),intro::LESS);
	EXPECT_EQ(recReal.checkSubtype(&recTop),intro::LESS);

	EXPECT_EQ(recTop.checkSubtype(&recA),intro::GREATER);
	EXPECT_EQ(recTop.checkSubtype(&recAB),intro::GREATER);
	EXPECT_EQ(recTop.checkSubtype(&recReal),intro::GREATER);
}

TEST(SubtypeRelationTests, BasicRecordSubtypes) {
	intro::Type integer1(intro::Type::Integer);
	intro::Type integer2(intro::Type::Integer);
	intro::Type real(intro::Type::Real);
	intro::Type real2(intro::Type::Real);
	intro::RecordType recTop;	// [] : Empty record, supertype of all records
	intro::RecordType recA;	// [ a:Integer; ]
	recA.addMember(L"a",&integer1);
	intro::RecordType recB;	// [ b:Integer; ]
	recB.addMember(L"b",&integer2);
	intro::RecordType recC; // [ c:Integer; ]
	recC.addMember(L"c",&integer1);
	intro::RecordType recAB; // [ a:Integer; b:Integer; ]
	recAB.addMember(L"a",&real);
	recAB.addMember(L"b",&real2);
	intro::RecordType recReal;	// [ a:Real; ]
	recReal.addMember(L"a",&real);
	intro::RecordType recInteger;	// [ a:Integer; ]
	recReal.addMember(L"a",&integer1);

	// Each record type must be equal to itself
	EXPECT_EQ(recTop.checkSubtype(&recTop),intro::EQUAL);
	EXPECT_EQ(recA.checkSubtype(&recA),intro::EQUAL);
	EXPECT_EQ(recAB.checkSubtype(&recAB),intro::EQUAL);
	EXPECT_EQ(recReal.checkSubtype(&recReal),intro::EQUAL);
	// member sybtyping
	// [ a:Real; ]<:[ a:Integer; ] because Real<:Integer
	// Note: This would coerce Real to Integer when used as output Parameter.
	// => Output parameters must be in equality relation (limited polymorphism with side effects).
	//EXPECT_EQ(recA.checkSubtype(&recReal),intro::GREATER); 
	//EXPECT_EQ(recReal.checkSubtype(&recA),intro::LESS);
	// subset subtyping
	// [ a:Integer; b:Integer]<:[ a:Integer; ] because 
	// 1) {a} subset {a,b}
	// 2) in both  records, a is Integer
	EXPECT_EQ(recAB.checkSubtype(&recA),intro::LESS);
	EXPECT_EQ(recA.checkSubtype(&recAB),intro::GREATER);
	// [ a:Integer; b:Integer]<:[ a:Real; ] because 
	// 1) {a} subset {a,b}
	// 2) [ a:Integer; ]<:[ a:Real; ]
	// Note: This would coerce Real to Integer when used as output Parameter.
	//EXPECT_EQ(recAB.checkSubtype(&recInteger),intro::LESS);
	//EXPECT_EQ(recReal.checkSubtype(&recAB),intro::GREATER);
	// No shared labels implies unrelated types
	EXPECT_EQ(recA.checkSubtype(&recB),intro::ERROR);
	EXPECT_EQ(recB.checkSubtype(&recA),intro::ERROR);
	EXPECT_EQ(recAB.checkSubtype(&recC),intro::ERROR);
	EXPECT_EQ(recC.checkSubtype(&recAB),intro::ERROR);
}

TEST(SubtypeRelationTests, NestedRecords) {
	intro::Type integer(intro::Type::Integer);
	intro::RecordType recA;	// [ a:Integer; ]
	recA.addMember(L"a",&integer);
	intro::RecordType recB;	// [ b:Integer; ]
	recB.addMember(L"b",&integer);
	intro::RecordType recC; // [ c:Integer; ]
	recC.addMember(L"c",&integer);
	intro::RecordType recAB; // [ a:Integer; b:Integer; ]
	recAB.addMember(L"a",&integer);
	recAB.addMember(L"b",&integer);
	intro::RecordType recARA; // [a:[ a:Integer; ];]
	recARA.addMember(L"a",&recA);
	intro::RecordType recARB; // [a:[ b:Integer; ];]
	recARB.addMember(L"a",&recB);
	intro::RecordType recBRA; // [b:[ a:Integer; ];]
	recBRA.addMember(L"b",&recA);
	intro::RecordType recARAB; // [a:[ a:Integer; b:Integer; ];]
	recARAB.addMember(L"a",&recAB);

	// Each record type must be equal to itself
	EXPECT_EQ(recARAB.checkSubtype(&recARB),intro::LESS);
	EXPECT_EQ(recARAB.checkSubtype(&recARA),intro::LESS);
	EXPECT_EQ(recARA.checkSubtype(&recARAB),intro::GREATER);
	EXPECT_EQ(recARB.checkSubtype(&recARAB),intro::GREATER);
	EXPECT_EQ(recARB.checkSubtype(&recARA),intro::ERROR);
	EXPECT_EQ(recARA.checkSubtype(&recARB),intro::ERROR);
	EXPECT_EQ(recBRA.checkSubtype(&recARB),intro::ERROR);
	EXPECT_EQ(recBRA.checkSubtype(&recARA),intro::ERROR);
}

TEST(SubtypeRelationTests, StringSubtyping) {
	intro::Type top(intro::Type::Top);
	intro::Type string(intro::Type::String);
	intro::Type sequence(intro::Type::Sequence);
	sequence.addParameter(&string);
	intro::Type comparable(intro::Type::Comparable);
	EXPECT_EQ(intro::EQUAL,string.checkSubtype(&string));
	EXPECT_EQ(intro::LESS,string.checkSubtype(&sequence));
	EXPECT_EQ(intro::GREATER,sequence.checkSubtype(&string));
	EXPECT_EQ(intro::LESS,string.checkSubtype(&comparable));
	EXPECT_EQ(intro::GREATER,comparable.checkSubtype(&string));
}

TEST(SubtypeRelationTests, SimpleListSubtyping) {
	intro::Type integer(intro::Type::Integer);
	intro::Type real(intro::Type::Real);
	intro::Type top(intro::Type::Top);
	intro::Type intlist(intro::Type::List,&integer);	
	intro::Type reallist(intro::Type::List,&real);
	intro::Type toplist(intro::Type::List,&top);
	intro::Type sequence(intro::Type::Sequence);
	sequence.addParameter(intro::Environment::fresh(L"?A"));
	// These are equal
	EXPECT_EQ(intlist.checkSubtype(&intlist),intro::EQUAL);
	EXPECT_EQ(reallist.checkSubtype(&reallist),intro::EQUAL);
	EXPECT_EQ(toplist.checkSubtype(&toplist),intro::EQUAL);
	EXPECT_EQ(sequence.checkSubtype(&sequence),intro::EQUAL);
	// List(Top) tops all lists
	EXPECT_EQ(toplist.checkSubtype(&reallist),intro::GREATER);
	EXPECT_EQ(toplist.checkSubtype(&intlist),intro::GREATER);
	EXPECT_EQ(intlist.checkSubtype(&toplist),intro::LESS);
	EXPECT_EQ(reallist.checkSubtype(&toplist),intro::LESS);
	EXPECT_EQ(sequence.checkSubtype(&toplist),intro::GREATER);
	EXPECT_EQ(toplist.checkSubtype(&sequence),intro::LESS);
	// Sequence tops all lists
	EXPECT_EQ(sequence.checkSubtype(&reallist),intro::GREATER);
	EXPECT_EQ(sequence.checkSubtype(&intlist),intro::GREATER);
	EXPECT_EQ(intlist.checkSubtype(&sequence),intro::LESS);
	EXPECT_EQ(reallist.checkSubtype(&sequence),intro::LESS);
	// List(Real)<:List(Integer)
	//EXPECT_EQ(intlist.checkSubtype(&reallist),intro::GREATER);
	//EXPECT_EQ(reallist.checkSubtype(&intlist),intro::LESS);
}

TEST(SubtypeRelationTests, FunctionSubtyping) {
	intro::Type integer(intro::Type::Integer);
	intro::Type real(intro::Type::Real);
	intro::Type number(intro::Type::Number);
	intro::Type top(intro::Type::Top);
	intro::FunctionType id(&top);
	id.addParameter(&top);
	intro::FunctionType intfun(&integer);
	intfun.addParameter(&integer);
	intro::FunctionType realfun(&real);
	realfun.addParameter(&real);
	// Equality checks again
	EXPECT_EQ(id.checkSubtype(&id),intro::EQUAL);
	EXPECT_EQ(intfun.checkSubtype(&intfun),intro::EQUAL);
	EXPECT_EQ(realfun.checkSubtype(&realfun),intro::EQUAL);
	
}

TEST(SubtypeRelationTests, VariantSubtyping) {
	intro::Type integer(intro::Type::Integer);
	intro::Type real(intro::Type::Real);
	intro::Type number(intro::Type::Number);
	intro::Type top(intro::Type::Top);
	intro::TypeVariable *var=intro::Environment::fresh(L"?value");
	
	intro::RecordType recEmpty;	// [ ]
	intro::RecordType recA;	// [ a<:Top; ]
	recA.addMember(L"value",var);

	intro::VariantType varNone,varSome,varBoth;
	varNone.addTag(L"None",&recEmpty);
	varSome.addTag(L"Some",&recA);
	varBoth.addTag(L"None",&recEmpty);
	varBoth.addTag(L"Some",&recA);

	// Equality checks again
	EXPECT_EQ(varNone.checkSubtype(&varNone),intro::EQUAL);
	EXPECT_EQ(varSome.checkSubtype(&varSome),intro::EQUAL);
	EXPECT_EQ(varBoth.checkSubtype(&varBoth),intro::EQUAL);
	
	EXPECT_EQ(varNone.checkSubtype(&varSome),intro::ERROR);
	EXPECT_EQ(varSome.checkSubtype(&varNone),intro::ERROR);

	EXPECT_EQ(varNone.checkSubtype(&varBoth),intro::LESS);
	EXPECT_EQ(varSome.checkSubtype(&varBoth),intro::LESS);

	EXPECT_EQ(varBoth.checkSubtype(&varNone),intro::GREATER);
	EXPECT_EQ(varBoth.checkSubtype(&varSome),intro::GREATER);

}

TEST(SubtypeRelationTests, VariantFieldSubtyping) {
	intro::Type integer(intro::Type::Integer);
	intro::Type real(intro::Type::Real);
	intro::Type number(intro::Type::Number);
	intro::Type top(intro::Type::Top);
	intro::TypeVariable *var=intro::Environment::fresh(L"?value");
	
	intro::RecordType recVec2N;	
	recVec2N.addMember(L"x",&number);
	recVec2N.addMember(L"y",&number);

	intro::RecordType recVec2I;	
	recVec2I.addMember(L"x",&integer);
	recVec2I.addMember(L"y",&integer);

	intro::RecordType recVec3N;	
	recVec3N.addMember(L"x",&number);
	recVec3N.addMember(L"y",&number);
	recVec3N.addMember(L"z",&number);

	intro::RecordType recVec3I;	
	recVec3I.addMember(L"x",&integer);
	recVec3I.addMember(L"y",&integer);
	recVec3I.addMember(L"z",&integer);

	
	intro::VariantType varVec2,varVec3,varBoth;
	varVec2.addTag(L"Vector2",&recVec2I);
	varVec3.addTag(L"Vector3",&recVec3I);
	varBoth.addTag(L"Vector2",&recVec2N);
	varBoth.addTag(L"Vector3",&recVec3N);

	// Equality checks again
	EXPECT_EQ(varVec2.checkSubtype(&varVec2),intro::EQUAL);
	EXPECT_EQ(varVec3.checkSubtype(&varVec3),intro::EQUAL);
	EXPECT_EQ(varBoth.checkSubtype(&varBoth),intro::EQUAL);
	
	EXPECT_EQ(varVec2.checkSubtype(&varVec3),intro::ERROR);
	EXPECT_EQ(varVec3.checkSubtype(&varVec2),intro::ERROR);

	EXPECT_EQ(varVec2.checkSubtype(&varBoth),intro::LESS);
	EXPECT_EQ(varVec3.checkSubtype(&varBoth),intro::LESS);

	EXPECT_EQ(varBoth.checkSubtype(&varVec2),intro::GREATER);
	EXPECT_EQ(varBoth.checkSubtype(&varVec3),intro::GREATER);

}


TEST(TypeInferenceTests, LiteralTypes) {
	//intro::Environment::resetFreshCounter();
	const char *test="var x <- 12; var y <- \"Hallo\"; var z<-3.1415; var b<-true; var id(x) -> return x; end;";
	parse::Parser::iterator iter;
	parse::Scanner scanner((const unsigned char *)test,strlen(test));
	parse::Parser parser(&scanner);
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	ASSERT_TRUE(isOK);

	intro::ValueStatement *stmt;
	intro::Type *type;
	iter=parser.parseResult.begin();
	
	ASSERT_NE(iter,parser.parseResult.end());
	stmt=dynamic_cast<intro::ValueStatement*>(*iter);
	ASSERT_NE(stmt,(void*)NULL);
	type=stmt->getValue()->getType();
	ASSERT_NE(type,(void*)NULL);
	ASSERT_EQ(type->getKind(),intro::Type::Integer);

	iter++;
	ASSERT_NE(iter,parser.parseResult.end());
	stmt=dynamic_cast<intro::ValueStatement*>(*iter);
	ASSERT_NE(stmt,(void*)NULL);
	type=stmt->getValue()->getType();
	ASSERT_NE(type,(void*)NULL);
	ASSERT_EQ(type->getKind(),intro::Type::String);

	iter++;
	ASSERT_NE(iter,parser.parseResult.end());
	stmt=dynamic_cast<intro::ValueStatement*>(*iter);
	ASSERT_NE(stmt,(void*)NULL);
	type=stmt->getValue()->getType();
	ASSERT_NE(type,(void*)NULL);
	ASSERT_EQ(type->getKind(),intro::Type::Real);


	iter++;
	ASSERT_NE(iter,parser.parseResult.end());
	stmt=dynamic_cast<intro::ValueStatement*>(*iter);
	ASSERT_NE(stmt,(void*)NULL);
	type=stmt->getValue()->getType();
	ASSERT_NE(type,(void*)NULL);
	ASSERT_EQ(type->getKind(),intro::Type::Boolean);

	iter++;
	ASSERT_NE(iter,parser.parseResult.end());
	stmt=dynamic_cast<intro::ValueStatement*>(*iter);
	ASSERT_NE(stmt,(void*)NULL);
	type=stmt->getValue()->getType();
	ASSERT_NE(type,(void*)NULL);
	ASSERT_EQ(type->getKind(),intro::Type::Function);
	intro::FunctionType *funtype=dynamic_cast<intro::FunctionType*>(type);
	ASSERT_EQ(funtype->parameterCount(),1);
	ASSERT_EQ(funtype->getFirstParameter(),funtype->getReturnType());
	
	parser.deleteStatements();
	
}

TEST(TypeInferenceTests, BasicPolymorphism) {
	//intro::Environment::resetFreshCounter();
	const char *test="var x<-2; var id(x)-> return x; end; id(true); id(x); id(\"Boo\"); id(y); id;";
	parse::Parser::iterator iter;
	parse::Scanner scanner((const unsigned char *)test,strlen(test));
	parse::Parser parser(&scanner);
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	ASSERT_TRUE(isOK);

	// Integer, (?a,?a)->?a, Boolean, Integer,String, Error, (?a,?a)->?a
	iter=parser.parseResult.begin();
	ASSERT_NE(iter,parser.parseResult.end());
	intro::ValueStatement *stmt=dynamic_cast<intro::ValueStatement*>(*iter);
	ASSERT_NE(stmt,(void*)NULL);
	intro::Type *type=stmt->getValue()->getType();
	ASSERT_NE(type,(void*)NULL);
	ASSERT_EQ(type->getKind(),intro::Type::Integer);

	iter++;
	ASSERT_NE(iter,parser.parseResult.end());
	stmt=dynamic_cast<intro::ValueStatement*>(*iter);
	ASSERT_NE(stmt,(void*)NULL);
	type=stmt->getValue()->getType();
	ASSERT_NE(type,(void*)NULL);
	ASSERT_EQ(type->getKind(),intro::Type::Function);
	intro::FunctionType *funtype=dynamic_cast<intro::FunctionType*>(type);
	ASSERT_EQ(funtype->parameterCount(),1);
	ASSERT_EQ(funtype->getFirstParameter(),funtype->getReturnType());

	iter++;
	ASSERT_NE(iter,parser.parseResult.end());
	intro::ExpressionStatement *expr=dynamic_cast<intro::ExpressionStatement*>(*iter);
	ASSERT_NE(expr,(void*)NULL);
	type=expr->getExpression()->getType();
	ASSERT_NE(type,(void*)NULL);
	ASSERT_EQ(type->getKind(),intro::Type::Boolean);

	iter++;
	ASSERT_NE(iter,parser.parseResult.end());
	expr=dynamic_cast<intro::ExpressionStatement*>(*iter);
	ASSERT_NE(expr,(void*)NULL);
	type=expr->getExpression()->getType();
	ASSERT_NE(type,(void*)NULL);
	ASSERT_EQ(type->getKind(),intro::Type::Integer);

	iter++;
	ASSERT_NE(iter,parser.parseResult.end());
	expr=dynamic_cast<intro::ExpressionStatement*>(*iter);
	ASSERT_NE(expr,(void*)NULL);
	type=expr->getExpression()->getType();
	ASSERT_NE(type,(void*)NULL);
	ASSERT_EQ(type->getKind(),intro::Type::String);

	iter++;
	ASSERT_NE(iter,parser.parseResult.end());
	expr=dynamic_cast<intro::ExpressionStatement*>(*iter);
	ASSERT_NE(expr,(void*)NULL);
	type=expr->getExpression()->getType();
	ASSERT_NE(type,(void*)NULL);
	ASSERT_EQ(type->getKind(),intro::Type::Error);

	iter++;
	ASSERT_NE(iter,parser.parseResult.end());
	expr=dynamic_cast<intro::ExpressionStatement*>(*iter);
	ASSERT_NE(expr,(void*)NULL);
	type=expr->getExpression()->getType();
	funtype=dynamic_cast<intro::FunctionType*>(type);
	ASSERT_NE(funtype,(void*)NULL);
	ASSERT_EQ(funtype->parameterCount(),1);
	ASSERT_EQ(funtype->getFirstParameter(),funtype->getReturnType());
	
	parser.deleteStatements();
	
}

TEST(TypeInferenceTests, SimpleSubtypePolymorphism) {
	const char *test="var f(a,b) -> return a*a+b*b; end; f(1,2); f(1.0,2); f(1,2.0); f(1.0,2.0); f(\"a\",true); ";
	parse::Parser::iterator iter;
	parse::Scanner scanner((const unsigned char *)test,strlen(test));
	parse::Parser parser(&scanner);
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	ASSERT_TRUE(isOK);

	// (?a<Number,?a<Number)->?a<Number; Integer; Real; Real; Real; Error

	iter=parser.parseResult.begin();
	ASSERT_NE(iter,parser.parseResult.end());
	intro::ValueStatement *stmt=dynamic_cast<intro::ValueStatement*>(*iter);
	ASSERT_NE(stmt,(void*)NULL);
	intro::Type *type=stmt->getValue()->getType();
	ASSERT_NE(type,(void*)NULL);
	ASSERT_EQ(type->getKind(),intro::Type::Function);
	intro::FunctionType *funtype=dynamic_cast<intro::FunctionType*>(type);
	ASSERT_EQ(funtype->parameterCount(),2);
	ASSERT_EQ(funtype->getFirstParameter(),funtype->getReturnType());
	
	parser.deleteStatements();
	
}
