This is the Java implementation for the language, as a Tree-Walk interpreter

# Build Instructions

## Prerequisites
- OpenJDK 21 or higher

## Building
```bash
./gradlew build
```

The compiled jar can be found at `app/build/libs/app.jar`

## Running
```bash
./gradlew run
```

# Language Tour

## Table of Contents
- [Types](#types)
- [Operations Supported](#operations-supported)
  - [Unary Operators](#unary-operators)
  - [Binary Operators](#binary-operators)
  - [Comparison and Equality](#comparison-and-equality)
  - [Logical Operators](#logical-operators)
  - [Bitwise Operators](#bitwise-operators)
  - [Other Operators](#other-operators)
- [REPL](#repls)
  - [Launching](#launching)
  - [Exiting](#exiting)
  - [Expressions and Statements](#expressions-and-statements)
  - [Errors](#errors)
- [Built-in Functions](#built-in-functions)
- [Truthiness](#truthiness)
- [Comments](#comments)
- [Output](#output)
- [Variables](#variables)
- [Conditionals](#conditionals)
- [Loops](#loops)
- [Scopes and Blocks](#scopes-and-blocks)
- [Functions & Lambdas](#functions--lambdas)
- [Classes](#classes)
- [Inheritance](#inheritance)
- [Mixins](#mixins)
- [Method Modifiers](#method-modifiers)

### Types
This language is dynamically typed, with basic types like:
1. **Strings**:
```
"Hello, World"
"h"
```
> [!NOTE]
> Escape sequences are not supported. Instead of using `\n` for newlines, use an actual newline at the source.
> ```
> "Actual
> Newline"
> ```

2. **Numbers**:
Numbers are represented internally as Java Doubles.
```
1
-6
2.4
-6.7
```
> [!NOTE]
> Numbers like .5 are not valid. Numbers must start with a digit, like 0.5.

3. **Booleans**:
Literal true and false values.
```
true
false
```
4. **Nil**:
Represents a null or absent undefined value via `nil`
5. **Class**:
Objects representing class definitions.
```
class Hello {
}
// prints "<class Hello>"
print Hello;
```
6. **Instances**:
Instances of classes holding dynamic fields and methods.
Created by calling a class with ().
```
class Hello {
}
// prints "<Hello instance>"
print Hello();
```
7. **Functions**:
Named functions, anonymous lambdas, and builtin-natives are also first-class types.
```
fn add(a, b) {
    ret a + b;
}
// prints <fn add>
print add;

let add = fn (a, b) {
    ret a + b;
};

// prints <fn>
print add;

// prints 3
print add(1, 2);
```

Collection types like Arrays and Hashmaps are not yet supported in this language.

---

### Operations Supported

#### Unary operators
1. `+` Unary plus. Valid only on numbers.
2. `-` Unary minus. Also valid only on numbers.
3. `!` Logical NOT. Converts the operand to a boolean if it isn't already and negates it based on [truthiness](#truthiness).
4. `~` Bitwise NOT. Converts the operand to a long integer, applies the NOT operation and returns a double.
```
// 5
+5

// -5
+-5

// -5
-5

// 5
--5

// false
!true

// true
!nil

// -2
~1
```

#### Binary Operators
- `+`: Adds two numbers. If the left operand is a `string`, it coerces right which can be of any type to a string.
- `-`: Subtracts two numbers.
- `*`: Multiplies two numbers.
- `/`: Divides two numbers. Throws a runtime error on division by zero.

```
// 15
10 + 5

// "hello5"
"hello" + 5

// "hellotrue"
"hello" + true

// "hellonil"
"hello" + nil

// 5
10 - 5

// 50
10 * 5

// 2
10 / 5

// error: Division by zero
// 10 / 0
```

#### Comparison and Equality
- `==`, `!=`: Equality checks using Java's `.equals()`.
- `<`, `>`, `<=`, `>=`: Lexicographical comparison if both operands are strings. Otherwise, non-double operands are converted via their length (strings) or 0.0, then compared as doubles.

```
// true
5 == 5

// false
"apple" == "orange"

// true
"apple" != "orange"

// true
"apple" < "banana"

// true
"apple" > 3

// false
nil >= 2.3
```

#### Logical Operators
- `&&`: Short-circuiting logical AND.
- `||`: Short-circuiting logical OR.

`&&` and `||` return one of their operands, not a boolean. `&&` returns the left operand if it is falsy, otherwise returns the right operand. `||` returns the left operand if it is truthy, otherwise returns the right operand. They work naturally in conditions, since the returned value is still evaluated for truthiness. It also makes it so that you can use idioms like `value || "default"` for fallbacks.

```
// false
false && true

// true
true || false

// nil, short-circuits, returns left operand which is nil
nil && "ignored"

// 2, both truthy, returns right operand.
1 && 2

// "fallback"
false || "fallback"
```

Short circuiting is when the program stops evaluating the rest of the condition. `&&` short-circuits when the left operand is falsy. `||` short-circuits when the left operand is truthy.


#### Bitwise Operators
All bitwise operators convert their operands to long integers before operating, and return a double.
- `&`: Bitwise AND.
- `|`: Bitwise OR.
- `^`: Bitwise XOR.
- `<<`: Left shift.
- `>>`: Signed right shift.
- `>>>`: Unsigned right shift.

```
// 1
5 & 3

// 7
5 | 3

// 6
5 ^ 3

// 20
5 << 2

// 2
10 >> 2

// 3
-1 >>> 62
```

#### Other Operators
- `? :` Ternary conditional expression. Evaluates the expression before `?`. If true, returns the expression after `?`, else, returns the expression after `:`
- `,`: Comma operator. Evaluates the left expression, discards it, and returns the right expression. This isn't available inside function call argument lists, `foo(a,b)` always means two arguments and **NOT** a comma expression.
- `.`: Property access on instances and classes.

```
// "yes"
true ? "yes" : "no"

// "no"
false ? "yes" : "no"

// 3
(1, 2, 3)

// "value"
instance.field = "value"

// <fn method>
instance.method
```

---

### REPLs
The REPL (Read-Eval-Print Loop) is an interactive prompt for running code line by line.

#### Launching
Run the interpreter with no arguments:
```bash
java -jar app.jar
```

You'll see a `>` prompt ready for input.

#### Exiting
Type `exit` (case-insensitive) to quit. Or press one of the following:
- CTRL+D (Recommended)
- CTRL+C
- CTRL+Z

#### Expressions and Statements
In REPL mode, evaluating an expression alone automatically prints its stringified result to stdout. Declaring variables, functions, and classes work normally.
```
> let x = 10;
> x;
10
> x + 5;
15
> "Hello " + "World";
Hello World
```

> [!NOTE]
> Declaration statements like `let`, `fn` and `class` produce no output on their own. Only expressions auto-print

#### Errors
Errors don't kill the session. After an error the REPL recovers and continues accepting input.

```
> 10 / 0;
error: Division by zero
  --> <stdin>:1:4
  |
1 | 10 / 0;
  |    ^
> 10 / 0
error: Expect ';' after expression.
  --> <stdin>:1:6
  |
1 | 10 / 0
  |      ^
>
```

---

### Built-in functions
`clock()`: A function that takes 0 arguments. It returns the current system time in seconds as a number. Useful for benchmark performance testing.

---

### Truthiness
`nil` and `false` are falsey. Everything else is truthy.

---

### Comments
Single-line comments use `//`, Multi-line comments use `/*` to open and `*/` to close.
```
// This is a single-line comment

/* This is a
   multi-line comment */

/* Multi-line comments /* can be nested */ in this language */
```
Nested multiline comments make it safe to comment out large code blocks even if they have existing comments.

---

### Output
Use the print statement to output values to standard output followed by a newline.
```
// Hello, World!
print "Hello, World!";

// 15
print 5 + 10;
```

---

### Variables
Declare variables using the `let` keyword. Variables without an explicit initializer automatically default to nil.

> [!NOTE]
> If you try to reference uninitialized variables, it will throw an error. Unused local variables will also throw errors.

```
let x = 10;
let name = "Squirrel";
let uninitialized;

// error: Cannot reference uninitialized variable 'uninitialized'.
// print uninitialized;

// 10
print x;

// Squirrel
print name;

uninitialized = "Initialized";

// Initialized
print uninitialized;

fn unused() {
    // error: Unused variable.
    // let unused;
}
```

---

### Conditionals
Use `if` and `else` for branching logic. Paranthesis around the condition expression are required, just like in C.

You can also use [ternary operators](#other-operators)
```
let score = 85;
// B
if (score >= 90) {
    print "A";
} else if (score >= 80) {
    print "B";
} else {
    print "F";
}

// Failure
print score >= 90 ? "A" : "Failure";
```

---

### Loops
The language supports `while` loops, C-style `for` loops, and the `break` statement to exit loops prematurely.

The language does not support `continue`
```
let i = 0;
// 0
// 1
// 2
while (i < 3) {
    print i;
    i = i + 1;
}

// 0
// 1
for (let j = 0; j < 5; j = j + 1) {
    if (j == 2) {
        break;
    }
    print j;
}

// error: Must be in loop to use 'break'.
// break;
```

---

### Scopes and Blocks
Curly braces `{}` define a block statement and introduce a new lexical scope level. Variables declared in an inner scope shadow the variables in outer scopes.

```
let volume = 10;

{
    let volume = 20;
    // 20
    print volume;
}

// 10
print volume;
```

You cannot declare a variable with the same name twice within the exact same local scope block. Doing so triggers an immediate compilation error.

> [!TIP] Closures
>
> Functions capture (or close over) the variables available in their surrounding lexical scope at the time they are defined. This environment stays alive with the function even after the outer scope finishes executing.
> ```
>fn makeCounter() {
>    let count = 0
>    fn counter() {
>        count = count + 1
>        ret count
>    }
>    ret counter
>}
>
> let c = makeCounter()
>
> // 1
> c()
>
> // 2
> c()
> ```

---

### Functions & Lambdas
Functions are first-class. They can be passed as arguments, returned from other functions, and stored in variables. Parameters are limited to a maximum of 255. Functions return `nil` by default, unless an explicit `ret` statement is hit.

Lambdas are anonymous functions defined inline as expressions.

```
fn makeAdder(x) {
    fn adder(y) {
        ret x + y;
    }
    ret adder;
}

let addFive = makeAdder(5);
// 12
addFive(7)

let lambdaMultiply = fn(a, b) {
     ret a * b;
};
// 20
lambdaMultiply(4, 5)
```

---

### Classes
Classes hold methods, or blueprinted behaviors, but instances hold data or fields dynamically. Fields are created instantly upon assignment.

The special `init` method is the constructor, which automatically returns `self`. Explicit `ret` with a value inside init is an error. A bare `ret;` with no value is allowed and exits the initializer early.

> [!NOTE]
> Using modifiers like `static` or `private` on `init` is not allowed and will error.

`self` references the current instance object. It allows methods to read or update fields on that specific object. You cannot use `self` outside a class or inside a static method.

```
class Person {
    init(name) {
        self.name = name;
    }

    greet() {
        print "Hi, I am " + self.name;
    }
}

let user = Person("Bob");
// Hi, I am Bob
user.greet()

user.age = 25;
// 25
user.age
```

---

### Inheritance
Single inheritance uses a colon (`:`). A subclass inherits all methods from its parent class. A class cannot inherit from itself, and multiple inheritance is not supported.

`base` lets a subclass call an overridden method from its parent class using `base.methodName()`. You cannot use `base` outside a class, inside a static method, or if the class doesn't have a parent class.

```
class Animal {
    makeSound() {
        print "Animal sounds";
    }
}

class Dog : Animal {
    makeSound() {
        base.makeSound();
        print "Bark!";
    }
}

let dog = Dog();
// Animal sounds
// Bark!
dog.makeSound()
```
---

### Mixins
Mixins let you pull methods from other classes into a subclass using the `with` keyword. All non-private methods from the mixin classes are copied directly into the target class definition. A class cannot mix in itself.

If multiple methods with the same name are defined in every mixin, the last mixin with that method wins.

You can combine single inheritance with mixins.
If a mixin method has the same name as a method from the inherited base class, the mixin method wins.

```
class Logger {
    log() {
        print "Logging from Logger";
    }
}

class Debugger {
    log() {
        print "Logging from Debugger";
    }
}

class Machine with Logger, Debugger {}

class Do {
    log() {
        print "Doing something";
    }
}

class V1: Machine with Do {}

let machine = Machine();
// Logging from Debugger
machine.log();

let supremeMachine = V1();
// Doing something
supremeMachine.log();
```

---

### Method Modifiers
Methods can alter their scope, access, and invocation requirements using prefixes.

- `static`: Binds a method directly to the class instead of its instances. Static methods cannot reference `self` or `base`.
- `private`: Restricts the invocation only to the defining class. Private methods are ignored during mixin operations and cannot be called from subclasses or instances.
- Getters: Methods declared without parameter paranthesis. They evaluate instantly when the property name is accessed, removing the need for `()` tokens.
Getter methods are designed for read-only calculated properties.

> [!NOTE]
>Variable assignments and property assignments like `self.x = ...` inside a getter body is forbidden. However, calling a mutating method within the getter method is not blocked.

Modifiers can be combined. So something like `static private method() {}` is allowed.

The method is bound to the class and only callable within another method of the same class via `ClassName.method()`.

Note that calling `ClassName.method()` from toplevel code will fail the privacy check.

You can go a step further and use `static private method {}` using all three modifiers together.

```
class MathUtils {
    static square(x) {
        ret x * x;
    }
}

// 16
MathUtils.square(4)

class Circle {
    init(radius) {
        self.radius = radius;
    }

    // private getter
    private pi {
        ret 3.14159;
    }

    area {
        ret self.pi() * self.radius * self.radius;
    }
}

let c = Circle(5);
// 78.53975
c.area
```
