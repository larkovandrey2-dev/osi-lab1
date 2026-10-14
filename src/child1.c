#include <ctype.h>
#include <stdio.h>
#include "os.h"

int main(void)
{
    if (os_init() == -1) {
        return 1;
    }

    if (setvbuf(stdout, NULL, _IONBF, 0) != 0) {
        fprintf(stderr, "Failed to disable stdout buffering\n");
        return 1;
    }

    int ch;

    while ((ch = getchar()) != EOF) {
        ch = tolower((unsigned char)ch);

        if (putchar(ch) == EOF) {
            perror("putchar");
            return 1;
        }
    }

    if (ferror(stdin)) {
        perror("getchar");
        return 1;
    }

    return 0;
}
