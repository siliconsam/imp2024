// definitions of COFF structures.  Some systems may define these
// for you, but for portability they are provided here...
// Note however that we also define some structure sizes, because the
// lame MS compiler can mess you up when you do "sizeof"
// the only likely relocations we'll use
#define R_386_NONE  0
#define R_386_32    1
#define R_386_PC32  2

#ifdef MSVC
    #define SYMNMLEN    8
#else
    #define SYMNMLEN    9
#endif

#define FILNMLEN    14
#define	DIMNUM      4

struct cofffilehdr
{
    unsigned short  f_magic;    // magic number
    unsigned short  f_nscns;    // number of sections
    long            f_timdat;   // time and date stamp
    long            f_symptr;   // file pointer to symbol table
    long            f_nsyms;    // number of symbol table entries
    unsigned short  f_opthdr;   // size of (opt hdr)
    unsigned short  f_flags;    // flags
};

#define SZFILEHDR	(sizeof(struct cofffilehdr))

struct coffscnhdr
{
    char            s_name[SYMNMLEN];   // section name
    long            s_paddr;            // physical address
    long            s_vaddr;            // virtual address
    long            s_size;             // section size
    long            s_scnptr;           // file ptr to raw data
    long            s_relptr;           // file pointer to relocation list
    long            s_lnnoptr;          // file point to line numbers
    unsigned short  s_nreloc;           // number of relocations
    unsigned short  s_nlnno;            // number of line numbers
    long            s_flags;            // section flags
};

#define SZSECHDR	(sizeof(struct coffscnhdr))

struct coffreloc
{
    long            r_vaddr;    // (virtual) address of reference
    long            r_symndx;   // index into symbol table
    unsigned short  r_type;     // relocation type
};

#define SZRELOC		10

struct cofflineno
{
    union
    {
        long    l_symndx;
        long    l_paddr;
    } l_addr;
    unsigned short  l_lnno;
};

#define	SZLINENO    6

struct coffsyment
{
    union
    {
        char        n_name[SYMNMLEN];   // the actual name (if <= 8 chars)
        struct
        {
            long    n_zeroes;           // == 0L if the name is in the string table
            long    n_offset;           // the string table pointer
        } n_n;
    } n;
    long            n_value;            // value of the symbol
    short           n_scnum;            // section number
    unsigned short  n_type;             // type and derived type
    char            n_sclass;           // storage class
    char            n_numaux;           // number of auxilliary entries
};

#define SZSYMENT    18

struct coffauxscn
{
    long            x_scnlen;   // section length
    unsigned short  x_nreloc;   // number of relocations
    unsigned short  x_nlnno;    // number of line numbers
    long            x_chsum;    // COMDAT checksum
    unsigned short  x_secno;    // COMDAT section number
    unsigned short  x_selno;    // COMDAT selection number
    unsigned short  x_pad;      // not used
};

