# Intro
Intro is a programming language intended to be easy to learn. It is a script language, 
and like most such languages it provides a read-eval-print loop. (Currenly the only way to produce output.)

It is also supposed to be a useful language throughout a programmers career.

This is version 0.1 and a few things are still missing:
* Modules
* generated code leaks memory
* standard library, I/O especially

# Building the source
tested with 
* MSVC 2015: use solution file
* cygwin gcc: use makefile

Requires LLVM 4 installed, check the solutions path under windows to make sure it points the right way.
Building tests requires google-test installed - MSVC will try to build tests by default. Again, check project directories.

# Main Features (as of V0.1):
* Small language focusing on fundamental programming language concepts, with syntax hopefully promoting readable code
* Strict static typing based on type inference (Hindley-Milner with Records and Subtyping a.k.a F1sub)
* Higher order functions and parametric polymorphism
* LLVM based code generation

Note that object orientation is not considered sufficiently fundamental or necessary to be included. 
It's application is complex enough that beginners are usually not able to say when it is needed, 
and actual exploitation requires design for OOP. OO also leads to an undecidable type inference problem,
and type inference is much more helpful for programmers at any skill level. 

That said, only the core trait of inheritance cannot be emulated, 
as higher order functions with closures can be placed in records
and records are structurally subtyped.

# Super short quick start guide

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
* Lists are curly brackets containing comma separated elements which must have the same type: { 1,2,3 }, {"a","foo","gurgle"}
* Dictionaries are like lists, but contain key-value pairs connected with a => (all keys must be of the same type and all values as well): { 1=>"a",2=>"b",3=>"c" }
* Dictionary lookup uses the [] operator which returns a maybe variant: d["a"]
* lists and dictionaries may also have one expression for element or key-value pair, followed by a pipe 
symbol | and a generator statement. See below
* Records contain an arbitrar number of fields, which are labeled by an identifier. Fields may have distinct types.
The records consists of square brackets with semicolon terminated field assignments identifier <- Expression; e.g.
[ x<-1; y<-2; active<-false; name<-"foobar";]
* Record elements can be accessed by label with the . operator
* Variants are written like records, but the begin with a colon and an identifier: [:Vec2 x<-1; y<-2;]
* The dot oprator does not work on variants, instead the case statemetn must be used.
* Generators look like functions, but instead or return they use the keyword yield. Repeated applications of a generator
continue after the last executed yield statement, unless it is a "yield done". Generators can only be used in generator statements:
fun(a,b,c)->yield a; yield b; yield c; yield done; end;

The maybe variant hsa type {[:None]+[:Some value:?Key]}.

Other expressions:
* Arithmetic (integer and real): +,*,-,/,%
* boolean: and, or, xor, not
* compares: ==, != (all types) and >, <, >=, <= (only string, integer, real)
* list and string splice: l[1:last] (new list without the first element)

Statements (must end with a semicolon):
* Every expression is a statement.
* variable definitions are "var" followed by an identifier followed by "<-" followed by an initializer expression.
Variables must always be initialized explicitely when defined: var x<-3; var f<-fun(a,b)->return a*a+b*b; end;
* Some syntax sugar is provided for declarations of variables with function types, remofing the "<-fun":
var line(slope,offset)->return fun(x)->return slope*x+offset; end; end;
* Conditions use the common if...then...else with the last branch ending with an end, additional conditions use the single word eslif
if x==1 then # do someting

elsif x==2 then # do something else

else #give up

end;

if error then # panic

end;
* while loops: while condition do bla done;
* for loops
* case statements

Generators Statements: 
soon... see cheatsheat in docs


Questions for Ory:
* Does case allow mutating a variant? could be useful
