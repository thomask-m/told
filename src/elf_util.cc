#include "elf_util.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace elf {

void parse_object(const std::string &file_path) {
  const fs::path obj_path{file_path};
  assert(fs::exists(obj_path) && "Input object file needs to exist");

  ElfBinary elf_binary;
  std::ifstream obj_file{obj_path, std::ios::binary};
  // N.B. - ifstream read doesn't actually handle endianness, it just
  //        reads the bytes in the order they're stored and interprets
  //        based on native endianness (so in this case, since I'm
  //        mostly testing this code on my debian x64 machine, it just
  //        so happens that it interprets the bytes in the way that I
  //        want.
  //        For the purposes of this toy linker - this is okay.
  obj_file.read(reinterpret_cast<char *>(&elf_binary.elf_header),
                sizeof(ElfHeader));

  assert_expected_elf_header(elf_binary.elf_header);
}

// Assert some basic assumptions that told makes about its given ELF object
// files.
void assert_expected_elf_header(const ElfHeader &elf_header) {
  unsigned char magic[4]{0x7f, 'E', 'L', 'F'};
  assert(std::memcmp(magic, elf_header.e_ident, SELFMAG) == 0 &&
         "Not an ELF file");
  assert(elf_header.e_ident[EI_CLASS] == ELFCLASS64 && "Not a 64-bit object");
  assert(elf_header.e_ident[EI_DATA] == ELFDATA2LSB && "Not little-endian");
  assert(elf_header.e_ident[EI_OSABI] == ELFOSABI_SYSV && "Not SystemV ABI");
  assert(elf_header.e_machine == EM_X86_64 && "Not AMD x64");
}

}  // namespace elf
