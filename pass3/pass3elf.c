// IMP Compiler for 80386 - pass 3
// ELF format object file generator

// This reads an intermediate object file produced by the
// second pass, performs the jump and stack allocation fixups.
// It reads the input file twice
// - once to collect all the jump and stack information
// - a second time to actually write the object file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include "pass3core.h"
#include "pass3elf.h"

#define BYTESZ          1
#define SWTABENTRYSZ    4
#define MAINPROGNAME  "main"
#define TRAPLIMITNAME   "traplimit"
#define TRAPENTRYSZ     32
#define TRAPENTRYRELOC  4
#define LINELIMITNAME   "linelimit"
#define LINEENTRYSZ     8
#define LINEENTRYRELOC  1
#define LINEHEADERSZ  256

// So that we can always find the trap table we define two
// special symbols IFF this is a main program block
#define TRAPBASE	"_imptrapbase"
#define	TRAPLIMIT   "_imptraplimit"
// So that we can always find the line number table we define two
// special symbols IFF this is a main program block
#define LINEBASE	"_implinebase"
#define	LINELIMIT	"_implinelimit"

// Pass 3 builds an in-store model of the application as a series of
// literal code blocks, data blocks, and so on.
struct item {
    // what this block describes
    int what;
     // the address in the image
    int address;
    // type dependent extra information
    int info;
    // size this block occupies in the image (generally in bytes)
    int size;
    // symbol spec this item refers to
    int spec;
};
#define MAXITEM 20000
struct item m[MAXITEM];
int nm = 0;

#define LABELISUSED    1
#define LABELISLOCAL   2
// Jumps and calls all go to logical labels (a label ID is a 16 bit number)
// We create a simple database of labels and their corresponding code addresses
struct label {
    int labelid;
    int address;
    int flags;
};
#define MAXLABEL 5000
struct label labels[MAXLABEL];
int nl = 1;

// The entry to each routine includes code to adjust the stack frame for
// local variables.  Pass 2 plants a fixup record at the end of the
// routine.  We use this table to match fixups with their corresponding
// entry sequence, and we also use it to plant trap records
struct stfix {
     // arbitrary ID passed by Pass 2 (derived from P2's nominal code address)
    int id;
    // pointer to the M record corresponding to the entry code
    int hint;
    // events trapped in this subroutine (a 16 bit bitmask)
    int events;
    // actual start address of subroutine;
    int start;
    // actual end address of subroutine
    int end;
    // label of the event trap entry point (if events != 0)
    int trap;
    // label of the start of the event protected area
    int evfrom;
    // pointer to debug name of this routine
    int namep;
    // symbol table index of this routine
    int symid;
};
#define MAXSTACK 500
struct stfix stackfix[MAXSTACK];
int ns = 0;

// The name dictionary is filled by external import or export
// names, zero terminated - the M record points to the first
// character of the corresponding name.
// NOTE - ELF requires that there is always an entry at offset zero
// that is a zero byte (null pointer == null name)
#define MAXNAME 5000
char named[MAXNAME] = { 0, };
int namedp = 1;

// The share name dictionary is filled by section names zero terminated
// - the M record points to the first character of the corresponding name.
// NOTE - ELF requires that there is always an entry at offset zero
// that is a zero byte (null pointer == null name)
#define MAXSHNAME 500
char shared[MAXSHNAME] = { 0, };
int sharedp = 1;

// Line number information is collected as the object file is output
// so that we can write a linenumber section for the debugger
struct lineno {
    int line;
    int offset;
};
#define MAXLINENO 4000
struct lineno lines[MAXLINENO];
int nlines = 0;

#define SYMISUSED    1
#define SYMISLOCAL   2
#define SYMISDATA    4
#define SYMISTRAP    8
// Pass 2 passes on all symbols that are "spec'ed", and allocates an index
// in sequence.  We check to see if they ever actually get used so that we
// can prune the unwanted ones, and remap the indexes into the symbol table
// accordingly...
struct symspec {
    // the actual symbol index
    int p3index;
    // flags
    // bit 0 (1 => used,  0 => unused)
    // bit 1 (1 => local, 0 => global)
    // bit 2 (1 => data,  0 => code)
    // bit 3 (1 => trap,  0 => normal)
    int flags;
};
#define MAXSPECS 250
struct symspec specs[MAXSPECS];
int nspecs = 0;

//////// Database routines
// the code address at the last line record assigned
int lastlinead = -1;

// report this line number as being at this address
static void newlineno(int line, int addr)
{
    // is this line at the same address we already have?
    // i.e. have the line numbers advanced, but code didn't?
    if (addr == lastlinead)
    {
        // update value of current line record
        lines[nlines-1].line = line;
    }
    else
    {
        // but only add line info if there is room
        if (nlines == MAXLINENO)
        {
            fprintf(stderr, "Program has too many lines\n");
            fprintf(stderr, "Increase the value of MAXLINENO\n");
            exit(1);
        }
        lines[nlines].line = line;
        lines[nlines].offset = addr;
        nlines += 1;
        lastlinead = addr;
    }
}

// return the index of the next item block
static int newitem( int whatType )
{
    if (nm == MAXITEM)
    {
        fprintf(stderr, "Program too big\n");
        fprintf(stderr, "Increase the value of MAXITEM\n");
        exit(1);
    }
    m[nm].what = whatType;
    m[nm].size = 0;
    m[nm].spec = 0;

    nm = nm + 1;
    return (nm - 1);
}

// return the index of the next label record
static int newlabel()
{
    if (nl == MAXLABEL)
    {
        fprintf(stderr, "Too many labels\n");
        fprintf(stderr, "Increase the value of MAXLABEL\n");
        exit(1);
    }
    nl = nl + 1;
    return (nl - 1);
}

// return the index of the next procedure/stack record
static int newstack()
{
    if (ns == MAXSTACK)
    {
        fprintf(stderr, "Too many subroutines\n");
        fprintf(stderr, "Increase the value of MAXSTACK\n");
        exit(1);
    }
    ns = ns + 1;
    return (ns - 1);
}

static int newspec()
{
    // clear it and count it...
    nspecs += 1;
    if (nspecs == MAXSPECS)
    {
        fprintf(stderr, "Too many %%spec's\n");
        fprintf(stderr, "Increase the value of MAXSPECS\n");
        exit(1);
    }
    // assume this spec is an unused, global, function symbol
    // So, clear all the bits of the spec flag
    specs[nspecs].flags = 0;

    return nspecs;
}

// copy a new name into the name dictionary, and return
// the index of the first character
static int newname(char * name)
{
    int l;

    l = strlen(name);
    if ((l + namedp) >= MAXNAME)
    {
        fprintf(stderr, "Too many names\n");
        fprintf(stderr, "Increase the value of MAXNAME\n");
        exit(1);
    }
    strcpy(&named[namedp], name);
    namedp += l + 1;
    return (namedp - (l + 1));
}

// copy a new name into the name dictionary, and return
// the index of the first character
static int newsharename(char * name)
{
    int l;

    l = strlen(name);
    if ((l + sharedp) >= MAXSHNAME)
    {
        fprintf(stderr, "Too many share names\n");
        fprintf(stderr, "Increase the value of MAXSHNAME\n");
        exit(1);
    }
    strcpy(&shared[sharedp], name);
    sharedp += l + 1;
    return (sharedp - (l + 1));
}

// Code relocations are interspersed by Pass2 in with the code, but
// are output en-mass in the Object file.  We count them here because
// we need to know how many there are when constructing the Object file.
int nreloc = 0;

// As we build the external symbol table we count them too...
int nsymdefs = 0;

// Pass3 needs to know whether this is a main program or a
// file of external routines, because main programs get a
// special symbol defined for the trap table
int mainprogflag = 0;
// We also need to determine if this is the traplimit module
// which will then contain the _imptraplimit symbol
int traplimitflag = 0;
// We also need to determine if this is the linelimit module
// which will then contain the _imptraplimit symbol
int linelimitflag = 0;

char modulename[256];

