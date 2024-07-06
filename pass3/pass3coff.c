// IMP Compiler for 80386 - pass 3
// COFF format object file generator

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
#include "pass3coff.h"

#define BYTESZ          1
#define SWTABENTRYSZ    4
#define MAINPROGNAME    "_main"
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
#define LINESTART	"_implinebase"
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
#define MAXLABEL 10000
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
#define MAXNAME 5000
char named[MAXNAME];
int namedp = 0;

// The share name dictionary is filled by section names zero terminated
// - the M record points to the first character of the corresponding name.
#define MAXSHNAME 500
char shared[MAXSHNAME];
int sharedp = 0;

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
                // it is also the location of the trapbase symbol definition
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
                    //and tell the world we've done good
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

// NB the various xxxsize variables are used at various points
//    but the values are defined by initobjectfile routine
// the CODE section
static int codecount = 0;
static int codesize = 0;

// the CONST section
static int constcount = 0;
static int constsize = 0;

// the DATA section
static int datacount = 0;
static int datasize = 0;

// the BSS section
static int bsscount = 0;
static int bsssize = 0;

// the SWTAB section (the switch table)
static int swtabcount = 0;
static int swtabsize = 0;

// the TRAP section
static int trapcount = 0;
static int trapsize = 0;

// the TRAPLIMIT section
// this is only present if this the traplimit module
// "traplimitflag" indicate presence/absence of TRAPLIMIT_SECTION
static int traplimitsize = 0;

// the LINENO section
static int linecount = 0;
static int linesize = 0;

// the LINELIMIT section
// this is only present if this is a linelimit module
// "linelimitflag" indicate presence/absence of LINELIMIT_SECTION
static int linelimitsize = 0;

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

    // finally, the trap section will contain one record for
    // every procedure we've found
    trapcount = ns;

    if (linelimitflag == 0) linecount = nlines;
}

// Code for creating COFF object data
// The object data depends on the data-structure formed from the .ibj file
// and the calculated "size" data

// set up the data structures for the COFF file
// we are going to have one file header, then five section headers
// for each of code, const, data, switch table and trap data, plus
// a sixth dummy section for the trap table end if we happen to be
// a main program.
// The sections are going to be in that order, each directly following
// the preceding one.  Then there is going to be the relocation list,
// the line-number list, the symbol table, and the string table (for
// names longer than 8 chars).
struct cofffilehdr	filehead;

// Internal coff sections, according to the file writer
#define DIRECTIVE_SECTION   1
#define CODE_SECTION        2
#define CONST_SECTION       3
#define DATA_SECTION        4
#define SWTAB_SECTION       5
#define TRAP_SECTION        6
#define TRAPLIMIT_SECTION   7
#define LINE_SECTION        8
#define LINELIMIT_SECTION   9

// and three pseudo sections for our file write that correspond to the
// three parts of the relocation table (code, switch table, and trap table)
#define CODEREL_SECTION    10
#define SWTABREL_SECTION   11
#define	TRAPREL_SECTION    12
#define LINEREL_SECTION    13

// External coff sections, as they actually appear in the file.  We need
// this because we tidily remove empty sections, so the later ones get
// shifted up, so to speak...
int directsection;
int codesection;
int constsection;
int datasection;
int swtabsection;
int trapsection;
int traplimitsection;
int linesection;
int linelimitsection;

struct coffscnhdr   directhead;
struct coffscnhdr   codehead;
struct coffscnhdr   consthead;
struct coffscnhdr   datahead;
struct coffscnhdr   swtabhead;
struct coffscnhdr   traphead;
struct coffscnhdr   traplimithead;
struct coffscnhdr   linehead;
struct coffscnhdr   linelimithead;

// file offset of the relocation records for the code segment
int codereloffset;
// file offset of the relocation records for the switch table
int swtabreloffset;
// file offset of the relocation records for the trap table
int trapreloffset;
// file offset for the code source line number table
int lineoffset;
// file offset of the relocation records for the line number table
int linereloffset;
// file offset for the symbol table
int symtaboffset;
// file offset for the string table
int strtaboffset;
int nsections;

// directives that we'd like to pass on to the linker...
static char directive[] = "-defaultlib:LIBI77 ";
// we skip the trailing zero
#define SZDIRECTIVE		(sizeof(directive)-1)

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

