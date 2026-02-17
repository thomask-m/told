// compile me but don't link me:
// gcc -c basic_main.c -o basic_main.o
// gcc -c add -o add.o

#include <stdio.h>

int add(int, int);

int main() {
  printf("%d\n", add(2, 3));
}
