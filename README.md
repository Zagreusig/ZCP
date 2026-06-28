# Z Lang!
This is a WIP compiler for my own coding language! It's a sort of amalgamation of various other languages.  
Inside of the *semantics* directory, there's a vague overview of how the program is parsed and compiled, with just about every possible use (there could be a few missing if I forget.....)


## Current functionality

### Functions:  
Currently, functions can only accept and return integers, and have a cap of 6 arguments.  
Declare a function using either `fn` or `func` keywords. Optionally, you are able to specify what type your function returns with either the `:` symbol or the `->` symbol. 
```
fn foo() {}
func bar(int x): int {}
fn example(int a, int b, int c) -> int {}
``` 

### Conditions:  
Boolean and logical conditions are both supported, as well as if statements.
- `!=` (Solo not operator not yet supported)
- `||`
- `&&`
- `>`/`>=`
- `<`/`<=`
- `==`
```
if (a == b && b != c)
```

### Increment / Decrement and Compound operators:  
Support for both prefix and suffix increment/decrement are both supported, as well as all binary operations. All operators in this section are supported inside of for loops.
- `+=`
- `-=`
- `*=`
- `/=`
- `i++` / `++i`
- `i--` / `--i`

### Loops:  
`for` and `while` loops are both supported, with plans to implement `do / while` in the future.
```
for (have i = 0; i <= 10; i++)
```

### Variables:  
Currently, only integers and single characters are supported when declaring a variable, although you can create stack allocated arrays. You can do so like the example below.  
```
have x = 9;
have y = 'z';
have arr: int[5];
have arr2 = ['h', 'i'];
```
Small note, you can declare an empty array---which the memory get's 0'd for---however you must annotate the type.

### Baked in functions:  
You are able to use print and read functions to interact with the console.
- `print` - Prints whats inside it.
- `println` - Same as above, appends a newline.
- `readc` - Reads a single character, leaves newline in buffer.
- `readi` - Reads an integer, consumes newline.
```
have x = readi();
have y = readc();
println("This is a sentence!");
println(x);
println(readi());
```

# Planned Features:
1. Strings
2. If / while / for not requiring a {} pair
3. #define
4. #include & headers
5. structs -> later classes
6. Ternary operators

# NOTE:  
I'm planning to implement an optimization pass after the assembly code generates. I'm using the stack to move things between calls and such,
which internally always has the evaluator push and the caller pop. This will (often times) generate useless instruction blocks. Such as a function doing:
"mov rax, rbx
 push rax
 pop rax
 mov rbx, rax"
This will use a technique called a "peephole" pass. It will look at 2-3 adjacent instructions to rewrite or remove wasteful patterns / instructions.


# Compiler Flags:
You are able to combine multiple flags into one, or keep them as separate statements.
```
./zcp -f -astj -o {exec name} myfile.z
```
- `-h` Displays help (there needs to be a file ending in .z at the moment).
- `-a` Leaves the generated assembly file.
- `-j` Leaves the generated object file.
- `-t` Prints the lexer's tokens `{ TOKENTYPE, \<value>, line:col }
- `-s` Prints a visual representation of the parser's AST
- `-f` Prints the flags passed to the program
- `-o` Declare the name of the final executable `./zcp -o myProg file.z`

# Compiler Warnings:  
I tried to create a similar style of error to that of g++ / gcc, however the error it points to is usually the statement AFTER the actual issue.  
**NOTE:** There are currently no *warnings*, only errors.