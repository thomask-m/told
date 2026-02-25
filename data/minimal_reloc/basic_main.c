// compile me but don't link me:
// gcc -c basic_main.c -o basic.o
// gcc -c exit7.c -o exit7.o
// ld / told exit7.o basic.o -o exit7
//
// The output assembly looks like this with no relocs.
// 0:  55                      push   rbp
// 1:  48 89 e5                mov    rbp,rsp
// 4:  48 c7 c0 3c 00 00 00    mov    rax,0x3c
// b:  bf 07 00 00 00          mov    edi,0x7
// 10: 0f 05                   syscall
// 12: 90                      nop
// 13: 5d                      pop    rbp
// 14: c3                      ret
// 15: 55                      push   rbp
// 16: 48 89 e5                mov    rbp,rsp
// 19: b8 00 00 00 00          mov    eax,0x0
// 1e: e8 00 00 00 00          call   0x23
// PSSST NOTICE THIS GUY HERE ^ NO RELOCS
// 23: 90                      nop
// 24: 5d                      pop    rbp
// 25: c3                      ret

extern int exit7();

extern int exit119();

int _start() {
  if (0) {
    exit7();
  } else {
    exit119();
  }
}
