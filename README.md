Fiz - A Tcl-like scripting language.

I was inspired to write a Tcl interpreter after I realised how simple
the language really was (it took me some time to become enlightened)
and how elegant the API for embedding it in another program could be.

I call it a "Tcl-like" language because I'm not too familiar 
with Tcl proper, so there may be some differences.

Due to limits in the expression evaluator you can't just write an
if- or while-statement condition like

	if {$x<2} {puts "Yea"};

because the {$x<2} will be treated as a command. You should rather use 
the 'expr' command, as follows:

	if {expr $x<2} {puts "Yea"};

This interpreter goes for simplicity. It uses strings to represents all
variables, with the result that it has to use atoi() and snprintf() to
convert to and from numbers when performing arithmetic. It also does a 
lot of strdup()ing internally. The result is that that it could be too
slow for some tastes. Please don't hold it against me.

Another note:
I have never seen a call to malloc() and friends fail. Therefore I have
removed all checks on the return values of these functions to make the
interpreter a little bit leaner. I would not recommend using the interpreter
in environments where you may run out of memory.

Iacknowledge that I took some inspiration (like how the 
callframes are handled and several of the API functions) from 
Salvatore Sanfilippo's Picol Tcl interpreter, which can be found at 
http://antirez.com/page/picol. 

This implementation is entirely my own, however. It was implemented from
scratch without refering to Picol's code.
