# Compiler Process  
Here are the general steps the compiler takes when given a file:

## Lex -> Parse -> Generate (assembly)  

### Lexing
The lexing stage is very straightforward. Unless the compiler comes across a symbol that could be used in a compound symbol (think something like `+=` or `->`), it pushes a token into a vector that contains the token's type (enumerator), an optional value (std::string), as well as its line # and the column it begins at. 

### Parsing
The parser is far more complex. Using a data structure known as an Asymetric Syntax Tree (AST), it creates a sort of tree that looks similar to this:
```
Program
  Function: main -> INT
    Body:
      Println:
        StrLit: Hello world!
      Return
        IntLit: 0
```
When coming to something like a Binary Expression or a Logical Operation, the tree grows using an operator precedence.
Something like `if (a == b && b != c)` will turn into:
```
If
   Condition:
      Logic: AND
         Cmp: EQUAL
            Ident: a
            Ident: b
         Cmp: NOT_EQUAL
            Ident: b
            Ident: c
   Then:
         { Body }
```
This is a similar process for `for` loops as well, `for (have i = 0; i < 10; i++)` becomes:
```
For
   Init:
      Have: i
         IntLit: 0
   Condition:
      Cmp: LESS_THAN
         Ident: i
         IntLit: 10
   Increment:
      StmtExpr:
         IncDec:
            Ident:  i
            Mode:   ADD
            Prefix: False
```

Statements and Expressions all get lumped under a generic Node* using c++'s `std::variant`.  
To avoid the slowdown that would inevitably come from so many `new` / `delete` calls, I chose to employ an arena allocator, which allocates about 4MB of memory at the start, and then allocates new chunks as needed using `std::max` between the provided amount (4MB in this case) or the size of the type attempting to gain a pointer.

### Code Generation  
This part acts by following the tree down from the top. Initially, it emits a few protocols, such as the ones used for `println` and `readi`. It also emits a variety of constants and preallocates buffers for reading and writing.  
The generator also utilizes the `std::visit` that comes with `std::variant` to quickly match each Node type and emit the translated assembly code.  
Each step is written to an `std::sstream`, which then is returned as an `std::string` after completion.


### Syscaller (MSC).
The Syscaller---which as you'd expect---handles system calls. It is the final step between the `.z` file and the finished executable. This class calls `nasm`, then `ld`, then `rm` to remove the `.asm` and `.o` files (unless otherwise specified).