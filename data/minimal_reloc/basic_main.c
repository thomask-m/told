// compile me but don't link me:
// gcc -c basic_main.c -o basic.o
// gcc -c exit7.c -o exit7.o
// ld / told exit7.o basic.o -o exit7

extern int exit7();

int _start() {
  exit7();
}
