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
    while(isspace(h->str[0])) h->str++;
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
static int or(struct expr_handler *h);
static int and(struct expr_handler *h);
static int not(struct expr_handler *h);
static int comp(struct expr_handler *h);
static int term(struct expr_handler *h);
static int factor(struct expr_handler *h);
static int unary(struct expr_handler *h);
static int atom(struct expr_handler *h);

/* Entry point for the expression evaluator.
 * - 'str' contains the expression to be parsed.
 * - 'err' should be a pointer to a (const char *). When the
 *   function returns, if the pointer pointed to by 'err' is NULL
 *   the expression was parsed successfully, otherwise *err will
 *   point to a string containing an error message.
 */
int expr(const char *str, const char **err) {
    int n;
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
static int or(struct expr_handler *h) {
    int n = and(h), r;
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

static int and(struct expr_handler *h) {
    int n = not(h), r;
    while(getnext(h) == '&' && peeknext(h) == '&') {
        gobblenext(h);
        /* potential short-circuit problem; see or() above */
        r = not(h);
        n = n && r;
    }
    reset(h);
    return n;
}

static int not(struct expr_handler *h) {
    if(getnext(h) == '!')
        return !comp(h);
    reset(h);
    return comp(h);
}

static int comp(struct expr_handler *h) {
    int n = term(h);
    char c = getnext(h);
    if(c == '=' || c == '>' || c == '<' || (c == '!' && peeknext(h) == '=')) {
        if(h->str[0] == '=') {
            gobblenext(h);
            switch(c) {
                case '=' : return n == term(h);
                case '>' : return n >= term(h);
                case '<' : return n <= term(h);
                case '!' : return n != term(h);
            }
        } else {
            switch(c) {
                case '=' : return n == term(h);
                case '>' : return n > term(h);
                case '<' : return n < term(h);
            }
        }
    } else
        reset(h);
    return n;
}

static int term(struct expr_handler *h) {
    int n = factor(h), c;
    while((c=getnext(h)) == '+' || c == '-') {
        if(c == '+')
            n += factor(h);
        else
            n -= factor(h);
    }
    reset(h);
    return n;
}

static int factor(struct expr_handler *h) {
    int n = unary(h), c;
    while((c=getnext(h)) == '*' || c == '/' || c == '%') {
        int x = unary(h);
        if(c == '*')
            n *= x;
        else {
            if(x == 0) do_error(h, "divide by zero");
            if(c == '/')
                n /= x;
            else
                n %= x;
        }
    }
    reset(h);
    return n;
}

static int unary(struct expr_handler *h) {
    int c = getnext(h);
    if(c == '-' || c == '+') {
        if(c == '-')
            return -atom(h);
        return atom(h);
    }
    reset(h);
    return atom(h);
}

static int atom(struct expr_handler *h) {
    int n = 0;

    if(getnext(h) == '(') {
        n = or(h);
        if(getnext(h) != ')')
            do_error(h, "missing ')'");
        return n;
    }
    reset(h);
    while(isspace(h->str[0])) h->str++;
    if(!isdigit(h->str[0]))
        do_error(h, "number expected");
    while(isdigit(h->str[0])) {
        n = n * 10 + (h->str[0] - '0');
        h->str++;
    }
    return n;
}

/**********************************************************************
 * Test program
 **********************************************************************/
#ifdef TEST
#include <stdio.h>
int main(int argc, char *argv[]) {
    int i, r;
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
