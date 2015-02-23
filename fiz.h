/*! FIZ - A Tcl-like scripting language.
 *-
 *[
 *# Author: Werner Stoop
 *# This is free and unencumbered software released into the public domain.
 *# http://unlicense.org/
 *]
 *-
 */

struct hash_tbl;
struct fiz_callframe;

/*
 * Main interpreter data structure.
 * Create it with fiz_create(), and destroy it after use
 * with fiz_destroy()
 */
struct Fiz
{
	struct hash_tbl *commands;
	struct hash_tbl *dicts;
	struct fiz_callframe *callframe;
	char *return_val;
};

enum Fiz_Code {FIZ_OK, FIZ_ERROR, FIZ_RETURN, FIZ_CONTINUE, FIZ_BREAK};

/*
 * Prototype for C-functions that can be added to the interpreter.
 */
typedef enum Fiz_Code (*fiz_func)(struct Fiz *f, int argc, char **argv, void *data);

/*
 * Creates a new interpreter structure.
 */
struct Fiz *fiz_create();

/*
 * Adds the auxillary functions to the interpreter
 */
void fiz_add_aux(struct Fiz *F);

/*
 * Deletes an interpreter structure.
 */
void fiz_destroy(struct Fiz *F);

/*
 * Executes a string in the interpreter.
 */
enum Fiz_Code fiz_exec(struct Fiz *F, const char *str);

void fiz_set_return(struct Fiz *F, const char *s);

void fiz_set_return_ex(struct Fiz *F, const char *fmt, ...);

const char *fiz_get_return(struct Fiz *F);

void fiz_set_var(struct Fiz *F, const char *name, const char *value);

void fiz_set_var_ex(struct Fiz *F, const char *name, const char *fmt, ...);

const char *fiz_get_var(struct Fiz *F, const char *name);

/*
 * Adds a C-function matching the fiz_func prototype to the
 * interpreter.
 */
void fiz_add_func(struct Fiz *F, const char *name, fiz_func fun, void *data);

void fiz_dict_insert(struct Fiz *F, const char *dict, const char *key, const char *value);

void fiz_dict_insert_ex(struct Fiz *F, const char *dict, const char *key, const char *fmt, ...);

const char *fiz_dict_find(struct Fiz *F, const char *dict, const char *key);

void fiz_dict_delete(struct Fiz *F, const char *dict, const char *key);

const char *fiz_dict_next(struct Fiz *F, const char *dict, const char *key);

/*=====================================================================
 * Some utility functions
 *====================================================================*/

/*
 * Reads an entire script file into memory.
 * The returned pointer should be free()'ed afterwards.
 */
char *fiz_readfile(const char *filename);

/* The expression evaluator used with the 'expr' command.
 * See expr.c for details */
int expr(const char *str, const char **err);

/*
 * Performs $variable substitution on a string. It also evaluates
 * statements within [angle brackets].
 * It was intended to evaluate the conditions of 'if' and
 * 'while' but is exposed in the API because it may be useful elsewhere.
 * The returned string is dynamic, so it must be free()'ed afterwards
 */
char *fiz_substitute(struct Fiz *F, const char *s);

/*
 * Helper function to report errors when the wrong number of 
 * parameters is passed to a command.
 * See any of the built-in functions for its usage.
 */
enum Fiz_Code fiz_argc_error(struct Fiz *F, const char *cmd, int exp);
