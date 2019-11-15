/*1 Fiz - A {/Tcl-like/} scripting language.
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

struct fiz;
typedef void (*Fiz_Abort_func)(struct fiz* F, void* data);

/*2 Interpreter Structure
 */
/*@ typedef struct fiz Fiz
 *# Main interpreter data structure.\n
 *# Create it with {{fiz_create()}}, and destroy it after use
 *# with {{fiz_destroy()}}
 */
typedef struct fiz {
	struct hash_tbl *commands;
	struct hash_tbl *dicts;
	struct fiz_callframe *callframe;
	char *return_val;
	char const* last_statement_begin;
	char const* last_statement_end;
	int abort;
	Fiz_Abort_func abort_func;
	void* abort_func_data;
} Fiz;

/*@ typedef enum fiz_code {FIZ_OK, FIZ_ERROR, FIZ_OOM, FIZ_RETURN, FIZ_CONTINUE, FIZ_BREAK} Fiz_Code;
 *# Values that can be returned by functions implementing the various commands.
 */
typedef enum fiz_code {FIZ_OK, FIZ_ERROR, FIZ_OOM, FIZ_RETURN, FIZ_CONTINUE, FIZ_BREAK} Fiz_Code;

/*@ typedef Fiz_Code (*fiz_func)(Fiz *f, int argc, char **argv, void *data);
 *# Prototype for C-functions that can be added to the interpreter.
 */
typedef Fiz_Code (*fiz_func)(Fiz *f, int argc, char **argv, void *data);

/*@ Fiz *fiz_create();
 *# Creates a new interpreter structure.
 */
Fiz *fiz_create();

/*@ void fiz_add_aux(Fiz *F);
 *# Adds the auxillary functions declared in {{auxfuns.c}} to the interpreter.\n
 *# These functions are not added by default for cases where a smaller interpreter
 *# may be preferred.
 */
void fiz_add_aux(Fiz *F);

/*@ void fiz_destroy(Fiz *F);
 *# Deletes an interpreter structure.
 */
void fiz_destroy(Fiz *F);

/*@ void fiz_abort(Fiz *F);
 *# Requests a gracefull abort of the currently running script.
 *# It is possible to attach a callback to the abort event using {{Fiz::abort_func}} 
 *# and {{Fiz::abort_func_data}}, which will be called just after executing this call
 *# from the thread of the callee.
 */
void fiz_abort(Fiz *F);

/*2 Executing the Interpreter
 */
 
/*@ Fiz_Code fiz_exec(Fiz *F, const char *str);
 *# Executes a string {{str}} in the interpreter {{F}}.
 */
Fiz_Code fiz_exec(Fiz *F, const char *str);

/*@ void fiz_add_func(Fiz *F, const char *name, fiz_func fun, void *data);
 *# Adds a C-function matching the {{fiz_func}} prototype to the
 *# interpreter.
 */
void fiz_add_func(Fiz *F, const char *name, fiz_func fun, void *data);

/*@ void fiz_set_return(Fiz *F, const char *s);
 *# Sets the return value of the command.
 */
void fiz_set_return(Fiz *F, const char *s);

/*@ void fiz_set_return_ex(Fiz *F, const char *fmt, ...);
 *# Sets the return value of the command.\n
 *# This version takes a {{printf()}} style format string and multiple
 *# arguments.
 */
void fiz_set_return_ex(Fiz *F, const char *fmt, ...);

/*@ const char *fiz_get_return(Fiz *F);
 *# Retrieves the return value of the last command.
 */
const char *fiz_get_return(Fiz *F);

/*@ void fiz_set_var(Fiz *F, const char *name, const char *value);
 *# Sets the value of a variable within the current callframe.
 */
void fiz_set_var(Fiz *F, const char *name, const char *value);

/*@ void fiz_set_var_ex(Fiz *F, const char *name, const char *fmt, ...);
 *# Sets the value of a variable within the current callframe.\n
 *# This version takes a {{printf()}} style format string and multiple
 *# arguments.
 */
void fiz_set_var_ex(Fiz *F, const char *name, const char *fmt, ...);

