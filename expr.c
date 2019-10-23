/*
 * A simple recursive descent expression evaluator.
 *
 * Compile the test program like so:
 * $ gcc -o expr -Wall -Werror -pedantic -O3 -DTEST expr.c
 *
 * This is free and unencumbered software released into the public domain.
 * http://unlicense.org/
 */
#include <stdlib.h>
#include <ctype.h>
#include <setjmp.h>
#include <assert.h>
#include <stdint.h>

/* 
 * For integer based math, define FIZ_INTEGER_EXPR symbol in the whole project or
 * use add MATH=int flag when running make. If not defined, double based math is used.
 *
 * Double based math supports numbers in range of +/- 1000000000000000000
 * And up to 8 decimals precision in printing and comparasion
 */

#ifdef FIZ_INTEGER_EXPR
    typedef int number;
    inline static number operator_modulo(const number a, const number b) { return a % b; }
    inline static char operator_equals(const number a, const number b) { return a == b; }
    inline static char operator_gt(const number a, const number b) { return a == b; }
#else
    #include <math.h>
    typedef double number;
    inline static number operator_modulo(const number a, const number b) { return fmod(a,b); }
    inline static char operator_equals(const number a, const number b) 
    {
        const double epsilon = 0.00000001;
        const number fabsa = fabs(a);
        const number fabsb = fabs(b);
        number largest = 1.0;

        if(fabsa > largest) largest = fabsa;
        if(fabsb > largest) largest = fabsb;
        return fabs(a - b) <= epsilon * largest;
    }
    inline static char operator_gt(const number a, const number b) 
    { 
        return a > b && !operator_equals(a, b); 
    }
#endif

struct expr_handler {
    const char *str, *prev;
    jmp_buf on_err;
    const char **err;
};

static void do_error(struct expr_handler *eh, const char *msg) {
    *(eh->err) = msg;
    longjmp(eh->on_err, 1);
}

static char getnext(struct expr_handler *h) {
    char c;
    while(isspace((int)h->str[0])) h->str++;
    h->prev = h->str;
    c = h->str[0];
    h->str++;
    return c;
}

/* Macros that hopefully makes my intention clearer */
#define peeknext(h) (h->str[0])
#define gobblenext(h) (h->str++)

static void reset(struct expr_handler *h) {
    /* We're only looking ahead one token, so don't
     * use it in situations where this may happen: */
    assert(h->prev != NULL);
    h->str = h->prev;
    h->prev = NULL;
}

/* Prototypes for the recursive descent parser's functions defined below */
static number or(struct expr_handler *h);
static number and(struct expr_handler *h);
static number not(struct expr_handler *h);
static number comp(struct expr_handler *h);
static number term(struct expr_handler *h);
static number factor(struct expr_handler *h);
static number unary(struct expr_handler *h);
static number atom(struct expr_handler *h);

/* Entry point for the expression evaluator.
 * - 'str' contains the expression to be parsed.
 * - 'err' should be a pointer to a (const char *). When the
 *   function returns, if the pointer pointed to by 'err' is NULL
 *   the expression was parsed successfully, otherwise *err will
 *   point to a string containing an error message.
 */
number expr(const char *str, const char **err) {
    number n;
    const char *dummy;
    struct expr_handler eh;
    if(err) {
        *err = NULL;
        eh.err = err;
    } else
        eh.err = &dummy;
    eh.str = str;
    eh.prev = NULL;

    if(setjmp(eh.on_err))
        return 0;
    n = or(&eh);
    if(getnext(&eh) != '\0')
        do_error(&eh, "end of expression expected");
    return n;
}

/*
 * Recursive descent starts here.
 */
static number or(struct expr_handler *h) {
    number n = and(h), r;
    while(getnext(h) == '|' && peeknext(h) == '|') {
        gobblenext(h);
        /* Short-circuit caught me here:
         * If you try to write this as n = n || and(h);
         * and n is "true", then the and(h) won't get called,
         * meaning the pointer h->str won't be moved along, causing
         * problems down the road.
         * I just hope a smart optimiser doesn't cause problems
         * (so far I've checked GCC, and it doesn't), so test with
         * something like "1||0" or "0&&1" to check for problems
         */
        r = and(h);
        n = n || r;
    }
    reset(h);
    return n;
}

