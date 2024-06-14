// definitions of ELF/COFF structures.  Some systems may define these
// for you, but for portability they are provided here...
// Note however that we also define some structure sizes, because the
// lame MS compiler can mess you up when you do "sizeof"

typedef unsigned        Elf32_Addr;     // Unsigned program address
typedef unsigned short  Elf32_Half;     // Unsigned medium integer
typedef unsigned        Elf32_Off;      // Unsigned file offset
typedef int             Elf32_Sword;    // Signed large integer
typedef unsigned        Elf32_Word;     // Unsigned large integer

#define EI_NIDENT	16

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    Elf32_Half  e_type ;
    Elf32_Half  e_machine;
    Elf32_Word  e_version;
    Elf32_Addr  e_entry;
    Elf32_Off   e_phoff;
    Elf32_Off   e_shoff;
    Elf32_Word  e_flags;
    Elf32_Half  e_ehsize;
    Elf32_Half  e_phentsize;
    Elf32_Half  e_phnum;
    Elf32_Half  e_shentsize;
    Elf32_Half  e_shnum;
    Elf32_Half  e_shstrndx;
} Elf32_Ehdr;

// some simple defines to go in some holes here
#define	ET_REL      1   // This is a relocatable object file
#define EM_386      3   // For an Intel 80386
#define EV_CURRENT  1   // in version 1 of ELF

// indexes to the ELF ident string
#define EI_MAG0     0   // File identification
#define EI_MAG1     1   // File identification
#define EI_MAG2     2   // File identification
#define EI_MAG3     3   // File identification
#define EI_CLASS    4   // File class
#define EI_DATA     5   // Data encoding
#define EI_VERSION  6   // File version
#define EI_PAD      7   // Start of padding bytes

#define ELFCLASS32  1   // for the class
#define	ELFDATA2LSB 1   // little endian encoding

// ELF Section Header
typedef struct {
    Elf32_Word  sh_name;
    Elf32_Word  sh_type;
    Elf32_Word  sh_flags;
    Elf32_Addr  sh_addr;
    Elf32_Off   sh_offset;
    Elf32_Word  sh_size;
    Elf32_Word  sh_link;
    Elf32_Word  sh_info;
    Elf32_Word  sh_addralign;
    Elf32_Word  sh_entsize;
} Elf32_Shdr;

// and some section types we will use...
#define SHT_NULL           0
#define SHT_PROGBITS       1
#define SHT_SYMTAB         2
#define SHT_STRTAB         3
#define SHT_RELA           4
#define SHT_HASH           5
#define SHT_DYNAMIC        6
#define SHT_NOTE           7
#define SHT_NOBITS         8
#define SHT_REL            9
#define SHT_SHLIB         10
#define SHT_DYNSYM        11
#define SHT_INIT_ARRAY    14
#define SHT_FINI_ARRAY    15
#define SHT_PREINIT_ARRAY 16
#define SHT_GROUP         17
#define SHT_SYMTAB_CHECK  18

// section flags...
#define SHF_WRITE       0x1 // writable
#define SHF_ALLOC       0x2 // actually occupies some space
#define SHF_EXECINSTR   0x4 // executable
#define SHF_MERGE       0x10 // data can be merged
#define SHF_STRINGS     0x20 // contains null-terminated strings
#define SHF_INFO_LINK   0x40 // sh_info has a section header table index
#define SHF_LINK_ORDER  0x80 // used to build a table (text/data) in address order
#define SHF_OS_NONCONFORMING 0x100
#define SHF_GROUP       0x200 // section is a member of a group
#define SHF_TLS         0x400 // section hold thread-local storage
#define SHF_ORDERED     0x40000000 // superceded by SHD_LINK_ORDER
#define SHF_EXCLUDE     0x80000000 // exclude from link of executable/shared object

// ELF symbol table entry
typedef struct {
    Elf32_Word      st_name;
    Elf32_Addr      st_value;
    Elf32_Word      st_size;
    unsigned char   st_info;
    unsigned char   st_other;
    Elf32_Half      st_shndx;
} Elf32_Sym;

// symbol table bindings
#define STB_LOCAL   0
#define STB_GLOBAL  1

// symbol table types
#define STT_NOTYPE  0
#define STT_OBJECT  1
#define STT_FUNC    2
#define STT_SECTION 3
#define STT_FILE    4

// special section indexes
#define SHN_ABS     0xfff1

// relocation entries
typedef struct{
    Elf32_Addr  r_offset;
    Elf32_Word  r_info;
} Elf32_Rel;

typedef struct{
    Elf32_Addr  r_offset;
    Elf32_Word  r_info;
    Elf32_Sword r_addend;
} Elf32_Rela;

// the only likely relocations we'll use
#define R_386_NONE      0
#define R_386_32        1
#define R_386_PC32      2
#define R_386_GOT32     3
#define R_386_PLT32     4
#define R_386_COPY      5
#define R_386_GLOB_DAT  6
#define R_386_JMP_SLOT  7
#define R_386_RELATIVE  8
#define R_386_GOTOFF    9
#define R_386_GOTPC    10
#define R_386_32PLT    11
