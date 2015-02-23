/*
 * 
 * This is free and unencumbered software released into the public domain.
 * http://unlicense.org/
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>

#include "fiz.h"
#include "hash.h"

/* Size of the internal buffer used for the *_ex() functions */
#define EX_BUFFER_SIZE 128

/* Initial maximum size of a word (but it gets resized as needed) */
#define INITIAL_WORD_SIZE 40

/* Initial number of arguments (but it gets resized as needed) */
#define INITIAL_NUM_ARGS 5

/*
 * Internal structure to store C-functions and procs
 */
struct proc {
	enum {FIZ_PROC, FIZ_CFUN} type;
	union {
		struct {fiz_func fun; void *data;} cfun;
		struct {char *params; char *body;} proc;
	} fun;
};

/*
 * Manages the "stack" (and variable scope) when calling procedures.
 */
struct fiz_callframe {
	struct fiz_callframe *parent;
	struct hash_tbl *vars;
};

/*======================================================================
 * Data structure for the parser used internally.
======================================================================*/

/*
 * On a high level, the parser works by reading a character from
 * 'FooParser.txt'. If it is a '"', '$' quote or a '[', substitution 
 * is performed and the result added to 'FooParser.word', otherwise the
 * character is added to 'FooParser.word' directly.
 * 'FooParser.word' is dynamically resized as the need arises.
 */
struct FizParser 
{
	const char *txt;
	char *word;
	size_t w_size, a_size;
};

enum FI_CODE 
{
	FI_WORD,  /* A word was read */
	FI_EOS,   /* End of statement was read */
	FI_EOI,   /* End of input was read */
	FI_ERR    /* Internal error */
};

static int init_parser(struct FizParser *FI, const char *txt)
{
	FI->a_size = INITIAL_WORD_SIZE;
	FI->word = malloc(FI->a_size);
	FI->w_size = 0;
	FI->txt = txt;
	return 1;
}

static void destroy_parser(struct FizParser *FI)
{
	free(FI->word);	
}

/* Adds a single character to the word being parsed */
static void add_char(struct FizParser *FI, char c)
{
	if(FI->w_size + 1 == FI->a_size - 1)
	{
		FI->a_size <<= 1;
		FI->word = realloc(FI->word, FI->a_size);
	}
	FI->word[FI->w_size++] = c;
	FI->word[FI->w_size] = '\0';
	assert(strlen(FI->word) < FI->a_size);
	assert(strlen(FI->word) == FI->w_size);
}

/* Adds a string to the word being parsed */
static void add_word(struct FizParser *FI, const char *w)
{
	size_t wlen = strlen(w);
	if(FI->w_size + wlen >= FI->a_size - 1)
	{
		while(FI->w_size + wlen >= FI->a_size - 1)
			FI->a_size <<= 1;
		FI->word = realloc(FI->word, FI->a_size);
	}
	
	strcat(FI->word, w);	
	FI->w_size += wlen;
	FI->word[FI->w_size] = '\0';
	assert(strlen(FI->word) < FI->a_size);
	assert(strlen(FI->word) == FI->w_size);
}

/* Handles escape sequences */
static char get_escape(char c)
{
	switch(c)
	{
		case 'n' : return '\n';
		case 'r' : return '\r';
		case 't' : return '\t';
		default: return c; 
	}
}

/*====================================================================
 * The parser 
 *====================================================================*/

/*
 * Used to parse [a b c] and "Hello, $x" words.
 * 'term' specifies the terminating character ']' or '"'
 */
static enum FI_CODE parse_quote(struct Fiz *F, struct FizParser *FI, char term)
{
	while(FI->txt[0] != term)
	{	
		char c = FI->txt[0];	
		if(!c) 
		{
			fiz_set_return_ex(F, "Missing \'%c\'", term);
			return FI_ERR;
		}
		
		if(c == '[')
		{
			enum FI_CODE fic;
			struct FizParser FIi;
			init_parser(&FIi, ++FI->txt);
			fic = parse_quote(F, &FIi, ']');
			if(fic == FI_WORD)
			{								
				/* Evaluate the [expression]*/					
				if(fiz_exec(F, FIi.word) != FIZ_OK)
					fic = FI_ERR;		
				add_word(FI, fiz_get_return(F));
			}
			FI->txt = FIi.txt;
			destroy_parser(&FIi);
			if(fic != FI_WORD) return fic;
			continue;	
		}
		else if(c == '$')
		{
			/* Variable substitution, like $foo */
			const char *p, *val;
			char *name;
			p = ++FI->txt;
			while(isalnum(*FI->txt)) FI->txt++;
			if(FI->txt == p)
			{
				fiz_set_return(F, "Identifier expected after $");
				return FI_ERR;
			}				
			name = malloc(FI->txt - p + 1);
			strncpy(name, p, FI->txt-p);
			name[FI->txt-p] = '\0';
			val = fiz_get_var(F, name);								
			if(!val)
			{					
				fiz_set_return_ex(F, "Unknown variable '%s'", name);
				free(name);
				return FI_ERR;
			}
			free(name);
			add_word(FI, val);
			continue; /* Skip the add_char() below */
		}
		else if(c == '\\')
		{				
			FI->txt++;			
			c = get_escape(FI->txt[0]);
		}	
			
		add_char(FI, c);
		FI->txt++;		
	}
	assert(FI->txt[0] == term);
	FI->txt++;
	return FI_WORD;
}

