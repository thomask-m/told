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

#include <iostream>
#include <string>
#include <vector>

#include "elf_util.h"
#include "told.h"

static const std::string executable_output_path = "a.out";

void print_usage() {
  std::cerr << "Usage --\n";
  std::cerr << "  ./told FILE1 .. FILEN\n";
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
    elf::ElfBinary module = elf::parse_object(argv[i]);
    std::cout << "module: " << module.given_path << std::endl;

    // need some way for the module to fetch the section names.

    for (auto sh_iter = module.section_headers.begin();
         sh_iter != module.section_headers.end(); ++sh_iter) {
      std::string name = sh_iter->first;
      // elf::ElfSectionHeader sh = sh_iter->second;
      std::cout << "  name: " << name << std::endl;
      // std::cout << "   section types: " << sh.sh_type << std::endl;
      // std::cout << "   section flags: " << sh.sh_flags << std::endl;
      // std::cout << "   section size: " << sh.sh_size << std::endl;
      // std::cout << "   addr: " << sh.sh_addr << std::endl;
      // std::cout << "   address align: " << sh.sh_addralign << std::endl;
      // std::cout << "   section header offset: " << sh.sh_offset << std::endl;

      // std::cout << "----" << std::endl;
    }
    modules.push_back(std::move(module));
  }

  auto e = told::convert_obj_to_exec(modules);
}
