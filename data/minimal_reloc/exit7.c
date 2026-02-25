int exit7() {
  asm volatile("mov $60, %rax; mov $7, %edi; syscall");
}

int exit119() {
  asm volatile("mov $60, %rax; mov $119, %edi; syscall");
}
