#include <stdio.h>

#define STN_USE_STRING
#include "../ext/stn.h"

int main()
{
    printf("Hello World!\n");
    printf("That string has %d characters\n", StringLength("Hello World!\n"));
    return 0;
}