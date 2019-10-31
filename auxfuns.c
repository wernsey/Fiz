/*
 * Auxillary functions go here.
 * These functions don't need intimate knowledge of the interpreter
 * for their implementation.
 *
 * This is free and unencumbered software released into the public domain.
 * http://unlicense.org/
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "fiz.h"

#ifndef FIZ_DISABLE_INCLUDE_FILES
char *fiz_readfile(const char *filename) {
    FILE *f;
    long len,r;
    char *str;

    if(!(f = fopen(filename, "rb")))
        return NULL;

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    rewind(f);

    if(!(str = malloc(len+2)))
        return NULL;
    r = fread(str, 1, len, f);

    if(r != len) {
        free(str);
        return NULL;
    }

    fclose(f);
    str[len] = '\0';
    return str;
}
#endif

static Fiz_Code aux_puts(Fiz  *F, int argc, char **argv, void *data) {
    if(argc != 2)
        return fiz_argc_error(F, argv[0], 2);
    puts(argv[1]);
    fiz_set_return(F, argv[1]);
    return FIZ_OK;
}

static Fiz_Code aux_expr(Fiz *F, int argc, char **argv, void *data) {
    char *e;
    const char *err;
    int i;
    size_t len = 0;
    if(argc < 2)
        return fiz_argc_error(F, argv[0], 2);
    for(i = 1; i < argc; i++)
        len += strlen(argv[i]);
    e = malloc(len+1);
    e[0] = '\0';
    for(i = 1; i < argc; i++)
        strcat(e, argv[i]);
    assert(strlen(e) == len);
    i = expr(e, &err);
    free(e);
    if(err) {
        fiz_set_return_ex(F, "expr: %s", err);
        return FIZ_ERROR;
    }
    fiz_set_return_ex(F, "%d", i);
    return FIZ_OK;
}

static Fiz_Code aux_eqne(Fiz *F, int argc, char **argv, void *data) {
    if(argc != 3)
        return fiz_argc_error(F, argv[0], 3);
    if(!strcmp(argv[0], "eq"))
        fiz_set_return_ex(F, "%d", !strcmp(argv[1],argv[2]));
    else
        fiz_set_return_ex(F, "%d", !!strcmp(argv[1],argv[2]));
    return FIZ_OK;
}

static Fiz_Code aux_incr(Fiz *F, int argc, char **argv, void *data) {
    const char *val;
    int i;
    if(argc != 2)
        return fiz_argc_error(F, argv[0], 2);
    val = fiz_get_var(F, argv[1]);
    if(!val) {
        fiz_set_return_ex(F, "%s not found", argv[1]);
        return FIZ_ERROR;
    }
    i = atoi(val);
    if(!strcmp(argv[0], "decr"))
        i--;
    else
        i++;
    fiz_set_var_ex(F, argv[1], "%d", i);
    fiz_set_return_ex(F, "%d", i);
    return FIZ_OK;
}

static Fiz_Code aux_dict(Fiz *F, int argc, char **argv, void *data) {
    const char *v = NULL;
    if(argc < 3)
        return fiz_argc_error(F, argv[0], 3);
    if(!strcmp(argv[2], "put")) {
        if(argc < 5)
            return fiz_argc_error(F, argv[0], 5);
        fiz_dict_insert(F, argv[1], argv[3], argv[4]);
        fiz_set_return(F, argv[4]);
    } else if(!strcmp(argv[2], "get")) {
        if(argc < 4)
            return fiz_argc_error(F, argv[0], 4);
        v = fiz_dict_find(F, argv[1], argv[3]);
        if(!v) {
            fiz_set_return_ex(F, "no key %s in dict %s", argv[3], argv[1]);
            return FIZ_ERROR;
        }
        fiz_set_return(F, v);
    } else if(!strcmp(argv[2], "has")) {
        if(argc < 4)
            return fiz_argc_error(F, argv[0], 4);
        v = fiz_dict_find(F, argv[1], argv[3]);
        if(!v)
            fiz_set_return(F, "0");
        else
            fiz_set_return(F, "1");
    } else if(!strcmp(argv[2], "first")) {
        if(argc < 3)
            return fiz_argc_error(F, argv[0], 3);
        v = fiz_dict_next(F, argv[1], NULL);
        if(!v) {
            fiz_set_return_ex(F, "dict %s is empty or does not exist", argv[1]);
            return FIZ_ERROR;
        }
        fiz_set_return(F, v);
    } else if(!strcmp(argv[2], "next")) {
        if(argc < 4)
            return fiz_argc_error(F, argv[0], 4);
        v = fiz_dict_next(F, argv[1], argv[3]);
        if(!v)
            fiz_set_return(F, "");
        else
            fiz_set_return(F, v);
    } else if(!strcmp(argv[2], "remove")) {
        if(argc < 4)
            return fiz_argc_error(F, argv[0], 4);
        fiz_dict_delete(F, argv[1], argv[3]);
        fiz_set_return(F, "");
    } else if(!strcmp(argv[2], "foreach")) {
        if(argc < 7)
            return fiz_argc_error(F, argv[0], 7);
        if(strcmp(argv[5], "do")) {
            fiz_set_return_ex(F, "syntax is: %s %s %s key val do {body}", argv[0], argv[1], argv[2]);
            return FIZ_ERROR;
        }
        while((v = fiz_dict_next(F, argv[1], v)) != NULL) {
            fiz_set_var(F, argv[3], v);
            fiz_set_var(F, argv[4], fiz_dict_find(F, argv[1], v));
            if(fiz_exec(F, argv[6]) != FIZ_OK)
                return FIZ_ERROR;
        }
    } else {
        fiz_set_return_ex(F, "unknown command %s to %s", argv[2], argv[0]);
        return FIZ_ERROR;
    }

    return FIZ_OK;
}

#ifndef FIZ_DISABLE_INCLUDE_FILES
static Fiz_Code aux_include(Fiz *F, int argc, char **argv, void *data) {
    Fiz_Code rc;
    char *str;
    if(argc != 2)
        return fiz_argc_error(F, argv[0], 2);
    str = fiz_readfile(argv[1]);
    if(!str) {
        fiz_set_return_ex(F, "unable to read %s", argv[1]);
        return FIZ_ERROR;
    }
    rc = fiz_exec(F, str);
    free(str);
    return rc;
}
#endif

void fiz_add_aux(Fiz *F) {
    fiz_add_func(F, "puts", aux_puts, NULL);
    fiz_add_func(F, "expr", aux_expr, NULL);
    fiz_add_func(F, "eq", aux_eqne, NULL);
    fiz_add_func(F, "ne", aux_eqne, NULL);
    fiz_add_func(F, "incr", aux_incr, NULL);
    fiz_add_func(F, "decr", aux_incr, NULL);
    fiz_add_func(F, "dict", aux_dict, NULL);
#ifndef FIZ_DISABLE_INCLUDE_FILES
    fiz_add_func(F, "include", aux_include, NULL);
#endif
}
