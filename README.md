# Intro
Intro is a programming language intended to be easy to learn. It is a script language, 
and like most such languages it provides a read-evaluate-print loop.

It is also supposed to be a useful language throughout a programmer's career.

## Main Features (as of V0.2):
* Small language focusing on fundamental programming language concepts, with syntax hopefully promoting readable code
* Strict static typing based on type inference (Hindley-Milner with records and sub-typing a.k.a. F1sub)
* Higher order functions, parametric polymorphism, generators
* Simple Module system to help organize larger projects (probably buggy, and only basic opaque type features work)
* LLVM based code generation - performance is not a primary concern, yet decent despite some features incurring overhead (e.g. polymorphism, closures).

Note that object orientation is not considered sufficiently fundamental or necessary to be included. 
Its application is complex enough that beginners are usually not able to say when it is needed, 
and actual exploitation requires design for OOP. OO also leads to an undecidable type inference problem,
and type inference is much more helpful for programmers at any skill level. 

That said, only the core trait of inheritance cannot be emulated, 
as higher order functions with closures can be placed in records
and records are structurally sub-typed.

This is version 0.2 and a few things are still missing:

* generated code leaks memory
* standard library currently very limited, and likely buggy.
* the repl is flaky, it likes to exit on syntax errors and arrow keys and the like... no getline.

## Building the source
tested with 

* MSVC 2015: use solution file to build.
* cygwin gcc: use makefile. Just type "make" in the shell, from the project root directory.