/* This handles complex cases like quotes within braces, eg: {["foo"]}
 */
static enum FI_CODE gobble_quote(struct Fiz *F, struct FizParser *FI, char term)
{		
	add_char(FI, *(FI->txt++));
	while(FI->txt[0] != term)
	{				
		char c = FI->txt[0];
		if(!c)
		{
			fiz_set_return_ex(F, "Missing \'%c\'", term, FI->word);
			return FI_ERR;
		}
		else if(c == '\\')
		{
			add_char(FI, c);
			c = *(++FI->txt);
		}		
		else if(c == '[' || c == '"')
		{
			enum FI_CODE fic = gobble_quote(F, FI, (c == '[')?']':'"');
			if(fic != FI_WORD)
				return fic;
			continue;
		}				
		add_char(FI, c);
		FI->txt++;
	}
	add_char(FI, *(FI->txt++));
	return FI_WORD;
}

/*
 * Parses words in braces, like {puts "hello"}
 */
static enum FI_CODE parse_brace(struct Fiz *F, struct FizParser *FI)
{
	int level = 1;
	for(;;)
	{	
		char c = FI->txt[0];
		
		if(c == '[' || c == '"')
		{
			enum FI_CODE fic = gobble_quote(F, FI, (c == '[')?']':'"');
			if(fic != FI_WORD)
				return fic;
			continue;
		}
		
		switch(c)
		{
		case '\0':
			{
				fiz_set_return(F, "Missing \'}\'");
				return FI_ERR;
			}
		case '{': level++; break;
		case '}':
			{ 
				if(--level == 0)
				{
					FI->txt++;
					return FI_WORD;	
				}
			} break;
		case '\\': 
			add_char(FI, c);
			c = *(++FI->txt); 
			break;		
		}		
		add_char(FI, c);
		FI->txt++;
	}
}

/*
 * Entry point for the parser: It chooses the type of word
 * read depending on the next character read and returns that
 * word in the FooParser structure, skipping any whitespace and
 * comments in the process.
 */
static enum FI_CODE get_word(struct Fiz *F, struct FizParser *FI)
{
	/* Reset the interpreter's word tracker */
	FI->word[0] = '\0';
	FI->w_size = 0;
	
restart:
	/* Remove whitespace */
	while(isspace(FI->txt[0])) 
	{
		if(FI->txt[0] == '\n')
		{
			FI->txt++;
			return FI_EOS;
		}
		FI->txt++; 
	}
	
	/* End of input? */
	if(FI->txt[0] == '\0')
		return FI_EOI;
	else if(FI->txt[0] == ';')
	{		
		FI->txt++; 
		return FI_EOS;
	}
	else if(FI->txt[0] == '#') /* Comment */
	{
		while(FI->txt[0] != '\n')
		{
			if(!FI->txt[0]) return FI_EOS;
			FI->txt++;
		}
		assert(FI->txt[0] == '\n');
		/*FI->txt++;*/
		goto restart;
	}
	
