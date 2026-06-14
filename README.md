# JS Interpreter in C++

<div align="center">

**A JavaScript interpreter built from scratch in C++17 — no external libraries, no embedded JS engines.**

*Built for Thunder Hackathon 2.0 · Challenge: Build Your Own JavaScript Runtime*

</div>

---

##  What is this?

This project is a **complete JavaScript interpreter** written in a single C++ file (`js_interpreter.cpp`).  
It takes JavaScript source code as input, executes it, and prints the correct output — just like Node.js would — but built entirely from scratch using only the C++ standard library.

The interpreter implements:
- A hand-written **Lexer** (tokenizer)
- A **Recursive-Descent Parser** that builds an Abstract Syntax Tree (AST)
- A **Tree-Walking Interpreter** that executes the AST

---

## Test Results

| # | Test Case | Points | Status |
|---|-----------|--------|--------|
| TC-1 | Odd / Even Checker | 20 | PASS |
| TC-2 | Triangle Pattern using For Loop | 20 | PASS |
| TC-3 | Armstrong Number | 20 |  PASS |
| TC-4 | Array Reverse | 20 | PASS |
| TC-5 | String Palindrome Check | 20 | PASS |
| | **TOTAL** | **100 / 100** | 

---

##  Quick Start

### Requirements
- C++17 compatible compiler (`g++`, `clang++`, or MSVC 2017+)
- No external libraries needed

### Build

```bash
g++ -std=c++17 -O2 -o js_interpreter js_interpreter.cpp
```

### Run

```bash
# Run a JavaScript file
./js_interpreter script.js

# Or pipe JS code directly via stdin
echo "console.log('Hello, World!');" | ./js_interpreter
```

---

## Running the Official Test Cases

Save each snippet as a `.js` file and run it:

**TC-1 — Odd/Even Checker**
```js
let num = 7;
if (num % 2 === 0) {
  console.log(num + " is Even");
} else {
  console.log(num + " is Odd");
}
// Output: 7 is Odd
```

**TC-2 — Triangle Pattern**
```js
for (let i = 1; i <= 5; i++) {
  let row = "";
  for (let j = 1; j <= i; j++) {
    row += "*";
  }
  console.log(row);
}
// Output:
// *
// **
// ***
// ****
// *****
```

**TC-3 — Armstrong Number**
```js
function isArmstrong(num) {
  let temp = num, sum = 0;
  while (temp > 0) {
    let digit = temp % 10;
    sum += digit ** 3;
    temp = Math.floor(temp / 10);
  }
  return sum === num;
}
console.log(isArmstrong(153)); // true
console.log(isArmstrong(123)); // false
```

**TC-4 — Array Reverse**
```js
let arr = [1, 2, 3, 4, 5];
let reversed = [...arr].reverse();
console.log("Original: " + arr.join(", "));
console.log("Reversed: " + reversed.join(", "));
// Output:
// Original: 1, 2, 3, 4, 5
// Reversed: 5, 4, 3, 2, 1
```

**TC-5 — String Palindrome**
```js
let str = "racecar";
let reversed = str.split("").reverse().join("");
if (str === reversed) {
  console.log(str + " is a Palindrome");
} else {
  console.log(str + " is not a Palindrome");
}
// Output: racecar is a Palindrome
```

---

## Project Structure

```
js-interpreter-cpp/
├── js_interpreter.cpp   ← Entire interpreter (~2642 lines, single file)
└── README.md
```

Internal layout of `js_interpreter.cpp`:

| Lines | Component | Description |
|-------|-----------|-------------|
| 1 – 240 | **Lexer** | Tokenizes JS source into 60+ token types |
| 241 – 380 | **AST Nodes** | NodeType enum + ASTNode struct |
| 381 – 1150 | **Parser** | Recursive descent, all grammar rules |
| 1151 – 1300 | **Value System** | Value, JSArray, JSObject, JSFunction |
| 1301 – 1380 | **Environment** | Scope chain, closures, variable lookup |
| 1381 – 2600 | **Interpreter** | execNode, evalExpr, getProp, built-ins |
| 2601 – 2642 | **main()** | File / stdin entry point |

---

## How It Works

The interpreter follows the classic three-stage pipeline:

```
JavaScript Source Code
        │
        ▼
  ┌───────────┐
  │   LEXER   │  →  Tokens: [LET] [IDENTIFIER:'num'] [ASSIGN] [NUMBER:7] ...
  └───────────┘
        │
        ▼
  ┌───────────┐
  │  PARSER   │  →  Abstract Syntax Tree (AST)
  └───────────┘       Program
        │               ├── VarDecl (num = 7)
        │               └── IfStmt
        │                     ├── BinOp (num % 2 === 0)
        │                     ├── CallExpr (console.log)
        │                     └── CallExpr (console.log)
        ▼
  ┌───────────────┐
  │ INTERPRETER   │  →  Walks AST, manages scope, executes nodes
  └───────────────┘       → prints "7 is Odd"
```

