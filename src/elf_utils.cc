#include "elf_utils.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace elf {

SectionType s_type_from_name(const std::string &n) {
  if (n == ".text") {
    return SectionType::Text;
  } else if (n == ".data") {
    return SectionType::Data;
  } else if (n == ".symtab") {
    return SectionType::SymTable;
  } else if (n == ".strtab") {
    return SectionType::StrTable;
  }
  return SectionType::None;
}

ElfBinary parse_object(const std::string &file_path) {
  const fs::path obj_path{file_path};
  assert(fs::exists(obj_path) && "Input object file needs to exist");

  fs::path canonicalized_path = fs::canonical(obj_path);
  ElfBinary module{canonicalized_path};
  std::ifstream obj_file{obj_path, std::ios::binary};
  // N.B. - ifstream read doesn't actually handle endianness, it just
  //        reads the bytes in the order they're stored and interprets
  //        based on native endianness (so in this case, since I'm
  //        mostly testing this code on my debian x64 machine, it just
  //        so happens that it interprets the bytes in the way that I
  //        want).
  //        For the purposes of this toy linker - this is okay.
  obj_file.read(reinterpret_cast<char *>(&module.elf_header),
                sizeof(ElfHeader));
  assert_expected_elf_header(module.elf_header);

  obj_file.seekg(module.elf_header.e_shoff);
  std::vector<ElfSectionHeader> s_headers(module.elf_header.e_shnum);
  for (size_t i = 0; i < module.elf_header.e_shnum; ++i) {
    obj_file.read(reinterpret_cast<char *>(&s_headers[i]),
                  sizeof(ElfSectionHeader));
  }

  std::unordered_map<SectionType, ElfSectionHeader>
      section_headers_with_types{};
  section_headers_with_types.reserve(s_headers.size());

  uint64_t shstr_offset = s_headers[module.elf_header.e_shstrndx].sh_offset;
  for (size_t i = 0; i < s_headers.size(); ++i) {
    std::string name{};
    obj_file.seekg(shstr_offset + s_headers[i].sh_name);
    std::getline(obj_file, name, '\0');
    // std::cout << "section name: " << name << std::endl;
    section_headers_with_types.emplace(s_type_from_name(name), s_headers[i]);
  }

  module.section_headers = std::move(section_headers_with_types);

  std::unordered_map<SectionType, std::vector<char>> sections_with_types;
  sections_with_types.reserve(s_headers.size());
  for (const auto &sh : module.section_headers) {
    if (sh.first != SectionType::Text) // are there other sections are basically
                                       // just blocks of data?
      continue;
    std::vector<char> section_data_buffer(sh.second.sh_size);

    obj_file.seekg(sh.second.sh_offset);
    obj_file.read(section_data_buffer.data(), sh.second.sh_size);

    sections_with_types.emplace(sh.first, std::move(section_data_buffer));
  }
  module.sections = std::move(sections_with_types);

  std::unordered_map<Symbol, ElfSymbolTableEntry> sym_table;
  ElfSectionHeader sym_table_header =
      module.section_headers.at(SectionType::SymTable);
  ElfSectionHeader str_table_header =
      module.section_headers.at(SectionType::StrTable);
  for (size_t i = 0; i < sym_table_header.sh_size;
       i += sym_table_header.sh_entsize) {
    obj_file.seekg(sym_table_header.sh_offset + i);
    ElfSymbolTableEntry ste{};
    obj_file.read(reinterpret_cast<char *>(&ste), sizeof(ElfSymbolTableEntry));
    // std::cout << "symbol table st_name: " << ste.st_name << std::endl;
    std::cout << "symbol table st_bind: " << ELF64_ST_BIND(ste.st_info)
              << " st_type: " << ELF64_ST_TYPE(ste.st_info) << std::endl;
    obj_file.seekg(str_table_header.sh_offset + ste.st_name);
    Symbol s_name{};
    std::getline(obj_file, s_name, '\0');
    std::cout << "symbol table name: " << s_name << std::endl;
  }
  module.symbol_table = std::move(sym_table);
  return module;
}

// Assert some basic assumptions that linker makes about its given ELF object
// files.
void assert_expected_elf_header(const ElfHeader &elf_header) {
  unsigned char magic[4]{0x7f, 'E', 'L', 'F'};
  assert(std::memcmp(magic, elf_header.e_ident, SELFMAG) == 0 &&
         "Not an ELF file");
  assert(elf_header.e_ident[EI_CLASS] == ELFCLASS64 && "Not a 64-bit object");
  assert(elf_header.e_ident[EI_DATA] == ELFDATA2LSB && "Not little-endian");
  assert(elf_header.e_ident[EI_OSABI] == ELFOSABI_SYSV && "Not SystemV ABI");
  assert(elf_header.e_machine == EM_X86_64 && "Not AMD x64");
  assert(elf_header.e_shstrndx != SHN_UNDEF &&
         "Section header string table section needs to be present");
  // TODO - add more constraints if necessary
}

} // namespace elf