// The first pass through the input file, where we collect all the
// data we will need to map out the object code
static void readpass1(char *inname)
{
    FILE *input;

    int lineno;
    int type, length, current, ptr, id, value, cad;
    int count;
    int i;
    unsigned char buffer[256];

    input = fopen(inname, "r");
    if (input == NULL)
    {
        perror("Can't open input file");
        fprintf(stderr, "Can't open input file '%s'\n",inname);
        exit(1);
    }

    lineno = 0;
    cad = 0;

    current = newitem(IF_OBJ);
    for(;;)
    {
        // Always clean the buffer!
        for (i = 0; i<255; i++) buffer[i] = 0;

        // Now to read the IBJ record
        lineno++;
        readifrecord(input, &type, &length, buffer);
        // Are we at the end of file marker?
        if (type < 0)
        {
            fclose(input);
            return;
        }

        switch(type)
        {
        case IF_OBJ:
            // plain object code
            // All of these are treated as "object code" for the first passs
            if (m[current].what != IF_OBJ)
            {
                current = newitem(IF_OBJ);
            }
            // we don't bother to remember the code, just how big it is..
            m[current].size += length;
            cad += length;
            break;

        case IF_DATA:
            // data section offset word
            // All of these are treated as "object code" for the first passs
            if (length != WORDSIZE)
                fprintf(stderr, "ERR_BADRECSZ: IF_DATA, line %d\n",lineno);
            nreloc += 1;
            if (m[current].what != IF_OBJ)
            {
                current = newitem(IF_OBJ);
            }
            // we don't bother to remember the code, just how big it is..
            m[current].size += WORDSIZE;
            cad += WORDSIZE;
            break;

        case IF_CONST:
            // const section offset word
            // All of these are treated as "object code" for the first passs
            if (length != WORDSIZE)
                fprintf(stderr, "ERR_BADRECSZ: IF_CONST, line %d\n",lineno);
            nreloc += 1;
            if (m[current].what != IF_OBJ)
            {
                current = newitem(IF_OBJ);
            }
            // we don't bother to remember the code, just how big it is..
            m[current].size += WORDSIZE;
            cad += WORDSIZE;
            break;

        case IF_DISPLAY:
            // display section offset word
            // All of these are treated as "object code" for the first passs
            if (length != WORDSIZE)
                fprintf(stderr, "ERR_BADRECSZ: IF_DISPLAY, line %d\n",lineno);
            nreloc += 1;
            if (m[current].what != IF_OBJ)
            {
                current = newitem(IF_OBJ);
            }
            // we don't bother to remember the code, just how big it is..
            m[current].size += WORDSIZE;
            cad += WORDSIZE;
            break;

        case IF_JUMP:
            // unconditional jump to label
            current = newitem(IF_JUMP);
            // assume long to begin with (1 byte instruction, 4 bytes address)
            m[current].size = 5;
            // get the target label number
            m[current].info = (buffer[1] << 8) | buffer[0];
            cad += 5;
            break;

        case IF_JCOND:
            // cond jump to label JE, JNE, JLE, JL, JGE, JG etc
            current = newitem(IF_JCOND);
            // assume long to begin with (2 bytes instruction, 4 bytes address)
            m[current].size = 6;
            // condition code is buffer[0] - not needed on this first pass
            // get the target label number
            m[current].info = (buffer[2] << 8) | buffer[1];
            cad += 6;
            break;

        case IF_CALL:
            // call a label
            current = newitem(IF_CALL);
            // assume long to begin with (1 byte instruction, 4 bytes address)
            m[current].size = 5;
            // get the target label number
            m[current].info = (buffer[1] << 8) | buffer[0];
            cad += 5;
            break;

        case IF_LABEL:
            // define a label
            current = newitem(IF_LABEL);
            // labels occupy no space
            m[current].size = 0;
            // get the label number
            m[current].info = (buffer[1] << 8) | buffer[0];
            break;

        case IF_FIXUP:
            // define location for stack fixup instruction
            current = newitem(IF_FIXUP);
            // space will be an ENTER instruction (C8, nnnn, ll)
            m[current].size = 4;
            // amount to subtract from the stack will be filled later
            m[current].info = 0;
            cad += 4;
            ptr = newstack();
            // get the id number for fixup
            stackfix[ptr].id = (buffer[1] << 8) | buffer[0];
            // point to this code item
            stackfix[ptr].hint = current;
            // assume no events are trapped
            stackfix[ptr].events = 0;
            // no known label yet
            stackfix[ptr].trap = 0;
            // make sure we zero terminate the proc name
            buffer[length] = 0;
            // remember the debug name of this routine
            stackfix[ptr].namep = newname((char *)&buffer[3]);
            break;

        case IF_SETFIX:
            // stack fixup <location> <amount> <eventmask> <event entry>
             // get the id number for this fixup
            id = (buffer[1] << 8) | buffer[0];
            // search for the fixup
            for (ptr = 0; ptr < ns; ptr++)
            {
                // have we found it?
                if (stackfix[ptr].id == id)
                {
                    // found, so point to M record
                    id = stackfix[ptr].hint;
                    // get the amount to subtract
                    value = (buffer[3] << 8) | buffer[2];
                    // compiler passes value as a 16 bit negative number,
                    // but we're going to plant an ENTER instruction,
                    // so we make it positive...
                    value = - value;
                    value &= 0xffff;
                    m[id].info = value;
                    // now fill in the event stuff...
                    stackfix[ptr].events = (buffer[5] << 8) | buffer[4];
                    stackfix[ptr].trap   = (buffer[7] << 8) | buffer[6];
                    stackfix[ptr].evfrom = (buffer[9] << 8) | buffer[8];
                    break;
                }
            }
            if (ptr == ns)
                fprintf(stderr, "Stack fixup for undefined ID?\n");
            break;

        case IF_REQEXT:
            // external name spec
            current = newitem(IF_REQEXT);
            // note the spec
            m[current].spec = newspec();

            // definitions/specs occupy no space
            m[current].size = 0;
            // make the name null terminated
            buffer[length] = 0;
            m[current].info = newname((char *)buffer);
            break;

        case IF_REFLABEL:
            // label reference as relative address with optional offset
            // add it to list and update current address pointer...
            current = newitem(IF_REFLABEL);
            // label reference == relative address, so WORDSIZE
            m[current].size = WORDSIZE;
            // get the label number
            m[current].info = (buffer[1] << 8) | buffer[0];
            // buffer[3],buffer[2] offset is ignored in this pass
            cad += WORDSIZE;
            break;

        case IF_REFEXT:
            // external name relative offset code word
            // which "spec" are we dealing with
            id = (buffer[1]<<8)|buffer[0];

            // update our "used" flag
            specs[id].flags = specs[id].flags | SYMISUSED;

            // and fall through to treat as a general code word
            // All of these are treated as "object code" for the first pass
            if (length != WORDSIZE)
                fprintf(stderr, "ERR_BADRECSZ: IF_REFEXT, line %d\n",lineno);
            nreloc += 1;
            if (m[current].what != IF_OBJ)
            {
                current = newitem(IF_OBJ);
            }
            // remember the associateed symbol spec
            m[current].spec = id;
            // we don't bother to remember the code, just how big it is..
            m[current].size += WORDSIZE;
            cad += WORDSIZE;
            break;

        case IF_BSS:
            // BSS section offset code word
            // All of these are treated as "object code" for the first pass
            if (length != WORDSIZE)
                fprintf(stderr, "ERR_BADRECSZ: IF_BSS, line %d\n",lineno);
            nreloc += 1;
            if (m[current].what != IF_OBJ)
            {
                current = newitem(IF_OBJ);
            }
            // we don't bother to remember the code, just how big it is..
            m[current].size += WORDSIZE;
            cad += WORDSIZE;
            break;

        case IF_COTWORD:
            // Constant table word
            if (m[current].what != IF_COTWORD)
            {
                current = newitem(IF_COTWORD);
            }
            // NOTE - these are actually halfwords (=2 bytes)
            m[current].size += 2;
            break;

        case IF_DATWORD:
            // Data section word
            if (m[current].what != IF_DATWORD)
            {
                current = newitem(IF_DATWORD);
                m[current].size = 0;
            }
            // determine how many halfwords (= number of 2 byte chunks)
            // Ensure count is zero
            count = 0;
            if (length == 2)
            {
                // old format of IF_DATWORD
                count = 1;
            }
            else if (length == 4)
            {
                // new format of IF_DATWORD
                count = buffer[2] | (buffer[3] << 8);
            }

            // determine how many bytes required
            m[current].size += (2*count);
            break;

        case IF_SWTWORD:
            // switch table entry - actually a label ID
            if (m[current].what != IF_SWTWORD)
            {
                current = newitem(IF_SWTWORD);
            }
            // NOTE - these are actually halfwords (=2 bytes)
            m[current].size += 2;
            break;

        case IF_SOURCE:
            // name of the source file
            // do nothing - not even advance the "current"
            // Actually on the first pass remember the source filename
            strcpy(modulename,buffer);
            break;

        case IF_DEFEXTCODE:
            // external name spec
            // make the name null terminated
            buffer[length] = 0;
            // this is a cheesy/hack way to find if this is a main program
            if (strcmp((char *)buffer, MAINPROGNAME ) == 0)
            {
                // Yes, this is the main program
                mainprogflag = 1;
            }
            // define a code label that is external
            current = newitem(IF_DEFEXTCODE);
            m[current].info = newname((char *)buffer);
            nsymdefs += 1;
            // definitions/specs occupy no space
            m[current].spec = newspec();
            break;

        case IF_DEFEXTDATA:
            // external name spec
            // make the name null terminated
            buffer[length] = 0;
            // this is a cheesy/hack way to find if this is the traplimit module
            if (strcmp((char *)buffer, TRAPLIMITNAME ) == 0)
            {
                // Yes, this is the traplimit symbol module!
                traplimitflag = 1;
            }
            // this is a cheesy/hack way to find if this is the linelimit module
            if (strcmp((char *)buffer, LINELIMITNAME ) == 0)
            {
                // Yes, this is the linelimit symbol module!
                linelimitflag = 1;
            }

            // define a data label that is external
            current = newitem(IF_DEFEXTDATA);
            m[current].info = newname((char *)buffer);
            nsymdefs += 1;
            // definitions/specs occupy no space
            m[current].spec = newspec();
            // flag this spec as an external data symbol
            specs[m[current].spec].flags = specs[m[current].spec].flags | SYMISDATA;
            break;

        case IF_SWT:
            // SWITCH table section offset code word
            // All of these are treated as "object code" for the first pass
            if (length != WORDSIZE)
                fprintf(stderr, "ERR_BADRECSZ: IF_SWT, line %d\n",lineno);
            nreloc += 1;
            if (m[current].what != IF_OBJ)
            {
                current = newitem(IF_OBJ);
            }
            // we don't bother to remember the code, just how big it is..
            m[current].size += WORDSIZE;
            cad += WORDSIZE;
            break;

        case IF_LINE:
            // line number info for the debugger
            // note we will recalculate this information on the second pass when
            // jump optimisation will have changed the code addresses, but in the
            // meantime we need to know how many records we will have for the line
            // number section of the object file.

            // get the source line number
            value = (buffer[1] << 8) | buffer[0];
            newlineno(value, cad);
            break;

        case IF_ABSEXT:
            // external name relative offset code word
            // Get the specs sequence number
            id = (buffer[1]<<8) | buffer[0];
            // clear all flag bits for this symbol
            // and, flag this spec as a used global data symbol
            specs[id].flags = SYMISUSED | SYMISDATA;

            if (length != WORDSIZE)
                fprintf(stderr, "ERR_BADRECSZ: IF_ABSEXT, line %d\n",lineno);
            nreloc += 1;
            // All of these are treated as "object code" for the first passs
            if (m[current].what != IF_OBJ)
            {
                current = newitem(IF_OBJ);
            }
            m[current].spec = id;
            // we don't bother to remember the code, just how big it is..
            m[current].size += WORDSIZE;
            cad += WORDSIZE;
            break;

        default:
            fprintf(stderr, "Unexpected tag at line#%d - not handled\n",lineno);
            // all other directives don't consume space
            break;
        }
    }
}

// Given a label ID, return the index of the label record
// Return zero if there's no such label
static int findlabel(int id)
{
    int i;

    for (i = 1; i < nl; i++)
        if (labels[i].labelid == id)
            return i;

    return 0;
}

