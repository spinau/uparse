### uparse - a tiny parsing utility package

A set of macros and functions to provide a consistent and readable framework for defining 
cli-type parsing jobs. 
Probably don't write a compiler with it. But it can be handy for
interpreting ad-hoc command language syntax. Written in C.

#### Usage

##### `#include "uparse.h"`

The header will pull in all standard headers that it requires if they
have not already been included. `uparse.o` is linked with your application.

#### Defined macros and globals

##### `char *uuline`

Pointer to the text to scan; uparse does not alter the pointer and the
text is not overwritten.
The input line must be null-terminated and a final newline is not required.

##### `char *uulp`

Current scan position within uuline; updated by uparse to point to the
next non-space character following a successful match.
Set `uulp = uuline` before calling the first accept/expect.

##### `on_error stmt;`

##### `on_error { ... }`

The statement or block is executed when uuerror() is
called, or when expect() and expectall() fails.
This uses setjmp to establish a return point for scanning errors, and
therefore will work across functions.

on_error is required before using any uparse function.
A subsequent on_error will set a new error return point. on_error cannot
be nested.

A typical on_error would be located above a command-line entry loop
and would likely output the error message at a minimum:
```
on_error
    puts(uumsg);
```
#### Matching functions

These functions are used to match built-in or user-defined terminals and literals (strings) to input.
Up to 10 arguments are supported for the variadic functions.

##### `accept(x) stmt;`

##### `accept(x) { ... }`

Execute the statement or block if the input matches x; see matching rules below.

##### `accept(x, y, ...) stmt;`

##### `accept(x, y, ...) { ... }`

Execute statement or block if any one of x, y, ... is matched.

##### `acceptall(x, y, ...) stmt;`

##### `acceptall(x, y, ...) { ... }`

Execute statement or block only if **all** the symbols x, y, ... match the
input in sequence.

##### `expect(x);`

Expect terminal x; if input does not match x, an error message is saved
in `uumsg` and control jumps to on_error.
If x matches, expect() returns and parsing continues.
expect(x) is equivalent to:
```
accept(x) {
    ;
} else
    uuerror("expected %s", arg[0].term);
```
##### `expect(x, y, ...);`

Expect terminals x or y or ...
If **none** of the arguments match then jump to on_error block with
error message saved in `uumsg`.
Otherwise, expect() returns on the first match. `argmatch` will be set
to the argument number that matched.

##### `expectall(x, y, ...);`

Expect terminals x and y and ...
If **any** argument fails to match then jump to on_error block with
error message saved in `uumsg`.
expectall() returns only if all arguments match.

##### `int argmatch`

Argument number of match from the most recent accept() or expect(); first argument is 0.
After acceptall/expectall argmatch will always refer to the last 
argument (by definition all the terminals are matched).

##### `uuerror(fmt, ...);`

Produce a printf-style formatted message in uumsg, then jump to the on_error block.
Normally, uumsg is output there, along with other error information
as required by the application.

##### `repeat { ... }`

Repeat contents of block. To break a repeat block, use return or `break_repeat`, for example:
```
repeat {
    accept(eol)
        break_repeat;
}
```
repeat {} is a while(1) {} loop and break_repeat is break: they are used only to
make a parsing loop obvious.

#### Extend uparse with your own terminals

There are two ways to customise uparse to scan for new terminal types:
1. Edit uparse.c: add to, or alter the matching code in `_match()`, or,

2. Declare a new terminal name which refers to a scanning function
that you supply. No changes to uparse.c required.

Using method 2, to define a new terminal:

##### `decl_terminal(x, y, ...);`

Forward declaration of 1 or more user-defined terminals. 
Must be done in file scope. Under the hood, terminal names are `char *` to
a string containing the terminal name. The pointer is used internally
to identify the terminal (not what it points to).

##### `new_terminal(x, fn);`

Defines a new lexical terminal x. x can then be used in accept/expect.
fn is the function uparse will call for scanning/converting this terminal. Signature:
```
	int fn(char *lp, int argnum)
```
`lp` points to the current scan position in uuline.
`argnum` is the current argument index for saving the converted argument value (if any).
The return value must be 1 when input matches the terminal and conversion (if any)
occurred without error, or 0 on no match
or a scanning/conversion error occurred. If an error occurred, use
`uuerror()` to set an error message.
fn must not call accept/acceptall/expect/expectall: these functions are not re-entrant.
fn should not alter the text at uulp nor update uulp.

#### Utility functions

##### `char *substr(dest, src, len, max);`

