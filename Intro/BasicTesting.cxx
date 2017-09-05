#include "stdafx.h"
#include "Parser.h"

//#include <vld.h>

#include <stdio.h>
//#include "mpir.h"
#include <iostream>

using namespace std;

////////////////////////////////////////////////
/// Helpers
namespace intro
{
	llvm::Function * generateCode(const std::list<intro::Statement*> &statements);
}

void deleteStatements(parse::Parser &parser);

void dumpGeneratedStatements(parse::Parser &parser)
{
	intro::generateCode(parser.parseResult); 
}

void printSubTypes(intro::Type *type,const std::vector<intro::Type*> &v)
{
	std::wcout << L"\n\nSource Type ";
	type->print(std::wcout);
	std::wcout << L", Subtypes: \n";
	std::vector<intro::Type*>::const_iterator iter=v.begin();
	if (iter!=v.end())
	{
		(*iter)->print(std::wcout);
		iter++;
	}
	for (;iter!=v.end();++iter)
	{
		std::wcout << L", ";
		(*iter)->print(std::wcout);
	}
	std::wcout << L".\n";
}

////////////////////////////////////////////////
/// Tests

bool inferTypes(parse::Parser &parser,intro::Environment *env);

/*
parse::Parser &&getParser(const char *test)
{
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test,strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive=false;
	parser.Parse();
	return std::move(parser);
}*/

/* Check simplest type inference case: types for literals
	the command
	var x <- 12;
	creates a variable x that is assigned th value 12.
	There are two forms of function definition demonstrated
	var id(x) -> return x; end;
	is syntactic sugar for the anonymous function assigned to a variable
	var id<-fun(x)->return x; end;
*/
bool literalTyping(void)
{
	cout << "\n---\nLiteral Typing:\n";
	const char *test="var x <- 12; var y <- \"Hallo\"; var s <- \"${y} ${x}\"; var b<-true; var id(x) -> return x; end; var id2<-fun(x)->return x; end; var mybool(a,b) -> return (not a and b) or (a and not b); end; -id(2);";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
	
}