void initobjectfile(FILE * output)
{
    int dataoffset, i, filesyms;

    if (mainprogflag != 0)
    {
        // because we will also define the trap table base and limit symbols
        nsymdefs = nsymdefs + 2;
        // because we will also define the line table base and limit symbols
        nsymdefs = nsymdefs + 2;
    }

    // how many sections we'll need (always one for directive)
    nsections = 1;
    if (codecount  != 0) nsections += 1;
    if (constcount != 0) nsections += 1;
    if (datacount  != 0) nsections += 1;
    if (swtabcount != 0) nsections += 1;
    if (trapcount  != 0) nsections += 1;
    if (linecount  != 0) nsections += 1;
    // for the dummy trap table limit section
    if (traplimitflag != 0) nsections += 1;
    // for the dummy line table limit section
    if (linelimitflag != 0) nsections += 1;

    // Calculate the varoius section sizes
    codesize = codecount * BYTESZ;
    datasize = datacount * BYTESZ;
    constsize = constcount * BYTESZ;
    swtabsize = swtabcount * SWTABENTRYSZ;
    bsssize = bsscount * BYTESZ;
    trapsize = trapcount * TRAPENTRYSZ;
    traplimitsize = traplimitflag * TRAPENTRYSZ;
    // LINEHEADERSZ must be a power of 2
    // and match the alignment value for
    // the two LINE and LINELIMIT sections
    // Current LINEHEADERSZ is 256
    // 252 byte for filename string
    // 4 bytes for count of entries
    if (linelimitflag == 0)
    {
        linesize = LINEHEADERSZ*(2 + (linecount * LINEENTRYSZ)/LINEHEADERSZ);
    }
    linelimitsize = LINEHEADERSZ;

    // use a little magic to know in advance how many records the .file symbol takes
    filesyms       = (strlen(path_buffer) + 36)/18;

    intsyms = (nsections*2) + filesyms + 1;
    extsyms = nsymdefs + nspecs;

    // data starts after all the headers
    dataoffset     = SZFILEHDR      + nsections * SZSECHDR;
    codereloffset  = dataoffset     
                    + SZDIRECTIVE
                    + codesize
                    + datasize
                    + constsize
                    + swtabsize
                    + bsssize
                    + trapsize
                    + traplimitsize
                    + linesize
                    + linelimitsize;
    swtabreloffset = codereloffset  + nreloc * SZRELOC;
    trapreloffset  = swtabreloffset + swtabcount * SZRELOC;
    linereloffset  = trapreloffset  + trapcount * TRAPENTRYRELOC * SZRELOC;
    symtaboffset   = linereloffset  + nlines * SZRELOC;
    strtaboffset   = symtaboffset   + (nsymdefs + nspecs + (nsections*2) + filesyms + 1) * SZSYMENT;

    setfile(output, dataoffset);
    setsize(DIRECTIVE_SECTION, SZDIRECTIVE);
    setsize(CODE_SECTION,      codesize);
    setsize(CONST_SECTION,     constsize);
    setsize(DATA_SECTION,      datasize);
    setsize(SWTAB_SECTION,     swtabsize);
    setsize(TRAP_SECTION,      trapsize);
    setsize(TRAPLIMIT_SECTION, traplimitsize);
    setsize(LINE_SECTION,      linesize);
    setsize(LINELIMIT_SECTION, linelimitsize);

    setsize(CODEREL_SECTION,   nreloc * SZRELOC);
    setsize(SWTABREL_SECTION,  swtabcount * SZRELOC);
    setsize(TRAPREL_SECTION,   trapcount * TRAPENTRYRELOC * SZRELOC);
    setsize(LINEREL_SECTION,   nlines * SZRELOC);

    // now we manufacture each of the section headers
    strcpy(directhead.s_name, ".drectve");
    directhead.s_paddr   = 0;
    directhead.s_vaddr	 = 0;
    directhead.s_size    = SZDIRECTIVE;
    directhead.s_scnptr  = dataoffset;
    directhead.s_relptr  = 0;
    directhead.s_lnnoptr = 0;
    directhead.s_nreloc  = 0;
    directhead.s_nlnno   = 0;
    // as used by MS C
    directhead.s_flags   = 0x00100a00;
    dataoffset += SZDIRECTIVE;

    strcpy(codehead.s_name, ".text");
    codehead.s_paddr    = 0;
    codehead.s_vaddr    = 0;
    codehead.s_size	    = codesize;
    codehead.s_scnptr   = dataoffset;
    if (nreloc == 0)
        codehead.s_relptr = 0;
    else
        codehead.s_relptr = codereloffset;
//    codehead.s_lnnoptr  = lineoffset;
    codehead.s_lnnoptr  = 0;
    codehead.s_nreloc   = nreloc;
//    codehead.s_nlnno    = nlines;
    codehead.s_nlnno    = 0;
    // readable executable 16 byte aligned code
    codehead.s_flags    = 0x60500020;
    dataoffset += codesize;

    strcpy(consthead.s_name, ".rdata");
    consthead.s_paddr   = 0;
    consthead.s_vaddr   = 0;
    consthead.s_size    = constsize;
    consthead.s_scnptr  = dataoffset;
    consthead.s_relptr  = 0;
    consthead.s_lnnoptr = 0;
    consthead.s_nreloc  = 0;
    consthead.s_nlnno   = 0;
    // read only 4 byte aligned initialised data
    consthead.s_flags   = 0x40300040;
    dataoffset += constsize;

    strcpy(datahead.s_name, ".data");
    datahead.s_paddr    = 0;
    datahead.s_vaddr    = 0;
    datahead.s_size	    = datasize;
    datahead.s_scnptr   = dataoffset;
    datahead.s_relptr   = 0;
    datahead.s_lnnoptr  = 0;
    datahead.s_nreloc   = 0;
    datahead.s_nlnno    = 0;
    // read/writable 4 byte aligned initialised data
    datahead.s_flags    = 0xC0300040;
    dataoffset += datasize;

    strcpy(swtabhead.s_name, "_SWTAB");
    swtabhead.s_paddr   = 0;
    swtabhead.s_vaddr   = 0;
    swtabhead.s_size    = swtabsize;
    swtabhead.s_scnptr  = dataoffset;
    swtabhead.s_relptr  = swtabreloffset;
    swtabhead.s_lnnoptr = 0;
    // every 32 bit entry is relocated
    swtabhead.s_nreloc  = swtabcount;
    swtabhead.s_nlnno   = 0;
    // read only 4 byte aligned initiliased data
    swtabhead.s_flags   = 0x40300040;
    dataoffset += swtabsize;

    // In order that we can traverse the trap table at run time we want
    // to ensure that our main trap table base is loaded first in the executable image.
    // The COFF linker groups same-name sections in alphabetical order using the token
    // after a $ symbol (the $ and token are then discarded by the linker).
    // Why letters B, D, F and not A,B,C?  In case we ever want to insert some other
    // sections in the sequence...
    if (mainprogflag != 0)
        strcpy(traphead.s_name, "_ITRAP$B");
    else
        strcpy(traphead.s_name, "_ITRAP$D");
    traphead.s_paddr    = 0;
    traphead.s_vaddr    = 0;
    traphead.s_size     = trapsize;
    traphead.s_scnptr   = dataoffset;
    traphead.s_relptr   = trapreloffset;
    traphead.s_lnnoptr  = 0;
    // every 32 byte entry has 4 addresses to be relocated
    traphead.s_nreloc   = trapcount * TRAPENTRYRELOC;
    traphead.s_nlnno    = 0;
    // read only 32 byte aligned initialised data
    traphead.s_flags    = 0x40600040;
    dataoffset += trapsize;

    // the limit section will be linked after all other trap tables
    strcpy(traplimithead.s_name, "_ITRAP$F");
    traplimithead.s_paddr   = 0;
    traplimithead.s_vaddr   = 0;
    traplimithead.s_size    = traplimitsize;
    traplimithead.s_scnptr  = dataoffset;
    // no relocations - it's just a placeholder
    traplimithead.s_relptr  = 0;
    traplimithead.s_lnnoptr = 0;
    traplimithead.s_nreloc  = 0;
    traplimithead.s_nlnno   = 0;
    // read only 32 byte aligned initialised data
    traplimithead.s_flags   = 0x40600040;
    dataoffset += traplimitsize;

    // In order that we can traverse the line table at run time we want
    // to ensure that our main line table base is loaded first in the executable image.
    // The COFF linker groups same-name sections in alphabetical order using the token
    // after a $ symbol (the $ and token are then discarded by the linker).
    // Why letters B, D, F and not A,B,C?  In case we ever want to insert some other
    // sections in the sequence...
    if (mainprogflag != 0)
    {
        strcpy(linehead.s_name, "_ILINE$B");
        linehead.s_paddr    = 0;
        linehead.s_vaddr    = 0;
        linehead.s_size     = linesize;
        linehead.s_scnptr   = dataoffset;
        linehead.s_relptr   = linereloffset;
        linehead.s_lnnoptr  = 0;
        // every line entry has 1 address to be relocated
        linehead.s_nreloc   = linecount * LINEENTRYRELOC;
        linehead.s_nlnno    = 0;
        // read only 256 byte aligned initialised data
        linehead.s_flags    = 0x40900040;
        dataoffset += linesize;
    }
    else if (linelimitflag == 0)
    {
        strcpy(linehead.s_name, "_ILINE$D");
        linehead.s_paddr    = 0;
        linehead.s_vaddr    = 0;
        linehead.s_size     = linesize;
        linehead.s_scnptr   = dataoffset;
        linehead.s_relptr   = linereloffset;
        linehead.s_lnnoptr  = 0;
        // every line entry has 1 address to be relocated
        linehead.s_nreloc   = linecount * LINEENTRYRELOC;
        linehead.s_nlnno    = 0;
        // read only 256 byte aligned initialised data
        linehead.s_flags    = 0x40900040;
        dataoffset += linesize;
    }
    else
    {
        // the limit section will be linked after all other line tables
        strcpy(linelimithead.s_name, "_ILINE$F");
        linelimithead.s_paddr   = 0;
        linelimithead.s_vaddr   = 0;
        linelimithead.s_size    = linelimitsize;
        linelimithead.s_scnptr  = dataoffset;
        // no relocations - it's just a placeholder
        linelimithead.s_relptr  = 0;
        linelimithead.s_lnnoptr = 0;
        linelimithead.s_nreloc  = 0;
        linelimithead.s_nlnno   = 0;
        // read only 256 byte aligned initialised data
        linelimithead.s_flags   = 0x40900040;
        dataoffset += linelimitsize;
    }

    // Now assemble the main file header
    filehead.f_magic    = 0x014C;
    filehead.f_nscns    = nsections;
    filehead.f_timdat   = (time(NULL) & 0xffffffff);
    filehead.f_symptr   = symtaboffset;
    filehead.f_nsyms    = nsymdefs + nspecs + (nsections*2) + filesyms + 1;
    filehead.f_opthdr   = 0;
    filehead.f_flags    = 0;

    // write the file header first
    fwrite(&filehead, 1, SZFILEHDR, output);

    // we always have a directive section
    fwrite(&directhead, 1, SZSECHDR, output);
    directsection = 1;

    // to count off the sections - directive is #1, so next will be #2
    i = 2;

    if (codecount  != 0)
    {
        fwrite(&codehead, 1, SZSECHDR, output);
        codesection = i++;
    }
    else
        codesection = 0;

    if (constcount != 0)
    {
        fwrite(&consthead, 1, SZSECHDR, output);
        constsection = i++;
    }
    else
        constsection = 0;

    if (datacount  != 0)
    {
        fwrite(&datahead, 1, SZSECHDR, output);
        datasection = i++;
    }
    else
        datasection = 0;

    if (swtabcount != 0)
    {
        fwrite(&swtabhead, 1, SZSECHDR, output);
        swtabsection = i++;
    }
    else
        swtabsection = 0;

    if (trapcount  != 0)
    {
        fwrite(&traphead, 1, SZSECHDR, output);
        trapsection = i++;
    }
    else
        trapsection = 0;

    if (traplimitflag != 0)
    {
        fwrite(&traplimithead, 1, SZSECHDR, output);
        traplimitsection = i++;
    }
    else
        traplimitsection = 0;

    if (linecount  != 0)
    {
        fwrite(&linehead, 1, SZSECHDR, output);
        linesection = i++;
    }
    else
        linesection = 0;

    if (linelimitflag != 0)
    {
        fwrite(&linelimithead, 1, SZSECHDR, output);
        linelimitsection = i++;
    }
    else
        linelimitsection = 0;

    // since it's not really part of anything else useful, we output
    // the linker directive now...
    for (i=0; i < SZDIRECTIVE; i++)
        writebyte(DIRECTIVE_SECTION, directive[i]);
}

