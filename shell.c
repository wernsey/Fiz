/*
 * Simple interactive shell program.
 *
 * This is free and unencumbered software released into the public domain.
 * http://unlicense.org/
 */
#ifndef FIZ_DISABLE_INCLUDE_FILES
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

#include "fiz.h"

Fiz *F;
int sigint_called = 0;
void handle_sigint(int signal) {
    fprintf(stderr, "Caught signal %d, aborting\n", signal);
    fflush(stderr);
    if(sigint_called > 0) {
        fprintf(stderr, "Signal caught more than once, force quit\n");
        exit(EXIT_FAILURE);
    }
    sigint_called++;
    fiz_abort(F);
}

Fiz_Code shellfunc_delay(Fiz* F, int argc, char**argv, void* data)
{
    if(argc != 2) return fiz_argc_error(F, argv[0], 2);
    int msecs = atoi(argv[1]);
    if(msecs <= 0) {
        fiz_set_return_ex(F, "Incorrect delay (must be > 0, was '%s')", argv[1]);
        return FIZ_ERROR;
    }
    // inefficient, inprecise but abortable, should use something like select
    struct timespec millisecond;
    millisecond.tv_sec = 0;
    millisecond.tv_nsec = 1000000;
    for(int msec = 0; msec < msecs; msec++) {
        if(F->abort)
            return FIZ_OK;
        nanosleep(&millisecond, NULL);
    }
    return FIZ_OK;
}

#define PROMPT ">>> "

int main(int argc, char *argv[]) {
    Fiz_Code c;

    printf("== Fiz interpreter ==\n%s %s\n", __DATE__, __TIME__);

    F = fiz_create();

    fiz_add_aux(F);

    signal(SIGINT, handle_sigint); 
    fiz_add_func(F, "delay", shellfunc_delay, NULL);

    if(argc < 2) {
        char buffer[256];
        printf("Interactive mode; press Ctrl-D to exit\n%s", PROMPT);
        while(fgets(buffer, sizeof buffer, stdin)) {
            c = fiz_exec(F, buffer);
            if(c == FIZ_OK) {
                printf("ok: %s\n", fiz_get_return(F));
            } else if(c == FIZ_ERROR) {
                fprintf(stderr, "error: %s\n", fiz_get_return(F));
            } else if(c == FIZ_OOM) {
                fprintf(stderr, "out of memory error\n");
            }
            printf("%s", PROMPT);
        }
        printf("\n");
    } else {
        char *script = fiz_readfile(argv[1]);
        if(!script) {
            fprintf(stderr, "error: unable to read %s\n", argv[1]);
            fiz_destroy(F);
            return 1;
        }
        c = fiz_exec(F, script);
        if(c == FIZ_ERROR)
        {
            char* last_statement = fiz_get_last_statement(F);
            fprintf(stderr, "error: %s in \"%s\"\n", fiz_get_return(F), last_statement);
            free(last_statement);
        }
        else if(c == FIZ_OOM)
            fprintf(stderr, "out of memory error\n");
        free(script);
    }

    fiz_destroy(F);
    return 0;
}
#endif