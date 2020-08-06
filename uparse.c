// uparse - micro parsing package as described in README.md
//
// accept(), expect(), acceptall(), expectall()
// uuerror, on_error, repeat
// uuline, uulp, argmatch, arg[]
// decl_terminal(), new_terminal(), substr()

// 2/6/20-SP github.com/spinau/uparse
// -DTEST for standalone testing

#include "uparse.h"

char *uuline = NULL; // start of line ptr
char *uulp = NULL;  // curr scan pos ptr
char *lpfail; // ptr to line pos where match failed
int argmatch; // matching arg number
int argfail; // arg number that failed
struct uuargval arg[UUMAX_ARGS];

jmp_buf _errjmp;
int _errjmpok = -1; // init with on_error

// built in terminals
// declared this way to prevent constant folding
// allows accept("integer", integer) to work as expected
char eol[] =  { 'e','n','d',' ','o','f',' ','l','i','n','e', '\0' };
char string[] = { 'q','u','o','t','e','d',' ','s','t','r','i','n','g', '\0' };
char ident[] = { 'i','d','e','n','t','i','f','i','e','r', '\0' };
char identwc[] = { 'i','d','e','n','t','i','f','i','e','r','+','g','l','o','b', '\0' };
char integer[] = { 'i','n','t','e','g','e','r', '\0' };
char word[] = { 'w', 'o', 'r', 'd', '\0' };

static struct _term_s {
    char *name;
    int (*fn)(char *, int);
} _term[UUMAX_TERMS] = {
    eol, NULL, string, NULL, ident, NULL, identwc, NULL, integer, NULL, word, NULL
};
#define USRTERMS 6 // index into _term where user terminals start
#define ALLTERMS 0
int _termcnt = 6; // incremented for each new_terminal()

char uumsg[MAX_UUMSG]; // message produced by uuerror()

static struct _term_s *
lookup_term(char *term, int scope)
{
    for (int i = scope; i < _termcnt; ++i)
        if (_term[i].name == term) // ptr comparison
            return &_term[i];
    return NULL;
}

// build an error message for user to print (usually at on_error)
// message will list terminals expected from failure onward
// ac count of variadic arguments
// all true if expectall
// lpfail = ptr to line position where _match stopped
// argfail = arg number that failed
void
_expect_msg(int ac, int all, ...)
{
    sprintf(uumsg, "expected %s", argfail<ac-1 && !all? "one of: " : "");
    va_list args;
    va_start(args, all);

    int i = 0;
    while (ac--) {
        char *va = va_arg(args, char *);
        if (i >= argfail) {
            if (lookup_term(va, ALLTERMS))
                sprintf(uumsg+strlen(uumsg), "<%s>", va);
            else
                sprintf(uumsg+strlen(uumsg), "%s", va);
            sprintf(uumsg+strlen(uumsg), "%s ", ac>0? "," : "");
        }
        ++i;
    }

    if (lpfail > uuline)
        sprintf(uumsg+strlen(uumsg), "at position %ld", 1+lpfail-uuline);
}

char *
_new_terminal(char *name, int (*fn)()) 
{ 
    assert(_termcnt < UUMAX_TERMS);
    _term[_termcnt].fn = fn;
    // have to strdup the name due to constant folding
    return _term[_termcnt++].name = strdup(name); 
}

char *
substr(char *dest, char *src, int len, int max)
{
    if (len > max)
        len = max;
    assert(len >= 0);
    memcpy(dest, src, len);
    dest[len] = '\0';
    return dest;
}

