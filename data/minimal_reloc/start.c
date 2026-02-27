// compile me but don't link me:
// gcc -c start.c -o start.o
// gcc -c exit.c -o exit.o
// ld / told exit.o start.o

extern int exit7();

extern int exit119();

int _start() {
  int a = 100 + 19; // edit this value to get different results.
  if (a != 119) {
    exit7();
  } else {
    exit119();
  }
}
