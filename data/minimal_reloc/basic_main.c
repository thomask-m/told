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

// TODO(02-23-2026) -
// Here are the problems with my output executable:
// 1. It's starting straight from the first line of assembly in the text segment.
//    That is no good because that isn't necessarily my program's entrypoint.
//    I need to actually have the program entrypoint address set to the address
//    of _start.
// 2. There are no relocs happening. That call instruction needs to have an actual
//    relative address written.
// 3. I need better ways to actually assess the state of my executables. This will
//    mean passing along more debug info from the other sections that I didn't do
//    because I was too lazy :p

extern int exit7();

int _start() {
  exit7();
}