/*@ const char *fiz_get_var(Fiz *F, const char *name);
 *# Gets the value of a variable in the current callframe.
 */
const char *fiz_get_var(Fiz *F, const char *name);

/*2 Utility Functions
 */

/*@ char *fiz_readfile(const char *filename);
 *# Reads an entire script file into memory.
 *# The returned pointer should be {{free()}}'ed afterwards.
 */
char *fiz_readfile(const char *filename);

#ifdef FIZ_INTEGER_EXPR
/*@ int expr(const char *str, const char **err);
 *# The expression evaluator used with the {{expr}} command.
 *# See {{expr.c}} for details */
int expr(const char *str, const char **err);
#else
/*@ double expr(const char *str, const char **err);
 *# The expression evaluator used with the {{expr}} command.
 *# See {{expr.c}} for details */
double expr(const char *str, const char **err);
#endif

/*@ char *fiz_substitute(Fiz *F, const char *s);
 *# Performs $variable substitution on a string. It also evaluates
 *# statements within {{[angle brackets]}}.
 *# It was intended to evaluate the conditions of {{if}} and
 *# {{while}} but is exposed in the API because it may be useful elsewhere.
 *# The returned string is dynamic, so it must be {{free()}}'ed afterwards
 */
char *fiz_substitute(Fiz *F, const char *s);

/*@ Fiz_Code fiz_argc_error(Fiz *F, const char *cmd, int exp);
 *# Helper function to report errors when the wrong number of
 *# parameters is passed to a command.
 *# See any of the built-in functions for its usage.
 */
Fiz_Code fiz_argc_error(Fiz *F, const char *cmd, int exp);

/*@ Fiz_Code fiz_oom_error(Fiz *F);
 *# Helper function to report out of memory errors
 */
Fiz_Code fiz_oom_error(Fiz *F);

/*2 Functions for Manipulating Dictionaries
 */
 
/*@ void fiz_dict_insert(Fiz *F, const char *dict, const char *key, const char *value);
 *#
 */
void fiz_dict_insert(Fiz *F, const char *dict, const char *key, const char *value);

/*@ void fiz_dict_insert_ex(Fiz *F, const char *dict, const char *key, const char *fmt, ...);
 *#
 */
void fiz_dict_insert_ex(Fiz *F, const char *dict, const char *key, const char *fmt, ...);

/*@ const char *fiz_dict_find(Fiz *F, const char *dict, const char *key);
 *#
 */
const char *fiz_dict_find(Fiz *F, const char *dict, const char *key);

/*@ void fiz_dict_delete(Fiz *F, const char *dict, const char *key);
 *#
 */
void fiz_dict_delete(Fiz *F, const char *dict, const char *key);

/*@ const char *fiz_dict_next(Fiz *F, const char *dict, const char *key);
 *#
 */
const char *fiz_dict_next(Fiz *F, const char *dict, const char *key);

/*@ char *fiz_get_last_statement(Fiz *F, const char* body);
 *# Returns last statement that was executed by the engine,
 *# good for diagnostics or error reporting
 *# Will return "(none)" if no statement was captured
 *# Will return "(inaccessible)" if the memory was already deallocated
 *# The returned string is dynamic, so it must be {{free()}}'ed afterwards
 */
char *fiz_get_last_statement(Fiz *F, const char* body);

/*@ int fiz_get_location_of_last_statement(Fiz* F, const char** proc_name, const char* body);
 *# Returns the line number of the last statement or 0, when it could not find it
 *# The optional {{body}} pointer is used as the script body (if provided)
 *# The values are returned via pointers to {{line}} and {{proc_name}}, {{proc_name}} will be NULL if found in {{body}}
 */
int fiz_get_location_of_last_statement(Fiz* F, const char** proc_name, const char* body);

/*@ void fiz_set_return_normalized_double(Fiz* F, const double result);
 *# Sets the return value to double floating point number, 
 *# removing not needed trailing zeroes.
 */
void fiz_set_return_normalized_double(Fiz* F, const double result);
