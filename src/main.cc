/// told - toy/tom's linker
/// by thomask-m
///
/// The main goal is to take some simple set of ELF relocatable files and create
/// an actual loadable ELF executable. The target machine is my x86_64 linux
/// machine.
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

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "told.h"

namespace fs = std::filesystem;

void print_usage() {
  std::cerr << "Usage --\n";
  std::cerr << "  ./told FILE1 .. FILEN\n";
}

void make_executable() {
  fs::path binary {"a.told"};
  if (fs::exists(binary)) {
    std::cout << "told: -- making binary executable" << std::endl;;
    fs::permissions(binary, fs::perms::all ^ fs::perms::others_write);
  }
}

// g++ main.cc elf_util.cc -o main --std=c++17 && ./main ../data/basic_main.o
// ../data/add.o
int main(int argc, char *argv[]) {
  if (argc == 1) {
    print_usage();
  }

  // Takes some filepaths that are supposed to be elf binaries and attempt to
  // link them into an executable
  std::vector<elf::ElfBinary> modules{};
  modules.reserve(argc - 1);
  for (int i = 1; i < argc; i++) {
    elf::ElfBinary module = told::parse_object(argv[i]);
    modules.push_back(std::move(module));
  }

  auto e = told::convert_obj_to_exec(modules);
  told::write_out(e);
  make_executable();
}
