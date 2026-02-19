#include "told.h"

#include <cassert>
#include <cstdint>
#include <iostream>

namespace told {

Executable convert_obj_to_exec(const std::vector<elf::ElfBinary> &modules) {
  Executable exec{};
  // TODO: this should not be hardcoded.
  exec.path = "a.out";

  // handle .text segments
  uint64_t text_segment_size{};
  for (const auto &mod : modules) {
    const elf::ElfSectionHeader &text_sh = mod.section_headers.at(".text");

    assert((text_sh.sh_flags & SHF_ALLOC) &&
           (text_sh.sh_flags & SHF_EXECINSTR));
    text_segment_size += text_sh.sh_size;
  }

  Segment text_sg = {TOLD_START_ADDR + TOLD_PAGE_SIZE, text_segment_size, true};
  exec.segments.emplace(".text", std::move(text_sg));

  // write the text section addresses
  for (const auto &mod : modules) {
    elf::ElfSectionHeader text_sh = mod.section_headers.at(".text");
    // TODO: this is not generally correct - we need to assign this start addr
    //       to the actual program entrypoint (e.g. _start for minimal.c)
    //       For now, I am going to just optimize for having minimal.c run as
    //       executable.
    text_sh.sh_addr = text_sg.start_addr;
  }

  // TODO: handle .data segments
  // TODO: handle .bss segments
  // TODO: handle .symtab segments
  // TODO: handle .eh_frame segments
  // TODO: handle .comment segments

  uint64_t num_segments = exec.segments.size() + 1;
  uint64_t header_size =
      sizeof(elf::ElfHeader) + num_segments * sizeof(elf::ElfProgramHeader);

  Segment header_sg = {TOLD_START_ADDR, header_size, true};
  exec.segments.emplace("_first", std::move(header_sg));

  return exec;
}

void write_out(const Executable &exec,
               const std::vector<elf::ElfBinary> &modules) {}

}  // namespace told
