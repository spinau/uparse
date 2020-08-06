// uparse exercise - parse an infix grammar
//
// expr: 
//      term { "," term } eol
// term: 
//      factor 
//      | term "+" factor 
//      | term "-" factor
// factor: 
//      primary
//      | factor "*" expr
//      | factor "/" expr
// primary:
//      identifier
//      | identifier "(" expr-list ")"
//      | constant
//      | "-" primary | "+" primary
//      | "(" expr ")"
// expr-list:
//      | <empty>
//      | expr
//      | expr "," expr
// constant:
//      integer
//      

#include "uparse.h"

// the base type for calculations
// this exercise handles only integer types
typedef long calc_t;

// define some built-in functions:

calc_t
fn_min(int ac, calc_t av[])
{
    calc_t min = av[0];
    for (int i = 1; i < ac; ++i)
        if (av[i] < min)
            min = av[i];
    return min;
}

calc_t
fn_max(int ac, calc_t av[])
{
    int max = av[0];
    for (int i = 1; i < ac; ++i)
        if (av[i] > max)
            max = av[i];
    return max;
}

calc_t
fn_rnd(int ac, calc_t av[])
{
    if (ac != 0)
        uuerror("no arguments for rnd() function");
    return (calc_t) rand();
}

// table of built in functions

typedef calc_t (*fn_t)(int, calc_t *);
struct fn_list {
    char *name;
    fn_t fn;
} builtin[] = {
    "min", fn_min,
    "max", fn_max,
    "rnd", fn_rnd,
    NULL, NULL
};

fn_t
lookup_fn(char *name)
{
    for (struct fn_list *listp = builtin; listp; ++listp)
        if (strcmp(listp->name, name) == 0)
            return listp->fn;
    return NULL;
}
    
#define MAXARGS 10 // max args for function calls

calc_t primary(), factor(), term(), expr();

calc_t
primary()
{
    calc_t n;

    acceptall(ident, "(") {
        calc_t (*fn_call)(), fn_argv[MAXARGS];
        int fn_argc = 0;
        char name[20];

        substr(name, arg[0].lp, arg[0].len, sizeof(name));
        if ((fn_call = lookup_fn(name)) == NULL)
            uuerror("undefined function %s", name);

        repeat {
            accept(")")
                break;

            if (fn_argc < MAXARGS)
                fn_argv[fn_argc++] = term();
            else 
                uuerror("function %s: too many args", name);

            accept(",")
                continue;

            accept(eol)
                uuerror("unclosed paren on function call %s", name);
        }
        return (fn_call)(fn_argc, fn_argv);
    }

    accept(ident) {
        char name[20], *cp;
        substr(name, arg[0].lp, arg[0].len, sizeof(name));
        // typically, look up a symbol table
        // but for this exercise, just look in env:
        if (cp = getenv(name))
            return (calc_t) atoi(cp);
        else
            uuerror("%s not found in environment", name);
    }
    
    accept("(") {
        n = term();
        expect(")");
        return n;
    }

    accept("-") 
        return -primary();

    accept("+")
        return primary();

    accept(integer)
        return arg[0].i;

    uuerror("syntax error at %c", *uulp);
}

calc_t
factor()
{
    calc_t n = primary();

    repeat {
        accept("*") {
            n *= primary();
            continue;
        }
        accept("/") {
            n /= primary();
            continue;
        }
        return n;
    }
}

calc_t
term()
{
    calc_t n = factor();

    repeat {
        accept("+") {
            n += factor();
            continue;
        }
        accept("-") {
            n -= factor();
            continue;
        }
        return n;
    }
}

calc_t
expr()
{
    calc_t n = term();
    expect(eol, ",");
    return n;
}

void
main()
{
    size_t linesz = 0;
    int len;

    on_error
        puts(uumsg);

    while ((len = getline(&uuline, &linesz, stdin)) > 0) {
        uuline[len-1] = '\0';
        uulp = uuline;

        accept("q", "quit")
            break;

        repeat {
            accept(eol)
                break;
            printf(" = %d\n", expr());
        }
    }
}
