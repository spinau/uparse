/* micro-parse - see uparse.c
 * 2/6/20-SP
 */

#ifndef _UPARSE_H
#define _UPARSE_H

#ifndef _STDIO_H
#include <stdio.h>
#endif
#ifndef _STDARG_H
#include <stdarg.h>
#endif
#ifndef _STRING_H
#include <string.h>
#endif
#ifndef _CTYPE_H
#include <ctype.h>
#endif
#ifndef _SETJMP_H
#include <setjmp.h>
#endif
#ifndef _ASSERT_H
#include <assert.h>
#endif
#ifndef _STDLIB_H
#include <stdlib.h>
#endif
#ifndef _LIMITS_H
#include <limits.h>
#endif

#ifndef MAX_UUMSG // your app may want more or less than default:
#define MAX_UUMSG 120 // max chars for uumsg string
#endif

// gnu c should have built-in __VA_ARGC__ but doesn't...why?
// this is a hack to count number of arguments in a variadic macro
// works for up to 10 args (last number in _argc_n - 1)
// (note that __VA_ARGC__ must be able to detect zero arguments as well)
#ifndef __VA_ARGC__
#define _argc_n( _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, N, ...) N
#define _argseq 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
#define _argc(...) _argc_n(__VA_ARGS__)
// count the number of arguments:
#define __VA_ARGC__(...) _argc(_, ##__VA_ARGS__, _argseq)
#endif

struct uuargval {
    char *term; // the terminal (builtin, user defined, or literal string)
    char *lp; // ptr to matched string in line
    int len; // generally, length of matched string at lp
    union { // the converted value
        long i; // integer
        void *vp; // user-defined
    };
};

char *_new_terminal(char *, int (*)());
char *substr(char *, char *, int, int);
int _match(int, int, ...);
void _expect_msg(int, int, ...);

// standard terminals:
extern char eol[], string[], ident[], identwc[], integer[], word[];
extern char *uuline, *uulp, *lpfail;
extern char uumsg[120];
extern int argmatch, argfail;
extern struct uuargval arg[];
extern jmp_buf _errjmp;
extern int _errjmpok;

#define UUMAX_TERMS 12 // built ins take first 7
#define UUMAX_ARGS 10

#define new_terminal(x,fn)  x = _new_terminal(#x,fn) // user-defined terminal
#define decl_terminal(...)  char * __VA_ARGS__ // forward declaration

// save and restore current _errjmp: single save, not stacked
#define save_jmp_buf()    jmp_buf _savejb; memcpy(_savejb, _errjmp, sizeof(_errjmp))
#define restore_jmp_buf() memcpy(_errjmp, _savejb, sizeof(_errjmp))

#define on_error        if (_errjmpok = 1, setjmp(_errjmp))
#define _do_errjmp      assert(_errjmpok != -1); \
                        if (_errjmpok) longjmp(_errjmp, 1)

#define uuerror(...)    do{ sprintf(uumsg,## __VA_ARGS__, "");\
                        _do_errjmp; }while(0)

#define accept(...)     if (_match(0,__VA_ARGC__(__VA_ARGS__),## __VA_ARGS__))
#define acceptall(...)  if (_match(1,__VA_ARGC__(__VA_ARGS__),## __VA_ARGS__))

#define expect(...)     do{ int _ac = __VA_ARGC__(__VA_ARGS__);\
                        if (_match(0,_ac, __VA_ARGS__)==0) {\
                        _expect_msg(_ac, 0, __VA_ARGS__);\
                        _do_errjmp; } }while(0)
                            
#define expectall(...)  do{ int _ac = __VA_ARGC__(__VA_ARGS__);\
                        if (_match(1,_ac, __VA_ARGS__)==0) {\
                        _expect_msg(_ac, 1, __VA_ARGS__);\
                        _do_errjmp; } }while(0)

#define repeat          while(1)
#define break_repeat    break
#endif // _UPARSE_H