// accept(), acceptall(), expect(), expectall():
// loop over all arguments and attempt to match against current uulp
// save result of matches in arg[0...ac-1]
// if all=1 return on first fail, keep looping on matches
// if all=0 return on first match, keep looping on fails
// returns 0 if no match, 1 if match
int
_match(int all, int ac, ...)
{
    int l;
    char *cp, *lp, *resetlp;
    struct _term_s *tn;

    assert(uulp);
    assert(uuline);

    va_list args;
    va_start(args, ac);
    argmatch = -1;
    lpfail = resetlp = lp = uulp; // in case of failed matchall
    for (int i = 0; i < ac; ++i) {
        arg[i].term = cp = va_arg(args, char *);
        arg[i].len = 0;

        while (isspace(*lp))
            ++lp;
        arg[i].lp = lp;

        if (cp == eol) {
            if (*lp != '\0')
                goto no_match;

        } else if (cp == string) {
            if (*lp == '"') {
                ++arg[i].lp;
                while (*++lp) {
                    // TODO escapes?
                    if (*lp == '"' && *(lp-1) != '\\')
                        break;
                    ++arg[i].len;
                }
                if (*lp != '"')
                    uuerror("unterminated string");
                else 
                    ++lp;
            } else
                goto no_match;

        } else if (cp == ident) {
            if (isalpha(*lp) || *lp == '_') {
                do
                    ++arg[i].len, ++lp;
                while (*lp && (isalnum(*lp) || *lp=='_'));
            } else 
                goto no_match;

        // match a wildcarded ident only if wc_flag chars are present
        // (for mon use, an exclusive wildcard-only match is useful)
        } else if (cp == identwc) {
            char *wcyes = NULL, *wc_all = "?*[!-]", *wc_flag = "?*[";
            if (*lp && (isalpha(*lp) || *lp == '_' || (wcyes = strchr(wc_flag, *lp)))) {
                do {
                    if (wcyes == NULL) 
                        wcyes = strchr(wc_flag, *lp);
                    ++arg[i].len, ++lp;
                } while (*lp && (isalnum(*lp) || *lp=='_' || strchr(wc_all, *lp)));
                if (wcyes == NULL)
                    goto no_match;
            } else
                goto no_match;

        } else if (cp == integer) {
            if (isdigit(*lp)) {
                int d, limit = LONG_MAX % 10; // last digit of max long
                long max = LONG_MAX / 10; // for overflow check without overflowing
                long val = 0;
                while (*lp && isdigit(*lp)) {
                    d = *lp - '0';
                    if (val > max || (val == max && d > limit))
                        uuerror("integer overflow");
                    val *= 10;
                    val += d;
                    ++lp;
                }
                arg[i].i = val;
            } else
                goto no_match;

        } else if (cp == word) {
            while (*lp && !isspace(*lp))
                ++arg[i].len, ++lp;
            if (arg[i].len == 0)
                goto no_match;

        } else if ((tn = lookup_term(cp, USRTERMS))) {
            int n;
            assert(tn->fn); // every user-defined must have fn ptr
            if ((n = (tn->fn)(lp, i)))
                lp += n;
            else
                goto no_match;

        } else { // everything else: try and match literally
            arg[i].len = l = strlen(cp);
            if (strncmp(cp, lp, l) == 0) {
                if (isalpha(cp[l-1])) {
                    if (isalpha(lp[l])) 
                        goto no_match;
                } else if (isdigit(cp[l-1])) {
                    if (isdigit(uulp[l])) 
                        goto no_match;
                } // else must be 1 or more punct
                lp += l;
            } else
                goto no_match;
        } 

        while (isspace(*lp))
            ++lp;

        // fall through to match
match:
        if (all)
            continue;
        else {
            uulp = lp;
            argmatch = i;
            return 1;
        }

no_match:
        if (all) {
            argfail = i;
            lpfail = lp;
            uulp = resetlp;
            return 0;
        } else {
            lp = uulp;
            continue;
        }
    }

    va_end(args);
    // arrive here at end of args only if: 
    // all==1 and all matched or all==0 and none matched
    if (all)
        uulp = lp;
    else {
        argfail = 0; // all args must have failed
        //lpfail = lp;
        uulp = resetlp;
    }

    return all;
}

#ifdef TEST
#include <errno.h>

char *terminals[] = {
"", // ← will slot an example user-defined terminal here
    // built in terminals:
    ident,
    identwc,
    integer,
    word,
    string,
    eol,
    // some literal matches:
    "match1",
    "match-two",
    "match3",
    "-",
    "",
    " ",
    "     ",
    ".",
    ",,",
    ",,,,",
    NULL };

