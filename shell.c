/*
 * Simple interactive shell program.
 *
 * This is free and unencumbered software released into the public domain.
 * http://unlicense.org/
 */
#ifndef FIZ_DISABLE_INCLUDE_FILES
#include <stdio.h>
#include <stdlib.h>

#include "fiz.h"

#define PROMPT ">>> "

int main(int argc, char *argv[]) {
    Fiz *F;
    Fiz_Code c;

    printf("== Fiz interpreter ==\n%s %s\n", __DATE__, __TIME__);

    F = fiz_create();

    fiz_add_aux(F);

    if(argc < 2) {
        char buffer[256];
        printf("Interactive mode; press Ctrl-D to exit\n%s", PROMPT);
        while(fgets(buffer, sizeof buffer, stdin)) {
            c = fiz_exec(F, buffer);
            if(c == FIZ_OK) {
                printf("ok: %s\n", fiz_get_return(F));
            } else if(c == FIZ_ERROR) {
                fprintf(stderr, "error: %s\n", fiz_get_return(F));
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
            fprintf(stderr, "error: %s\n", fiz_get_return(F));
        free(script);
    }

    fiz_destroy(F);
    return 0;
}
#endif