// Reset the label list and then do a pass through the data
// to reset label records.
// We also want to check if the label is actually used.
static void initlabels()
{
    int cad, i, type, ptr, id;

    cad = 0;
    nl = 1;
    // loop over the item records
    for (i = 0; i < nm; i++)
    {
        type = m[i].what;
        m[i].address = cad;

        // Perform explicit action for each IF_XXX
        // So we can tweak actions in case of future enhancements
        // NB. There may be forward references to a label.
        // In that case we pre-define the label and can tag it as used.
        // Backward label references just cause the label to be tagged as used.
        switch(type)
        {
            case IF_OBJ:
                // plain object code
                cad += m[i].size;
                break;

            case IF_DATA:
                // data section offset word
                cad += m[i].size;
                break;

            case IF_CONST:
                // const section offset word
                cad += m[i].size;
                break;

            case IF_DISPLAY:
                // display section offset word
                cad += m[i].size;
                break;

            case IF_JUMP:
                // unconditional jump to label
                cad += m[i].size;
                // tag referenced label as used
                // pick up label id
                ptr = m[i].info;
                // get table index
                ptr = findlabel(ptr);
                if (ptr == 0)
                {
                    // Ah! We have a forward label reference
                    ptr = newlabel();
                    labels[ptr].flags = 0;
                }
                labels[ptr].flags = labels[ptr].flags | LABELISUSED;
                break;

            case IF_JCOND:
                // cond jump to label JE, JNE, JLE, JL, JGE, JG etc
                cad += m[i].size;
                // tag referenced label as used
                // pick up label id
                ptr = m[i].info;
                // get table index
                ptr = findlabel(ptr);
                if (ptr == 0)
                {
                    // Ah! We have a forward label reference
                    ptr = newlabel();
                    labels[ptr].flags = 0;
                }
                labels[ptr].flags = labels[ptr].flags | LABELISUSED;
                break;

            case IF_CALL:
                // call a label
                cad += m[i].size;
                // tag referenced label as used
                // pick up label id
                ptr = m[i].info;
                // get table index
                ptr = findlabel(ptr);
                if (ptr == 0)
                {
                    // Ah! We have a forward label reference
                    ptr = newlabel();
                    labels[ptr].flags = 0;
                }
                labels[ptr].flags = labels[ptr].flags | LABELISUSED;
                break;

            case IF_LABEL:
                // define a label
                id = m[i].info;
                // DANGER! Pass 2 redefines labels sometimes
                ptr = findlabel(id);
                if (ptr == 0)
                {
                    // Ah! We have an un-referenced label
                    // So tag as unused and let later records
                    // determine if this label is referenced
                    ptr = newlabel();
                    labels[ptr].flags = 0;
                }
                labels[ptr].labelid = id;
                labels[ptr].address = cad;
                break;

            case IF_FIXUP:
                // define location for stack fixup instruction
                cad += m[i].size;
                break;

            case IF_SETFIX:
                // stack fixup <location> <amount> <eventmask> <event entry>
                cad += m[i].size;
                break;

            case IF_REQEXT:
                // external name spec
                cad += m[i].size;
                break;

            case IF_REFLABEL:
                // label reference as relative address with optional offset
                cad += m[i].size;
                // tag referenced label as used
                // pick up label id
                ptr = m[i].info;
                // get table index
                ptr = findlabel(ptr);
                if (ptr == 0)
                {
                    // Ah! We have a forward label reference
                    ptr = newlabel();
                    labels[ptr].flags = 0;
                }
                labels[ptr].flags = labels[ptr].flags | LABELISUSED;
                break;

            case IF_REFEXT:
                // external name relative offset code word
                cad += m[i].size;
                break;

            case IF_BSS:
                // BSS section offset code word
                cad += m[i].size;
                break;

            case IF_COTWORD:
                // Constant table word
                break;

            case IF_DATWORD:
                // Data section word (repeated)
                break;

            case IF_SWTWORD:
                // switch table entry - actually a label ID
                // tag referenced label as used
                // pick up label id
                ptr = m[i].info;
                // get table index
                ptr = findlabel(ptr);
                if (ptr == 0)
                {
                    // Ah! We have a forward label reference
                    ptr = newlabel();
                    labels[ptr].flags = 0;
                }
                labels[ptr].flags = labels[ptr].flags | LABELISUSED;
                break;

            case IF_SOURCE:
                // name of the source file
                cad += m[i].size;
                break;

            case IF_DEFEXTCODE:
                // external name spec
                cad += m[i].size;
                break;

            case IF_DEFEXTDATA:
                // external name spec
                cad += m[i].size;
                break;

            case IF_SWT:
                // SWITCH table section offset code word
                cad += m[i].size;
                // This is a jump into the combined switch table entries.
                // The switch table head should be defined else where
                // This should generate a jump into the switch table
                // as a calculated offset from the switch table start
                break;

            case IF_LINE:
                // line number info for the debugger
                cad += m[i].size;
                break;

            case IF_ABSEXT:
                // external name relative offset code word
                cad += m[i].size;
                break;

            default:
                break;
        }
    }
}

// Simple routine that tries to "improve" the jumps.
// It returns "true" if it found an improvement.
// Unfortunately we need to iterate because every
// improvement moves all the downstream labels
// (which may allow further improvement).
// So this routine is called more than once.
static int improvejumpsizes()
{
    int i, type, ptr, distance, success;

    success = 0;
    for (i = 0; i < nm; i++)
    {
        type = m[i].what;
        if ((type == IF_JUMP) || (type == IF_JCOND))
        {
            // jump size not already improved?!?!
            if (m[i].size > 2)
            {
                // pick up label id
                ptr = m[i].info;
                // get table index
                ptr = findlabel(ptr);
                distance = labels[ptr].address - (m[i].address + 2);
                // could this be converted to a short byte jump?
                if ((-127 < distance) && (distance < 127))
                {
                    // Yes! so JFDI (= make it so)
                    m[i].size = 2;
                    // and tell the world we've done good
                    success = 1;
                }
            }
        }
    }
    return success;
}

// run through the list of external specs, removing those that
// have not actually been used, and mapping the indexes of those
// that remain to a simple zero-based index
void remapspecs()
{
    int i, index;

    // note, although Pass2 references are 1-based, our map is 0-based
    index = 0;
    for (i = 1; i <= nspecs; i++)
    {
        // default the value of the p3Index
        specs[i].p3index = 0;
        // However, check if this "spec" is used
        if ((specs[i].flags & SYMISUSED) != 0)
        {
            specs[i].p3index = index;
            index += 1;
        }
    }
    // reassign the specs counter so we know how many we will plant
    nspecs = index;
}

// Remember the sizes of various sections
// If another section type is required then
// define another "size" variable AND
// Global counter used to plant linker section size definitions

// also add code to determine the "size" value
// e.g. section ".text.get_pc" for use in shareable libraries

// the CODE section
static int codecount = 0;

// the CONST section
static int constcount = 0;

// the DATA section
static int datacount = 0;

// the BSS section
static int bsscount = 0;

// the SWTAB section (the switch table)
static int swtabcount = 0;

// the TRAP section
static int trapcount = 0;

// the LINENO section
// how many lines are present
static int linecount = 0;

// run through the database adding up the various section sizes
void computesizes()
{
    int i, type, size;

    for (i = 0; i < nm; i++)
    {
        // what .ibj directive is this
        type = m[i].what;
        // importantly, what size does it represent;
        size = m[i].size;

        // now to update the various "size" values
        // Remember, if a new "size" type is added
        // This is where it is evaluated
        switch(type) {
        case IF_OBJ:
            // plain object code
            codecount += size;
            break;

        case IF_DATA:
            // data section offset word
            codecount += size;
            break;

        case IF_CONST:
            // const section offset word
            codecount += size;
            break;

        case IF_DISPLAY:
            // display offset words
            codecount += size;
            break;

        case IF_JUMP:
            // unconditional jump to label
            codecount += size;
            break;

        case IF_JCOND:
            // cond jump to label JE, JNE, JLE, JL, JGE, JG
            codecount += size;
            break;

        case IF_CALL:
            // call a label
            codecount += size;
            break;

        case IF_LABEL:
            // this directive doesn't consume space
            break;

        case IF_FIXUP:
            // define location for stack fixup instruction
            codecount += size;
            break;

        case IF_SETFIX:
            // this directive doesn't consume space
            break;

        case IF_REQEXT:
            // this directive doesn't consume space
            break;

        case IF_REFLABEL:
            // label reference as a relative address with optional offset
            codecount += size;
            break;

        case IF_REFEXT:
            // external name relative offset code word
            codecount += size;
            break;

        case IF_BSS:
            // BSS section offset word
            codecount += size;
            break;

        case IF_COTWORD:
            // Constant table word
            constcount += size;
            break;

        case IF_DATWORD:
            // Data section word (repeated)
            datacount += size;
            break;

        case IF_SWTWORD:
            // switch table entry - actually a label ID
            // so "size" is 16 bit words (= label id),
            // but the entries will be 32 bits
            swtabcount += (size/2);
            break;

        case IF_SOURCE:
            // directive doesn't consume space
            break;

        case IF_DEFEXTCODE:
            // directive doesn't consume space
            break;

        case IF_DEFEXTDATA:
            // directive doesn't consume space
            break;

        case IF_SWT:
            // SWITCH table section offset code word
            codecount += size;
            break;

        case IF_LINE:
            // directive doesn't consume space
            break;

        case IF_ABSEXT:
            // external name relative offset code word
            codecount += size;
            break;

        default:
            fprintf(stderr, "Unexpected tag - not handled\n");
            // all other directives don't consume space
            break;
        }
    }

    // the trap section will contain one record for
    // every procedure we've found
    trapcount = ns;
    // remember the number of lines in the file
    linecount = nlines;
}

// Code for creating ELF object data
// The object data depends on the data-structure formed from the .ibj file
// and the calculated "size" data

// ELF Magic String
static char elfmagic[EI_NIDENT] = {
    0x7F, 'E', 'L', 'F', ELFCLASS32, ELFDATA2LSB, EV_CURRENT,
       0, 0, 0, 0, 0, 0, 0, 0, 0
};

// compiler version comment
static char vsncomment[] = "IMP2024 1st June 2024";

// N.B. path_buffer is initialised with the input filename
// The input filename MUST include the .ibj file extension
// This char array can then be tweaked to change .ibj
// to become .obj, .imp, .o as required
#ifdef MSVC
// working areas for file lookup
static char path_buffer[_MAX_PATH];
#else
// alternative working areas for file lookup
static char path_buffer[256];
#endif

// location of the result in the string table
static int  path_index;

// set up the data structures for the ELF file
Elf32_Ehdr  filehead;

