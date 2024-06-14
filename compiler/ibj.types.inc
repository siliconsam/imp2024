%endoflist

{ Intermediate file types with description )
{             IBJ Type        Id       Description }
%constinteger IF OBJ		=  0 { A - plain object code }
%constinteger IF DATA	    =  1 { B - data section offset code word }
%constinteger IF CONST	    =  2 { C - const section offset code word }
%constinteger IF DISPLAY	=  3 { D - display section offset code word }
%constinteger IF JUMP		=  4 { E - unconditional jump to label }
%constinteger IF JCOND	    =  5 { F - cond jump to label JE, JNE, JLE, JL, JGE, JG }
%constinteger IF CALL		=  6 { G - call a label }
%constinteger IF LABEL	    =  7 { H - define a label }
%constinteger IF FIXUP	    =  8 { I - define location for stack fixup instruction }
%constinteger IF SETFIX	    =  9 { J - stack fixup <location> <amount> }
%constinteger IF REQEXT	    = 10 { K - external name spec }
%constinteger IF REFLABEL   = 11 { L - relative address of label (JDM JDM new IBJ command) }
%constinteger IF REFEXT	    = 12 { M - external name relative offset code word (call external) }
%constinteger IF BSS		= 13 { N - BSS section offset code word }
%constinteger IF COTWORD	= 14 { O - Constant table word }
%constinteger IF DATWORD	= 15 { P - Data section word (repeated) with repeat count }
%constinteger IF SWTWORD	= 16 { Q - switch table entry - actually a label ID }
%constinteger IF SOURCE	    = 17 { R - name of the source file }
%constinteger IF DEFEXTCODE = 18 { S - define a code label that is external }
%constinteger IF DEFEXTDATA = 19 { T - define a data label that is external }
%constinteger IF SWT        = 20 { U - switch table offset code word }
%constinteger IF LINE       = 21 { V - line number info for debugger }
%constinteger IF ABSEXT     = 22 { W - external name absolute offset code word (data external) }

%list
%endoffile