	if(FI->txt[0] == '\"')
	{
		FI->txt++;
		return parse_quote(F, FI, '\"');
	}
	else if(FI->txt[0] == '[')
	{
		enum FI_CODE fic;
		struct FizParser FIi;
		init_parser(&FIi, ++FI->txt);
		fic = parse_quote(F, &FIi, ']');
		if(fic == FI_WORD)
		{				
			/* Evaluate the [expression]*/	
			if(fiz_exec(F, FIi.word) != FIZ_OK)
				fic = FI_ERR;
			add_word(FI, fiz_get_return(F));
		}
		FI->txt = FIi.txt;
		destroy_parser(&FIi);
		return fic;		
	}
	else if(FI->txt[0] == '{')
	{
		FI->txt++;
		return parse_brace(F, FI);
	}
	else /* Must be a printable character */
	{
		do {	
			char c = FI->txt[0];	
			if(c == '$')
			{
				/* Variable substitution, like $foo */
				const char *p, *val;
				char *name;
				p = ++FI->txt;
				while(isalnum(*FI->txt)) FI->txt++;
				if(FI->txt == p)
				{
					fiz_set_return(F, "Identifier expected after $");
					return FI_ERR;
				}				
				name = malloc(FI->txt - p + 1);
				strncpy(name, p, FI->txt-p);
				name[FI->txt-p] = '\0';
				val = fiz_get_var(F, name);								
				if(!val)
				{					
					fiz_set_return_ex(F, "Unknown variable '%s'", name);
					free(name);
					return FI_ERR;
				}
				free(name);
				add_word(FI, val);
				continue; /* Skip the add_char() below */
			}
			else if(c == '\\')
			{				
				FI->txt++;
				c = get_escape(FI->txt[0]);
			}	
			else if(c == ';' || c == '[') /* Handle cases like bar; or b[d e]*/ 
				break;	
				
			add_char(FI, c);
			FI->txt++;
		} while(FI->txt[0] && !isspace(FI->txt[0]));
		return FI_WORD;
	}
	return FI_ERR; /* keeps some compilers happy */
}

/*====================================================================
 * The interpreter 
 *====================================================================*/

static void add_callframe(struct Fiz *F)
{
	struct fiz_callframe *cf = malloc(sizeof *cf);
	cf->vars = ht_create(0);
	cf->parent = F->callframe;
	F->callframe = cf;
}

static void delete_callframe(struct Fiz *F)
{
	struct fiz_callframe *cf = F->callframe;
	assert(F->callframe);
	F->callframe = cf->parent;	
	ht_free(cf->vars, free);
	free(cf);
}

static void add_bifs(struct Fiz *F);

struct Fiz *fiz_create()
{
	struct Fiz *F = malloc(sizeof *F);
	F->callframe = NULL;
	add_callframe(F);
	F->commands = ht_create(0);
	F->dicts = ht_create(16);
	F->return_val = strdup("");
	add_bifs(F);
	return F;
}

static void free_proc(void *vp)
{
	struct proc *p = vp;
	if(p->type == FIZ_PROC)
	{
		free(p->fun.proc.params);
		free(p->fun.proc.body);		
	}
	free(p);		
}

static void free_dict(void *vp)
{
	struct hash_tbl *d = vp;
	ht_free(d, free);
}

void fiz_destroy(struct Fiz *F)
{
	if(!F) return;
	ht_free(F->commands, free_proc); 
	ht_free(F->dicts, free_dict); 
	free(F->return_val);
	delete_callframe(F);
	free(F);
}

static void clear_argv(int argc, char **argv)
{
	int i;
	for(i = 0; i < argc; i++)
	{
		free(argv[i]);
		argv[i] = NULL;
	}
}

enum Fiz_Code fiz_exec(struct Fiz *F, const char *str)
{
	enum FI_CODE fic;
	struct FizParser FI;
	enum Fiz_Code rc = FIZ_OK;
	
	/* Where to store the parameters */
	char **argv;
	int a_argc = INITIAL_NUM_ARGS, argc;
		
	init_parser(&FI, str);
	argv = calloc(a_argc, sizeof *argv);
		
