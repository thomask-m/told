/// Utility to parse an ELF binary file.
///
/// All the ELF-specific sizing stuff I took from:
///  https://sites.uclouvain.be/SystInfo/usr/include/elf.h.html
#pragma once

#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#define EI_NIDENT (16)

#define SELFMAG (4)

#define EI_CLASS (4)
#define ELFCLASS64 (2)

#define EI_DATA (5)
#define ELFDATA2LSB (1)

#define EI_OSABI (7)
#define ELFOSABI_SYSV (0)
#define EI_VERSION (6) /* File version byte index */

/* Legal values for e_type (object file type). */
#define ET_NONE (0)
#define ET_REL (1)  /* Relocatable file */
#define ET_EXEC (2) /* Executable file */

#define EM_X86_64 (62)

#define EV_CURRENT (1) /* Current version */

#define SHN_UNDEF (0) /* Undefined section */

#define SHT_NULL (0)     /* Section header table entry unused */
#define SHT_PROGBITS (1) /* Program data */

#define SHF_WRITE (1 << 0)     /* Writable */
#define SHF_ALLOC (1 << 1)     /* Occupies memory during execution */
#define SHF_EXECINSTR (1 << 2) /* Executable */

#define PT_NULL (0) /* Program header table entry unused */
#define PT_LOAD (1) /* Loadable program segment */

#define PF_X (1 << 0) /* Segment is executable */
#define PF_W (1 << 1) /* Segment is writable */
#define PF_R (1 << 2) /* Segment is readable */

namespace elf {

typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef uint64_t Elf64_Xword;
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;

typedef std::vector<char> Block;

enum class SectionType { None, Text, Data, Header };

SectionType s_type_from_name(const std::string &n);

struct ElfHeader {
  unsigned char e_ident[EI_NIDENT]; /* Magic number and other info */
  Elf64_Half e_type;                /* Object file type */
  Elf64_Half e_machine;             /* Architecture */
  Elf64_Word e_version;             /* Object file version */
  Elf64_Addr e_entry;               /* Entry point virtual address */
  Elf64_Off e_phoff;                /* Program header table file offset */
  Elf64_Off e_shoff;                /* Section header table file offset */
  Elf64_Word e_flags;               /* Processor-specific flags */
  Elf64_Half e_ehsize;              /* ELF header size in bytes */
  Elf64_Half e_phentsize;           /* Program header table entry size */
  Elf64_Half e_phnum;               /* Program header table entry count */
  Elf64_Half e_shentsize;           /* Section header table entry size */
  Elf64_Half e_shnum;               /* Section header table entry count */
  Elf64_Half e_shstrndx;            /* Section header string table index */
};

struct ElfSectionHeader {
  Elf64_Word sh_name;       /* Section name (string tbl index) */
  Elf64_Word sh_type;       /* Section type */
  Elf64_Xword sh_flags;     /* Section flags */
  Elf64_Addr sh_addr;       /* Section virtual addr at execution */
  Elf64_Off sh_offset;      /* Section file offset */
  Elf64_Xword sh_size;      /* Section size in bytes */
  Elf64_Word sh_link;       /* Link to another section */
  Elf64_Word sh_info;       /* Additional section information */
  Elf64_Xword sh_addralign; /* Section alignment */
  Elf64_Xword sh_entsize;   /* Entry size if section holds table */
};

struct ElfProgramHeader {
  Elf64_Word p_type;    /* Segment type */
  Elf64_Word p_flags;   /* Segment flags */
  Elf64_Off p_offset;   /* Segment file offset */
  Elf64_Addr p_vaddr;   /* Segment virtual address */
  Elf64_Addr p_paddr;   /* Segment physical address */
  Elf64_Xword p_filesz; /* Segment size in file */
  Elf64_Xword p_memsz;  /* Segment size in memory */
  Elf64_Xword p_align;  /* Segment alignment */
};

struct ElfBinary {
  ElfHeader elf_header;
  std::unordered_map<SectionType, ElfSectionHeader> section_headers;
  std::unordered_map<SectionType, Block> sections;
  std::string given_path;

  ElfBinary(const std::string &given_path) : given_path(given_path) {}
};

ElfBinary parse_object(const std::string &file_path);

void assert_expected_elf_header(const ElfHeader &elf_header);

}; // namespace elf
