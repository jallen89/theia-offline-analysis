#include <stdio.h>

int foo() {
    char buf[1];
    FILE *f = fopen("dummy.txt", "rb");
    fread(buf, 1, 10, f);
    fclose(f);
    return 0;
}

int main() {
    foo();
    printf("failed\n");
    return 0;
}
