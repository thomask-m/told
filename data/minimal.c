// to compile but not link:
// gcc -c minimal.c -nostdlib -o minimal_system_link.o

// _start is the entrypoint if we don't have libc
int _start() {
  // x86 for exit(5);
  // syscall number for exit: 60 - we store it in rax by convention.
  // pass 5 to rdi/edi as argument.
  asm("mov $60, %rax; mov $5, %edi; syscall");
}
