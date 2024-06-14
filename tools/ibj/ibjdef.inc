!  %endoflist
{ **************************************** }
{ *                                      * }
{ * Copyright (c) 2020 J.D.McMullin PhD. * }
{   All rights reserved.                 * }
{ *                                      * }
{ **************************************** }
  ! Intermediate file types:
%constbyte IF OBJ        =  0; ! 'A' plain object code
%constbyte IF DATA       =  1; ! 'B' dataseg offset code word
%constbyte IF CONST      =  2; ! 'C' const seg offset code word
%constbyte IF DISPLAY    =  3; ! 'D' display seg offset code word
%constbyte IF JUMP       =  4; ! 'E' unconditional jump to label
%constbyte IF JCOND      =  5; ! 'F' jump to label if JE,JNE,JG,JGE,JL,JLE
%constbyte IF CALL       =  6; ! 'G' call a label
%constbyte IF LABEL      =  7; ! 'H' define a label
%constbyte IF FIXUP      =  8; ! 'I' define location for stack fixup instruction
%constbyte IF SETFIX     =  9; ! 'J' stack fixup <location> <amount>
%constbyte IF REQEXT     = 10; ! 'K' external name spec
%constbyte IF REFLABEL   = 11; ! 'L' reference a label address
%constbyte IF REFEXT     = 12; ! 'M' external name relative offset code word
%constbyte IF BSS        = 13; ! 'N' BSS segment offset code word
%constbyte IF COTWORD    = 14; ! 'O' Constant table word
%constbyte IF DATWORD    = 15; ! 'P' Data segment word
%constbyte IF SWTWORD    = 16; ! 'Q' switch table entry - actually a label ID
%constbyte IF SOURCE     = 17; ! 'R' name of the source file
%constbyte IF DEFEXTCODE = 18; ! 'S' define a code label that is external
%constbyte IF DEFEXTDATA = 19; ! 'T' define a data label that is external
%constbyte IF SWT        = 20; ! 'U' switch table offset code word
%constbyte IF LINE       = 21; ! 'V' line number info for debugger
%constbyte IF ABSEXT     = 22; ! 'W' external name absolute offset code word

  %conststring(16)%array ibj name(IF OBJ:IF ABSEXT) = "IF_OBJ",
                                                      "IF_DATA",
                                                      "IF_CONST",
                                                      "IF_DISPLAY",
                                                      "IF_JUMP",
                                                      "IF_JCOND",
                                                      "IF_CALL",
                                                      "IF_LABEL",
                                                      "IF_FIXUP",
                                                      "IF_SETFIX",
                                                      "IF_REQEXT",
                                                      "IF_REFLABEL",
                                                      "IF_REFEXT",
                                                      "IF_BSS",
                                                      "IF_COTWORD",
                                                      "IF_DATWORD",
                                                      "IF_SWTWORD",
                                                      "IF_SOURCE",
                                                      "IF_DEFEXTCODE",
                                                      "IF_DEFEXTDATA",
                                                      "IF_SWT",
                                                      "IF_LINE",
                                                      "IF_ABSEXT"

  %conststring(16)%array condition name(0:5) = "JE",
                                               "JNE",
                                               "JG",
                                               "JGE",
                                               "JL",
                                               "JLE"

%list
%endoffile