//
// when we map the sections into symbols we need a transformation, because
// (a) the section table is 1 based but the symbol table is zero based, and
// (b) the aux records we use mean that the symbol table has two entries per section
// (c) we miss out any null sections, so they don't get symbols
// (d) the pseudo-symbol "filename" is output first, so the section symbols are
//     offset by however many records it takes to fit the path name
int directivesymbol;
int codesymbol;
int constsymbol;
int datasymbol;
int swtabsymbol;
int trapsymbol;
int traplimitsymbol;
int linesymbol;
int linelimitsymbol;
// the first user symbol table entry (offset by the above junk)
int firstusersymbol;

static char auxzeroes[18] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

// Output the dummy symbols in the symbol table for the filename and for each section
static void putsectionsymbols(FILE *output)
{
    struct coffsyment sym;
    struct coffauxscn aux;
    int sequence, count, length, symbol;
    char * filename;

    fseek(output, symtaboffset, 0);

    // the first pseudo-symbol is the source filename, which we've stored in path_buffer
    filename = path_buffer;
    // plus one 'cos we need to zero terminate it (I think)
    length = strlen(filename) + 1;
    // this is how many 18 byte aux records that will take
    count = (length + 17)/18;

    // to make sure we get a clean name
    sym.n.n_n.n_offset	= 0;
    strcpy(sym.n.n_name, ".file");
    // special debug entry
    sym.n_scnum     = -2;
    // no value
    sym.n_value     = 0;
    // no type
    sym.n_type      = 0;
    // a filename
    sym.n_sclass    = 103;
    // the number of auxilliary records we're using
    sym.n_numaux    = count;
    fwrite(&sym, 1, SZSYMENT, output);
    symtaboffset += SZSYMENT;

    // now write the filename, padding the last 18 bytes with zeroes
    fwrite(filename, length, 1, output);
    length = count * 18 - length;
    fwrite(auxzeroes, length, 1, output);
    symtaboffset += count * SZSYMENT;

    // The next symbol we write out is the mysterious COMP.ID symbol.
    // Why?  I have no idea, but Microsoft tools like to find this....
    strcpy(sym.n.n_name, "@comp.id");
    // special absolute entry
    sym.n_scnum     = -1;
    // mystery value
    sym.n_value     = 0x151FE8;
    // no type
    sym.n_type      = 0;
    // static
    sym.n_sclass    = 3;
    // no aux record
    sym.n_numaux    = 0;
    fwrite(&sym, 1, SZSYMENT, output);
    symtaboffset += SZSYMENT;

    // OK - the next symbol we will output is going to be...
    symbol = count + 2;

    // set up all the common elements of a section symbol
    // zero (section base)
    sym.n_value     = 0;
    // NOT a function
    sym.n_type      = 0;
    // static
    sym.n_sclass    = 3;
    // each has one auxilliary
    sym.n_numaux    = 1;

    // we don't checksum the sections
    aux.x_chsum = 0;
    aux.x_secno = 0;
    aux.x_selno = 0;
    aux.x_pad   = 0;

    sequence = 1;

    // directive first
    // to make sure we get a clean name
    sym.n.n_n.n_offset  = 0;
    strcpy(sym.n.n_name, ".drectve");
    sym.n_scnum = sequence;
    fwrite(&sym, 1, SZSYMENT, output);
    symtaboffset += SZSYMENT;

    aux.x_scnlen = SZDIRECTIVE;
    aux.x_nreloc = 0;
    aux.x_nlnno = 0;
    fwrite(&aux, 1, SZSYMENT, output);
    symtaboffset += SZSYMENT;

    directivesymbol = symbol;
    symbol += 2;

    // code
    if (codecount != 0)
    {
        sequence += 1;
        // to make sure we get a clean name
        sym.n.n_n.n_offset = 0;
        strcpy(sym.n.n_name, ".text");
        sym.n_scnum = sequence;
        fwrite(&sym, 1, SZSYMENT, output);
        symtaboffset += SZSYMENT;

        aux.x_scnlen = codecount*BYTESZ;
        aux.x_nreloc = nreloc;
        aux.x_nlnno = 0;
        fwrite(&aux, 1, SZSYMENT, output);
        symtaboffset += SZSYMENT;

        codesymbol = symbol;
        symbol += 2;
    }

    // const
    if (constcount != 0)
    {
        sequence += 1;
        // to make sure we get a clean name
        sym.n.n_n.n_offset = 0;
        strcpy(sym.n.n_name, ".rdata");
        sym.n_scnum = sequence;
        fwrite(&sym, 1, SZSYMENT, output);
        symtaboffset += SZSYMENT;

        aux.x_scnlen = constcount*BYTESZ;
        aux.x_nreloc = 0;
        aux.x_nlnno = 0;
        fwrite(&aux, 1, SZSYMENT, output);
        symtaboffset += SZSYMENT;

        constsymbol = symbol;
        symbol += 2;
    }

    // data
    if (datacount != 0)
	{
        sequence += 1;
        // to make sure we get a clean name
        sym.n.n_n.n_offset = 0;
        strcpy(sym.n.n_name, ".data");
        sym.n_scnum = sequence;
        fwrite(&sym, 1, SZSYMENT, output);
        symtaboffset += SZSYMENT;

        aux.x_scnlen = datacount*BYTESZ;
        aux.x_nreloc = 0;
        aux.x_nlnno = 0;
        fwrite(&aux, 1, SZSYMENT, output);
        symtaboffset += SZSYMENT;

        datasymbol = symbol;
        symbol += 2;
    }

    // switch
    if (swtabcount != 0)
    {
        sequence += 1;
        // to make sure we get a clean name
        sym.n.n_n.n_offset = 0;
        strcpy(sym.n.n_name, "_SWTAB");
        sym.n_scnum = sequence;
        fwrite(&sym, 1, SZSYMENT, output);
        symtaboffset += SZSYMENT;

        aux.x_scnlen = swtabsize;
        aux.x_nreloc = swtabcount;
        aux.x_nlnno = 0;
        fwrite(&aux, 1, SZSYMENT, output);
        symtaboffset += SZSYMENT;

        swtabsymbol = symbol;
        symbol += 2;
    }

    // trap
    if (trapcount  != 0)
    {
        sequence += 1;
        // to make sure we get a clean name
        sym.n.n_n.n_offset = 0;
        if (mainprogflag != 0)
            strcpy(sym.n.n_name, "_ITRAP$B");
        else
            strcpy(sym.n.n_name, "_ITRAP$D");
        sym.n_scnum = sequence;
        fwrite(&sym, 1, SZSYMENT, output);
        symtaboffset += SZSYMENT;

        aux.x_scnlen = trapsize;
        aux.x_nreloc = trapcount * TRAPENTRYRELOC;
        aux.x_nlnno = 0;
        fwrite(&aux, 1, SZSYMENT, output);
        symtaboffset += SZSYMENT;

        trapsymbol = symbol;
        symbol += 2;
    }

    // trap limit
    if (traplimitflag  != 0)
    {
        sequence += 1;
        // to make sure we get a clean name
        sym.n.n_n.n_offset = 0;
        strcpy(sym.n.n_name, "_ITRAP$F");
        sym.n_scnum = sequence;
        fwrite(&sym, 1, SZSYMENT, output);
        symtaboffset += SZSYMENT;

        aux.x_scnlen = 16;
        aux.x_nreloc = 0;
        aux.x_nlnno = 0;
        fwrite(&aux, 1, SZSYMENT, output);
        symtaboffset += SZSYMENT;

        traplimitsymbol = symbol;
        symbol += 2;
    }

    // line
    if (linecount  != 0)
    {
        sequence += 1;
        // to make sure we get a clean name
        sym.n.n_n.n_offset = 0;
        if (mainprogflag != 0)
            strcpy(sym.n.n_name, "_ILINE$B");
        else
            strcpy(sym.n.n_name, "_ILINE$D");
        sym.n_scnum = sequence;
        fwrite(&sym, 1, SZSYMENT, output);
        symtaboffset += SZSYMENT;

        aux.x_scnlen = linesize;
        aux.x_nreloc = linecount * LINEENTRYRELOC;
        aux.x_nlnno = 0;
        fwrite(&aux, 1, SZSYMENT, output);
        symtaboffset += SZSYMENT;

        linesymbol = symbol;
        symbol += 2;
    }

    // line limit
    if (linelimitflag  != 0)
    {
        sequence += 1;
        // to make sure we get a clean name
        sym.n.n_n.n_offset = 0;
        strcpy(sym.n.n_name, "_ILINE$F");
        sym.n_scnum = sequence;
        fwrite(&sym, 1, SZSYMENT, output);
        symtaboffset += SZSYMENT;

        aux.x_scnlen = linelimitsize;
        aux.x_nreloc = 0;
        aux.x_nlnno = 0;
        fwrite(&aux, 1, SZSYMENT, output);
        symtaboffset += SZSYMENT;

        linelimitsymbol = symbol;
        symbol += 2;
    }

    // this is where the program symbol table will start
    firstusersymbol = symbol;
    // See if we need to add in the trap symbols
    // plus an extra one (if needed) for the trap table trapbase symbol
    if (mainprogflag != 0)
        firstusersymbol += 1;
    // plus an extra one (if needed) for the trap table traplimit symbol
    if (traplimitflag != 0)
        firstusersymbol += 1;

    // See if we need to add in the line symbols
    // plus an extra one (if needed) for the line table linebase symbol
    if (mainprogflag != 0)
        firstusersymbol += 1;
    // plus an extra one (if needed) for the line table linelimit symbol
    if (linelimitflag != 0)
        firstusersymbol += 1;
}