// There will be (up to) 12 sections in the ELF file:
//      a pseudo-section containing the file header
//      string table of names        - .strtab
//      the code itself              - .text
//      the constant data            - .rodata
//      the writable (own) data      - .data
//      the switch table(s)          - .switch
//      the trap table               - .trap
//      the trap limit marker        - .traplimit
//      compiler version             - .comment
//      the symbol table             - .symtab
//      the code relocations         - .rel.text
//      the switch table relocations - .rel.switch
//      the trap table relocations   - .rel.trap
//      a pseudo-section containing the section header table
//
// On the first pass through we determine the size of each
// section (eliminating any of zero size) and preset the file
// pointers for the scatter-write routines.  On the second pass
// we can then write to the correct places in the file

// Internal ELF sections, according to the file writer
// zeroth section is a pseudo-section containing the file header
//     (for the file writer)
// the actual zeroth section is the NULL section (according to ELF)
// we define the section buffers
#define NULL_SECTION       0 // ELF null section
#define CODE_SECTION       1 // code itself              - .text
#define CODEREL_SECTION    2 // code relocations         - .rel.text
#define CONST_SECTION      3 // constant data            - .rodata
#define DATA_SECTION       4 // writable (own) data      - .data
#define BSS_SECTION        5 // (uninitialised) data     - .bss
#define SWTAB_SECTION      6 // switch table(s)          - .switch
#define SWTABREL_SECTION   7 // switch table relocations - .rel.switch
#define TRAP_SECTION       8 // trap table               - .trap (+_imptrapbase for main program)
#define TRAPREL_SECTION    9 // trap table relocations   - .rel.trap
#define TRAPLIMIT_SECTION 10 // trap limit marker        - .traplimit
#define LINE_SECTION      11 // line table               - .line (+_implinebase for main program)
#define LINEREL_SECTION   12 // line table relocations   - .rel.line
#define LINELIMIT_SECTION 13 // line limit marker        - .line.limit

#define COMMENT_SECTION   14 // compiler version         - .comment
#define SYMTAB_SECTION    15 // symbol table             - .symtab
#define STRTAB_SECTION    16 // string table of names    - .strtab
#define SHSTRTAB_SECTION  17 // string table of shnames  - .shstrtab
// Add in #define entries for extra sections as needed.
// Don't forget to update SHDR_SECTION to represent the last value
#define SHDR_SECTION      18 // fake section for the section header table

// define the relocation type to use
#define RELOCSZ      sizeof(Elf32_Rel)
#define RELOCTYPE    SHT_REL

// remember the size of an Elf symbol record
#define SYMSZ       sizeof(Elf32_Sym)

// Use an array for section[] and section_header[] so we can add
// extra sections as we update the IMP object files to implement
// shareable library capability.
// For that, we probably need to add global symbol _GLOBAL_OFFSET_TABLE_
// and extra .text.??? and .rel.??? sections

// Sequence of External ELF sections after we have stripped out empty ones...
// Some entries of section[] will be 0 (indicating unused)
// section[] entries > 0 indicate the sequence of loading sections
int section[SHDR_SECTION];
// Have a section header for every section.
// Some section headers might not be output
Elf32_Shdr section_header[SHDR_SECTION];

// Global counters used to plant linker section size definitions
// Also, when we map the sections into symbols we need a transformation,
// because we miss out any empty sections, so they don't get symbols
// These are defined to follow the order given by their XXX_SECTION
// Currently used for a limited number of XXX_SECTION
int symbols[SHDR_SECTION];

// the first user symbol table entry (offset by the above junk)
int firstusersymbol;

// Count of ELF sections actually being output
int nsections;

// these will get put into the string table and the indexes are here
static int trapbase_index;
static int traplimit_index;

// these will get put into the string table and the indexes are here
static int linebase_index;
static int linelimit_index;

// location of the _GLOBAL_OFFSET_TABLE_ name in the string table
static int got_index;

// intsyms == count of internal symbols defined
// This includes local symbols and section symbols
int intsyms;
// extsyms == count of external symbols defined
// This includes the global symbols (defined and referenced)
int extsyms;
// This is the count of internal + external defined symbols
// syms = intsyms + extsyms
// local variable used to avoid repeated addition calculation
int syms;
// Number of "real" ELF sections generated
int nsectsyms;

Elf32_Off populatesection(    int sectionid,
                            Elf32_Word size,
                            Elf32_Word link,
                            Elf32_Word info,
                            Elf32_Word type,
                            Elf32_Word flags,
                            Elf32_Addr addr,
                            Elf32_Word addralign,
                            Elf32_Word entsize,
                            Elf32_Off offset)
{
    section_header[sectionid].sh_size = size;
    section_header[sectionid].sh_link = link;
    section_header[sectionid].sh_info = info;
    section_header[sectionid].sh_type = type;
    section_header[sectionid].sh_flags = flags;
    section_header[sectionid].sh_addr = addr;
    section_header[sectionid].sh_addralign = addralign;
    section_header[sectionid].sh_entsize = entsize;
    section_header[sectionid].sh_offset = offset;
    setsize(sectionid, size);

    return offset + size;
}

static void writesymbol( Elf32_Word      nameindex,
                         unsigned char   info,
                         Elf32_Half      shndx,
                         Elf32_Addr      value)
{
    Elf32_Sym sym;

    sym.st_name = nameindex;
    sym.st_info = info;
    sym.st_shndx = shndx;
    sym.st_value = value;
    sym.st_size = 0;
    sym.st_other = 0;
    writeblock(SYMTAB_SECTION, (unsigned char *)&sym, SYMSZ);
}

