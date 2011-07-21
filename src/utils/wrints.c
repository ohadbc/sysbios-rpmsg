#include <stdio.h>
#include <stdlib.h>

int main(int argc, char * argv[])
{
    int i;
    unsigned int word;

    for (i = 1; i < argc; i++) {
        word = strtoul(argv[i], NULL, 0);
        fwrite(&word, 4, 1, stdout);
    }

    return 0;
}