Copies minimum of (len, max) chars from src to dest, terminating dest with \0.
substr returns dest.
Use this to copy out parts of uuline to an isolated string, using max
to keep within bounds of dest. For example
```
char id[21];
accept(ident, "=", integer) {
    substr(id, arg[0].lp, arg[0].len, sizeof(id)-1);
    ...
}
```

#### Matching rules

Matching input stops at the first character not in the class of characters
defined for the terminal. For example:
```
    literal terminal    input that matches      input that doesn't match
    ------------------- ----------------------- --------------------------
    "-d"                -d5                     -delete
    "abc"               abc-d                   a bc
    "a56"               a56b99                  a5660
    ","                 ,,                      any char not ,
    " "                 1 or more space         any char not space
    ",xyz"              ,xyz,                   ,xyzz
    "del all"           del      all            delall
```
uparse pre-defines the following terminals:
```
    terminal name    matches
    ---------------- ----------------------------------------
    integer          [0-9]+
    ident            [a-zA-Z_]+[0-9a-zA-Z]*
    identwc          ident plus wildcard chars [, ], *, ?, - 
    string           ["][^"]*["]
    eol              [\0]
    word             run of chars up to next space
```
##### `integer`

Scans for decimal digits and tests for `unsigned int`
overflow against LONG_MAX. For signed integers, scan sign separately:
```
int n, sign = 1;
accept("-", "+")
    sign = argmatch==0? -1 : 1;
accept(integer)
    n = arg[0].i * sign;
```
Note that LONG_MIN is not scannable.

##### `identwc`

This terminal will only match when one or more `fnmatch(3)` wildcard chars (?, \*, or [) are 
present in a normal identifier pattern.
It does not validate correct wildcarding.
To accept either a plain ident or wildcarded ident, put identwc first:
```
accept(identwc, ident)
    if (argmatch == 0)
        // do wildcard matching
    else
        // plain ident
```
Spaces in input are required only to separate terminals of similar type.
Leading and trailing spaces in the input are skipped over.
Therefore accept(" ") will not match a space in the input.
Example:
```
accept("hist") {
    accept(eol)
        // default action
    accept("-d", "delete")
        ...
    accept("-l", "list");
        ...
}
```
Accepted input: "hist-d", "hist  -d", "hist    delete". Not accepted: "histdelete", "hist - d".

If space seperation is required, include a trailing space:
```
accept("hist", eol)
    // default action
accept("hist ") {
    accept("-d", "delete")
        // delete action
    ...
}
```
Accepted: "hist   delete", "hist  -d".
Not accepted: "hist-d", "histdelete".

#### Matched argument values

A successful match fills the arg[] structure:
```
    char *arg[].term   the terminal print name 
    char *arg[].lp     ptr into uuline where the match begins
    int   arg[].len    the length of the match 
    int   arg[].i      converted integer
    void *arg[].vp     ptr to user-defined converted data
```
Up to 10 (`UUMAX_ARGS`) arguments can be filled in acceptall and expectall.

#### Build

`cc -c uparse.c` ready to link with your application.

`cc -DTEST uparse.c` run some basic tests.

#### Rationale

Code for parsing a command syntax in an interactive project was looking
messy and repetitive. After a failed, and possibly incomplete,
search for a simple set
of functions to assist (as opposed to take over), this method was developed,
using the familiar token accept / expect pattern of recursive descent parsing\[[1]\].
Seems to work well enough in that project, so the code was split out
and made generic: here it is. Contributions welcomed!

#### Example

See repo for more examples.

```c
#include "uparse.h"

// parse this command:
//     hist [m[-n]] [cmd]
// where
//     cmd: "d"|"delete" | ("w"|"write" file)
//     m,n: integer
//     file: text to eol

int
main()
{
  int n;
  size_t bufsz = 0;

  on_error
      puts(uumsg);

  while ((n = getline(&uuline, &bufsz, stdin)) > 0) {
      uuline[n-1] = '\0'; // remove newline
      uulp = uuline;

      int m = 0, n = 500; // set some defaults
      accept("hist", "h", "history") {
          accept(integer) {
              m = n = arg[0].i;
              accept("-") {
                  expect(integer);
                  n = arg[0].i;
              } 
              if (m > n)
                  uuerror("not a valid range");
          }
          accept("d", "delete") {
              expect(eol);
              printf("do delete cmd %d-%d\n", m, n);
              continue;
          } 
          accept("w", "write") {
              // file name is rest of line
              printf("do write cmd %d-%d to [%s]\n", m, n, uulp);
              continue;
          }
          accept(eol) {
              printf("do list cmd %d-%d\n", m, n);
              continue;
          }
      }
      uuerror("not a history command");
  }
}
```

[1]: https://en.wikipedia.org/wiki/Recursive_descent_parser