void initobjectfile(FILE * output)
{
    Elf32_Off dataoffset;
    int i;

    // First tag each section[] as being unwanted
    for(i=0; i < SHDR_SECTION; i++)
    {
        section[i] = 0;
    }

    trapbase_index = newname(TRAPBASE);
    nsymdefs +=1;

    // Make every module require the _imptraplimit symbol
    traplimit_index = newname(TRAPLIMIT);
    nsymdefs +=1;

    linebase_index = newname(LINEBASE);
    nsymdefs +=1;

    // Make every module require the _implinelimit symbol
    linelimit_index = newname(LINELIMIT);
    nsymdefs +=1;

    // We add the special symbol _GLOBAL_OFFSET_TABLE_
    got_index = newname("_GLOBAL_OFFSET_TABLE_");
    nsymdefs += 1;

    // we always start with null
    nsections = 1;
    // only "real" sections (like code, const) also have symbols
    nsectsyms = 0;

    // Now step through each "section" to map from logical to real sections
    // in order to strip empty sections from the output file...

    // Now pass over each section[] to see if it is required
    if (codecount  != 0)
    {
        section[CODE_SECTION] = nsections++;
        section[CODEREL_SECTION] = nsections++;
        nsectsyms += 1;
    }

    if (constcount != 0)
    {
        section[CONST_SECTION] = nsections++;
        nsectsyms += 1;
    }

    if (datacount != 0)
    {
        section[DATA_SECTION] = nsections++;
        nsectsyms += 1;
    }

    if (bsscount = 0)
    {
        section[BSS_SECTION] = nsections++;
        nsectsyms += 1;
    }

    if (swtabcount != 0)
    {
        section[SWTAB_SECTION] = nsections++;
        section[SWTABREL_SECTION] = nsections++;
        nsectsyms += 1;
    }

    // add the TRAP section if there are trap records present
    // or we are dealing with the traplimit module
    if (trapcount != 0)
    {
        section[TRAP_SECTION] = nsections++;
        section[TRAPREL_SECTION] = nsections++;
    }

    // NB We should not have any procedures in the traplimit module
    // so the above code checking trapcount will find trapcount == 0
    if (traplimitflag != 0)
    {
        section[TRAPLIMIT_SECTION] = nsections++;
    }

    // add the LINE section even if no line records are present
    section[LINE_SECTION] = nsections++;
    // add the LINEREL section only if there are line records present
    if (linecount != 0)
    {
        section[LINEREL_SECTION] = nsections++;
    }

    // add the LINELIMIT section only if we are in the linelimit module
    if (linelimitflag != 0)
    {
        section[LINELIMIT_SECTION] = nsections++;
    }

    section[COMMENT_SECTION] = nsections++;
    section[SYMTAB_SECTION] = nsections++;
    section[STRTAB_SECTION] = nsections++;
    section[SHSTRTAB_SECTION] = nsections++;

    // Firstly, name the symbol table, string table, and section table
    section_header[SYMTAB_SECTION].sh_name = newsharename(".symtab");
    section_header[STRTAB_SECTION].sh_name = newsharename(".strtab");
    section_header[SHSTRTAB_SECTION].sh_name = newsharename(".shstrtab");

    section_header[CODE_SECTION].sh_name = newsharename(".text");
    // Name the relocation sections
    section_header[CODEREL_SECTION].sh_name = newsharename(".rel.text");
    section_header[SWTAB_SECTION].sh_name = newsharename(".imp.switch");
    section_header[SWTABREL_SECTION].sh_name = newsharename(".rel.imp.switch");
    if (mainprogflag != 0)
    {
        section_header[TRAPREL_SECTION].sh_name = newsharename(".rel.imp.trap.B");
    }
    else
    {
        section_header[TRAPREL_SECTION].sh_name = newsharename(".rel.imp.trap.D");
    }
    section_header[CONST_SECTION].sh_name = newsharename(".rodata");
    section_header[DATA_SECTION].sh_name = newsharename(".data");
    section_header[BSS_SECTION].sh_name = newsharename(".bss");

    if (mainprogflag != 0)
    {
        // name the trap section for the main IMP module
        section_header[TRAP_SECTION].sh_name = newsharename(".imp.trap.B");
    }
    else
    {
        // name the trap section for the non-main IMP modules
        section_header[TRAP_SECTION].sh_name = newsharename(".imp.trap.D");
    }

    if (traplimitflag != 0)
    {
        section_header[TRAPLIMIT_SECTION].sh_name = newsharename(".imp.trap.F");
    }

    if (mainprogflag != 0)
    {
        // name the line section for the main IMP module
        section_header[LINE_SECTION].sh_name = newsharename(".imp.line.B");
    }
    else
    {
        // name the line section for the non-main IMP modules
        section_header[LINE_SECTION].sh_name = newsharename(".imp.line.D");
    }

    if (linelimitflag != 0)
    {
        section_header[LINELIMIT_SECTION].sh_name = newsharename(".imp.line.F");
    }

    section_header[COMMENT_SECTION].sh_name = newsharename(".comment");

    // now set up our file writer so that it can work out the section offsets
    // First jump over the ELF header
    dataoffset = sizeof(Elf32_Ehdr);
    // Start writing just after the ELF header
    setfile(output, dataoffset);

    // Remember some size values may be 0
    // So the dataoffset values will still be accurate
    // even if the XXX_SECTION has zero size
    // now we fill in the rest of each of the section headers
    // We populate the section_header details in the order
    // given by the XXX_SECTION sequence
    dataoffset = populatesection(CODE_SECTION,
                                 codecount * BYTESZ,
                                 0,
                                 0,
                                 SHT_PROGBITS,
                                 SHF_ALLOC|SHF_EXECINSTR,
                                 0,
                                 4,
                                 0,
                                 dataoffset);

    dataoffset = populatesection(CODEREL_SECTION,
                                 nreloc * RELOCSZ,
                                 section[SYMTAB_SECTION],
                                 section[CODE_SECTION],
                                 RELOCTYPE,
                                 0,
                                 0,
                                 4,
                                 RELOCSZ,
                                 dataoffset);

    dataoffset = populatesection(CONST_SECTION,
                                 constcount * BYTESZ,
                                 0,
                                 0,
                                 SHT_PROGBITS,
                                 SHF_ALLOC,
                                 0,
                                 4,
                                 0,
                                 dataoffset);

    dataoffset = populatesection(DATA_SECTION,
                                 datacount * BYTESZ,
                                 0,
                                 0,
                                 SHT_PROGBITS,
                                 SHF_ALLOC|SHF_WRITE,
                                 0,
                                 4,
                                 0,
                                 dataoffset);

    dataoffset = populatesection(BSS_SECTION,
                                 bsscount * BYTESZ,
                                 0,
                                 0,
                                 SHT_NOBITS,
                                 SHF_ALLOC|SHF_WRITE,
                                 0,
                                 4,
                                 0,
                                 dataoffset);

    dataoffset = populatesection(SWTAB_SECTION,
                                 swtabcount * SWTABENTRYSZ,
                                 0,
                                 0,
                                 SHT_PROGBITS,
                                 SHF_ALLOC|SHF_WRITE,
                                 0,
                                 SWTABENTRYSZ,
                                 0,
                                 dataoffset);

    dataoffset = populatesection(SWTABREL_SECTION,
                                 swtabcount * RELOCSZ,
                                 section[SYMTAB_SECTION],
                                 section[SWTAB_SECTION],
                                 RELOCTYPE,
                                 0,
                                 0,
                                 SWTABENTRYSZ,
                                 RELOCSZ,
                                 dataoffset);

    // So that we can traverse the trap table at run time we must ensure that
    // the main trap table base is loaded first in the executable image.
    // The ELF linker groups those same-name sections in alphabetical
    // order using the token after a core name ".imp.trap."
    // (where the token is then discarded by the linker).
    // Why use letters B, D, F and not A,B,C?
    // In case we ever want to insert some other sections in the sequence...
    dataoffset = populatesection(TRAP_SECTION,
                                 trapcount * TRAPENTRYSZ,
                                 0,
                                 0,
                                 SHT_PROGBITS,
                                 SHF_ALLOC|SHF_WRITE,
                                 0,
                                 TRAPENTRYSZ,
                                 0,
                                 dataoffset);

    dataoffset = populatesection(TRAPREL_SECTION,
                                 trapcount * TRAPENTRYRELOC * RELOCSZ,
                                 section[SYMTAB_SECTION],
                                 section[TRAP_SECTION],
                                 RELOCTYPE,
                                 0,
                                 0,
                                 4,
                                 RELOCSZ,
                                 dataoffset);

    // the end section will be linked after all other trap tables
    // if present, the section contains one traptable entry
    // ASS-U-ME the TRAPLIMIT section is not required
    dataoffset = populatesection(TRAPLIMIT_SECTION,
                                 traplimitflag * TRAPENTRYSZ,
                                 0,
                                 0,
                                 SHT_PROGBITS,
                                 SHF_ALLOC|SHF_WRITE,
                                 0,
                                 TRAPENTRYSZ,
                                 0,
                                 dataoffset);

    // So that we can traverse the line table at run time we must ensure that
    // the main line table base is loaded first in the executable image.
    // The ELF linker groups those same-name sections in alphabetical
    // order using the token after a core name ".imp.line."
    // (where the token is then discarded by the linker).
    // Why use letters B, D, F and not A,B,C?
    // In case we ever want to insert some other sections in the sequence...
    // The LINE_SECTION is always present
    // but might not contain any lines
    dataoffset = populatesection(LINE_SECTION,
                                 LINEHEADERSZ + (linecount * LINEENTRYSZ),
                                 0,
                                 0,
                                 SHT_PROGBITS,
                                 SHF_ALLOC|SHF_WRITE,
                                 0,
                                 LINEHEADERSZ,
                                 0,
                                 dataoffset);

    // If there are no lines the LINEREL_SECTION is "empty"
    dataoffset = populatesection(LINEREL_SECTION,
                                 linecount * LINEENTRYRELOC * RELOCSZ,
                                 section[SYMTAB_SECTION],
                                 section[LINE_SECTION],
                                 RELOCTYPE,
                                 0,
                                 0,
                                 4,
                                 RELOCSZ,
                                 dataoffset);

    // the line limit section will be linked after all other line tables
    // if requested, the section contains one LINEHEADER entry
    // ASS-U-ME the LINELIMIT section is not required
    dataoffset = populatesection(LINELIMIT_SECTION,
                                 linelimitflag * LINEHEADERSZ,
                                 0,
                                 0,
                                 SHT_PROGBITS,
                                 SHF_ALLOC|SHF_WRITE,
                                 0,
                                 LINEHEADERSZ,
                                 0,
                                 dataoffset);

    dataoffset = populatesection(COMMENT_SECTION,
                                 sizeof(vsncomment),
                                 0,
                                 0,
                                 SHT_PROGBITS,
                                 0,
                                 0,
                                 1,
                                 0,
                                 dataoffset);

    // SYMTAB also includes null + comment symbols
    intsyms = 2 + nsectsyms + ns;
    extsyms = nsymdefs + nspecs;
    syms = intsyms + extsyms;
    dataoffset = populatesection(SYMTAB_SECTION,
                                 syms * SYMSZ,
                                 section[STRTAB_SECTION],
                                 intsyms,
                                 SHT_SYMTAB,
                                 0,
                                 0,
                                 4,
                                 SYMSZ,
                                 dataoffset);

    // We need to add the STRTAB_SECTION name
    // BEFORE creating the STRTAB_SECTION data
    dataoffset = populatesection(STRTAB_SECTION,
                                 namedp,
                                 0,
                                 0,
                                 SHT_STRTAB,
                                 0,
                                 0,
                                 1,
                                 0,
                                 dataoffset);

    // We need to add the SHSTRTAB_SECTION name
    // BEFORE creating the SHSTRTAB_SECTION data
    dataoffset = populatesection(SHSTRTAB_SECTION,
                                 sharedp,
                                 0,
                                 0,
                                 SHT_STRTAB,
                                 0,
                                 0,
                                 1,
                                 0,
                                 dataoffset);

    // Now assemble the main file header
    setsize(SHDR_SECTION, nsections * sizeof(Elf32_Shdr));
    for(i=0; i < EI_NIDENT; i++)
        filehead.e_ident[i] = elfmagic[i];
    filehead.e_type = ET_REL;
    filehead.e_machine = EM_386;
    filehead.e_version = EV_CURRENT;
    filehead.e_entry = 0;
    filehead.e_phoff = 0;
    filehead.e_shoff = dataoffset;
    filehead.e_flags = 0;
    filehead.e_ehsize = sizeof(Elf32_Ehdr);
    filehead.e_phentsize = 0;
    filehead.e_phnum = 0;
    filehead.e_shentsize = sizeof(Elf32_Shdr);
    filehead.e_shnum = nsections;
    filehead.e_shstrndx = section[SHSTRTAB_SECTION];

    // write the file header to the start of the file
    fwrite( &filehead, 1, sizeof(Elf32_Ehdr), output);

    // now write each section header to the appropriate part
    writeblock(SHDR_SECTION, (unsigned char *)&section_header[NULL_SECTION], sizeof(Elf32_Shdr));
    if (codecount != 0)
    {
        writeblock(SHDR_SECTION, (unsigned char *)&section_header[CODE_SECTION], sizeof(Elf32_Shdr));
        writeblock(SHDR_SECTION, (unsigned char *)&section_header[CODEREL_SECTION], sizeof(Elf32_Shdr));
    }

    if (constcount != 0)
        writeblock(SHDR_SECTION, (unsigned char *)&section_header[CONST_SECTION], sizeof(Elf32_Shdr));

    if (datacount != 0)
    {
        writeblock(SHDR_SECTION, (unsigned char *)&section_header[DATA_SECTION], sizeof(Elf32_Shdr));
    }

    if (bsscount != 0)
    {
        writeblock(SHDR_SECTION, (unsigned char *)&section_header[BSS_SECTION], sizeof(Elf32_Shdr));
    }

    if (swtabcount != 0)
    {
        writeblock(SHDR_SECTION, (unsigned char *)&section_header[SWTAB_SECTION], sizeof(Elf32_Shdr));
        writeblock(SHDR_SECTION, (unsigned char *)&section_header[SWTABREL_SECTION], sizeof(Elf32_Shdr));
    }

    if (trapcount != 0)
    {
        writeblock(SHDR_SECTION, (unsigned char *)&section_header[TRAP_SECTION], sizeof(Elf32_Shdr));
        writeblock(SHDR_SECTION, (unsigned char *)&section_header[TRAPREL_SECTION], sizeof(Elf32_Shdr));
    }

    if (traplimitflag != 0)
    {
        writeblock(SHDR_SECTION, (unsigned char *)&section_header[TRAPLIMIT_SECTION], sizeof(Elf32_Shdr));
    }


    if (linecount != 0)
    {
        writeblock(SHDR_SECTION, (unsigned char *)&section_header[LINE_SECTION], sizeof(Elf32_Shdr));
        writeblock(SHDR_SECTION, (unsigned char *)&section_header[LINEREL_SECTION], sizeof(Elf32_Shdr));
    }

    if (traplimitflag != 0)
    {
        writeblock(SHDR_SECTION, (unsigned char *)&section_header[LINELIMIT_SECTION], sizeof(Elf32_Shdr));
    }

    writeblock(SHDR_SECTION, (unsigned char *)&section_header[COMMENT_SECTION], sizeof(Elf32_Shdr));

    writeblock(SHDR_SECTION, (unsigned char *)&section_header[SYMTAB_SECTION],  sizeof(Elf32_Shdr));

    writeblock(SHDR_SECTION, (unsigned char *)&section_header[STRTAB_SECTION], sizeof(Elf32_Shdr));

    writeblock(SHDR_SECTION, (unsigned char *)&section_header[SHSTRTAB_SECTION], sizeof(Elf32_Shdr));

    // since it's not really part of anything else useful, we output
    // the linker directive now...
    writeblock(COMMENT_SECTION, (unsigned char *)vsncomment, sizeof(vsncomment));
}

