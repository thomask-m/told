#pragma once

#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "elf_util.h"

// Default linker script specifies this as the executable start.
// Reminder: you can look at the default script by running ld --verbose
#define TOLD_START_ADDR (0x400000)
// Standard page size is 4KB on x86-64
#define TOLD_PAGE_SIZE (0x1000)

namespace told {

struct Segment {
  uint64_t start_addr;
  uint64_t size;
  bool loadable;
  bool executable;

  Segment(uint64_t start_addr, uint64_t size, bool loadable, bool executable)
      : start_addr(start_addr),
        size(size),
        loadable(loadable),
        executable(executable) {}

  Segment(Segment&& other) {
    start_addr = other.start_addr;
    size = other.size;
    loadable = other.loadable;
    executable = other.executable;

    other.start_addr = 0;
    other.size = 0;
    other.loadable = false;
    other.executable = false;
  }
};

struct Executable {
  std::string path;
  std::unordered_map<std::string, Segment> segments;
};

Executable convert_obj_to_exec(const std::vector<elf::ElfBinary>& modules);

void write_out(const Executable& exec,
               const std::vector<elf::ElfBinary>& modules);

}  // namespace told
