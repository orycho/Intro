# Intro
Intro is a programming language intended to be easy to learn. It is a script language, 
and like most such languages it provides a read-eval-print loop. (Currenly the only way to produce output.)

It is also supposed to be a useful language throughout a programmers career.

This is version 0.1 and a few things are still missing:
* Modules
* generated code leaks memory
* standard library, I/O especially

# Main Features (as of V0.1):
* Small language focusing on fundamental programming language concepts, with syntax hopefully promoting readable code
* Strict static typing based on type inference (Hindley-Milner with Records and Subtyping a.k.a F1sub)
* Higher order functions and parametric polymorphism
* LLVM based code generation

Note that object orientation is not considered sufficiently fundamental or necessary to be included. 
Its application is complex enough that beginners are usually not able to say when it is needed, 
and actual exploitation requires design for OOP. OO also leads to an undecidable type inference problem,
and type inference is much more helpful for programmers at any skill level. 

That said, only the core trait of inheritance cannot be emulated, 
as higher order functions with closures can be placed in records
and records are structurally subtyped.

# Building the source
tested with 
* MSVC 2015: use solution file
* cygwin gcc: use makefile

Requires LLVM 4 installed, check the solutions path under windows to make sure it points the right way.
Building tests requires google-test installed - MSVC will try to build tests by default. Again, check project directories.

To modify the grammar, you will need COCO/R installed - use namespace "parser" and the frame files in subdirectory coco.

# Super short quick start guide

(This is so short that even the cheatsheet.pdf in docs is more in depth.)

All statements are terminated by a semicolon (except modules, but those are not implemented yet).
Single line comments begin with a pound sign (hastag) #
Multiline comments are like in C, from  /* to */

All values in Intro are just written out, each built in type has distinct syntax.

Simple types:
* Booleans can be: true, false
* Strings are in quotes: "some string", "abc"
* Integers consist of only digits: 0, 123, 400246
* Reals have at least one dot between digits: .0, 1.23, 2360897.

Compound types:
* Functions begin with the keyword "fun" followed by a list of comma separated identifiers in parentheses (parameters)
followed by "->" and the body. They must end on a return or return  like statement, and are terminated with the keyword end:
fun(a,b,x)->return a*x+b; end
* Functions never have names, no matter the syntax suger.
* a function (e.g. from a variable) is called by following the function with the parameters as comma separated expressions in parentheses. The correct number of parameters and their types, as wel as the return type, are specified by the functions type.: sin(2*pi), foo(1-a,1/a)
* Lists are curly brackets containing comma separated elements which must have the same type: { 1,2,3 }, {"a","foo","gurgle"}
* Dictionaries are like lists, but contain key-value pairs connected with a => (all keys must be of the same type and all values as well): { 1=>"a",2=>"b",3=>"c" }
* Dictionary lookup uses the [] operator which returns a maybe variant on lookup: d["a"]. Can also be used for assignment
d["x"]<-6;
* lists and dictionaries may also have one expression for element or key-value pair, followed by a pipe 
symbol | and a generator statement. See below
* Records contain an arbitrar number of fields, which are labeled by an identifier. Fields may have distinct types.
The records consists of square brackets with semicolon terminated field assignments identifier <- Expression; e.g.
[ x<-1; y<-2; active<-false; name<-"foobar";]
* Record elements can be accessed by label with the . operator
* Variants are written like records, but the begin with a colon and an identifier: [:Vec2 x<-1; y<-2;]
* The dot oprator does not work on variants, instead the case statemetn must be used.
* Generators look like functions, but instead of return they use the keyword yield. Repeated applications of a generator
continue after the last executed yield statement, unless it is a "yield done". Generators can only be used in generator statements:
fun(a,b,c)->yield a; yield b; yield c; yield done; end;
* Lists, dictionaries and strings are all generators that range over the contents of the value. For lists, the elements, for strings the characters, and for dictionaries records with labels key and value with the contents accordingly.

The maybe variant hsa type {[:None]+[:Some value:?Key]}.

Other expressions:
* Arithmetic (integer and real): +,*,-,/,%
* boolean: and, or, xor, not
* compares: ==, != (all types) and >, <, >=, <= (only string, integer, real)
* list and string splice: l[1:last] (new list without the first element)

Statements (must end with a semicolon):
* Every expression is a statement. No other statements return values.
* variable definitions are "var" followed by an identifier followed by "<-" followed by an initializer expression.
Variables must always be initialized explicitely when defined: var x<-3; var f<-fun(a,b)->return a*a+b*b; end;
* Some syntax sugar is provided for declarations of variables with function types, removing the "<-fun":
var line(slope,offset)->return fun(x)->return slope*x+offset; end; end;
* Conditions use the common if...then...else with the last branch ending with an end, additional conditions use the single word elsif

if x==1 then # do someting

elsif x==2 then # do something else

else #give up

end;


if error then # panic

end;
* while loops: while condition do work(); morework(); done;
* for loops: are basically a wrapper for generator statements and will be discussed there
* case statements are conditionals that branch not on a boolean condition, but on variant tags. The keyword case
is followed by an expression that must be a variant, and the word "of".
Each branch begins with a tag and is followed by labels that must exist in the variant with that label 
(otherwise there will be a type error). Those labels are bound to the corresponding fields in the variant. 
After the tag and labels comes the word "then", followed by the branche's statements. 
The branches are separated by pipe symbols "|":
case variantExpr of 
A x then return x; 
| B a b then return a+b; 
end;

Generators Statements: 
Generators can only be used in these statemens, which are actually always part of another construct: 
in list or dictionary expressions, they provide the values that are turned into the contents of the containers.
The for statements just combines a generator statements with a list of statemetns that operate on each set of values generated.

Generator statemetns consist of statements that bind a variable (name) to a generator's result valeues using the infix keyword "in":
* x in somelist
* char in string

Special syntax sugar is provided for generating integer values:
x from startvalue to endvalue by stepvalue
Where s ranges from start value to at most endvalue and is incremented by stepvalue. Stepvalue is optional and defaults to one,
e.g. to generate all values from 1 to 10:
x from 1 to 10

Multiple values can be generated in one staement, each additional value begins with the operator "&&", and generator
applications (maybe better instantiation) can use the variables introduced to their left:

x from 1 to 10 && y from x to 10 && z from y to 10

A generator value is moved to the next every time the generator to the left (if there) has cmopleted a full cycle.
So the above example generates x=1, y=1, z=1, then z=2 and so on. Once z is 10, y is increased then z is reset to the new y.
It is probably easiest to type it into the repl as a list to see the result:

{ {a<-x; b<-y; c<-z;] | x from 1 to 10 && y from x to 10 && z from y to 10};

Finally, anywhere after the first generator binding conditions can be introduced by the operator "??", and it can use all
generator values defined to their left. Any value combination for which the condition is false will be skipped, which
includes any generators to the right. So here is a list of right triangles, without isomorphism and with the hypothenuse
always in c (in the records in the resulting list):

{ {a<-x; b<-y; c<-z;] | x from 1 to 10 && y from x to 10 && z from y to 10 ?? x*x+y*y==z*z};


Modules

Are not implemented yet!


Questions for Ory:
* Does case allow mutating a variant? could be useful