### Requirements
#### Core Requirements
* [LLVM 5](http://llvm.org/) 
* Building tests requires [google-test](https://github.com/google/googletest).

#### MS Visual Studio Requirements
* The [Visual Leak Detector](https://vld.codeplex.com/) is used in debug mode.

Check the project directories configured under VC++ to make sure the libraries and headers are found.

#### Syntax Change Requirements
* To modify the grammar, you will need [COCO/R](http://www.ssw.uni-linz.ac.at/Coco/) installed - use namespace "parser" and the frame files in subdirectory coco, or check makefile/solution


# Example Session
Let's jump right in and see the language in action. It is started with e.g.
<pre>
$ build/intro
</pre>
Where "$" is the shell prompt. The interpreter uses ">" when in interactive mode, and when interactive
it also prints out the results of the root expression.
This can be shown with some simple arithmetic, using the interpreter as a calculator:
<pre>
>  1+2*3;
 = 7
</pre>

When a script is passed to the interpreter it is not in interactive mode, and then will
not print out the results. In such cases the program is expected to use library functions
to produce output. And even in interactive mode expressions inside functions are never pritned out.

Next up: strings! It's always useful to concatenate strings, using string interpolation:
<pre>
>  var cat(a,b)->return "${a}${b}"; end;
cat:(?T2&lt;:Top,?T3&lt;:Top) -> String
> cat("Hello, ","World");
 = Hello, World
> cat(12*4,true xor false);
 = 48true
</pre>

It turns out that "cat" does not care about the inputs' types, it just converts everthing to strings first!


Let's compute a list of right angled triangles, using two helper functions 
and a list defined by a generator statement.
Triple returns a record that contains three labels (a,b and c), while cond returns true
if the passed values satisfy Pythagoras' theorem.
The variable definitions have their name and type printed out:
<pre>
 > var triple(x,y,z)->return [a&lt;-x;b&lt;-y;c&lt;-z;]; end;
triple:(?T4&lt;:Top,?T5&lt;:Top,?T6&lt;:Top) -> [ a : ?T4&lt;:Top; b : ?T5&lt;:Top; c : ?T6&lt;:Top; ]
> var cond(a,b,c)->return a*a+b*b==c*c; end;
cond:(?asup&lt;:Number,?asup&lt;:Number,?asup&lt;:Number) -> Boolean
> var l&lt;-{triple(a,b,c)|a from 1 to 10 && b from a to 10 && c from b to 10 ?? cond(a,b,c) };
l:List([ a : Integer; b : Integer; c : Integer; ])
> l;
 = {[ a&lt;-3; b&lt;-4; c&lt;-5; ], [ a&lt;-6; b&lt;-8; c&lt;-10; ]}
</pre>
Here are some variants in action;
<pre>
 > var f(a)-> case a of A x then return x; | B a b then return a+b; end; end;
f:(?T27sup&lt;:{ [ :A x : ?asup&lt;:Number; ] + [ :B a : ?asup&lt;:Number; b : ?asup&lt;:Number; ]}) -> ?asup&lt;:Number
>  f([:A x&lt;-3;]);
 = 3
> f([:B a&lt;-3; b&lt;-4;]);
 = 7
</pre>

Dictionaries are as easy: 
<pre>
> var d&lt;-{"a"=>1, "b"=>2, "c"=>3, "d"=>4};
d:Dictionary(String,Integer)
> d["c"];
 = [ :Some value&lt;-3; ]
> d\"c";
 = {d=>4, a=>1, b=>2}
> d["c"];
 = [ :None ]
> d["x"]&lt;-20;
 = 20
> "d is ${d}";
 = d is {d=>4, x=>20, a=>1, b=>2}
</pre>

Moving to something more advanced, here is a generator which takes two sequences, and returns
all elements of the first, then all elements of the second sequence.
<pre>
> var append(s1,s2)->for x in s1 do yield x; done; for x in s2 do yield x; done; yield done; end;
append:(?T38sup&lt;:Generator(?T39&lt;:Top),?T40sup&lt;:Generator(?T39&lt;:Top)) -> Generator(?T39&lt;:Top)
> {x | x in append("Hello ","World")};
 = {H, e, l, l, o,  , W, o, r, l, d}
</pre>
Note that the last command really states that a list of characters is wanted.

Now for some actual fun with functions. The following function line takes a slope and an offset, two numbers.
It returns a function which computes points on that line. And the return functions type is dependent
on the original parameters.
<pre>
> var line(slope,offset)->return fun(x)->return slope*x+offset; end; end;
line:(?asup&lt;:Number,?asup&lt;:Number) -> (?asup&lt;:Number) -> ?asup&lt;:Number
</pre>
First, a line of integers.
<pre>
> var l1&lt;-line(2,3);
l1:(Integer) -> Integer
> l1(0);
 = 3
> l1(1);
 = 5
</pre>
Since functions are in variables, we can replace them with other functions with the same type:
<pre>
> l1&lt;-line(3,2);
 = function
> l1(0);
 = 2
> l1(1);
 = 5
</pre>
But when we move to real values lines we need a new variable
<pre>
> var l2&lt;-line(1.2,2.3);
l2:(Real) -> Real
> l2(0.0);
 = 2.300000
> l2(1.0);
 = 3.500000
> l2(1);
Type Error (5, 116): At least one parameter type does not fit what the function expects.!
Type errors detected!
> l1&lt;-l2;
Type Error (9, 167): Assignment between incompatible variable and expression types.!
Type errors detected!
</pre>
A function counting the number of elements a generator produces.
The function is polymorphic, and is applied first to a list and then a string:
<pre>
> var length(s)->var retval&lt;-0; for i in s do retval&lt;-retval+1; done; return retval; end;
length:(?T5sup&lt;:Generator(?T4&lt;:Top)) -> Integer
> length({1,2,3});
 = 3
> length("Hello foo");
 = 9
</pre>

This function returns a list with all elements from some generator that pass a given condition.
The condition is a function passed to the filter function.
<pre>
> var filter(s,c) -> return {x | x in s ?? c(x)}; end;
filter:(?T5sup<:Generator(?T4<:Top),?T7sup<:(?T4<:Top) -> Boolean) -> List(?T4<:Top)
> filter({x | x from 1 to 10},fun(x)->return x%2==0; end);
 = {2, 4, 6, 8, 10}
</pre>

The Fibonacci function in a classic recursive implementation:
<pre>
> var fib(n)->if n<=2 then return 1; else return fib(n-1) + fib(n-2); end; end;
fib:(Integer) -> Integer
</pre>

Dictionaries can be used to implement simple histograms. Again, the input is a Generator, which is iterated over using a for statement.
Note the expression "{=>}" returns an empty dictionary, for which the type is later inferred to have integer values, and
the key type is the same as the type generated by the value passed.
<pre>
> var histo(seq)->var result<-{=>}; for elem in seq do case result[elem] of None then result[elem]<-1; | Some value then result[elem]<-value+1; end; done; return result; end;
histo:(?T7sup<:Generator(?key<:Top)) -> Dictionary(?key<:Top,Integer)
> histo("Hello World");
 = {W=>1, e=>1, r=>1,  =>1, d=>1, o=>2, l=>3, H=>1}
> histo({1,2,3,4,1,4,1,1,8,9,10,10,2,3,1});
 = {1=>5, 2=>2, 3=>2, 4=>2, 8=>1, 9=>1, 10=>2}
</pre>

The examples provided with intro include functions to compute the hyper-geometric distribution.
The source keyword reads in an external script and executes it in it's place (if there are no errors).

<pre>
> source "examples/hypergeo.intro";
> getProbabilities(60,23,7,3);
P(X  = 3) = 0.302858
P(X  < 3) = 0.450659
P(X <= 3) = 0.753516
P(X  > 3) = 0.246484
P(X >= 3) = 0.549341
> getProbabilities(60,20,7,3);
P(X  = 3) = 0.269764
P(X  < 3) = 0.570763
P(X <= 3) = 0.840527
P(X  > 3) = 0.159473
P(X >= 3) = 0.429237
>
</pre>

Variants become especially flexible and can be used with dictionaries to create
classification schemes:

>> var map<-{ "A"=>fun()->return [:A]; end,"B"=>fun()->return [:B]; end,"C"=>fun()->return [:C]; end};
>map:Dictionary(String,() -> { [ :A ] + [ :B ] + [ :C ]})
>> case map["A"] of None then | Some value then ::sio::print(value()); end;
>[ :A ]>
>> case map["C"] of 
>>	None then 
>>	| Some value then 
>>		case value() of 
>>			A then ::sio::print("A\n");
>>			| B then ::sio::print("B\");
>>			| C then ::sio::print("C\n");
>>		end;
>>	end;
>C
>>
>> case map["A"] of None then | Some value then ::sio::print(value()); end;
>[ :A ]>
>> case map["C"] of 
	None then 
	| Some value then 
		case value() of 
			A then ::sio::print("A");
			| C then ::sio::print("C\n");
		end;
	end;


	
# Super short quick start guide
(This is so short that even the cheatsheet.pdf in docs is more in depth.)

* All statements are terminated by a semicolon (except modules, but those are not really statements).
* Single line comments begin with a pound sign (hashtag) #
* Multi-line comments are like in C, from  /* to */
* lexically scoped

All values in Intro are just written out, each built in type has distinct syntax.

## Simple types (are expressions represented by a single literal):
* Booleans can be: <pre>true, false</pre>
* Strings are in quotes: <pre>"some string", "abc"</pre>
* The backspace \ character in strings has special meaning to insert special characters (e.g. "\\n" new line, 
"\\t" tabulator, "\\\\" backslash, "\\"" quote), check the cheatsheet for a list.
* String interpolation: any occurence of ${id}, for some variable identifier id, inside the string is 
replaced with a string representing the value of 'id'. For instance an integer variabela with value 123
would turn 

>"the value is ${a}!!!"

into the output

>the value is 123!!!

All types can be turned to strings, but for functions and generators the output is simply "function" and "generator" respectively.
Note that only variables are allowed, while allowing expressions may be a tad more comfortable, it would quickly make
complex string definitions unreadable. Just define intermediate variables &emdash; maybe write a helper function.

* Integers consist of only digits: <pre>0, 123, 400246</pre>
* Reals have at exactly one dot amongst the digits: <pre>.0, 1.23, 2360897.</pre>

## Compound types (expressions with arbitrary many literals):
* Functions begin with the keyword "fun" followed by a list of comma separated identifiers in parentheses (parameters)
followed by -> (read "maps to") and the body. They must end on a return or return  like statement, and are terminated with the keyword end:
<pre>fun(a,b,x)->return a*x+b; end</pre>
* Functions never have names, no matter the syntax sugar.
* a function (e.g. from a variable) is called by following the function with the parameters as comma separated expressions in parentheses. The correct number of parameters and their types, as wel as the return type, are specified by the functions type.: <pre>sin(2*pi)
foo(1-a,1/a)</pre>
* Lists are curly brackets containing comma separated elements which must have the same type: <pre>{ 1,2,3 }, {"a","foo","gurgle"}</pre>
* Dictionaries are like lists, but contain key-value pairs connected with a => (all keys must be of the same type and all values as well): <pre>{ "a"=>1,"b"=>2,"c"=>3 }</pre>
* Dictionary lookup uses the [] operator which returns a maybe variant on lookup: <pre>d["a"]</pre> Can also be used for assignment
<pre>d["x"]&lt;-6;</pre>
* Elements can be removed from dictionaries using the "\\" operator (backslash), it can be chained since the operator 
returns the dictionary (which has been modified): <pre>d\\"a"\\"b"</pre>
* lists and dictionaries may also have one expression for element or key-value pair, followed by a pipe 
symbol | and a generator statement. See below
* Records contain an arbitrary number of fields, which are labeled by an identifier. Fields may have distinct types.
The records consists of square brackets with semicolon terminated field assignments identifier &lt;- Expression; e.g.
<pre>[ x&lt;-1; y&lt;-2; active&lt;-false; name&lt;-"foobar";]</pre>
* Record fields also support the function definition syntax sugar.
* Record elements can be accessed by label with the . operator, e.g. "r.x", "player.name".
* Variants are written like records, but the begin with a colon and an identifier: <pre>[:Vec2 x&lt;-1; y&lt;-2;]</pre>. Note the colon is not part of the tag.
* The dot operator does not work on variants, instead the case statement must be used.
* Generators look like functions, but instead of return they use the keyword yield. Repeated applications of a generator
continue after the last executed yield statement, unless it is a "yield done". Generators can only be used in generator statements:
<pre>fun(a,b,c)->yield a; yield b; yield c; yield done; end;</pre>
The above function however only constructs and returns the generator, which itself can be placed in a variable,
and then used in a generator statement.
* Lists, dictionaries and strings are all generators that range over the contents of the value. For lists the elements, for strings the characters, and for dictionaries records with labels key and value with the contents accordingly.

The maybe variant has type <pre>{[:None]+[:Some value:?Value]}</pre>
To actually use dictionary lookup, inspect the maybe variant with a case stament (both branches required):
<pre>
case d["a"] of 
Some value then workon(value);
| None then # throw a fit
end;
</pre>
That may look verbose, but it just forces the programmer to handle lookup failures.

## Other expressions (mostly binary and unary operators)
* Arithmetic (integer and real): <pre>+,*,-,/,%</pre>
* boolean: <pre>and, or, xor, not</pre>
* compares: <pre>==, !=</pre> (all types) and <pre>>, <, >=, <=</pre> (only string, integer, real)
* sequence (list and string) splice returns a subsequence based on the start and end index of the subsequence. Inside the square brackets, 
a special variable "last" holds the index of the last element in the input sequence : <pre>l[1:last]</pre> 
(new list without the first element)
* Assignment "&lt;-" (read: "is assigned") returns the value that was written to the destination on the left. E.g.: <pre>y&lt;-2</pre>

## Statements (must end with a semicolon):
* Every expression is a statement. No other statements return values.
* variable definitions are "var" followed by an identifier followed by "&lt;-" (read "is assigned") followed by an initializer expression.
Variables must always be initialized explicitly when defined: 
<pre>var x&lt;-3; 
var f&lt;-fun(a,b)->return a*a+b*b; end;</pre>
* Identifiers may consist of letters, digits and underscores, but may not begin with a digit. "abc", "_long_ident_344" are ok, "3d" is not.
(Quotes for readability only!)
* Some syntax sugar is provided for declarations of variables with function types, removing the "&lt;-fun":
<pre>var line(slope,offset)->return fun(x)->return slope*x+offset; end; end;</pre>
* Conditions use the common if...then...else with the last branch ending with an end, additional conditions use the single word elsif:
<pre>
if x==1 then # do someting
elsif x==2 then # do something else
else #give up
end;
</pre>
<pre>
if error then # panic
end;
</pre>
* while loops: <pre>while condition do work(); morework(); done;<pre>
* for loops: are basically a wrapper for generator statements and will be discussed there
* case statements are conditionals that branch not on a boolean condition, but on variant tags. The keyword case
is followed by an expression that must be a variant, and the word "of".
Each branch begins with a tag and is followed by labels that must exist in the variant with that label 
(otherwise there will be a type error). Those labels are bound to the corresponding fields in the variant. 
After the tag and labels comes the word "then", followed by the branch's statements. 
The branches are separated by pipe symbols "|":

> case variantExpr of 
> A x then return x; 
> | B a b then return a+b; 
> end;

The <tt>source</tt> statement tries to load a file and interpret it as a intro source file.
<pre>
	source "mysource.intro";
</pre>
If the file is found and successfully parsed the first time, it is type checked in a fresh global environment.
That means that it cannot be provided magically with data from the outside.
Then the code is executed, creating all symbols. These symbols are then all imported into the
current environment (global symbols only; modules are handled on their own)
After a file has been sourced successfully the first time, it is not loaded anymore,
instead the symbols created the first time are imported into the current scope,
which is more relevant in projects with multiple files.

Note the string passed to the source statement is static, string interpolation will not work.
It is not possible to select source files to load at runtime.

## Generator Statements
Generators can only be used in these statements, which are actually always part of another construct: 
in list or dictionary expressions, they provide the values that are turned into the contents of the containers.
The for statements just combines a generator statements with a list of statements that operate on each set of values generated.

Generator statements consist of statements that bind a variable (name) to a generator's result values using the infix keyword "in":
* <pre>x in somelist</pre> where x ha the type given for the list's elements
* <pre>char in string</pre> where char is a strying type with length exacty one.
* <pre>kv in somedict</pre> where kv is a record with labels key and value with the appropriate types geiven by the dictionary type.

The generator statements create new variables for the name to bind the values to, and this variable will hide
variables of the same name in the outer environments.

Special syntax sugar is provided for generating integer values:
identifier "from" startvalue "to" endvalue "by" stepvalue.
Where the values generated starts at start value, goes up to at most endvalue and is incremented by stepvalue. 
Stepvalue is optional and defaults to one, e.g. to generate all values from 1 to 10 in variable x:

>x from 1 to 10

Multiple values can be generated in one statement, each additional value begins with the operator "&&", and generator
applications (maybe better instantiation) can use the variables introduced to their left:

>i from 1 to 10 && y from x to 10 && z from y to 10

A generator value is moved to the next every time the generator to the left (if there) has completed a full cycle.
So the above example generates x=1, y=1, z=1, then z=2 and so on. Once z is 10, y is increased then z is reset to the new y.
It is probably easiest to type it into the REPL as a list to see the result:

>{ [a&lt;-x; b&lt;-y; c&lt;-z;] | x from 1 to 10 && y from x to 10 && z from y to 10};

Finally, anywhere after the first generator binding,  conditions can be introduced by the operator "??". Each condition
can use all generated values defined to its left. Any value combination for which the condition is false will be skipped, which
includes any generators to the right. So here is a list of right triangles, without isomorphism and with the hypotenuse
always in c (in the records in the resulting list):

>{ [a&lt;-x; b&lt;-y; c&lt;-z;] | x from 1 to 10 && y from x to 10 && z from y to 10 ?? x * x + y * y == z * z};

The for statement consists of the word "for" followed by a generator statement followed by "do", then a list of statements
that will be executed for each group of values generated, and then the keyword "done":
<pre>for x in somelist do process(x); done;</pre>

# Modules
(Preliminary documentation, may change with bug fixes and some details...)

Modules aim to support large scale programming, in Intro they serve two purposes:
* the simpler one is providing a hierarchical storage for variables, similar to file systems for files.
* the second is providing information hiding. This is done by a) defining an interface the module can be accessed through and b) providing a wa to define opaque types.
A script may contain multiple module definitions. If a module in another file is to be used, that file must first be loaded (that is TBD).

Modules have a path, much like files in file systems, but double colons "::" are used as separators (spaces between identifiers and separators are ignored).

Example relative paths:

> Pair, foo::bar

Example absolute paths:

> ::sio, ::gui::controls::buttons, 

The module at the root module is also called the global scope, and is written simply as a double colon without a prefix. 
All variables defined outside any other module or scope are placed in here, and it is always accessible.
If the path begins with an identifier, it is relative to the current module. 
The path may end with a module name where a module is expected, or with a member of the modules interface.
The last identifier in a path may refer to a module or a variable, that is context dependent: 
in an import statement or module definition, it ends with a module - in expressions it is a prefixed variable.

Modules make some or all variables defined in their body accessible to the outside by declaring them as part of their exports,
the exports make up the module'S interface. The values in a module are also called members to distinguish them from global 
or local variables. The interface only names the value and it's type.

## Defining Modules
The syntax for modules is:
<pre>
"module" path "exports"
    interface
"from"
    body
"end."
</pre>
* Note the fact that Modules end with a period "." after the closing "end", NOT a semicolon ";"! This
distinguishes the end of a long module from another statement.
* Also, modules can be placed in other modules via the path in their name.

The interface of a module is the only place in Intro where types are explicitly written out: 
it contains names of variables in the module and their type (or a super-type) of the variable 
with the same name in the body of the module. Name and type are separated by a colon ":"
just like when the REPL prints out a variable type, e.g. fib:(Integer)->Integer.

The body of a module is just a sequence of statements that are executed when the module is defined.
Any variables defined inside persist, but only those named in the interface are exposed.

Interfaces may also define opaque types, i.e. types where the implementation is not known outside the module.
An opaque type definition begins with the keyword "type", a name for the type and zero or more
parameters for the type in parentheses (just like in Dictionary(?A&lt;:Top,?B&lt;:Top)). A type with parameters is called polymorphic.
If no parameters are used the type is called monomorphic.In that case the parentheses can be omitted. 

?? The type can be followed by a subtype-of symbol "&lt;:" and the opaque type's 
super-type. This can be used to make part of the type known, e.g. a subset of the fields in the record
that implements the type. ??

Opaque types may have constructors which are functions with the same name as the type and a parameter for any parameter the
type has. This function must be in the body of the exporting module. A constructor must return a value of the opaque type's 
implementation, and the opaque type's parameters must match the constructor's parameters. If such a constructor is present,
the opaque type can be created by using it's name like a function. 
The constructor also helps the interpreter determine the relation between the type parameters and the values in the implementation.

?? If there is no one-to-one mapping between the type's parameter(s) and the constructors 
desired parameters, then the constructor short hand cannot be used. 
This can happen for example with 3D vectors which have three numbers of the same concrete type. ??

NOTE TO ORY: how about using the opaque type's super-type, e.g. Vector3(?a&lt;:Number)&lt;:[x:?a;y:?a;z:?a;]!
May work, but seems kludgey...

## Example Module
Here is a module that defines a pair of values in a type Pair.
It has two parameters representing the types of the two values in the pair. They are written without abound, e.g.
only ?a instead of ?a<:Top. If no bound is given, it defaults to Top.
There are also a few operations defined on Pairs:
<pre>
module ::Pair exports
	# The opaque type that represents the pairs.
	type Pair(?a,?b);
	# first is a function that takes a pair and returns the first element of the passed pair (note return type).
	first:(Pair(?a,?b))-> ?a;
	# second is a function that takes a pair and returns the second element of the passed pair (note return type).
	second:(Pair(?a,?b))-> ?b; 
	# swap returns a new pair, which has the same contents as the parameter, but in reversed order.
	# Compare the parameters of the input and output pairs!
	swap:(Pair(?a,?b))->Pair(?b,?a); 
from 
	# he constructor for pairs, it has type Pair:(a:?a,b:?b)->Pair(?a,?b).
	var Pair(a,b)->
		return [first&lt;-a;second&lt;-b;]; 
	end; 
	# implementation of function first.
	var first(a)-> 
		return a.first; 
	end; 
	# implementation of function second.
	var second(a)-> 
		return a.second; 
	end; 
	# implementation of function swap, which does not use constructor but could/should.
	# Note that in-place swapping of pairs is only possible if both elements have the same type,
	# so the creation of a new pair here enables a more general operation at the cost of speed.
	var swap(a)->
		return [first&lt;-a.second;second&lt;-a.first;]; 
	end; 
end.
</pre>

In this example, the Pair type is implemented as a record type [first:?a&lt;:Top; second:?b&lt;:Top;].
The module is also implemented in ./examples/pair.intro.

## Importing from Modules
While any interface member can always be named with an absolute path, this can become cumbersome. 
The import statement is provided to copy a modules interface (all of it) into the current scope, e.g.

> import ::sio;

The import statement can be used anywhere, and only the current scope is extended. That means modules' interfaces can be imported
for example into a function or loop body, and not clash with identifiers outside.

## Writing Run Time Library Modules
Several helper macros are defined in header LibBase.h, which help define both
static closures with run-time types for module members
and second register the library modules with the type inference and code generation environments
(This makes the addresses known to the interpreter).

The modules must also provide Intro Types (as opposed to e.g. LLVM Types) for all exported members, 
so that type inference has something to work with when checking the module.

# ::sio (Simple Input Output)

This is the first module of the run time library (RTL). It provides some very simple input output functions
designed for ease of use.
It also servers as a very simple example for implementing RTL modules, even though no types are defined in it.

## print
The function 

> ::sio::print(data) : (?a<:Top)->Unit

takes any value as input, turns it into a string and displays the result.
It does not return a value, only performing a side effect.

An example run in interpreter mode:
<pre>
&gt; ::sio::print("Hello World\n");
Hello World
&gt;
</pre>
Note there is no equal sign in the output, as this was not printed by the REPL.

## read
The function 

> ::sio::read() : ()->String

reads one line of text from the terminal, and returns it as a string. It has no parameters,
and can only read from the main keyboard.

An example run in interpreter mode:
<pre>
&gt; ::sio::read();
Hello World!
 = Hello World!
&gt;
</pre>
Note the line that says only "Hello World!" was entered after ::sio::read was executed.
The line with an equal sign in front was the return value of read, printed by the REPL.

## saveFile
The function 

> ::sio::saveFile(path,data) : (String,?a<:Top)->Boolean

converts data to a string and stores the result (UTF-8 encoded) in the file at path (which wil be created or cleared).
It returns true on success, and false if any error occurred. Usage is dead simple:
<pre>
&gt; ::sio::saveFile("xfiles.txt","Scully and Moulder where here!");
 = true
&gt;
</pre>


## loadFile
The function 

> ::sio::loadFile(path) : (String)->{[:None] + [:Some value:String]}

opens the file named in path which is expected to be an existing UTF-8 txt file.
It reads the entire file and converts it into a single string.
It returns the maybe variant: if no errors occurred, the "Some" tag contains the created string,
otherwise the "None" tag is returned. (An existing empty file returns an empty string.)

An example run in interpreter mode:
<pre>
&gt; var x<-::sio::loadFile("xfiles.txt");
x:{ [ :None ] + [ :Some value : String; ]}
&gt; x;
 = [ :Some value<-Scully and Moulder where here!; ]
&gt;
</pre>

In most cases, the None variant if returned if the file was not found.

# THE END...

Questions for Ory:

* Does case allow mutating a variant? could be useful - currently not, the bound variant eements are merked as constants!
* If modules can access anything already defined in lower modules, how does that interacting with sourcing scripts inside the module?
. E.g. it may be possible to have the same code use different modules as it's base implementation, if the mdules interface matches...
. Like using different logics to implement a planning and reasoning algorithms...