### Stage 1 · Lexer
Reads source character-by-character. Produces typed tokens. Handles strings (`"`, `'`, `` ` ``), numbers (integer, float, hex `0xFF`), all operators (`===`, `**`, `...`, `=>`), keywords, identifiers, and skips whitespace and comments (`//` and `/* */`).

### Stage 2 · Parser
Recursive-descent parser. Each grammar rule is a function that calls lower-precedence rules, naturally encoding operator precedence. Builds an AST of `ASTNode` structs linked by `shared_ptr`. Handles:
- Arrow function detection (speculative parse + backtrack)
- `for...of` / `for...in` lookahead
- Function hoisting (pre-scan each block)
- Destructuring, spread/rest, object shorthand

### Stage 3 · Interpreter
Tree-walking interpreter with two main methods:
- `execNode()` — handles statements (loops, if, try/catch, declarations…)
- `evalExpr()` — handles expressions (operators, function calls, member access…)

Control flow (`return`, `break`, `continue`, `throw`) is propagated using C++ exceptions as signals.

---

## Supported JavaScript Features

### Variables & Types
- `let`, `const`, `var` — with proper block scoping
- Types: `number`, `string`, `boolean`, `null`, `undefined`, `object`, `array`, `function`
- Destructuring: `let [a, b] = arr` and `let { x, y } = obj`

### Operators
- Arithmetic: `+`, `-`, `*`, `/`, `%`, `**`
- Comparison: `==`, `!=`, `===`, `!==`, `<`, `>`, `<=`, `>=`
- Logical: `&&`, `||`, `!`
- Bitwise: `&`, `|`, `^`, `~`, `<<`, `>>`
- Assignment: `=`, `+=`, `-=`, `*=`, `/=`, `%=`
- Other: `typeof`, `instanceof`, `in`, ternary `? :`, spread `...`, `++`, `--`

### Control Flow
- `if` / `else if` / `else`
- `for`, `while`, `do...while`
- `for...of`, `for...in`
- `switch` / `case` / `default` / `break`
- `break`, `continue`
- `try` / `catch` / `finally`, `throw`

### Functions
- Function declarations (hoisted)
- Function expressions
- Arrow functions: `x => x * 2` and `(a, b) => a + b`
- Arrow functions with block body: `x => { ... return x; }`
- Closures, recursion
- Rest parameters: `function f(...args)`
- Callback functions

### Array Methods
`push` `pop` `shift` `unshift` `reverse` `sort` `join` `slice` `splice` `concat`  
`indexOf` `includes` `find` `findIndex` `forEach` `map` `filter` `reduce`  
`some` `every` `flat` `fill`

### String Methods
`split` `join` `replace` `replaceAll` `toUpperCase` `toLowerCase` `trim`  
`trimStart` `trimEnd` `includes` `startsWith` `endsWith` `slice` `substring`  
`substr` `indexOf` `lastIndexOf` `charAt` `charCodeAt` `repeat` `padStart` `padEnd`

### Objects
- Object literals `{ key: value }`
- Method shorthand `{ greet() { ... } }`
- Object spread `{ ...obj, newKey: val }`
- Property access (dot & bracket notation)
- `Object.keys()`, `Object.values()`, `Object.entries()`, `Object.assign()`

### Built-in Globals

| Global | What's supported |
|--------|-----------------|
| `console` | `console.log()`, `console.error()` |
| `Math` | `floor` `ceil` `round` `abs` `sqrt` `pow` `min` `max` `random` `sin` `cos` `tan` `log` `log2` `log10` `trunc` `sign` `hypot` + `Math.PI`, `Math.E` |
| `Number` | `Number()`, `Number.isNaN()`, `Number.isFinite()`, `Number.isInteger()`, `Number.parseInt()`, `Number.parseFloat()` |
| `String` | `String()` conversion |
| `Boolean` | `Boolean()` conversion |
| `Array` | `Array(n)` constructor |
| `Object` | `keys` `values` `entries` `assign` |
| `Error` | `new Error(msg)` → `.message`, `.name` |
| `Date` | `new Date()` → `getTime`, `getFullYear`, `getMonth`, `getDate`, `toString` |
| `parseInt` / `parseFloat` | Global conversion functions |
| `isNaN` / `isFinite` | Global checks |
| `undefined`, `NaN`, `Infinity` | Global constants |

---

## Example: Closures Work

```js
function counter() {
  let count = 0;
  return function() {
    count++;
    return count;
  };
}

const c = counter();
console.log(c()); // 1
console.log(c()); // 2
console.log(c()); // 3
```

## Example: Higher-Order Functions

```js
const nums = [1, 2, 3, 4, 5, 6];

const result = nums
  .filter(n => n % 2 === 0)
  .map(n => n * n)
  .reduce((acc, n) => acc + n, 0);

console.log(result); // 56
```

## Example: Recursion

```js
function fib(n) {
  if (n <= 1) return n;
  return fib(n - 1) + fib(n - 2);
}
console.log(fib(10)); // 55
```

---

## Design Decisions

**Why a tree-walking interpreter?**  
Simpler to implement correctly than bytecode compilation, and fast enough for the hackathon's test cases.

**Why C++ exceptions for control flow?**  
`return`, `break`, `continue`, and `throw` all need to unwind arbitrary call depth. Using C++ `throw`/`catch` lets the recursive tree-walker unwind naturally without threading flags through every function.

**Why shared_ptr everywhere?**  
Values can be shared across closures and the environment chain. Reference counting via `shared_ptr` avoids dangling pointers without a garbage collector.

**Why a single file?**  
The hackathon submission is simpler as one `.cpp` file — no build system, no headers, just `g++ -o js_interpreter js_interpreter.cpp`.

---

## License

MIT License — free to use, modify, and distribute.

---

<div align="center">

Made with ❤️ for **Thunder Hackathon 2.0**  
*Challenge: Build Your Own JavaScript · Language: C++17*

</div>