// Output the dummy symbols in the symbol table for the filename and for each section
static void putsectionsymbols(FILE *output)
{
    int symbol;

    symbol = 0;
    // first we output the NULL symbol
    // (symbol 0)
    writesymbol( 0, 0, 0, 0);
    symbol += 1;

    // the first pseudo-symbol is the source filename, which we've stored in path_buffer
    // (symbol 1)
    writesymbol( path_index, (STB_LOCAL << 4) | STT_FILE, SHN_ABS, 0);
    symbol += 1;

    // now write out each of the section symbols (one for each section...)

    // code section symbol
    if (codecount  != 0)
    {
        symbols[CODE_SECTION] = symbol;
        writesymbol( 0, (STB_LOCAL << 4) | STT_SECTION, section[CODE_SECTION], 0 );
        symbol += 1;
    }

    // const section symbol
    if (constcount != 0)
    {
        symbols[CONST_SECTION] = symbol;
        writesymbol( 0, (STB_LOCAL << 4) | STT_SECTION, section[CONST_SECTION], 0 );
        symbol += 1;
    }

    // data section symbol
    if (datacount  != 0)
    {
        symbols[DATA_SECTION] = symbol;
        writesymbol( 0, (STB_LOCAL << 4) | STT_SECTION, section[DATA_SECTION], 0 );
        symbol += 1;
    }

    // bss section symbol
    if (bsscount != 0)
    {
        symbols[BSS_SECTION] = symbol;
        writesymbol( 0, (STB_LOCAL << 4) | STT_SECTION, section[BSS_SECTION], 0 );
        symbol += 1;
    }

    // switch table section symbol
    if (swtabcount != 0)
    {
        symbols[SWTAB_SECTION] = symbol;
        writesymbol( 0, (STB_LOCAL << 4) | STT_SECTION, section[SWTAB_SECTION], 0 );
        symbol += 1;
    }

    // remember where the program symbol table will start
    // NB this may need to be "tweaked" if there are special and or local symbols
    //    added as part of the symbol table.
    firstusersymbol = symbol;
}

// write the special global symbols to the symbol table area
static void putspecialsymbols(FILE *output)
{
    // Now write the _GLOBAL_OFFSET_TABLE_ symbol
    writesymbol( got_index, (STB_GLOBAL << 4), 0, 0);
    // Oops! We have added the special symbol _GLOBAL_OFFSET_TABLE_
    // So, we jump over this global symbol
    firstusersymbol += 1;

    // Now for the _imptrapbase and _imptraplimit symbols

    // Set up the default values for the _imptrapXXX symbols
    // first address of this symbol

    if (mainprogflag != 0)
    {
        // This is the main program
        // So "define" trapbase symbol ("_imptrapbase")
        // point to the trap section
        writesymbol( trapbase_index, (STB_GLOBAL << 4) | STT_OBJECT, section[TRAP_SECTION], 0 );
    }
    else
    {
        writesymbol( trapbase_index, (STB_GLOBAL << 4) | STT_OBJECT, 0, 0 );
    }
    firstusersymbol += 1;

    // We always add the _imptraplimit symbol
    // So add the traplimit symbol ("_imptraplimit")
    // But only define it in the TRAPLIMIT section
    if (traplimitflag != 0)
    {
        // This is the traplimit module
        // point to the relevant trapXXX section
        // The "_imptraplimit" symbol must be in the TRAPLIMIT section
        // because we place the symbol at the start of the section
        // i.e sym.st_value = 0
        writesymbol( traplimit_index, (STB_GLOBAL << 4) | STT_OBJECT, section[TRAPLIMIT_SECTION], 0 );
    }
    else
    {
        // This is not the traplimit module
        // So, say it's not defined
        writesymbol( traplimit_index, (STB_GLOBAL << 4) | STT_OBJECT, 0, 0 );
    }
    firstusersymbol += 1;

    // Now for the _implinebase and _implinelimit symbols
    // Set up the default values for the _implineXXX symbols
    // first address of this symbol
    if ((mainprogflag != 0) & (linecount != 0))
    {
        // This is the main program
        // So define the linebase symbol ("_implinebase")
        // It's definition is at the start of the line section
        // belonging to the main program
        writesymbol( linebase_index, (STB_GLOBAL << 4) | STT_OBJECT, section[LINE_SECTION], 0 );
    }
    else
    {
        writesymbol( linebase_index, (STB_GLOBAL << 4) | STT_OBJECT, 0, 0 );
    }
    firstusersymbol += 1;

    // We always add the _implinelimit symbol
    // So add the linelimit symbol ("_implinelimit")
    // But only define it in the LINELIMIT section
    if (linelimitflag != 0)
    {
        // This is the linelimit module
        // It's definition belongs to the LINELIMIT section
        // And the linelimit symbol is at the start of the section
        // (so sym.st_value = 0)
        writesymbol( linelimit_index, (STB_GLOBAL << 4) | STT_OBJECT, section[LINELIMIT_SECTION], 0 );
    }
    else
    {
        // This is not the linelimit module
        // So, say it's not defined
        writesymbol( linelimit_index, (STB_GLOBAL << 4) | STT_OBJECT, 0, 0 );
    }
    firstusersymbol += 1;

}

// write the external spec table to the symbol table area
static void putinternalspecs(FILE *output)
{
    int i;

    for (i = 0; i < ns; i++)
    {
        // remember the symbol id for this routine
        stackfix[i].symid = firstusersymbol;
        writesymbol( stackfix[i].namep,
                     (STB_LOCAL << 4) | STT_FUNC,
                     section[CODE_SECTION],
                     m[stackfix[i].hint].address);

        // Another symbol inserted before the user symbols
        firstusersymbol += 1;
    }
}

// write the external spec table to the symbol table area
// Each "spec" entry references an external(=global) function symbol
static void putexternalspecs(FILE *output)
{
    int i, type, ptr, index;
    Elf32_Sym sym;

    // pass 2 spec's use 1-based IDs
    index = 1;

    for (i = 0; i < nm; i++)
    {
        type = m[i].what;
        ptr = m[i].info;
        if (type == IF_REQEXT)
        {
            // but, only write this symbol if used
            if ((specs[index].flags & SYMISUSED) != 0)
            {
                // check if this is a reference to a data or function symbol
                if ((specs[index].flags & SYMISDATA) != 0)
                {
                    // tag as global data symbol
                    writesymbol( m[i].info, (STB_GLOBAL << 4) | STT_OBJECT, 0, 0);
                }
                else
                {
                    // tag as global code/function symbol
                    writesymbol( m[i].info, (STB_GLOBAL << 4) | STT_FUNC, 0, 0);
                }
            }
            index += 1;
        }
    }
}

// write the external definitions to the symbol table
static void putexternaldefs(FILE *output)
{
    int i, type;

    for (i = 0; i < nm; i++)
    {
        type = m[i].what;

        // Set up the symbol's data
        // We assume this is a global symbol
        // address of this item

        // Now detect if this is a genuine global symbol
        if (type == IF_DEFEXTCODE)
        {
            // This is a global symbol for a function
            // So, tag as global code symbol
            // So, point to the code section
            writesymbol( m[i].info, (STB_GLOBAL << 4) | STT_FUNC, section[CODE_SECTION], m[i].address);
        }
        if (type == IF_DEFEXTDATA)
        {
            // This is a global symbol for a data item
            // So, tag as global data symbol
            // So, point to the data section
            writesymbol( m[i].info, (STB_GLOBAL << 4) | STT_OBJECT, section[DATA_SECTION], m[i].address);
        }
    }
}

// cond jump to label JE, JNE, JG, JGE, JL, JLE, JA, JAE, JB, JBE
static unsigned char jcondop[10] = {
    0x74, 0x75, 0x7F, 0x7D, 0x7C, 0x7E, 0x77, 0x73, 0x72, 0x76,
};

static unsigned char jfalseop[10] = {
    0x75, 0x74, 0x7E, 0x7C, 0x7D, 0x7F, 0x76, 0x72, 0x73, 0x77,
};

