#include "elf_utils.h"
#include "told.h"

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

void merge_sections(Executable &e) {
  for (const auto &t : ACCEPTED_SECTIONS) {
    for (auto f : ACCEPTED_FLAGS) {
      size_t segment_size{};
      for (const auto &mod : e.input_modules) {
        if (mod.section_headers.at(t).sh_flags == f) {
          segment_size += mod.sections.at(t).size();
        }
      }
      if (segment_size == 0)
        continue;

      elf::Block b{};
      b.reserve(segment_size);
      bool filled = false;
      for (const auto &mod : e.input_modules) {
        if (mod.section_headers.at(t).sh_flags == f) {
          b.insert(b.end(), mod.sections.at(t).begin(),
                   mod.sections.at(t).end());
          filled = true;
        }
      }

      // the section block was not recorded (the flag was not encountered)
      if (!filled) {
        continue;
      }

      bool wr = f & SHF_WRITE;
      // TODO: figure out the significance of alloc flag.
      // bool alloc = f & SHF_ALLOC;
      bool ex = f & SHF_EXECINSTR;
      size_t bsz = b.size();
      Segment s{std::move(b) /* block */, 0 /* start_addr */,
                bsz /* size */,           0 /* relative_offset */,
                true /* readable*/,       false /* loadable */,
                ex /* executable */,      wr /* writable */};
      e.segments.emplace(t, s);
    }
  }
}

void assert_no_undefined_symbols(
    const std::unordered_map<elf::Symbol, elf::ElfSymbolTableEntry> &g_sym) {
  for (const auto &sym_entry : g_sym) {
    elf::Symbol sym = sym_entry.first;
    elf::ElfSymbolTableEntry entry = sym_entry.second;
    assert(ELF64_ST_TYPE(entry.st_info) != STT_NOTYPE &&
           "Undefined global symbols");
  }
}

void resolve_symbols(Executable &e) {
  // first, let's fill in the global symbol table
  std::unordered_map<elf::Symbol, elf::ElfSymbolTableEntry> g_sym{};
  for (const auto &mod : e.input_modules) {
    for (const auto &entry : mod.symbol_table) {
      elf::Symbol sym = entry.first;
      elf::ElfSymbolTableEntry curr_entry = entry.second;
      std::cout << "info about symbol table entry: "
                << ELF64_ST_BIND(curr_entry.st_info) << std::endl;
      if (ELF64_ST_BIND(curr_entry.st_info) == STB_GLOBAL) {
        if (auto e = g_sym.find(sym); e != g_sym.end()) {
          // symbol is already found in the global symbol table.
          // we need to check if it's already defined.
          elf::ElfSymbolTableEntry existing_entry = e->second;
          if (ELF64_ST_TYPE(curr_entry.st_info) != STT_NOTYPE) {
            assert(ELF64_ST_TYPE(existing_entry.st_info) == STT_NOTYPE &&
                   "Multiple definitions defined for a symbol");
          } else {
            // the current symbol we're processing is not defined. we check
            // later if there are undefined symbols, in the second pass.
          }
        } else {
          std::cout << "  Adding symbol to global symbol table: " << sym
                    << std::endl;
          g_sym.emplace(sym, curr_entry);
        }
      }
    }
  }

  assert_no_undefined_symbols(g_sym);

  e.g_symbol_table = std::move(g_sym);
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

void apply_addrs_and_adjustments(Executable &e) {
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

Executable init_exec(std::string &&output_path,
                     std::vector<elf::ElfBinary> &&modules) {
  Executable e{};
  e.input_modules = std::move(modules);
  e.path = std::move(output_path);
  elf::Block empty{};
  e.segments.emplace(elf::SectionType::Header,
                     Segment{std::move(empty), TOLD_START_ADDR, 0, 0, true,
                             false, false, false});
  return e;
}

Executable link(std::vector<elf::ElfBinary> &&modules) {
  Executable exec = init_exec("a.told", std::move(modules));
  merge_sections(exec);
  resolve_symbols(exec);
  apply_addrs_and_adjustments(exec);
  return exec;
}

elf::ElfHeader default_elf_header() {
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
  eh.e_phoff = sizeof(elf::ElfHeader);
  // TODO: implement these when section headers are added.
  eh.e_shnum = 0;
  eh.e_shoff = 0;
  // TODO: fill this out with a proper value
  eh.e_flags = 0;
  eh.e_ehsize = sizeof(elf::ElfHeader);
  eh.e_phentsize = sizeof(elf::ElfProgramHeader);
  eh.e_phnum = 0;
  eh.e_shentsize = sizeof(elf::ElfSectionHeader);
  // TODO: for now, i am not gonna care about symbols, but this is needed later.
  eh.e_shstrndx = SHN_UNDEF;
  return eh;
}

elf::ElfProgramHeader convert_to_ph(const Segment &sg) {
  elf::ElfProgramHeader ph{};
  ph.p_type = sg.loadable ? PT_LOAD : PT_NULL; // TODO: this feels .. wrong..
  ph.p_flags = sg.executable ? PF_X : 0;
  ph.p_flags = ph.p_flags | PF_R;
  ph.p_offset = 0;
  ph.p_vaddr = sg.start_addr;
  ph.p_paddr = sg.start_addr;
  ph.p_filesz = sg.size;
  ph.p_memsz = sg.size;
  ph.p_align = TOLD_PAGE_SIZE;
  return ph;
}

void write_to_fs(elf::ElfHeader &&eh, std::vector<elf::ElfProgramHeader> &&phs,
                 const Executable &exec) {
  std::ofstream output_exec(exec.path, std::ios::binary);
  size_t w_ptr{};
  if (!output_exec.is_open()) {
    std::cerr << "Output exec file is not open before writing\n";
    exit(1);
  }

  output_exec.write(reinterpret_cast<char *>(&eh), sizeof(elf::ElfHeader));
  w_ptr += sizeof(elf::ElfHeader);
  for (elf::ElfProgramHeader &ph : phs) {
    output_exec.write(reinterpret_cast<char *>(&ph),
                      sizeof(elf::ElfProgramHeader));
    w_ptr += sizeof(elf::ElfProgramHeader);
  }

  // TODO: write out section headers
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
}

void write_out(const Executable &exec) {
  elf::ElfHeader eh{default_elf_header()};
  eh.e_phnum = static_cast<elf::Elf64_Half>(exec.segments.size());

  std::vector<elf::ElfProgramHeader> p_headers{};
  p_headers.reserve(exec.segments.size());
  size_t i{};

  for (const auto &t : OUTPUT_SEGMENTS) {
    const Segment &s{exec.segments.at(t)};
    elf::ElfProgramHeader ph{convert_to_ph(s)};
    ph.p_offset = i * TOLD_PAGE_SIZE;
    p_headers.emplace_back(ph);
    ++i;
  }

  if (TRULY_WRITE_EXEC_FILE) {
    write_to_fs(std::move(eh), std::move(p_headers), exec);
  }
}

} // namespace told