	for(;;) /* For all the statements in the input */
	{
		struct proc *p;
		
		fic = get_word(F, &FI); /* get the command */
		if(fic == FI_EOI) break;
		if(fic == FI_EOS) continue;
		
		/* Get the parameters */
		argc = 0;
		argv[argc++] = strdup(FI.word);
		while((fic = get_word(F, &FI)) == FI_WORD)
		{
			if(argc == a_argc)
			{
				a_argc += INITIAL_NUM_ARGS;
				argv = realloc(argv, a_argc * sizeof *argv);
			}
			
			argv[argc++] = strdup(FI.word);
		}
		if(fic == FI_ERR)
			goto clean_error;
			
		/* Evaluate! */		
		p = ht_find(F->commands, argv[0]);
		if(!p)
		{
			fiz_set_return_ex(F, "undefined command '%s'", argv[0]);
			goto clean_error;			
		}
		
		if(p->type == FIZ_CFUN)
		{
			/* External C-function */
			rc = p->fun.cfun.fun(F, argc, argv, p->fun.cfun.data);
		}
		else
		{			
			/* Script defined procedure */
			char *pars = strdup(p->fun.proc.params), *c, *n;
			int i = 1, brk = 0;
			add_callframe(F);
			for(n=pars; !brk && *n; n++, i++)
			{
				while(n[0] && isspace(n[0])) n++;
				if(!n[0]) break;
				c = n;
				while(n[0] && !isspace(n[0])) n++;
				if(!n[0]) brk = 1;
				n[0] = '\0';
				if(i < argc)			
					fiz_set_var(F, c, argv[i]);				
			}				
			free(pars);		
			if(i != argc)
			{
				fiz_set_return_ex(F, "'%s' wanted %d parameters, but got %d", argv[0], i-1, argc - 1);
				delete_callframe(F);
				goto clean_error;	
			}			
			rc = fiz_exec(F, p->fun.proc.body);
			if(rc == FIZ_RETURN) rc = FIZ_OK;
			delete_callframe(F);
		}	
		clear_argv(argc, argv);
		if(rc != FIZ_OK) break;
	}
	
	free(argv);
	destroy_parser(&FI);
	return rc;
clean_error:
	clear_argv(argc, argv);
	free(argv);
	destroy_parser(&FI);
	return FIZ_ERROR;
}

/*====================================================================
 * Support API Functions 
 *====================================================================*/

const char *fiz_get_return(struct Fiz *F)
{
	assert(F->return_val);
	return F->return_val;
}

void fiz_set_return(struct Fiz *F, const char *s)
{	
	assert(F->return_val);	
	free(F->return_val);
	F->return_val = strdup(s);
}

void fiz_set_return_ex(struct Fiz *F, const char *fmt, ...)
{
	char buffer[EX_BUFFER_SIZE];
	va_list arg;
	va_start (arg, fmt);
  	vsnprintf (buffer, EX_BUFFER_SIZE-1, fmt, arg);
  	va_end (arg);
	fiz_set_return(F, buffer);
}

const char *fiz_get_var(struct Fiz *F, const char *name)
{
	assert(F->callframe);
	return ht_find(F->callframe->vars, name);
}

void fiz_set_var(struct Fiz *F, const char *name, const char *value)
{
	assert(F->callframe);
	ht_insert(F->callframe->vars, name, strdup(value));
}

void fiz_set_var_ex(struct Fiz *F, const char *name, const char *fmt, ...)
{
	char buffer[EX_BUFFER_SIZE];
	va_list arg;
	va_start (arg, fmt);
  	vsnprintf (buffer, EX_BUFFER_SIZE-1, fmt, arg);
  	va_end (arg);
	fiz_set_var(F, name, buffer);
}

void fiz_add_func(struct Fiz *F, const char *name, fiz_func fun, void *data)
{
	struct proc *p;
	p = malloc(sizeof *p);
	p->type = FIZ_CFUN;
	p->fun.cfun.fun = fun;
	p->fun.cfun.data = data;	
	ht_insert(F->commands, name, p);
}

void fiz_dict_insert(struct Fiz *F, const char *dict, const char *key, const char *value)
{
	char *v;
	struct hash_tbl *d = ht_find(F->dicts, dict);
	if(!d)
	{
		/* Create the dict if it doesn't exist */
		d = ht_create(16);
		ht_insert(F->dicts, dict, d);
	}
	/* Delete the key if it's already in the dict */
	v = ht_delete(d, key);
	if(v) free(v);
	/* Insert the value into the dict */
	ht_insert(d, key, strdup(value));
}

char *fiz_substitute(struct Fiz *F, const char *s)
{
	struct FizParser FI;
	char *subs;
	/* Misuse parse_quote to perform the substitution */
	init_parser(&FI, s);
	if(parse_quote(F, &FI, '\0') != FI_WORD)
	{
		destroy_parser(&FI);
		return NULL;
	}
	subs = strdup(FI.word);
	destroy_parser(&FI);
	return subs;
}

/*====================================================================
 * Functions to manipulate dicts
 *====================================================================*/

void fiz_dict_insert_ex(struct Fiz *F, const char *dict, const char *key, const char *fmt, ...)
{
	char buffer[EX_BUFFER_SIZE];
	va_list arg;
	va_start (arg, fmt);
  	vsnprintf (buffer, EX_BUFFER_SIZE-1, fmt, arg);
  	va_end (arg);
	fiz_dict_insert(F, dict, key, buffer);
}