// Main Pass - Reread the input file and write the object code
static void putcode(FILE *input, FILE *output)
{
    int type, length, current, ptr, id, value, condition, cad, i, segidx;
    int swtp, offset;
    int count;
    unsigned char buffer[256];

    // reset the line number information
    nlines = 0;
    lastlinead = -1;

    current = 0;
    cad = 0;
    swtp = 0;
    for(;;)
    {
        readifrecord(input, &type, &length, buffer);
        if (type < 0)
            break;
        switch(type)
        {
        case IF_OBJ:
            // plain object code
            if (m[current].what != IF_OBJ)
                current += 1;
            for (i = 0; i < length; i++)
                writebyte(CODE_SECTION,buffer[i]);
            cad += length;
            break;

        case IF_DATA:
            // data section offset word
            if (m[current].what != IF_OBJ)
                current += 1;
            for (i=0; i < WORDSIZE; i++)
                writebyte(CODE_SECTION, buffer[i]);

            segidx = symbols[DATA_SECTION];

            // offset in the section of the word to relocate
            writew32(CODEREL_SECTION, cad);
           // symbol for section
            writew32(CODEREL_SECTION, (segidx<<8)|R_386_32);
            cad += WORDSIZE;
            break;

        case IF_CONST:
            // const section offset word
            if (m[current].what != IF_OBJ)
                current += 1;
            for (i=0; i < WORDSIZE; i++)
                writebyte(CODE_SECTION, buffer[i]);

            segidx = symbols[CONST_SECTION];

            // offset in the section of the word to relocate
            writew32(CODEREL_SECTION, cad);
            // symbol for section
            writew32(CODEREL_SECTION, (segidx<<8)|R_386_32);
            cad += WORDSIZE;
            break;

        case IF_DISPLAY:
            // DISPLAY should have been converted to OBJ
            break;

        case IF_JUMP:
            // unconditional jump to label
            current += 1;
            // get the target label number
            id = buffer[0] | (buffer[1] << 8);
            ptr = findlabel(id);
            value = labels[ptr].address;
            // Is this a short jump?
            if (m[current].size == 2)
            {
                writebyte(CODE_SECTION, 0xEB);
                writebyte(CODE_SECTION, value - (m[current].address + 2));
                cad += 2;
            }
            else
            {
                // JMP
                writebyte(CODE_SECTION, 0xE9);
                value = value - (m[current].address + 5);
                writew32(CODE_SECTION, value);
                cad += 5;
            }
            break;

        case IF_JCOND:
            // cond jump to label JE, JNE, JLE, JL, JGE, JG
            current += 1;
            condition = buffer[0];
            // get the target label number
            id = buffer[1] | (buffer[2] << 8);
            ptr = findlabel(id);
            value = labels[ptr].address;
            // Is this a short jump?
            if (m[current].size == 2)
            {
                writebyte(CODE_SECTION, jcondop[condition]);
                writebyte(CODE_SECTION, value - (m[current].address + 2));
                cad += 2;
            }
            else
            {
                // prefix
                writebyte(CODE_SECTION, 0x0F);
                writebyte(CODE_SECTION, jcondop[condition] + 0x10);
                value = value - (m[current].address + 6);
                writew32(CODE_SECTION, value);
                cad += 6;
            }
            break;

        case IF_CALL:
            // call a label
            current += 1;
            // get the target label number
            id = buffer[0] | (buffer[1] << 8);
            ptr = findlabel(id);
            value = labels[ptr].address;
            // write a CALL instruction
            writebyte(CODE_SECTION, 0xE8);
            value = value - (m[current].address + 5);
            writew32(CODE_SECTION, value);
            cad += 5;
            break;

        case IF_LABEL:
            // define a label
            current += 1;
            break;

        case IF_FIXUP:
            // define location for stack fixup instruction
            current += 1;
            value = m[current].info;
            // For backward compatibility reasons (mostly because it kept messing
            // me up in development) we will plant suitable code whether this is
            // a "classic" 8086 fixup request - i.e. plant SUB SP,nnnn - or a new
            // style 80286 fixup request to plant ENTER nnnn,level.  We can tell
            // them apart because the classic passes only two parameter bytes.
            if (length == 2)
            {
                // SUB
                writebyte(CODE_SECTION, 0x81);
                // SP
                writebyte(CODE_SECTION, 0xEC);
                // Stack displacement
                writew16(CODE_SECTION, value);
            }
            else
            {
                // ENTER
                writebyte(CODE_SECTION, 0xC8);
                // Stack displacement
                writew16(CODE_SECTION, value);
                // level
                writebyte(CODE_SECTION, buffer[2]);
            }
            // We now update our procedure record with the actual block start location
            // get the id number for fixup
            id = buffer[0] | (buffer[1] << 8);
            for (ptr = 0; ptr < ns; ptr++)
            {
                // have we found it?
                if (stackfix[ptr].id == id)
                {
                    stackfix[ptr].start = cad;
                    break;
                }
            }
            cad += 4;
            break;

        case IF_SETFIX:
            // stack fixup <location> <amount> <eventmask> <event entry>
            // We don't need to do anything in the code stream here, but we use
            // this record to trigger an update of the end point in our block table

            // get the id number for fixup
            id = buffer[0] | (buffer[1] << 8);
            for (ptr = 0; ptr < ns; ptr++)
            {
                // have we found it?
                if (stackfix[ptr].id == id)
                {
                    stackfix[ptr].end = cad;
                    break;
                }
            }
            break;

        case IF_REQEXT:
            // external name spec
            // already taken care of, but need to advance "current"
            current += 1;
            break;

        case IF_REFLABEL:
            // plant a label's relative address with optional offset
            current += 1;
            // get the target label number
            id = buffer[0] | (buffer[1] << 8);
            // and the offset
            offset = buffer[2] | (buffer[3] << 8);
            ptr = findlabel(id);
            // get the relative address of label from code section start
            value = labels[ptr].address;
            // REFLABEL is WORDSIZE, then extra offset
            value = value - (m[current].address + WORDSIZE + offset);
            // we now have the relative address + optional offset of label from current location
            writew32(CODE_SECTION, value);
            cad += 4;
            break;

        case IF_REFEXT:
            // external name relative offset code word
            if (m[current].what != IF_OBJ) current += 1;

            // ABD - the GNU linker doesn't correctly do Intel PC relative fixups,
            // because it uses the location after the offset, not before it... so,
            // we need to plant an offset of -4 for the call
            writew32(CODE_SECTION,-4);

            // reference index is in buffer[0,1]
            id = (buffer[1]<<8)|buffer[0];
            // but the absolute 16-bit offset (should always be 0)
            offset = buffer[2] | (buffer[3] << 8);
            // now to remap according to our table
            id = specs[id].p3index;
            // skip the symbol table entries for the sections
            id += firstusersymbol;

            // note the offset in section of word to relocate
            writew32(CODEREL_SECTION, cad);
            // indicate the symbol index for this reference
            writew32(CODEREL_SECTION, (id<<8)|R_386_PC32);
            cad += WORDSIZE;
            break;

        case IF_BSS:
            // BSS should have been converted to OBJ
            break;

        case IF_COTWORD:
            // Constant table word
            if (m[current].what != IF_COTWORD)
            {
                current += 1;
            }
            for (i=0; i < 2; i++)
                writebyte(CONST_SECTION, buffer[i]);
            break;

        case IF_DATWORD:
            // Data section word
            if (m[current].what != IF_DATWORD)
            {
                current += 1;
            }
            // determine how many halfwords (= number of 2 byte chunks)
            // Ensure count is zero
            count = 0;
            if (length == 2)
            {
                // old format of IF_DATWORD
                count = 1;
            }
            else if (length == 4)
            {
                // new format of IF_DATWORD
                count = buffer[2] | (buffer[3] << 8);
            }
//fprintf(stderr, "PUTCODE: DATWORD has length=%d, count=%d\n", length,count);

            // determine how many bytes to add to the DATA section
            for (i=0; i < count;i++)
            {
                writebyte(DATA_SECTION, buffer[0]);
                writebyte(DATA_SECTION, buffer[1]);
            }
            break;

        case IF_SWTWORD:
            // switch table entry - actually a label ID
            if (m[current].what != IF_SWTWORD)
            {
                current += 1;
            }
            // get the target label number
            id = buffer[0] | (buffer[1] << 8);
            ptr = findlabel(id);
            value = labels[ptr].address;

            writew32(SWTAB_SECTION, value);
            // we must also plant a relocation record to make this a code address
            // put the offset in section of word to relocate
            writew32(SWTABREL_SECTION, swtp);
            // put the symbol for section
            writew32(SWTABREL_SECTION, (symbols[CODE_SECTION]<<8)|R_386_32);
            swtp += 4;
            break;

        case IF_SOURCE:
            // name of the source file
            // HACK ALERT - we actually ignore the file name from PASS2 because
            // we've got a nicer library interface and we can get the REAL path
            break;

        case IF_DEFEXTCODE:
            // define a code label that is external
            // already taken care of, but need to advance "current"
            current += 1;
            break;

        case IF_DEFEXTDATA:
            // define a data label that is external
            // already taken care of, but need to advance "current"
            current += 1;
            break;

        case IF_SWT:
            // SWITCH table section offset code word
            if (m[current].what != IF_OBJ)
                current += 1;
            for (i=0; i < WORDSIZE; i++)
                writebyte(CODE_SECTION, buffer[i]);

            segidx = symbols[SWTAB_SECTION];

            // note the offset in the section of the word to relocate
            writew32(CODEREL_SECTION, cad);
            // note the symbol for the section
            writew32(CODEREL_SECTION, (segidx<<8)|R_386_32);
            cad += WORDSIZE;
            break;

        case IF_LINE:
            // line number info for the debugger
            value = buffer[0] | (buffer[1] << 8);
            newlineno(value, cad);
            break;

        case IF_ABSEXT:
            // external name absolute offset code word (data external)
            if (m[current].what != IF_OBJ) current += 1;
            // Now obtain the id, offset values
            id = ((buffer[1]<<8) | buffer[0]);
            offset = (buffer[3]<<8) | buffer[2];

            writew32(CODE_SECTION, offset );

            // id == reference index is in buffer[2,3],
            // which we should already have to remap according to our table
            id = specs[id].p3index;
            // skip the symbol table entries for the sections
            id += firstusersymbol;

            // put the offset in the section of the word to relocate
            writew32(CODEREL_SECTION, cad);
            // put the symbol index for this reference
            writew32(CODEREL_SECTION, (id<<8)|R_386_32);
            // JDM JDM
            // The current intermediate code can now distinguish between
            // relative and absolute external relocations,
            // so we can do external data fixups.... !
            cad += WORDSIZE;
            break;

        default:
            fprintf(stderr, "Unexpected tag - not handled\n");
            // all other directives don't consume space
            break;
        }
    }
}