// write the external spec table to the symbol table area
static void putexternalspecs(FILE *output)
{
    int i, type, ptr, index;
    struct coffsyment sym;

    fseek(output, symtaboffset, 0);

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
                sym.n.n_n.n_zeroes = 0;
                sym.n.n_n.n_offset = 0;
                if (strlen(&named[ptr]) <= 8)
                    strcpy(sym.n.n_name, &named[ptr]);
                else
                    sym.n.n_n.n_offset = ptr + 4;
                // zero (undefined)
                sym.n_value = 0;
                // section zero (external)
                sym.n_scnum = 0;

                // check if this is a reference to a data or function symbol
                if ((specs[index].flags & SYMISDATA) != 0)
                {
                    // tag as global data symbol
                    sym.n_type = 0;
                }
                else
                {
                    // tag as global code/function symbol
                    sym.n_type = 0x20;
                }
                // external
                sym.n_sclass = 2;
                // no auxilliaries
                sym.n_numaux = 0;

                fwrite(&sym, 1, SZSYMENT, output);
                symtaboffset += SZSYMENT;
            }
            index += 1;
        }
    }
}

// write the external definitions to the symbol table
static void putexternaldefs(FILE *output)
{
    int i, type, ptr;
    struct coffsyment sym;
    char * name;

    for (i = 0; i < nm; i++)
    {
        type = m[i].what;
        ptr = m[i].info;
        if (type == IF_DEFEXTCODE)
        {
            name = &named[ptr];
            sym.n.n_n.n_zeroes = 0;
            sym.n.n_n.n_offset = 0;
            if (strlen(name) <= 8)
                strcpy(sym.n.n_name, name);
            else
                sym.n.n_n.n_offset = ptr + 4;
            // address of this item
            sym.n_value = m[i].address;
            // section - code
            sym.n_scnum = codesection;
            // this is a function
            sym.n_type = 0x20;
            // external
            sym.n_sclass = 2;
            // no auxilliaries
            sym.n_numaux = 0;
            fwrite(&sym, 1, SZSYMENT, output);
            symtaboffset += SZSYMENT;
        }
        if (type == IF_DEFEXTDATA)
        {
            name = &named[ptr];
            sym.n.n_n.n_zeroes = 0;
            sym.n.n_n.n_offset = 0;
            if (strlen(name) <= 8)
                strcpy(sym.n.n_name, name);
            else
                sym.n.n_n.n_offset = ptr + 4;
            // address of this item
            sym.n_value = m[i].address;
            // section - data
            sym.n_scnum = datasection;
            // this is NOT a function
            sym.n_type = 0;
            // external
            sym.n_sclass = 2;
            // no auxilliaries
            sym.n_numaux = 0;
            fwrite(&sym, 1, SZSYMENT, output);
            symtaboffset += SZSYMENT;
        }
    }
}

