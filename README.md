# Fiz - A Tcl-like scripting language.

I was inspired to write a Tcl interpreter after I realised how simple
the language really was (it took me some time to become enlightened)
and how elegant the API for embedding it in another program could be.

I call it a _Tcl-like_ language because it doesn't attempt to implement all the
features of Tcl. It has several useful features though, like `dict` and `expr`
and so forth.

Due to limits in the expression evaluator you can't just write an
`if`- or `while`-statement condition like

    if {$x<2} {puts "Yea"};

because the `{$x<2}` will be treated as a command. You should rather use 
the `expr` command, as follows:

    if {expr $x<2} {puts "Yea"};

This interpreter goes for simplicity. It uses strings to represents all
variables, with the result that it has to use `atoi()` and `snprintf()` to
convert to and from numbers when performing arithmetic. It also does a 
lot of `strdup()`ing internally. The result is that that it could be too
slow for some tastes. Please don't hold it against me.

Another note: I have removed all checks on the return values of `malloc()` and 
friends functions to make the interpreter a little bit leaner. I would not 
recommend using the interpreter in environments where you may run out of memory.

I took some inspiration (like how the callframes are handled and several of the 
API functions) from Salvatore Sanfilippo's Picol Tcl interpreter, which can be 
found at http://antirez.com/page/picol. This is, however, a new implementation.


## Compile time flags and options

To make Fiz usable in different configurations, some compile time flags have
been added. Define these in your environment for the desired effect.

* `FIZ_DISABLE_INCLUDE_FILES` - disables `fiz_readfile` function, `include`
  command and the interactive shell. Provided for embedding into other 
  applications

* `FIZ_INTEGER_EXPR` - changes the floating point expression evaluation
  to use integers

These options are also available:

* `FIZ_OVERRIDE_HASH_DEFAULT_SIZE` - set to override default hash size (default 512)
* 