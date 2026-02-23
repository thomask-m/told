extern int exit_5() {
  asm volatile("mov $60, %rax; mov $5, %edi; syscall");
}
