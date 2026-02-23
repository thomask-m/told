extern int exit7() {
  asm volatile("mov $60, %rax; mov $7, %edi; syscall");
}
