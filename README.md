# Intro
Intro is a programming language intended to be easy to learn. It is a script language, 
and like most such languages it provides a read-eval-print loop. (Currenly the only way to produce output.)

It is also supposed to be a useful language throughout a programmer's career.

This is version 0.1 and a few things are still missing:
* Modules
* generated code leaks memory
* standard library, I/O especially
* the repl is flaky, it likes to exit on syntax errors and arrow keys and the like... no getline.

# Main Features (as of V0.1):
* Small language focusing on fundamental programming language concepts, with syntax hopefully promoting readable code
* Strict static typing based on type inference (Hindley-Milner with Records and Subtyping a.k.a F1sub)
* Higher order functions, parametric polymorphism, generators
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

* All statements are terminated by a semicolon (except modules, but those are not implemented yet).
* Single line comments begin with a pound sign (hastag) #
* Multiline comments are like in C, from  /* to */
* lexically scoped

All values in Intro are just written out, each built in type has distinct syntax.

Simple types (re expessions represented by a single literal):
* Booleans can be: <pre>true, false</pre>
* Strings are in quotes: <pre>"some string", "abc"</pre>
* String interpolation: any occurence of ${id}, for some variable identifier id, inside the string is 
replaced with a string representing the value of 'id'. For instance an integer variabela with value 123
would turn 
<pre>"the value is ${a}!!!"</pre>
into the output
<pre>the value is 123!!!</pre>
All types can be turned to strings, but for functions and generators the output is simply "function" and "generator" respectively.
* Integers consist of only digits: <pre>0, 123, 400246</pre>
* Reals have at least one dot between digits: <pre>.0, 1.23, 2360897.</pre>

Compound types (require expressions with arbitrary many literals):
* Functions begin with the keyword "fun" followed by a list of comma separated identifiers in parentheses (parameters)
followed by <pre>-></pre> and the body. They must end on a return or return  like statement, and are terminated with the keyword end:
<pre>fun(a,b,x)->return a*x+b; end</pre>
* Functions never have names, no matter the syntax sugar.
* a function (e.g. from a variable) is called by following the function with the parameters as comma separated expressions in parentheses. The correct number of parameters and their types, as wel as the return type, are specified by the functions type.: <pre>sin(2*pi)
foo(1-a,1/a)</pre>
* Lists are curly brackets containing comma separated elements which must have the same type: <pre>{ 1,2,3 }, {"a","foo","gurgle"}</pre>
* Dictionaries are like lists, but contain key-value pairs connected with a => (all keys must be of the same type and all values as well): <pre>{ 1=>"a",2=>"b",3=>"c" }</pre>
* Dictionary lookup uses the [] operator which returns a maybe variant on lookup: <pre>d["a"]</pre> Can also be used for assignment
<pre>d["x"]<-6;</pre>
* Elements can be removed from dictionaries using the "\\" operator (backslash), it can be chained since the operator 
returns the dictionary (which has ben modified): <pre>d\\"a"\\"b"</pre>
* lists and dictionaries may also have one expression for element or key-value pair, followed by a pipe 
symbol | and a generator statement. See below
* Records contain an arbitrar number of fields, which are labeled by an identifier. Fields may have distinct types.
The records consists of square brackets with semicolon terminated field assignments identifier <- Expression; e.g.
<pre>[ x<-1; y<-2; active<-false; name<-"foobar";]</pre>
* Record elements can be accessed by label with the . operator
* Variants are written like records, but the begin with a colon and an identifier: <pre>[:Vec2 x<-1; y<-2;]</pre>
* The dot oprator does not work on variants, instead the case statement must be used.
* Generators look like functions, but instead of return they use the keyword yield. Repeated applications of a generator
continue after the last executed yield statement, unless it is a "yield done". Generators can only be used in generator statements:
<pre>fun(a,b,c)->yield a; yield b; yield c; yield done; end;</pre>
* Lists, dictionaries and strings are all generators that range over the contents of the value. For lists the elements, for strings the characters, and for dictionaries records with labels key and value with the contents accordingly.

The maybe variant has type {[:None]+[:Some value:?Key]}.

Other expressions:
* Arithmetic (integer and real): <pre>+,*,-,/,%</pre>
* boolean: <pre>and, or, xor, not</pre>
* compares: <pre>==, !=</pre> (all types) and <pre>>, <, >=, <=</pre> (only string, integer, real)
* list and string splice: <pre>l[1:last]</pre> (new list without the first element)

Statements (must end with a semicolon):
* Every expression is a statement. No other statements return values.
* variable definitions are "var" followed by an identifier followed by "<-" followed by an initializer expression.
Variables must always be initialized explicitely when defined: 
<pre>var x<-3; 
var f<-fun(a,b)->return a*a+b*b; end;</pre>
* Identifiers may contain of letters, digits and underscores, but may not begin with a digit. "abc", "_long_ident_344" are ok, "3d" is not.
* Some syntax sugar is provided for declarations of variables with function types, removing the "<-fun":
<pre>var line(slope,offset)->return fun(x)->return slope*x+offset; end; end;</pre>
* Conditions use the common if...then...else with the last branch ending with an end, additional conditions use the single word elsif:
<pre>
if x==1 then # do someting
elsif x==2 then # do something else
else #give up
end;

