// compile me but don't link me:
// gcc -c basic_main.c -o basic.o
// gcc -c exit_5.c -o exit_5.o
// ld / told exit_5.o basic.o -o exit_5

int exit_5();

int _start() {
  exit_5();
}
