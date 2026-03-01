#include "elf_utils.h"
#include "told.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <unordered_set>

// TODO: maybe this can just be a dry-run flag or maybe specify it can be
//       passed to stdout.
#define TRULY_WRITE_EXEC_FILE (true)

// N.B. - be care to ensure that lengths match input elements.
//        compiler did not catch that there were fewer elements
//        than were statically allocated (which zero-inits the rest).
static const std::array<elf::SectionType, 1> ACCEPTED_SECTIONS = {
    elf::SectionType::Text};
static const std::array<elf::SectionType, 2> OUTPUT_SEGMENTS = {
    elf::SectionType::Header, elf::SectionType::Text};
static const std::array<uint8_t, 1> ACCEPTED_FLAGS = {SHF_ALLOC |
                                                      SHF_EXECINSTR};
static const std::unordered_set<elf::SectionType> LOADABLE_SECTIONS{
    elf::SectionType::Text};

namespace told {

elf::ElfBinary parse_object(const std::string &file_path) {
  return elf::parse_object(file_path);
}

void compute_output_offsets(Executable &e) {
  size_t offset{};
  for (const auto &m : e.module_order) {
    e.text_segment_offsets.emplace(m, offset);
    elf::Block text_section =
        e.input_modules.at(m).sections.at(elf::SectionType::Text);
    offset += text_section.size();
  }
}

void merge_sections(Executable &e) {
  for (const auto &t : ACCEPTED_SECTIONS) {
    for (auto f : ACCEPTED_FLAGS) {
      size_t segment_size{};
      for (const auto &m : e.module_order) {
        if (e.input_modules.at(m).section_headers.at(t).sh_flags == f) {
          segment_size += e.input_modules.at(m).sections.at(t).size();
        }
      }
      if (segment_size == 0)
        continue;

      elf::Block b{};
      b.reserve(segment_size);
      bool filled = false;
      for (const auto &m : e.module_order) {
        if (e.input_modules.at(m).section_headers.at(t).sh_flags == f) {
          b.insert(b.end(), e.input_modules.at(m).sections.at(t).begin(),
                   e.input_modules.at(m).sections.at(t).end());
          filled = true;
        }
      }

      // the section block was not recorded (the flag was not encountered)
      if (!filled) {
        continue;
      }

      bool wr = f & SHF_WRITE;
      bool alloc = f & SHF_ALLOC;
      bool ex = f & SHF_EXECINSTR;
      size_t bsz = b.size();
      Segment s{std::move(b) /* block */, 0 /* start_addr */,
                bsz /* size */,           0 /* relative_offset */,
                true /* readable*/,       alloc /* loadable */,
                ex /* executable */,      wr /* writable */};
      e.segments.emplace(t, s);
    }
  }
}

void assert_no_undefined_global_symbols(const Executable &e) {
  for (const auto &mod : e.input_modules) {
    for (const auto &entry : mod.second.symbol_table) {
      elf::Symbol sym = entry.first;
      elf::ElfSymbolTableEntry curr_entry = entry.second;

      if (ELF64_ST_BIND(curr_entry.st_info) == STB_GLOBAL &&
          ELF64_ST_TYPE(curr_entry.st_info) == STT_NOTYPE) {
        assert(e.g_symbol_table.find(sym) != e.g_symbol_table.end() &&
               "Global symbol is not defined");
      }
    }
  }
}

void create_global_symtab(Executable &e) {
  std::unordered_map<elf::Symbol, GlobalSymTableEntry> g_sym{};
  for (const auto &mod : e.input_modules) {
    for (const auto &entry : mod.second.symbol_table) {
      const elf::Symbol &sym = entry.first;
      const elf::ElfSymbolTableEntry &curr_entry = entry.second;

      if (ELF64_ST_BIND(curr_entry.st_info) == STB_GLOBAL &&
          ELF64_ST_TYPE(curr_entry.st_info) != STT_NOTYPE) {
        assert(g_sym.find(sym) == g_sym.end() &&
               "Multiple definitions for symbol found");
        // TODO: it's not generally always the text type.
        g_sym.emplace(sym, GlobalSymTableEntry{mod.first, curr_entry.st_value,
                                               0, elf::SectionType::Text});
      }
    }
  }
  e.g_symbol_table = std::move(g_sym);
  assert_no_undefined_global_symbols(e);
}

void resolve_symbols(Executable &e) {
  create_global_symtab(e);
  assert(e.g_symbol_table.find(ENTRY_SYM) != e.g_symbol_table.end() &&
         "_start entrypoint needs to exist");
}

size_t padding_sz(size_t offset) {
  size_t alignment = offset % TOLD_PAGE_SIZE;
  if (alignment == 0)
    return 0;
  return TOLD_PAGE_SIZE - alignment;
}

std::optional<std::vector<char>> padding(size_t offset) {
  size_t p_sz{padding_sz(offset)};
  if (p_sz == 0)
    return std::nullopt;
  size_t to_pad{p_sz};

  std::vector<char> pad(to_pad);
  std::fill(pad.begin(), pad.end(), '\0');
  return pad;
}

void apply_addrs_and_adjustments_to_segments(Executable &e) {
  Segment &e_header = e.segments.at(elf::SectionType::Header);
  e_header.size = sizeof(elf::ElfHeader) +
                  (e.segments.size() * sizeof(elf::ElfProgramHeader));
  e_header.loadable = true;

  size_t addr{e.segments.at(elf::SectionType::Header).start_addr +
              e.segments.at(elf::SectionType::Header).size};
  for (const auto &t : ACCEPTED_SECTIONS) {
    Segment &s = e.segments.at(t);
    s.loadable = LOADABLE_SECTIONS.find(t) != LOADABLE_SECTIONS.end();
    addr += padding_sz(addr);
    s.start_addr = addr;
    addr += s.size;
  }
}

void apply_addrs_to_symbols(Executable &e) {
  for (auto &g_sym : e.g_symbol_table) {
    const std::string &mod = g_sym.second.def_module;
    size_t offset{e.text_segment_offsets.at(mod)};
    if (g_sym.second.type == elf::SectionType::Text) {
      const Segment &sg = e.segments.at(elf::SectionType::Text);
      g_sym.second.addr = offset + g_sym.second.value + sg.start_addr;
    }
  }
}

void update_block_content_with_reloc(std::vector<char> &block, size_t offset,
                                     uint32_t addr) {
  std::memcpy(&block[offset], &addr, sizeof(addr));
}

void apply_relocations(Executable &e) {
  for (const auto &m : e.input_modules) {
    for (const auto &reloc : m.second.rela_entries) {
      const std::string &sym = reloc.first;
      const auto rela_offset = reloc.second.r_offset;
      const elf::Elf64_Addr sym_addr = e.g_symbol_table.at(sym).addr;
      const size_t text_sg_start_addr =
          e.segments.at(elf::SectionType::Text).start_addr;
      const size_t text_sg_offset = e.text_segment_offsets.at(m.first);
      const size_t next_instr_addr =
          text_sg_offset + text_sg_start_addr + rela_offset;

      uint32_t new_addr = static_cast<uint32_t>(sym_addr - next_instr_addr +
                                                reloc.second.r_addend);
      update_block_content_with_reloc(
          e.segments.at(elf::SectionType::Text).block,
          static_cast<size_t>(text_sg_offset + rela_offset), new_addr);
    }
  }
}

Executable
init_exec(std::string &&output_path, std::vector<std::string> &&module_order,
          std::unordered_map<std::string, elf::ElfBinary> &&modules) {
  Executable e{};
  e.module_order = std::move(module_order);
  e.input_modules = std::move(modules);
  e.path = std::move(output_path);
  elf::Block empty{};
  e.segments.emplace(elf::SectionType::Header,
                     Segment{std::move(empty), TOLD_START_ADDR, 0, 0, true,
                             false, false, false});
  return e;
}

size_t compute_section_header_offset(const Executable &e) {
  size_t header_size = sizeof(elf::ElfHeader) +
                       sizeof(elf::ElfProgramHeader) * e.segments.size();
  size_t pad_between_header_segments = padding_sz(header_size);
  size_t pad_after_segments{};
  size_t segments_size{};
  for (const auto &t : ACCEPTED_SECTIONS) {
    segments_size = e.segments.at(t).block.size();
    pad_after_segments += padding_sz(segments_size);
  }

  return header_size + pad_between_header_segments + segments_size +
         pad_after_segments;
}

void add_elf_header(Executable &e) {
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
  eh.e_entry = e.g_symbol_table.at(ENTRY_SYM).addr;
  eh.e_phoff = sizeof(elf::ElfHeader);
  eh.e_shoff = static_cast<elf::Elf64_Off>(compute_section_header_offset(e));
  eh.e_flags = 0; // this is apparently the correct value for x86 arch.
  eh.e_ehsize = sizeof(elf::ElfHeader);
  eh.e_phentsize = sizeof(elf::ElfProgramHeader);
  eh.e_phnum = static_cast<elf::Elf64_Half>(e.segments.size());
  eh.e_shentsize = sizeof(elf::ElfSectionHeader);
  eh.e_shnum = static_cast<elf::Elf64_Half>(e.section_headers.size());

  eh.e_shstrndx = static_cast<elf::Elf64_Half>(e.section_headers.size() - 1);

  e.elf_header = std::move(eh);
}

std::vector<elf::ElfProgramHeader> create_program_headers(const Executable &e) {
  std::vector<elf::ElfProgramHeader> phs{};
  phs.reserve(e.segments.size());
  size_t i{};
  for (const auto &t : OUTPUT_SEGMENTS) {
    Segment sg = e.segments.at(t);
    elf::ElfProgramHeader ph{};
    ph.p_type = sg.loadable ? PT_LOAD : PT_NULL;
    ph.p_flags = sg.executable ? PF_X : 0;
    ph.p_flags = ph.p_flags | PF_R;
    ph.p_offset = i * TOLD_PAGE_SIZE;
    ph.p_vaddr = sg.start_addr;
    ph.p_paddr = sg.start_addr;
    ph.p_filesz = sg.size;
    ph.p_memsz = sg.size;
    ph.p_align = TOLD_PAGE_SIZE;
    phs.emplace_back(ph);
    ++i;
  }
  return phs;
}

StringTable setup_section_header_str_table(Executable &e) {
  assert(!e.section_headers.empty() &&
         "Section headers data needs to be written to Executable before "
         "setting up shstrtab");
  elf::ElfSectionHeader sh{};
  sh.sh_type = SHT_STRTAB;
  sh.sh_addralign = 1;
  std::vector<char> table_data = {'\0', '.', 't', 'e', 'x', 't', '\0', '.', 's',
                                  'h',  's', 't', 'r', 't', 'a', 'b',  '\0'};

  sh.sh_size = table_data.size();
  sh.sh_name = static_cast<elf::Elf64_Word>(table_data.size() - 10);
  size_t sh_offset = compute_section_header_offset(e);
  sh_offset += e.section_headers.size() * sizeof(elf::ElfSectionHeader);
  sh.sh_offset = static_cast<elf::Elf64_Off>(sh_offset + padding_sz(sh_offset));

  e.section_headers.emplace_back(sh);
  StringTable shstrtab{elf::SectionType::ShStrTable, std::move(table_data)};
  return shstrtab;
}

std::vector<elf::ElfSectionHeader> create_section_headers(const Executable &e) {
  std::vector<elf::ElfSectionHeader> shs{elf::ElfSectionHeader{}};
  shs.reserve(e.segments.size());

  // TODO: I kind of hate how the section string table is getting setup.. that
  //       is something that really needs a good refactor.
  size_t shstrtab_offset = 1;
  for (size_t i = 0; i < ACCEPTED_SECTIONS.size(); ++i) {
    const Segment &sg = e.segments.at(ACCEPTED_SECTIONS[i]);
    elf::ElfSectionHeader sh{};
    sh.sh_name = static_cast<elf::Elf64_Word>(shstrtab_offset);
    sh.sh_type = sg.loadable && sg.executable ? SHT_PROGBITS : SHT_NULL;
    // TODO: SHF_ALLOC is not the correct default.
    sh.sh_flags =
        sg.loadable && sg.executable ? SHF_ALLOC | SHF_EXECINSTR : SHF_ALLOC;
    sh.sh_addr = sg.start_addr;
    sh.sh_offset = TOLD_PAGE_SIZE * (i + 1);
    sh.sh_size = sg.size;
    sh.sh_link = 0;
    sh.sh_info = 0;
    // TODO: 1?
    sh.sh_addralign = 1;
    sh.sh_entsize = 0;

    shs.emplace_back(sh);
  }
  return shs;
}

void write_to_fs(const Executable &exec) {
  std::ofstream output_exec(exec.path, std::ios_base::out | std::ios::binary);
  size_t w_ptr{};
  if (!output_exec.is_open()) {
    std::cerr << "Output exec file is not open before writing\n";
    exit(1);
  }

  output_exec.write(reinterpret_cast<const char *>(&exec.elf_header),
                    sizeof(elf::ElfHeader));
  w_ptr += sizeof(elf::ElfHeader);
  for (const auto &ph : exec.program_headers) {
    output_exec.write(reinterpret_cast<const char *>(&ph),
                      sizeof(elf::ElfProgramHeader));
    w_ptr += sizeof(elf::ElfProgramHeader);
  }

  // TODO: write out symbol table

  std::optional<std::vector<char>> pad = padding(w_ptr);
  if (pad.has_value())
    output_exec.write(pad.value().data(), pad.value().size());
  for (const auto &t : ACCEPTED_SECTIONS) {
    w_ptr = 0;
    elf::Block s_block = exec.segments.at(t).block;
    w_ptr += s_block.size();
    output_exec.write(reinterpret_cast<char *>(s_block.data()), s_block.size());

    pad = padding(w_ptr);
    if (pad.has_value())
      output_exec.write(pad.value().data(), pad.value().size());
  }

  w_ptr = 0;
  for (const auto &sh : exec.section_headers) {
    output_exec.write(reinterpret_cast<const char *>(&sh),
                      sizeof(elf::ElfSectionHeader));
    w_ptr += sizeof(elf::ElfSectionHeader);
  }
  pad = padding(w_ptr);
  if (pad.has_value())
    output_exec.write(pad.value().data(), pad.value().size());
  w_ptr = exec.section_header_str_table.size();
  for (size_t i = 0; i < exec.section_header_str_table.strings.size(); ++i) {
    output_exec.write(reinterpret_cast<const char *>(
                          &exec.section_header_str_table.strings[i]),
                      1);
  }
  pad = padding(w_ptr);
  if (pad.has_value())
    output_exec.write(pad.value().data(), pad.value().size());
}

void write_out(const Executable &e) {
  if (TRULY_WRITE_EXEC_FILE) {
    write_to_fs(e);
  }
}

void apply_headers(Executable &e) {
  e.program_headers = std::move(create_program_headers(e));
  e.section_headers = std::move(create_section_headers(e));
  e.section_header_str_table = std::move(setup_section_header_str_table(e));
  add_elf_header(e);
}

Executable link(std::vector<std::string> &&module_order,
                std::unordered_map<std::string, elf::ElfBinary> &&modules) {
  Executable exec =
      init_exec("a.told", std::move(module_order), std::move(modules));
  compute_output_offsets(exec);
  resolve_symbols(exec);
  merge_sections(exec);
  apply_addrs_and_adjustments_to_segments(exec);
  apply_addrs_to_symbols(exec);
  apply_relocations(exec);
  apply_headers(exec);
  return exec;
}

} // namespace told