if error then # panic
end;
</pre>
* while loops: <pre>while condition do work(); morework(); done;<pre>
* for loops: are basically a wrapper for generator statements and will be discussed there
* case statements are conditionals that branch not on a boolean condition, but on variant tags. The keyword case
is followed by an expression that must be a variant, and the word "of".
Each branch begins with a tag and is followed by labels that must exist in the variant with that label 
(otherwise there will be a type error). Those labels are bound to the corresponding fields in the variant. 
After the tag and labels comes the word "then", followed by the branche's statements. 
The branches are separated by pipe symbols "|":
<pre>case variantExpr of 
A x then return x; 
| B a b then return a+b; 
end;</pre>

Generators Statements: 
Generators can only be used in these statemens, which are actually always part of another construct: 
in list or dictionary expressions, they provide the values that are turned into the contents of the containers.
The for statements just combines a generator statements with a list of statemetns that operate on each set of values generated.

Generator statements consist of statements that bind a variable (name) to a generator's result valeues using the infix keyword "in":
* <pre>x in somelist</pre>
* <pre>char in string</pre>

Special syntax sugar is provided for generating integer values:
x from startvalue to endvalue by stepvalue
Where s ranges from start value to at most endvalue and is incremented by stepvalue. Stepvalue is optional and defaults to one,
e.g. to generate all values from 1 to 10:
<pre>x from 1 to 10</pre>

Multiple values can be generated in one statement, each additional value begins with the operator "&&", and generator
applications (maybe better instantiation) can use the variables introduced to their left:

<pre>x from 1 to 10 && y from x to 10 && z from y to 10</pre>

A generator value is moved to the next every time the generator to the left (if there) has completed a full cycle.
So the above example generates x=1, y=1, z=1, then z=2 and so on. Once z is 10, y is increased then z is reset to the new y.
It is probably easiest to type it into the repl as a list to see the result:

<pre>{ [a<-x; b<-y; c<-z;] | x from 1 to 10 && y from x to 10 && z from y to 10};</pre>

Finally, anywhere after the first generator binding conditions can be introduced by the operator "??", and it can use all
generator values defined to their left. Any value combination for which the condition is false will be skipped, which
includes any generators to the right. So here is a list of right triangles, without isomorphism and with the hypothenuse
always in c (in the records in the resulting list):

<pre>{ [a<-x; b<-y; c<-z;] | x from 1 to 10 && y from x to 10 && z from y to 10 ?? x * x + y * y == z * z};</pre>

The for statement consists of the word "for" followed by a generator statement followed by "do", then a list of statements
that will be executed for each group of values generated, and then the keyword "done":
<pre>for x in somelist do process(x); done;</pre>

Modules

Are not implemented yet!

# Example Session

<pre>
$ build/intro
>  1+2*3;
 = 7
> var triple(x,y,z)->return [a<-x;b<-y;c<-z;]; end;
triple:(?T4<:Top,?T5<:Top,?T6<:Top) -> [ a : ?T4<:Top; b : ?T5<:Top; c : ?T6<:Top; ]
> var cond(a,b,c)->return a*a+b*b==c*c; end;
cond:(?asup<:Number,?asup<:Number,?asup<:Number) -> Boolean
> var l<-{triple(a,b,c)|a from 1 to 10 && b from a to 10 && c from b to 10 ?? cond(a,b,c) };
l:List([ a : Integer; b : Integer; c : Integer; ])
> l;
 = {[ a<-3; b<-4; c<-5; ], [ a<-6; b<-8; c<-10; ]}
> var f(a)-> case a of A x then return x; | B a b then return a+b; end; end;
f:(?T27sup<:{ [ :A x : ?asup<:Number; ] + [ :B a : ?asup<:Number; b : ?asup<:Number; ]}) -> ?asup<:Number
>  f([:A x<-3;]);
 = 3
> f([:B a<-3; b<-4;]);
 = 7
> var append(s1,s2)->for x in s1 do yield x; done; for x in s2 do yield x; done; yield done; end;
append:(?T38sup<:Generator(?T39<:Top),?T40sup<:Generator(?T39<:Top)) -> Generator(?T39<:Top)
> {x | x in append("Hello ","World")};
 = {H, e, l, l, o,  , W, o, r, l, d}
> var d<-{"a"=>1, "b"=>2, "c"=>3, "d"=>4};
d:Dictionary(String,Integer)
> d["x"]<-20;
 = 20
> "d is ${d}";
 = d is {d=>4, x=>20, c=>3, a=>1, b=>2}
> var line(slope,offset)->return fun(x)->return slope*x+offset; end; end;
line:(?asup<:Number,?asup<:Number) -> (?asup<:Number) -> ?asup<:Number
> var l1<-line(2,3);
l1:(Integer) -> Integer
> l1(0);
 = 3
> l1(1);
 = 5
> l1<-line(3,2);
 = function
> l1(0);
 = 2
> l1(1);
 = 5
> var l2<-line(1.2,2.3);
l2:(Real) -> Real
> l2(0.0);
 = 2.300000
> l2(1.0);
 = 3.500000
> l2(1);
Type Error (5, 116): At least one parameter type does not fit what the function expects.!
Type errors detected!
> l1<-l2;
Type Error (9, 167): Assignment between incompatible variable and expression types.!
Type errors detected!
>

</pre>

Questions for Ory:
* Does case allow mutating a variant? could be useful
