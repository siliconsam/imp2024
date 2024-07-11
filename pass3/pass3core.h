void readifrecord(FILE *infile, int *type, int *length, unsigned char *buffer);
void writeobjectrecord(FILE *outfile, int type, int count, unsigned char * data);

// Intermediate file types:
#define IF_OBJ          0 // A - plain object code
#define IF_DATA         1 // B - dataseg offset code word
#define IF_CONST        2 // C - const seg offset code word
#define IF_DISPLAY      3 // D - display seg offset code word
#define IF_JUMP         4 // E - unconditional jump to label
#define IF_JCOND        5 // F - cond jump to label JE, JNE, JG, JGE, JL, JLE, JA, JAE, JB, JBE
#define IF_CALL         6 // G - call a label
#define IF_LABEL        7 // H - define a label
#define IF_FIXUP        8 // I - define location for stack fixup instruction
#define IF_SETFIX       9 // J - stack fixup <location> <amount>
#define IF_REQEXT      10 // K - external name spec
#define IF_REFLABEL    11 // L - reference a label
#define IF_REFEXT      12 // M - external name relative offset code word
#define	IF_BSS         13 // N - BSS segment offset code word
#define IF_COTWORD     14 // O - Constant table word
#define IF_DATWORD     15 // P - Data section word (repeated) with repeat count
#define IF_SWTWORD     16 // Q - switch table entry - actually a label ID
#define	IF_SOURCE      17 // R - name of the source file
#define IF_DEFEXTCODE  18 // S - define a code label that is external
#define IF_DEFEXTDATA  19 // T - define a data label that is external
#define IF_SWT         20 // U - switch table offset code word
#define IF_LINE        21 // V - line number info for debugger
#define IF_ABSEXT      22 // W - external name absolute offset code word (data external)
#define IF_VERSION     23 // X - IBJ File Format version
#define IF_COMMENT     24 // Y - Text comment string

#define IBJMajor        1 // Current IBJ Major Version Level of IBJ File Format
#define IBJMinor        0 // Current IBJ Minor Version Level of IBJ File Format
#define IBJRevision     0 // Current IBJ Revision      Level of IBJ File Format

#define WORDSIZE	4

// Interface to ELF/COFF file writer
void setsize(int section, int s);
void setfile(FILE * out, int offset);
void writebyte(int section, unsigned char b);
void writew16(int section, int w);
void writew32(int section, int w);
void writeblock(int section, unsigned char *buffer, int count);

void flushout();

// definitions of ELF/COFF structures are in the corresponding
// <pass3elf.h> or <pass3coff.h>