static number and(struct expr_handler *h) {
    number n = not(h), r;
    while(getnext(h) == '&' && peeknext(h) == '&') {
        gobblenext(h);
        /* potential short-circuit problem; see or() above */
        r = not(h);
        n = n && r;
    }
    reset(h);
    return n;
}

static number not(struct expr_handler *h) {
    if(getnext(h) == '!')
        return !comp(h);
    reset(h);
    return comp(h);
}

static number comp(struct expr_handler *h) {
    number n = term(h);
    char c = getnext(h);
    if(c == '=' || c == '>' || c == '<' || (c == '!' && peeknext(h) == '=')) {
        if(h->str[0] == '=') {
            gobblenext(h);
            switch(c) {
                case '=' : return operator_equals(n, term(h));
                case '>' : return !operator_gt(term(h), n); //n >= term(h);
                case '<' : return !operator_gt(n, term(h)); //n <= term(h);
                case '!' : return !operator_equals(n, term(h));
            }
        } else {
            switch(c) {
                case '=' : return operator_equals(n, term(h));
                case '>' : return operator_gt(n, term(h)); // n > term(h);
                case '<' : return operator_gt(term(h), n); //n < term(h);
            }
        }
    } else
        reset(h);
    return n;
}

static number term(struct expr_handler *h) {
    number n = factor(h), c;
    while((c=getnext(h)) == '+' || c == '-') {
        if(c == '+')
            n += factor(h);
        else
            n -= factor(h);
    }
    reset(h);
    return n;
}

static number factor(struct expr_handler *h) {
    number n = unary(h), c;
    while((c=getnext(h)) == '*' || c == '/' || c == '%') {
        number x = unary(h);
        if(c == '*')
            n *= x;
        else {
            if(x == 0) do_error(h, "divide by zero");
            if(c == '/')
                n /= x;
            else
                n = operator_modulo(n, x);
        }
    }
    reset(h);
    return n;
}

static number unary(struct expr_handler *h) {
    number c = getnext(h);
    if(c == '-' || c == '+') {
        if(c == '-')
            return -atom(h);
        return atom(h);
    }
    reset(h);
    return atom(h);
}

static number atom(struct expr_handler *h) {
    if(getnext(h) == '(') {
        number n = or(h);
        if(getnext(h) != ')')
            do_error(h, "missing ')'");
        return n;
    }
    reset(h);
    while(isspace((int)h->str[0])) h->str++;
    if(!isdigit((int)h->str[0]))
        do_error(h, "number expected");
    uint64_t n = 0;
    while(isdigit((int)h->str[0])) {
        if(n >= 1000000000000000000L)
            do_error(h, "number too large");
        n = n * 10 + (h->str[0] - '0');
        h->str++;
    }
    #ifndef FIZ_INTEGER_EXPR
    if (peeknext(h) == '.') 
    {
        h->str++;
        if(!isdigit((int)h->str[0]))
            do_error(h, "floating point part expected");
        uint64_t m = 0, mbase = 1;
        while(isdigit((int)h->str[0])) {
            if(mbase >= 1000000000000000000L) 
                do_error(h, "number fractional part too large");
            m = m * 10 + (h->str[0] - '0');
            mbase *= 10;
            h->str++;
        }
        return (number)n + (number)m / (number)mbase;
    }
    #endif
    return (number)n;
}

/**********************************************************************
 * Test program
 **********************************************************************/
#ifdef TEST
#include <stdio.h>
int main(int argc, char *argv[]) {
    int i;
    number r;
    const char *err;
    for(i = 1; i < argc; i++) {
        r = expr(argv[i], &err);
        if(!err)
            printf("%s = %d\n", argv[i], r);
        else
            fprintf(stderr, "error: %s: %s\n", argv[i], err);
    }
    return 0;
}

#endif /* TEST */