const char *fiz_dict_find(struct Fiz *F, const char *dict, const char *key)
{
	struct hash_tbl *d = ht_find(F->dicts, dict);
	if(!d) /* Undefined dictionary */
		return NULL; 
	return ht_find(d, key);
}

void fiz_dict_delete(struct Fiz *F, const char *dict, const char *key)
{
	char *v;
	struct hash_tbl *d = ht_find(F->dicts, dict);
	if(!d) /* Undefined dictionary */
		return; 
	v = ht_delete(d, key);
	if(v) free(v);
}

const char *fiz_dict_next(struct Fiz *F, const char *dict, const char *key)
{
	struct hash_tbl *d = ht_find(F->dicts, dict);
	if(!d) /* Undefined dictionary */
		return NULL; 
	return ht_next(d, key);
}

/*====================================================================
 * Built-in functions 
 * These functions require some intimate knowledge of the interpreter's
 * implementation, so that cannot be moved to a different file
 *====================================================================*/

enum Fiz_Code fiz_argc_error(struct Fiz *F, const char *cmd, int exp)
{
	fiz_set_return_ex(F, "%s expected %d words", cmd, exp);
	return FIZ_ERROR;
}

static enum Fiz_Code bif_set(struct Fiz *F, int argc, char **argv, void *data)
{
	if(argc != 3) 
		return fiz_argc_error(F, argv[0], 3);
	fiz_set_var(F, argv[1], argv[2]);
	fiz_set_return(F, argv[2]);
	return FIZ_OK;
}

static enum Fiz_Code bif_proc(struct Fiz *F, int argc, char **argv, void *data)
{
	struct proc *p;
	if(argc != 4) 
		return fiz_argc_error(F, argv[0], 4);
	p = malloc(sizeof *p);
	p->type = FIZ_PROC;
	p->fun.proc.params = strdup(argv[2]);
	p->fun.proc.body = strdup(argv[3]);
	ht_insert(F->commands, argv[1], p);
	fiz_set_return(F, argv[1]);
	return FIZ_OK;
}

static enum Fiz_Code bif_return(struct Fiz *F, int argc, char **argv, void *data)
{
	if(argc != 2) 
		return fiz_argc_error(F, argv[0], 2);
	fiz_set_return(F, argv[1]);
	return FIZ_RETURN;
}

static enum Fiz_Code bif_if(struct Fiz *F, int argc, char **argv, void *data)
{
	if(argc != 3 && argc != 5) 
		return fiz_argc_error(F, argv[0], 3);
	if(argc == 5 && strcmp(argv[3], "else"))
	{
		fiz_set_return(F, "4th parameter must be else in 'if'");			
		return FIZ_ERROR;
	}
	if(fiz_exec(F, argv[1]) != FIZ_OK)
		return FIZ_ERROR;
	if(atoi(fiz_get_return(F)))
		return fiz_exec(F, argv[2]);		
	if(argc == 5)
		return fiz_exec(F, argv[4]);
	return FIZ_OK;
}

static enum Fiz_Code bif_while(struct Fiz *F, int argc, char **argv, void *data)
{
	enum Fiz_Code fc = FIZ_OK;
	if(argc != 3) 
		return fiz_argc_error(F, argv[0], 3);
	for(;;)
	{
		if(fiz_exec(F, argv[1]) != FIZ_OK) 
			return FIZ_ERROR;		
		if(!atoi(fiz_get_return(F))) break;
		fc = fiz_exec(F, argv[2]);
		if(fc == FIZ_BREAK) break;
		else if(fc == FIZ_ERROR) return FIZ_ERROR;
	}
	return FIZ_OK;
}

static enum Fiz_Code bif_cntrl(struct Fiz *F, int argc, char **argv, void *data)
{
	if(argc != 1) 
		return fiz_argc_error(F, argv[0], 1);
	if(!strcmp(argv[0], "continue")) return FIZ_CONTINUE;
	return FIZ_BREAK;
}

static void add_bifs(struct Fiz *F)
{
	fiz_add_func(F, "set", bif_set, NULL);
	fiz_add_func(F, "proc", bif_proc, NULL);
	fiz_add_func(F, "return", bif_return, NULL);
	fiz_add_func(F, "if", bif_if, NULL);
	fiz_add_func(F, "while", bif_while, NULL);
	fiz_add_func(F, "break", bif_cntrl, NULL);
	fiz_add_func(F, "continue", bif_cntrl, NULL);
}
