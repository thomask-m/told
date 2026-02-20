#include "told.h"

#include <cassert>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>

#define TRULY_WRITE_EXEC_FILE (true)

namespace told {
// TODO: it would be cool to write to the binary like this but nbd for now..
// std::ostream& operator<<(std::ostream& os, const elf::ElfHeader &eh) {
//   for (int i = 0; i < 4; ++i) {
//     os << eh.e_ident[i];
//   }
//   os << eh.e_type;
//   os << eh.e_machine;
//   os << eh.e_version;
//   os << eh.e_entry;
//   os << eh.e_phoff;
//   os << eh.e_shoff;
//   os << eh.e_flags;
//   os << eh.e_ehsize;
//   os << eh.e_phentsize;
//   os << eh.e_phnum;
//   os << eh.e_shentsize;
//   os << eh.e_shnum;
//   os << eh.e_shstrndx;
//   return os;
// }

// std::ostream& operator<<(std::ostream& os, const elf::ElfProgramHeader &ph) {
//   os << ph.p_type;
//   os << ph.p_flags;
//   os << ph.p_offset;
//   os << ph.p_vaddr;
//   os << ph.p_paddr;
//   os << ph.p_filesz;
//   os << ph.p_memsz;
//   os << ph.p_align;
//   return os;
// }

Executable convert_obj_to_exec(const std::vector<elf::ElfBinary> &modules) {
  Executable exec{};
  // TODO: this should not be hardcoded.
  exec.path = "a.told";

  // handle .text segments
  uint64_t text_segment_size{};
  for (const auto &mod : modules) {
    const elf::ElfSectionHeader &text_sh = mod.section_headers.at(".text");

    assert((text_sh.sh_flags & SHF_ALLOC) &&
           (text_sh.sh_flags & SHF_EXECINSTR));
    text_segment_size += text_sh.sh_size;
  }

  Segment text_sg = {TOLD_START_ADDR + TOLD_PAGE_SIZE, text_segment_size, true,
                     true};
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

  Segment header_sg = {TOLD_START_ADDR, header_size, true, false};
  exec.segments.emplace("_first", std::move(header_sg));

  return exec;
}

void write_out(const Executable &exec,
               const std::vector<elf::ElfBinary> &modules) {
  elf::ElfHeader eh{};
  eh.e_ident[0] = '\x7f';
  eh.e_ident[1] = 'E';
  eh.e_ident[2] = 'L';
  eh.e_ident[3] = 'F';
  eh.e_ident[EI_CLASS] = ELFCLASS64;
  eh.e_ident[EI_DATA] = ELFDATA2LSB;
  eh.e_ident[EI_OSABI] = ELFOSABI_SYSV;
  eh.e_ident[EI_VERSION] = EV_CURRENT;
  eh.e_type = ET_EXEC;
  eh.e_machine = EM_X86_64;
  eh.e_version = EV_CURRENT;
  // TODO: should not be hardcoded, but maybe it's good enough for a while.
  eh.e_entry = TOLD_START_ADDR + TOLD_PAGE_SIZE;
  // TODO: fill this out with a proper value
  eh.e_phoff = sizeof(elf::ElfHeader);  // TODO: is this correct?
  // TODO: fill this out with a proper value
  eh.e_shoff = 0;
  // TODO: fill this out with a proper value
  eh.e_flags = 0;
  eh.e_ehsize = sizeof(elf::ElfHeader);
  // TODO: verify that this is the right number.
  eh.e_phentsize = sizeof(elf::ElfProgramHeader);
  eh.e_phnum = exec.segments.size();
  // TODO: verify that this is the right number.
  eh.e_shentsize = sizeof(elf::ElfSectionHeader);
  // TODO: should not be hardcoded, needs to match the number of merged sections
  //       from all the input modules.
  eh.e_shnum = 0;
  // TODO: for now, i am not gonna care about symbols, but this is needed later.
  eh.e_shstrndx = SHN_UNDEF;

  std::vector<elf::ElfProgramHeader> p_headers{};
  p_headers.reserve(exec.segments.size());
  uint64_t page_header_i = 1;
  // hmm i need some way to go in sorted order
  for (const auto &sg : exec.segments) {
    elf::ElfProgramHeader ph{};
    ph.p_type =
        sg.second.loadable ? PT_LOAD : PT_NULL;  // TODO: this feels .. wrong..
    ph.p_flags = sg.second.executable ? PF_X : 0;
    ph.p_flags = ph.p_flags | PF_R;
    // TODO: fill this out with a proper value, for instance, a segment can be
    // bigger than
    //       just one page.
    ph.p_offset = sg.first == "_first" ? 0 : page_header_i * TOLD_PAGE_SIZE;
    ph.p_vaddr = sg.second.start_addr;
    ph.p_paddr = sg.second.start_addr;
    ph.p_filesz = sg.second.size;
    ph.p_memsz = sg.second.size;
    ph.p_align = TOLD_PAGE_SIZE;

    p_headers.emplace_back(ph);
  }

  if (TRULY_WRITE_EXEC_FILE) {
    std::ofstream output_exec(exec.path, std::ios::binary);
    uint32_t w_ptr{};
    if (!output_exec.is_open()) {
      std::cerr << "output exec file is not open before writing\n";
      exit(1);
    }
    output_exec.write(reinterpret_cast<char *>(&eh), sizeof(elf::ElfHeader));
    w_ptr += sizeof(elf::ElfHeader);
    for (elf::ElfProgramHeader &ph : p_headers) {
      output_exec.write(reinterpret_cast<char *>(&ph),
                        sizeof(elf::ElfProgramHeader));
      w_ptr += sizeof(elf::ElfProgramHeader);
    }

    // fill with zeroes until the next page alignment.
    std::vector<char> zeros(TOLD_PAGE_SIZE - w_ptr, 0);
    output_exec.write(zeros.data(), zeros.size());
    // TODO: we shouldn't be going thru each of the modules..
    //       the correct thing to do here is to have the combined segments
    //       and write them out already.
    //       that should be handled in the elf parser logic.
    for (const auto &mod : modules) {
      w_ptr = 0;
      std::vector<char> text_sg = mod.sections.at(".text");
      w_ptr += text_sg.size();
      output_exec.write(reinterpret_cast<char *>(text_sg.data()),
                        text_sg.size());
      // TODO: this feels like way too much hax
      std::vector<char> zeroes(TOLD_PAGE_SIZE - w_ptr, 0);
      output_exec.write(zeroes.data(), zeroes.size());
    }
  }
}

}  // namespace told