// plant the array of blocks used by %signal to trap
// events in the trap section.  These blocks contain:
// <StartAddr32><EndAddr32><TrapAddr32><FromAddr32><EventMask16><Name[14]>
static void puttraptable(FILE *output)
{
    int i, j, addr;
    Elf32_Sym sym;
    struct stfix *sp;
    char *namep;
    int address[4],offset[4];
    int segidx;

    for (i = 0; i < ns; i++)
    {
        sp = &stackfix[i];
        // Obtain the offset values of the various locations
        // Each value is an offset inside the .text section
        offset[0] = sp->start;
        offset[1] = sp->end;
        // trap and evfrom are actually labels, so we look them up
        // and get the relevant address
        offset[2] = labels[findlabel(sp->trap)].address;
        offset[3] = labels[findlabel(sp->evfrom)].address;

        // If adding relocations from the .trap section base then
        // 1) relocation symbol = .text symbol
        // 2) address[j] = offset[j]
        // that is:
        // segidx = symbols[CODE_SECTION];
        // address[0] = offset[0];
        // address[1] = offset[1];
        // address[2] = offset[2];
        // address[3] = offset[3];

        // If adding relocations from the routine symbol base then
        // 1) relocation symbol = routine symbol
        // 2) address[j] = offset[j] - offset[0]
        // that is:
        // segidx = sp->symid;
        // address[0] = offset[0] - offset[0];
        // address[1] = offset[1] - offset[0];
        // address[2] = offset[2] - offset[0];
        // address[3] = offset[3] - offset[0];

        // Decision:
        // Use as relocation base, the routine symbol
        segidx = sp->symid;
        for (j=0; j < 4; j++)
        {
            address[j] = offset[j] - offset[0];
        }

        // add the location of the routine start
        writew32(TRAP_SECTION, address[0]);
        // add the location of the routine end
        writew32(TRAP_SECTION, address[1]);

        // trap and evfrom are actually labels, so we look them up
        // find and put the location of the trap start
        writew32(TRAP_SECTION, address[2]);
        // find and put the location of the trap evfrom
        writew32(TRAP_SECTION, address[3]);
        // put the events caught by the trap
        writew16(TRAP_SECTION, sp->events);
        // write the name of the routine
        namep = &named[sp->namep];
        for(j=0; j<14; j++)
        {
            writebyte(TRAP_SECTION, *namep);
            if (*namep) namep++;
        }

        // Of course the four code addresses we've just planted
        // - start/end/entry/from
        // These all need relocating by the base symbol chosen
        // so we do four relocation records next...
        addr = i * 32;  // address of first words to relocate
        for (j=0; j < 4; j++)
        {
            // offset in this section of the word to relocate
            writew32(TRAPREL_SECTION, addr + (j * 4));
            // use the symbol index for the chosen relocation base
            // that is, symbol index for .text or for the local routine
            writew32(TRAPREL_SECTION, (segidx<<8)|R_386_32);
        }
    }

    // populate the special section TRAPLIMIT (if present)
    if (traplimitflag != 0)
    {
        // put an empty TRAPENTRY into the trap section
        for (i=0; i < TRAPENTRYSZ; i++) writebyte(TRAPLIMIT_SECTION, 0);
    }
}

static void putlineheader( int sectionid, int linecount )
{
    int i, length, maxlength;
    char * filename;

    // A line$X section has a record structure
    // Field 1) module name (counted string in IMP string format)
    // Field 2) line count (32-bit count of line entries)
    // Field 3..line count + 3) line id, lineaddress (an array of pair of 32-integers)

    // We need to determine the size of the longest IMP string
    // we can store in the lineheader data structure
    // Start off with maxlength = LINEHEADERSZ
    maxlength = LINEHEADERSZ;
    // take into account the 4 bytes used for the line count
    maxlength = maxlength - 4;
    // take into account the byte used to store the IMP string length
    maxlength = maxlength - 1;

    // Obtain the source filename, from the path_buffer
//    filename = path_buffer;
    filename = modulename;
    // plus one (to store the string length as a byte)
    length = strlen(modulename);

    // Now adjust the length of the filename
    // so we can store it in the lineheader
    if (length > maxlength) length = maxlength;

    // Now the write the "filename" in the lineheader in IMP string format
    // First the byte containing the IMP string length
    writebyte(sectionid, length&255 );

    // Now the chars of the (possibly truncated) filename 
    for (i = 0; i < length; i++)
    {
        writebyte(sectionid, filename[i]&255 );
    }

    // Now the padding of null chars
    for (i = length; i < maxlength; i++)
    {
        writebyte(sectionid, 0 );
    }

    // Now indicate how many line entries for this module
    writew32(sectionid, linecount);
}

// Fill in the line number section of the object file
static void putlinenumbers(FILE *output)
{
    int i, remainder;
    int segidx;

    // work out how many bytes to write
    remainder = LINEHEADERSZ + (linecount * LINEENTRYSZ);

    // output the LINE header
    putlineheader( LINE_SECTION, linecount );

    // ok, one LINEHEADERSZ bytes just written
    // so adjust the remainder bytes count yet to write
    remainder = remainder - LINEHEADERSZ;

    // Output the appropriate line details in the LINE_SECTION
    // each line entry will have the line number and its address
    if (linecount != 0)
    {
        // Add the relocation data for each line
        // Use as relocation base, the .text symbol
        segidx = symbols[CODE_SECTION];

        // Now output the lineentry data for each line
        for (i = 0;i < linecount; i++)
        {
            // Add the lineentry data for each line
            // Store the line number
            writew32(LINE_SECTION, lines[i].line);
            // Next store its offset
            // which is from the start of the modules CODE_SECTION
            writew32(LINE_SECTION, lines[i].offset);
            // Another LINEENTRYSZ just output
            // so adjust the count of bytes that remain to write
            remainder = remainder - LINEENTRYSZ;

            // Now add the relocation data for the offset code address we've just planted
            // offset in this section of the word to relocate
            // The offset is the second value of each 4-byte integer pair
            writew32(LINEREL_SECTION, LINEHEADERSZ + 4 * (2*i + 1));
            // use the symbol index for the chosen relocation base
            // that is relocate by the .text base symbol
            // use the symbol index for the chosen relocation base
            // that is, symbol index for .text or for the local routine
            writew32(LINEREL_SECTION, (segidx<<8)|R_386_32);
        }
        // However LINE_SECTION is a multiple of LINEHEADERSZ
        // so we need to padd out the section with zeros
        // current value of remainder tells how many zero bytes to write
        for (i=0;i < remainder;i++)
        {
            writebyte(LINE_SECTION, 0);
        }
    }

    // Now to write out data for the LINELIMIT_SECTION
    if (linelimitflag != 0)
    {
        // output the only LINELIMIT header
        putlineheader( LINELIMIT_SECTION, 0 );
    }
}

// Write the string table to the output file.  Note that we
// write all of our internal string table, even though not
// all of the entries are used/needed by the linker, or even
// referenced within this object file
static void putstringtables(FILE *output)
{
    writeblock(STRTAB_SECTION, (unsigned char *)named, namedp);
    writeblock(SHSTRTAB_SECTION, (unsigned char *)shared, sharedp);
}

void dumpobjectfile( char *inname, char *outname )
{
    FILE * in;
    FILE * out;
    int i;

    // So, first open the input file
    in = fopen(inname, "r");

    // Next tweak the inname to form the output (,o) filename
    i = strlen(inname);

    // First: tweak the file extension from .ibj to .obj
    // Only need to alter the last two chars
    // NB char array index starts at 0
    inname[i - 3] = 'o';
    inname[i - 2] = 0;
    inname[i - 1] = 0;

    // Now open the output file
    out = fopen(inname, "wb");
    if (out == NULL)
    {
        perror("Can't open output file");
        fprintf(stderr, "Can't open output file '%s'\n",inname);
        exit(1);
    }

    initobjectfile(out);
    // NB global symbols must come AFTER the local (internal) symbols
    // internal symbols:
    //    section symbols (including null + comment) + local function symbols
    // external/global symbols:
    //    special + global defs (data + code) + global refs (data + code)

    // First, write all the "internal" symbols
    // "internal" => they are well-defined within this module
    putsectionsymbols(out);
    putinternalspecs(out);

    // Now start the "external" symbols
    // output the special symbols (global but un-defined)
    putspecialsymbols(out);
    // Now continue with more "external" symbols
    // the external global references (data + code)
    putexternalspecs(out);
    // the external global definitions (data + code)
    putexternaldefs(out);

    // re-read the .ibj file and output the code data
    putcode(in, out);

    // now plant the trap table
    puttraptable(out);

    // now output the line number records for the debugger
    putlinenumbers(out);

    putstringtables(out);

    flushout();

    fclose(in);
    fclose(out);
}

int main(int argc, char **argv)
{
    int i;
    int datasize;

    if ((argc != 2) && (argc != 3))
    {
        fprintf(stderr, "Unexpected number of parameters for PASS3ELF!\n\n");
        fprintf(stderr, "Usage:  PASS3 <intermediatefile> <objfile>?\n");
        exit(1);
    }

    // in order to get a useful debug output,
    // we try to recreate the input file name by assuming that
    // the .ibj files have the same base name (as the .imp files)
    // and are in the same directory as the imp source.
#ifdef MSVC
    // turn it into a full name
    _fullpath(path_buffer, argv[1], _MAX_PATH);
#else
    strcpy(path_buffer,realpath(argv[1], NULL));
#endif
    // At this point we have the full filename of the input file
    // held in the path_buffer char array.

    // Now tweak the file extension from .ibj to .imp
    // Only need to alter the last two chars
    // NB char array index starts at 0
    i = strlen(path_buffer);
    path_buffer[i - 2] = 'm';
    path_buffer[i - 1] = 'p';
    // Now put it in the string table (as the first entry)
    path_index = newname(path_buffer);

    // we now continue with the file names specified by argv[1],argv[2]
    readpass1( argv[1] );

    initlabels();

    while (improvejumpsizes())
        initlabels();
    computesizes();

    remapspecs();

    if (argc == 2)
    {
        dumpobjectfile( argv[1], argv[1] );
    }
    else
    {
        dumpobjectfile( argv[1], argv[2] );
    }

    fprintf(stderr, "\n\n");
    fprintf(stderr, " ELF object file generated from IMP source file: '%s'\n",path_buffer);

    datasize = datacount * BYTESZ
             + constcount * BYTESZ
             + swtabcount * SWTABENTRYSZ
             + bsscount * BYTESZ;
    fprintf(stderr, " +----------+---------------------+---------+---------+---------+------------+\n");
    fprintf(stderr, " | Sections |       Symbols       | Code    | Data    | Diag    | Total size |\n");
    fprintf(stderr, " +----------+----------+----------+---------+---------+---------+------------+\n");
    fprintf(stderr, " |  (count) | Internal | External | (bytes) | (bytes) | (bytes) | (bytes)    |\n");
    fprintf(stderr, " +----------+----------+----------+---------+---------+---------+------------+\n");
    fprintf(stderr, " | %8d | %8d | %8d | %7d | %7d | %7d | %10d |\n",
                    nsections,
                    intsyms,
                    extsyms,
                    codecount * BYTESZ,
                    datasize,
                    trapcount * TRAPENTRYSZ,
                    datasize
                    + codecount * BYTESZ
                    + trapcount * TRAPENTRYSZ);
    fprintf(stderr, " +----------+----------+----------+---------+---------+---------+------------+\n");
    fprintf(stderr, "\n\n");

    exit(0);
}