/* Typing for simple arithmetic and logic operations
*/
bool expressionTyping(void)
{
	cout << "\n---\nBasic Expressions:\n";
	const char *test="var add <- 12+3+5; var mul <- 2*2*2; var log <- true or false or false; var mixAdd <- 1+2-3; \"some dumb string\"[1:5];";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Typing for comparison operations.
*/
bool comparisonTyping(void)
{
	cout << "\n---\nComparison Expressions:\n";
	const char *test="1==2; 1>2; 1.0<2.0; 1<2.0; 1.0<2; \"hello\"<\"world\"; [x<-0;]==[x<-1;]; 1==\"1\"; []==1; [x<-1;]!=[y<-1;]; [x<-1;]==[x<-2;y<-true;]; 1>2; 2>=0; true>false; true==false; \"some dumb string\"==\"dumb string\"; 1 < 2 <3; var c(a,b,c)-> return a<b<c; end; var c2(a,b,c)-> return a<b and b<c; end;";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Typing with record supertypes...
*/
bool recComparisonTyping(void)
{
	cout << "\n---\nComparison Expressions:\n";
	const char *test="var rcmp(r)->return r==[x<-1;]; end; rcmp([x<-2;y<-true;]);";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}


/* Ceck access control flags in types (for const values and immutable parameters)
*/
bool AccessControl(void)
{
	cout << "\n---\nAccess Control Type Annotations:\n";
	const char *test="var i<-1; var b<-true; i<-2; b<-false; 2<-i; 2<-3; 3<-b; fun(r)-> r.x<-2; end; var r<-[s<-\"\";]; s<-\"Hello!\"; r.s<-\"Hello!\"; b<-grrr; fun(x)->return x; end(1)<-2;";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Typing for [] operation on sequences and dictionaries
	For dictionaries, this operation provides access to the value referenced by the key passed,
	for Sequences it performs a splice operation (extract a subsequence).
*/
bool extractionOperation(void)
{
	cout << "\n---\nExtraction and Splice Operations:\n";
	const char *test="{1,2,3,4,5}[2]; \"StringMaster\"[1]; \"StringMaster\"[0:5]; var countElements(s)-> var counts<-{=>}; for i in s do case counts[i]of Some value then counts[i]<-value+1;|None then counts[i]<-0; end; done; return counts; end;";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Test typing of identity function f(x)=x
*/
bool basicPolymorphicApplication(void)
{
	cout << "\n---\nBasic Polymorphic Application (y undefined!):\n";
	const char *test="var x<-2; var id(x)-> return x; end; id(true); id(x); id(\"Boo\"); id(y); id;";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Tests for string interpolation, typing of interpolated values
*/
bool stringVarInterpolation(void)
{
	cout << "\n---\nString Interpolation of top bound variables:\n";
	const char *test="var greet(person)-> return \"Hello ${person} :)\"; end; greet(\"Charly Brown\"); greet(42);";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Test typing for polymorphic function where parameters are not top bound (numbers in this case).
*/
bool subtypePolymorphicApplication(void)
{
	cout << "\n---\nSubtype Polymorphic Application:\n";
	const char *test="var f(a,b) -> return a*a+b*b; end; f(1,2); f(1.0,2); f(1,2.0); f(1.0,2.0); f(\"a\",true); var x<-2; var y<-1.3; f(x,y);";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Infer that a parameter has a function type
*/
bool higherOrderFunctionTypying(void)
{
	cout << "\n---\nHigher Order Function Typying:\n";
	const char *test="var map(l,f)-> return {f(x)|x in l}; end; var line(a,b)->return fun(x)->return a*x+b; end; end;";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Propoagate error out of function
*/
bool errorFunctionTypying(void)
{
	cout << "\n---\nError in Function:\n";
	const char *test="var f(a) -> if a+2 then return true; else return false; end; end;";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Make sure integers and floats are typed correctly
*/
bool applicationSubtypeingTests(void)
{
	cout << "\n---\nApplication and subtyping:\n";
	const char *test="var inci(a) -> return a+1; end; var incf(a) -> return a+.1; end; inci(1); inci(1.2); incf(1); incf(1.2);";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Polymorhpic function over sequences.
*/
bool sequenceTyping(void)
{
	cout << "\n---\nSequence Typing:\n";
	const char *test="var f(s)->return s[1:last]; end; f({1,2,3,4}); f(\"Blooper\");";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Basic type inference cases for lists
*/
bool listTyping(void)
{
	cout << "\n---\nList Typing:\n";
	const char *test="var l <- { 1, 2, 3 }; l[1]; l[1:3];";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Splice list of records
*/
bool recListSpliceTyping(void)
{
	cout << "\n---\nSplice of List of records typing:\n";
	const char *test="{[x<-1;],[x<-3;],[x<-5;]}[1:1];";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Build a list using a generator expression ("List comprehension").
*/
bool listsAndGenerator(void)
{
	cout << "\n---\nLists and Generators:\n";
	const char *test="var l <- { [a<-x;b<-y;c<-z;] | x from 1 to 10 && y from 1 to 10 && z from 1 to 10 ?? x*x+y*y==z*z };";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Build list using generators and helper functions
*/
bool listsAndGeneratorRefactor(void)
{
	cout << "\n---\nLists and Generators after refactoring:\n";
	const char *test="var triple(x,y,z)-> return [a<-x;b<-y;c<-z;]; end; var isRightTri(x,y,z)-> return x*x+y*y==z*z; end; var l <- { triple(x,y,z) | x from 1 to 10 && y from 1 to 10 && z from 1 to 10 ?? isRightTri(x,y,z) };";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* As above, but checks less triangles (see "from" operations starting values)
*/
bool listsOfRightTriangles(void)
{
	cout << "\n---\nLists of right triangles:\n";
	const char *test="var triple(x,y,z)-> return [a<-x;b<-y;c<-z;]; end; var isRightTri(x,y,z)-> return x*x+y*y==z*z; end; var l <- { triple(x,y,z) | x from 1 to 10 && y from x to 10 && z from y to 10 ?? isRightTri(x,y,z) };";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Type inference for simple for loop and variable assignment.
*/
bool forAndUpdateTyping(void)
{
	cout << "\n---\nFor loop and update expression typing:\n";
	const char *test="var y<-0; for x from 1 to 10 do y <- x; done; ";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Test while loop.
*/
bool whileTyping(void)
{
	cout << "\n---\nWhile loop and update expression typing:\n";
	const char *test="var y<-0; while y <10 do y <- y+1; done; ";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Type inference for a simple record value.
*/
bool BasicRecordTyping(void)
{
	cout << "\n---\nBasic Record Typing:\n";
	const char *test="var y<-[x<-2;y<-3;]; y.x; y.y; y.z;";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Type inference for a value with nested records.
*/
bool NestedRecordTyping(void)
{
	cout << "\n---\nNested Record Typing:\n";
	const char *test="var y<-[ a<-[x<-2;y<-3;]; b<-[u<-8.7;v<-0.8;];]; y.a.x; y.a.y; y.b.u; y.b.v; y.b.x;";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Polymorphic function returns record
*/
bool FunctionReturnsRecord(void)
{
	cout << "\n---\nFunction Returns Record:\n";
	const char *test="var pair(a,b)->return [ first<-a; second<-b;]; end; pair(1,2); pair(\"Blue\",true);";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Infer that value passed to a function is epected to be a record.
*/
bool InferRecordParameter(void)
{
	cout << "\n---\nInfer Record Parameter:\n";
	const char *test="var lengthsq(a)->return a.x*a.x+a.y*a.y; end; ";//lengthsq([x<-1;y<-2;]); lengthsq([x<-1.2;y<-2.3;]); lengthsq(true,false);";
																	  //parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Type inference tests for variant usage.
*/
bool VariantTyping(void)
{
	cout << "\n---\nVariant Typing:\n";
	const char *test="var x <- [ :point2 x<-1; y<-3;]; var checkedDiv(val,div)-> if div==0.0 then return [:None]; else return [:Result value<-val/div;]; end; end;  case checkedDiv(1,2) of Result value then var x<-value; | Pair first second then var z<-first; | None then var y<-0; end;";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Type infrence for variants being passed to multiple functions. The functions
	have non-empty intersection in variant types , and the function that calls both helpers accepts
	the intersection of the labels.
*/
bool VariantParameterTyping(void)
{
	cout << "\n---\nVariant Paramter Typing:\n";
	const char *test="var f(v)->case v of Vector2 x y then return x+y; | Vector3 x y z then return x+y+z;end; end;\nvar g(v)->case v of Vector2 x y then return x+y; | Scalar value then return value; end; end;\nvar gh(v)->f(v); g(v); return; end; f([:Vector2 x<-1; y<-3;]); ";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Define a simple generator function
*/
bool GeneratorDefinition(void)
{
	cout << "\n---\nGenerator Definition:\n";
	const char *test="var edges(a)-> yield a.a; yield a.b; yield a.c; yield done; end;";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Basic type inference cases for dictionaries
*/
bool basicDictonary(void)
{
	cout << "\n---\nBasic Dictionary Definition:\n";
	const char *test="var d<-{ \"a\"=>1, \"b\"=>2, \"c\"=>3, \"d\"=>4 }; d[\"a\"]; d\\\"a\";";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Infer type of empty dictionary from subsequent uses.
*/
bool emptyDictTyping(void)
{
	cout << "\n---\nEmpty dictionary typing:\n";
	const char *test="var d<-{=>}; begin var x<-[key<-1;value<-2;];  d[x.value]<-x.key; end;  d[2];";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Complex test for dictionary typing: correctly infer type of function that swaps key and values ni dictionary.
	If a permutation is passed, the permutation is inverted.
*/
bool complexDictGenFor(void)
{
	cout << "\n---\nComplex test: For with generators and dictionaries:\n";
	const char *test="var invert(d)-> var r<-{=>}; for x in d do r[x.value]<-x.key; done; return r; end;  var permutation<-{1=>\"2\",2=>\"3\",3=>\"1\"}; invert(permutation);";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Formulation of permuattoin inversion function using comprehension instead of loop.
*/
bool alternateInvertTest(void)
{
	cout << "\n---\nComplex test: Alternate invert:\n";
	const char *test="var invert(d)-> return {x.value => x.key | x in d}; end;  var permutation<-{1=>2,2=>3,3=>1}; invert(permutation);";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Infer all labels in a record from multiple interactions with the value.
*/
bool supertypeRecordMerging(void)
{
	cout << "\n---\nSupertype Record Merging:\n";
	const char *test="var first(a)-> return a.first; end; var second(a)-> return a.second; end; var swap(a)->return [f <- second(a); s<-first(a);]; end;";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Treat list as generator
*/
bool containerGenList(void)
{
	cout << "\n---\nContainer generator inference (List) :\n";
	const char *test="var v<-{1,2,3,4,5}; for x in v do var y<-x; done; ";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Treat dictoinary as generator
*/
bool containerGenDict(void)
{
	cout << "\n---\nContainer generator inference (Dictionary) :\n";
	const char *test="var v<-{1=>\"a\",2=>\"b\",3=>\"c\",4=>\"d\",5=>\"e\"}; for p in v do var x<-p.key; var y<-p.value; done; ";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}

/* Define a simple module for pairs.
*/
bool basicModule(void)
{
	cout << "\n---\nBasic Module Definition:\n";
	const char *test="module Pair exports type Pair(?a,?b); first:(Pair(?a,?b))-> ?a; second:(Pair(?a,?b))-> ?b; swap:(Pair(?a,?b))->Pair(?b,?a); from var Pair(a,b)->return [first<-a;second<-b;]; end; var first(a)-> return a.first; end; var second(a)-> return a.second; end; var swap(a)->return [first<-a.second;second<-a.first;]; end; end. var p<-Pair::Pair(1,true); import Pair; var q<-swap(p); var b<-first(p)==second(q); ";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	parser.deleteStatements();
	return isOK;
}
/* Tests free variable collection method of class Type.
*/
bool freeVariableDetection(void)
{
	cout << "\n---\nFree variable determination:\n";
	const char *test="var x<-1; var s <- \"test\"; var f1(a)->return a+x; end; var line(a,b)-> return fun(x)-> return a*x+b; end; end; ";
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test,strlen(test));
	parse::Parser parser(&scanner);
	parser.Parse();
	std::list<intro::Statement*>::iterator iter;
	for (iter=parser.parseResult.begin();iter!=parser.parseResult.end();iter++)
	{
		std::list<intro::Function*> funcs;
		std::list<intro::Function*>::iterator fit;
		(*iter)->collectFunctions(funcs);
		for (fit=funcs.begin();fit!=funcs.end();fit++)
		{
			intro::VariableSet bound, free;
			(*fit)->getFreeVariables(free,bound);
			intro::VariableSet::iterator vit;
			wcout << L"Free variables: ";
			for (vit=free.begin();vit!=free.end();vit++)
			{
				wcout << *vit << L" ";
			}
			wcout << endl;
		}
	}
	
	parser.deleteStatements();

	return true;
}

/* Generate code for basic expressions with constant values.
*/
bool constantExpressionCodeGen(void)
{
	cout << "\n---\nConstant Expressions Code Generation:\n";
	const char *test="2+3; 2.1+3.3; true or false; true xor true; var ni<--1;# var nr<--2.3;";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	if (isOK) dumpGeneratedStatements(parser);
	parser.deleteStatements();
	return isOK;
}

/* Generate code for a simple string value
*/
bool stringLiteralsCodeGen(void)
{
	cout << "\n---\nString Literal Code Generation:\n";
	const char *test="\"Hello World\";";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	if (isOK) dumpGeneratedStatements(parser);
	parser.deleteStatements();
	return isOK;
}

/* Generate code for some global variables
*/
bool globalVariablesCodeGen(void)
{
	cout << "\n--- Global VariablesCode Generation:\n";
	const char *test="var x<-42; var y<-x+51; var z<-x+2.58;";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	if (isOK) dumpGeneratedStatements(parser);
	parser.deleteStatements();
	return isOK;
}

/* Generate code for string interpolation of several variables.
*/
bool stringInterpolationCodeGen(void)
{
	cout << "\n---\nString Interpolation Code Generation:\n";
	const char *test="var x <- 12; var d<-3.1415; var y <- \"Hallo\"; var s <- \"${y} ${d} ${x}\";";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	if (isOK) dumpGeneratedStatements(parser);
	parser.deleteStatements();
	return isOK;
}

/* Generate code for a simple record.
*/
bool basicRecordCodeGen(void)
{
	cout << "\n---\nBasic Record Code Generation:\n";
	const char *test="var r<-[ foo<-2; bar<-3.1415; ]; r.foo; r.bar; ";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	if (isOK) dumpGeneratedStatements(parser);
	parser.deleteStatements();
	return isOK;
}

/* Generate code for a simple while loop
*/
bool basicWhileLoopCodeGen(void)
{
	cout << "\n---\nBasic While Loop (TODO: ASSIGNMENT) :\n";
	const char *test="var x<-0; var y<-20; while x<y do x<-x+1; done;";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	if (isOK) dumpGeneratedStatements(parser);
	parser.deleteStatements();
	return isOK;
}

/* Generate code for a simple function.
*/
bool basicFunctionCodeGen(void)
{
	cout << "\n---\nBasic Function Code Generation:\n";
	const char *test="var id(value)->return value; end; id(1);";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	if (isOK) dumpGeneratedStatements(parser);
	parser.deleteStatements();
	return isOK;
}
/* Generate code for a simple function.
*/
bool arithFunctionCodeGen(void)
{
	cout << "\n---\nArithmetic Function Code Generation:\n";
	const char *test="var incr(value)->return value+1; end;";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	if (isOK) dumpGeneratedStatements(parser);
	parser.deleteStatements();
	return isOK;
}

/* Generate code for a simple function.
*/
bool polyFunctionCodeGen(void)
{
	cout << "\n---\nPolymorphic Function Code Generation:\n";
	const char *test=" var neg(x)->return -x; end; #var line(slope,offset,x)->return slope*x+offset; end;";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	if (isOK) dumpGeneratedStatements(parser);
	parser.deleteStatements();
	return isOK;
}

/* Generate code for a simple function.
*/
bool polyClosureCodeGen(void)
{
	cout << "\n---\nPolymorphic Closure Code Generation:\n";
	const char *test="var line(slope,offset)->return fun(x)->return slope*x+offset; end; end; var x <- line(0.5,2.0);";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	if (isOK) dumpGeneratedStatements(parser);
	parser.deleteStatements();
	return isOK;
}

/* Generate code for a simple record interaction.
*/
bool recordCodeGen(void)
{
	cout << "\n---\nBasic Record Code Generation:\n";
	const char *test="var rec <- [ first <- 2; second <- \"Hello World\";]; rec.second;";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	if (isOK) dumpGeneratedStatements(parser);
	parser.deleteStatements();
	return isOK;
}

/* Generate code for a variant interaction.
*/
bool variantCodeGen(void)
{
	cout << "\n---\nVariant Code Generation:\n";
	const char *test="var v1 <- [ :pair first <- 2; second <- \"Hello World\";]; var v2 <- [:vec2 x<-2.0; y<-3.3;];";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	if (isOK) dumpGeneratedStatements(parser);
	parser.deleteStatements();
	return isOK;
}

/* Generate code for basic comparisons
*/
bool basicCompareCodeGen(void)
{
	cout << "\n---\nBasic Compare Code Generation:\n";
	const char *test="var x <- 1; var b <- x < 3; var c <- x < 0.8; var d <- \"hello\" > \"world\";";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	if (isOK) dumpGeneratedStatements(parser);
	parser.deleteStatements();
	return isOK;
}

/* Generate code for generic comparisons
*/
bool polyCompareCodeGen(void)
{
	cout << "\n---\ngeneric Compares Code Generation:\n";
	const char *test="var sort(a,b,c) -> return a < b and b < c; end; var eq(a,b,c) -> return a==b and b==c; end;";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	if (isOK) dumpGeneratedStatements(parser);
	parser.deleteStatements();
	return isOK;
}

/* Generate code for basic lists
*/
bool basicListCodeGen(void)
{
	cout << "\n---\nList Code Generation:\n";
	const char *test="var l <- { 1, 2, 3, 4 }; var s <- l[1:last-1];";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	if (isOK) dumpGeneratedStatements(parser);
	parser.deleteStatements();
	return isOK;
}

/* Generate code for basic lists
*/
bool genListCodeGen(void)
{
	cout << "\n---\nGenerator based List Code Generation:\n";
	const char *test="var l <- { x | x from 1 to 10}; for x in l do var y <- x; done;";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	if (isOK) dumpGeneratedStatements(parser);
	parser.deleteStatements();
	return isOK;
}

/* Generate code for basic lists
*/
bool emptyGeneratorCodeGen(void)
{
	cout << "\n---\n Empty Generator definition Code Generation:\n";
	const char *test="var g()-> yield done; end; ";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	if (isOK) dumpGeneratedStatements(parser);
	parser.deleteStatements();
	return isOK;
}

/* Generator with condition code for basic lists
*/
bool conditionGeneratorCodeGen(void)
{
	cout << "\n---\n GeneratorStatement with Condition Code Generation:\n";
	const char *test="var f(s)->return {x | x in s } ; end; ";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	if (isOK) dumpGeneratedStatements(parser);
	parser.deleteStatements();
	return isOK;
}

/* Generate code for basic lists
*/
bool simpleGeneratorCodeGen(void)
{
	cout << "\n---\n Simple Generator definition Code Generation:\n";
	const char *test="var g(a,b,c)-> yield a; yield b; yield c; yield done; end; ";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	if (isOK) dumpGeneratedStatements(parser);
	parser.deleteStatements();
	return isOK;
}

bool complexGeneratorCodeGen(void)
{
	cout << "\n---\n Complex Generator definition Code Generation:\n";
	const char *test="var range(a,b)-> var retval<-a; while retval<=b do yield retval; retval<-retval+1; done; yield done; end; ";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	if (isOK) dumpGeneratedStatements(parser);
	parser.deleteStatements();
	return isOK;
}

bool elementCountCodeGen(void)
{
	cout << "\n---\n Histogram from sequence Code Generation:\n";
	const char *test="var hist(seq)-> var counts<-{=>}; for item in seq do case counts[item]of Some value then counts[item]<-value+1;|None then counts[item]<-1; end; done; return counts; end; ";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	if (isOK) dumpGeneratedStatements(parser);
	parser.deleteStatements();
	return isOK;
}

bool retLikeCodeGen(void)
{
	cout << "\n---\n return like statement Code Generation:\n";
	const char *test="var f(a)-> case a of A x then return x; | B x y then return x+y; end; end;";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK=parser.inferTypes(&global);
	if (isOK) dumpGeneratedStatements(parser);
	parser.deleteStatements();
	return isOK;
}


bool variantsUseCodeGen(void)
{
	cout << "\n---\n Variants in Action Code Generation:\n";
	const char *test = "var f(a)-> case a of A x then return x; | B x y then return x+y; end; end; f([:A x<-3;]); f([:B x<-3; y<-4;]);";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK = parser.inferTypes(&global);
	if (isOK) dumpGeneratedStatements(parser);
	parser.deleteStatements();
	return isOK;
}

bool filterCodeGen(void)
{
	cout << "\n---\n Filter function Code Generation:\n";
	const char *test = "var filter(s,c) -> return {x | x in s ?? c(x)}; end;";
	//parse::Parser parser=getParser(test);
	cout << test << endl << endl;
	parse::Scanner scanner((const unsigned char *)test, strlen(test));
	parse::Parser parser(&scanner);
	parser.isInteractive = false;
	parser.Parse();
	intro::Environment global;
	bool isOK = parser.inferTypes(&global);
	if (isOK) dumpGeneratedStatements(parser);
	parser.deleteStatements();
	return isOK;
}

// Execute all type tests
bool typeTests(void)
{
	cout << "\n\tType Tests:\n";
/*	literalTyping();
	expressionTyping();
	comparisonTyping();
	recComparisonTyping();
	AccessControl();
	extractionOperation();
	applicationSubtypeingTests();
	basicPolymorphicApplication();
	stringVarInterpolation();
	subtypePolymorphicApplication();
	higherOrderFunctionTypying();
	errorFunctionTypying();
	sequenceTyping();
	listTyping();
*/	//recListSpliceTyping();
	//listsAndGenerator();
	//listsAndGeneratorRefactor();
	//listsOfRightTriangles();
	//forAndUpdateTyping();
	//whileTyping();
	//BasicRecordTyping();
	//NestedRecordTyping();
	//FunctionReturnsRecord();
	//InferRecordParameter();
	//VariantTyping();
	//VariantParameterTyping();
	//GeneratorDefinition();
	//basicDictonary();	
	//supertypeRecordMerging();
	//containerGenList();
	//containerGenDict();
	//emptyDictTyping();
	//complexDictGenFor();
	//alternateInvertTest();
	//basicModule();
//	freeVariableDetection();
	return true;
}

// Execute all code generation tests
bool codeGenTests(void)
{
	constantExpressionCodeGen();
	//globalVariablesCodeGen();
	//stringLiteralsCodeGen();
	//stringInterpolationCodeGen();
	//basicFunctionCodeGen();
	//arithFunctionCodeGen();
	polyFunctionCodeGen();

	//polyClosureCodeGen();
	//recordCodeGen();
	//variantCodeGen();
	//basicCompareCodeGen();
	//polyCompareCodeGen();
	//basicListCodeGen();
	//genListCodeGen();
	//emptyGeneratorCodeGen();
	//simpleGeneratorCodeGen();
	//complexGeneratorCodeGen();
	//elementCountCodeGen();
	//conditionGeneratorCodeGen();
	//retLikeCodeGen();
	//variantsUseCodeGen();
	//filterCodeGen();
	return true;
}

void runBasicTests(void)
{
	//checkSubtypeEnum();
	typeTests();
	codeGenTests();
}