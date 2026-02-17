/// told - toy/tom's linker
/// by thomask-m
///
/// The main goal is to take some simple set of ELF relocatable files and create
/// an actual loadable ELF executable. The target machine is my x86_64 linux machine.
///
/// Another "side-effect" goal is using this project to get better at writing
/// C++.
///
/// An extension that I'd like to see happen is profiling the linker and find
/// ways to speed it up.
///
/// None of this code was or will be written by an AI agent. This is first and
/// foremost a learning project so using an LLM or other tool that takes out the
/// actual coding part of the process would be antithetical to my own learning.

#include <iostream>
#include <string>

#include "elf_util.h"
#include "told.h"

void print_usage() {
  std::cerr << "Usage --\n";
  std::cerr << "  ./told FILE1 .. FILEN\n";
}

int main(int argc, char *argv[]) {
  if (argc == 1) {
    print_usage();
  }

  // Takes some filepaths that are supposed to be elf binaries and attempt to
  // link them into an executable
  for (int i = 1; i < argc; i++) {
    elf::parse_object(argv[i]);
  }
}
