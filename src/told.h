#pragma once

#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "elf_utils.h"

// Default linker script specifies this as the executable start.
// Reminder: you can look at the default script by running ld --verbose
#define TOLD_START_ADDR (0x400000)
// Standard page size is 4KB on x86-64
#define TOLD_PAGE_SIZE (0x1000)

namespace told {

struct Segment {
  elf::Block block;
  size_t start_addr;
  size_t size;
  size_t relative_offset;
  bool readable;
  bool loadable;
  bool executable;
  bool writable;
};

struct Executable {
  std::string path;
  std::vector<elf::ElfBinary> input_modules;
  std::unordered_map<elf::SectionType, Segment> segments;
  std::unordered_map<elf::Symbol, elf::ElfSymbolTableEntry> g_symbol_table;
};

elf::ElfBinary parse_object(const std::string &file_path);

Executable link(std::vector<elf::ElfBinary> &&modules);

void write_out(const Executable &exec);

} // namespace told