char *inputs[] = {
    "",
    " ",
    "\n",
    "0",
    "9",
    "1024",
    "30984029380",
    "-9438098",
    "9223372036854775807 LONG_MAX",
    "-3.1214269",
    "   -31214269e-1",
    "4.",
    "_",
    "_33",
    "\n_ident_\n",
    "ident",
    "integer  ",
    "notwild]card",
    "wild]card?",
    "wc*a",
    "*",
    "match 1",
    "match-two",
    "      match3     ",
    "\"quoted text\"",
    "\"\"",
    "\"embedded \\\"quoted_text\\\"\"",
    ",,,",
    ".;+",
    "   ",
    // provoke error reporting:
    "9223372036854775808", //overflow
    "\"missing a quote",
    NULL };

// example user-defined terminal to scan a float
//      accept(flt)
//          ...
//
// the example below implements this as: if it can be converted
// to float using strtof, then it's a match.
//
// this all-or-nothing approach could be improved by further work
// to determine if it is "nearly a float" but with a format error:
// use a bunch of accept/expect calls to parse the format you want
// to accept, calling strof with confidence if input survives the parse.
//
// (the 1620 emulator monitor solves the problem the easy way by 
// requiring floats start with 0F or 0f)
//
// if your parse job allows integers and floats, you have to decide
// which takes precedence: is 1E1 a float or integer, "E", integer?

// define a sidecar to arg[] array to save converted floats:
// (or you could extend union in argval struct and save directly to arg[])
float fltarg[UUMAX_ARGS];

int
parse_flt(char *inp, int argnum)
{
    float f;
    char *endptr;

    errno = 0;
    f = strtof(inp, &endptr);

    if (inp == endptr)
        return 0; // no conversion? not a recognised float here

    if (errno == ERANGE)
        uuerror(f < 0.0? "underflow" : "overflow");
    else 
        // save up to UUMAX_ARGS floats
        fltarg[argnum] = f;

    return endptr - inp;
}

// run through each combination of terminals[] x inputs[]
// inspect output for expected results

void
run_test()
{
    char **inp, **term;

    decl_terminal(flt);
    new_terminal(flt, parse_flt);
    terminals[0] = flt;

    on_error {
        printf(" ERROR uumsg=%s\n", uumsg);
        goto resume;
    }

    for (inp = inputs; *inp; ++inp)
        for (term = terminals; *term; ++term) {
            uuline = uulp = *inp;
            printf("[%s] → accept(", *inp);
            printf(lookup_term(*term, ALLTERMS)? "%s)" : "\"%s\")", *term);

            accept(*term) {
                printf(" → OK ");
                if (*term == flt)
                    printf("strtof=%f", fltarg[0]);
                else
                    printf(" arg.len=%d, arg.lp=%.*s", arg[0].len, arg[0].len, arg[0].lp);

                if (*uulp == '\0')
                    putchar('\n');
                else
                    printf(", uulp=[%s]\n", uulp);
            } else
                printf(" → NOMATCH\n");

            resume:;
        }
}

char *list1[] = {
    "intege 1234",
    "integer ABC",
    "integer \"open string",
    NULL
};

char *list2[] = {
    "anident[1234] = \"open string",
    "_id202 [bad] = \"string\"",
    "1",
    NULL
};

void
uuerror_test(int testnum, char **inp)
{
    printf("test %d\n", testnum);

    on_error {
        puts(uumsg);
        goto next;
    }

    while (*inp) {
        uuline = uulp = *inp;
        printf("[%s]: ", *inp);
        switch (testnum) {
        case 1:
            expectall("integer", integer);
            puts("match!");
            break;
        case 2:
            expectall(ident, "[", integer, "]", "=", string);
            puts("match!");
            break;
        }
next:
        ++inp;
    }
}


// use for tinkering...
void
main()
{
    uuerror_test(1, list1);
    uuerror_test(2, list2);
    //run_test();
}
#endif