// Write the string table to the output file.  Note that we
// write all of our internal string table, even though not
// all of the entries are used/needed by the linker, or even
// referenced within this object file
static void putstringtable(FILE *output)
{
    int count;

    fseek(output, strtaboffset, 0);

    // the string table must include a size word at the start
    count = namedp + 4;
    // so, write the table size
    fwrite(&count, 4, 1, output);
    // then write the string table
    fwrite(named, 1, namedp, output);
}

// plant the array of blocks used by %signal to trap
// events in the trap section.  These blocks contain:
// <StartAddr32><EndAddr32><TrapAddr32><FromAddr32><EventMask16><Name[14]>
static void puttraptable(FILE *output)
{
    int i, j, addr;
    struct coffsyment sym;
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
        // Use as relocation base, the .text symbol
        segidx = codesymbol;
        for (j=0; j < 4; j++)
        {
            address[j] = offset[j];
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
            writew32(TRAPREL_SECTION, segidx);
            // relocate by actual 32 bit address
            writew16(TRAPREL_SECTION, 6);
        }
    }

    // Add the _imptrapbase symbol?
    // define a symbol that marks the end of the trap table
    if (mainprogflag != 0)
    {
        i = newname(TRAPBASE);
        sym.n.n_n.n_zeroes = 0;
        sym.n.n_n.n_offset = i + 4;
        // note first address of this section
        sym.n_value = 0;
        // note which section ID
        sym.n_scnum = trapsection;
        // this is NOT a function
        sym.n_type = 0;
        // and it's an external definition
        sym.n_sclass = 2;
        // with no auxilliaries
        sym.n_numaux = 0;
        fseek(output, symtaboffset, 0);
        fwrite(&sym, 1, SZSYMENT, output);
        symtaboffset += SZSYMENT;
    }

    // Add the _imptraplimit symbol?
    // define a symbol that marks the end of the trap table
    if (traplimitflag != 0)
    {
        // and similarly we put 16 bytes of zeroes into the traplimit section
        for (i=0; i < TRAPENTRYSZ; i++)
            writebyte(TRAPLIMIT_SECTION, 0);

        i = newname(TRAPLIMIT);
        sym.n.n_n.n_zeroes = 0;
        sym.n.n_n.n_offset = i + 4;
        // note first address of this section
        sym.n_value = 0;
        // note which section ID
        sym.n_scnum = traplimitsection;
        // this is NOT a function
        sym.n_type = 0;
        // and it's an external definition
        sym.n_sclass = 2;
        // with no auxilliaries
        sym.n_numaux = 0;
        fseek(output, symtaboffset, 0);
        fwrite(&sym, 1, SZSYMENT, output);
        fwrite(&sym, 1, SZSYMENT, output);
        symtaboffset += SZSYMENT;
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
    int i,remainder;
    struct coffsyment sym;
    int segidx;

    // A line$X section has a record structure
    // Field 1) module name (counted string in IMP string format)
    // Field 2) line count (32-bit count of line entries)
    // Field 3..line count + 3) line address, line number (array of pair of 32-integers)

    // Add the linelimit data + symbol?
    if (linelimitflag == 0)
    {
        // This is an ordinary LINE section
        putlineheader( LINE_SECTION, nlines );

        // Now output the lineentry data for each line
        for (i = 0;i < nlines; i++)
        {
            // Add the lineentry data for each line
            writew32(LINE_SECTION, lines[i].line);
            writew32(LINE_SECTION, lines[i].offset);
        }
        // However LINE_SECTION is a multiple of LINEHEADERSZ
        // so we need to padd out the section with zeros
        // nlines*8 bytes already entered
        // so linelimitsize - LINEHEADERSZ - nlines*8 bytes remaining
        remainder = linelimitsize - LINEHEADERSZ - nlines*8;
        for (i=0;i < remainder;i++)
        {
            writebyte(LINE_SECTION, 0);
        }

        // Add the relocation data for eaxh line
        // Use as relocation base, the .text symbol
        segidx = codesymbol;
        for (i = 0;i < nlines; i++)
        {
            // Now add the relocation data for the offset code address we've just planted
            // offset in this section of the word to relocate
            // The offset is the second value of each 4-byte integer pair
            writew32(LINEREL_SECTION, LINEHEADERSZ + 4 * (2*i + 1));
            // use the symbol index for the chosen relocation base
            // that is relocate by the .text base symbol
            writew32(LINEREL_SECTION, segidx);
            // relocate by actual 32 bit address
            writew16(LINEREL_SECTION, 6);
        }

        // Add the _implinebase symbol?
        // define a symbol that marks the end of the trap table
        if (mainprogflag != 0)
        {
            i = newname(LINESTART);
            sym.n.n_n.n_zeroes = 0;
            sym.n.n_n.n_offset = i + 4;
            // note first address of this section
            sym.n_value = 0;
            // note which section ID
            sym.n_scnum = linesection;
            // this is NOT a function
            sym.n_type = 0;
            // and it's an external definition
            sym.n_sclass = 2;
            // with no auxilliaries
            sym.n_numaux = 0;
            fseek(output, symtaboffset, 0);
            fwrite(&sym, 1, SZSYMENT, output);
            symtaboffset += SZSYMENT;
        }
    }
    else
    {
        // This is the unique LINELIMIT section
        putlineheader( LINELIMIT_SECTION, 0 );

        // Add the _implinelimit symbol?
        // define a symbol that marks the end of the line table
        i = newname(LINELIMIT);
        sym.n.n_n.n_zeroes = 0;
        sym.n.n_n.n_offset = i + 4;
        // note first address of this section
        sym.n_value = 0;
        // note which section ID
        sym.n_scnum = linelimitsection;
        // this is NOT a function
        sym.n_type = 0;
        // and it's an external definition
        sym.n_sclass = 2;
        // with no auxilliaries
        sym.n_numaux = 0;
        fseek(output, symtaboffset, 0);
        fwrite(&sym, 1, SZSYMENT, output);
        fwrite(&sym, 1, SZSYMENT, output);
        symtaboffset += SZSYMENT;
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

            segidx = datasymbol;

            // offset in the section of the word to relocate
            writew32(CODEREL_SECTION, cad);
            // symbol for section
            writew32(CODEREL_SECTION, segidx);
            // relocate by actual 32 bit address
            writew16(CODEREL_SECTION, 6);
            cad += WORDSIZE;
            break;

        case IF_CONST:
            // const section offset word
            if (m[current].what != IF_OBJ)
                current += 1;
            for (i=0; i < WORDSIZE; i++)
                writebyte(CODE_SECTION, buffer[i]);

            segidx = constsymbol;

            // offset in the section of the word to relocate
            writew32(CODEREL_SECTION, cad);
            // symbol for section
            writew32(CODEREL_SECTION, segidx);
            // relocate by actual 32 bit address
            writew16(CODEREL_SECTION, 6);
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
            // is this a short jump?
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
            // is this a short jump?
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
            for (i=0; i < WORDSIZE; i++)
                writebyte(CODE_SECTION,0);
//            writew32(CODE_SECTION,0);
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
            writew32(CODEREL_SECTION, id);
            // relocate by relative 32 bit address
            writew16(CODEREL_SECTION, 0x14);

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
            writew32(SWTABREL_SECTION, codesymbol);
            // relocate by actual 32 bit address
            writew16(SWTABREL_SECTION, 6);
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

            segidx = swtabsymbol;

            // note the offset in the section of the word to relocate
            writew32(CODEREL_SECTION, cad);
            // note the symbol for the section
            writew32(CODEREL_SECTION, segidx);
            // relocate by actual 32 bit address
            writew16(CODEREL_SECTION, 6);
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
            offset = (buffer[3]<<8) | buffer[2] ;

            writew32(CODE_SECTION, offset );

            // id == reference index is in buffer[2,3],
            // which we should already have to remap according to our table
            id = specs[id].p3index;
            // skip the symbol table entries for the sections
            id += firstusersymbol;

            // put the offset in the section of the word to relocate
            writew32(CODEREL_SECTION, cad);
            // put the symbol index for this reference
            writew32(CODEREL_SECTION, id);
            // and relocate by actual 32 bit address
            writew16(CODEREL_SECTION, 6);

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

void dumpobjectfile( char *inname, char *outname )
{
    FILE * in;
    FILE * out;
    int i;

    // in order to get a useful debug output, we try to recreate the input
    // file name by assuming that the intermediate files have the same base
    // name and are in the same directory as the source.
#ifdef MSVC
    // turn it into a full name
    _fullpath(path_buffer, inname, _MAX_PATH);
#else
    //    realpath(inname, path_buffer);
    strcpy(path_buffer,realpath(inname, NULL));
#endif
    // At this point we have the full filename of the input file
    // held in the path_buffer char array.

    // Now tweak the file extension from .ibj to .imp
    // Only need to alter the last two chars
    // NB char array index starts at 0
    i = strlen(path_buffer);
    path_buffer[i - 2] = 'm';
    path_buffer[i - 1] = 'p';
    // Now put it in the string table
    path_index = newname(path_buffer);

    // So, first open the input file
    in = fopen(inname, "r");

    // Next tweak the inname to form the output (,o) filename
    i = strlen(inname);

    // First: tweak the file extension from .ibj to .obj
    // Only need to alter the last two chars
    // NB char array index starts at 0
    inname[i - 3] = 'o';
    inname[i - 2] = 'b';
    inname[i - 1] = 'j';

    // Now open the output file
    out = fopen(inname, "wb");
    if (out == NULL)
    {
        perror("Can't open output file");
        fprintf(stderr, "Can't open output file '%s'\n",inname);
        exit(1);
    }

    initobjectfile(out);
    putsectionsymbols(out);

    // reset the line number information
    nlines = 0;
    lastlinead = -1;

    putcode(in, out);

    // now plant the trap table
    puttraptable(out);
    // now output the line number records for the debugger
    putlinenumbers(out);
    // Now the externals
    putexternalspecs(out);
    putexternaldefs(out);
    putstringtable(out);

    flushout();

    fclose(in);
    fclose(out);
}

int main(int argc, char **argv)
{
    int size;

    if ((argc != 2) && (argc != 3))
    {
        fprintf(stderr, "Unexpected number of parameters for PASS3COFF!\n\n");
        fprintf(stderr, "Usage:  PASS3 <intermediatefile> <objfile>?\n");
        exit(1);
    }

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
    fprintf(stderr, " COFF object file generated from IMP source file: '%s'\n",path_buffer);

    size = datasize
         + constsize
         + swtabsize
         + bsssize;
    fprintf(stderr, " +----------+---------------------+---------+---------+---------+------------+\n");
    fprintf(stderr, " | Sections |       Symbols       | Code    | Data    | Diag    | Total size |\n");
    fprintf(stderr, " +----------+----------+----------+---------+---------+---------+------------+\n");
    fprintf(stderr, " |  (count) | Internal | External | (bytes) | (bytes) | (bytes) | (bytes)    |\n");
    fprintf(stderr, " +----------+----------+----------+---------+---------+---------+------------+\n");
    fprintf(stderr, " | %8d | %8d | %8d | %7d | %7d | %7d | %10d |\n",
                    nsections,
                    intsyms,
                    extsyms,
                    codesize,
                    size,
                    trapsize,
                    size + codesize + trapsize);
    fprintf(stderr, " +----------+----------+----------+---------+---------+---------+------------+\n");
    fprintf(stderr, "\n\n");

    exit(0);
}
