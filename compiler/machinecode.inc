%endoflist

! Remember number of CPU registers (1..register limit)
! Beware for i386 there are 8@32-bit registers
! BUT the 16-bit and 8-bit registers overlay the 32-bit registers
%constant %integer register limit = 32

! A machine code string has the form *op_ (item(',' item)*)?
! where *,? have the usual regular grammar repetition semantics
! op is an instruction name
!   - (a sequence of alphanumeric chars terminated by '_')
! An item has one of the forms:
! B such that 0 <= B <= 255
! X such that 0 <= X <= 255 returning x = X - 128
! 1) varname  == ' ' BB   (BB is a definition tag)
! 2) constant == 'N' BBBB (BBBB is 32-bit signed integer)
! 3) text     == X+       (X+ converted to an ASCII sequence of x)
! The code string can also include ASCII chars
! (excluding any varname,constant,text)
! 4) chars    == c*
! (where c is one of '[',']','#',',')
!
! An instruction can have 0.."param limit" parameters
! where the parameters are separated by a ','
! One parameter type is a register mask of form
!  mask == '<' number (',' number)* '>'
! This is the ONLY other legal use of a ','
! The following defines the legal opcode parameters
! 1) register == constant
!                (a register index, beware register range)
! 2) number   == # constant
!                (a 32-bit signed integer)
! 3) variable == varname,
!                pointer
! 4) pointer  == '[' register ']',
!                '[' register '+' offset ']',
!                '{ register '-' offset ']'
! 5) offset   == constant
!                (signed integer with 32/16/8 bits)
!
! N.B. a variable could be the value held in varname
! N.B. register always refers to its value
! N.B. pointer is the address of a value
!
! Legal Intel 386 instruction formats
! No-op instruction
! *op_
!
! One-op instruction
! *op_ register
! *op_ number
! *op_ variable
!
! Two-op MOV instruction
! N.B. *op_ A ',' B          == A := B 
! *op_ register ',' register == register := register
! *op_ register ',' number   == register := number
! *op_ register ',' variable == register := variable
! *op_ variable ',' register == variable := register
! *op_ variable ',' number   == variable := number
!
! Two-op instruction (non-MOV instruction)
! *op_ register ',' register == register := register op register
! *op_ register ',' number   == register := register op number
! *op_ register ',' variable == register := register op variable
! *op_ variable ',' register == variable := variable op register
! *op_ variable ',' number   == variable := variable op number

! classify the type of the machine code instruction parameter
!
!                           type        value     data      offset
%constant %integer     unknown  = 0  {  ???       ???       N/A     }
%constant %integer     variable = 1  {  tag       tagName   N/A     }
%constant %integer     register = 2  {  regId     regName   N/A     }
%constant %integer     number   = 3  {  number    N/A       N/A     }
%constant %integer     pointer  = 4  {  regId     regName   number  }
!
! ass-u-me that a machine code instruction has at most 8 parameters
%constant %integer param limit = 8;

%recordformat paramFm(%string(255) data, name,
                      %integer scomma,
                               pcomma,
                               start,
                               end,
                               type,
                               value,
                               offset)
%string(5) instruction
%integer params count
%record(paramFm)%array params(1:param limit)

%routine initialise parameter( %integer index, type )
    params( index )_type = type
    params( index )_name = ""
    params( index )_value = 0
    params( index )_offset = 0
%end

%routine dump parameter( %integer index )
    %string(255) t
    %integer tag,n
    %string(255) name
    %integer value
    %integer offset 
    %switch sw(unknown:pointer)

    name = params( index )_name
    value = params( index )_value
    offset = params( index )_offset

    %if (index = 1) %then newlines(2)
    printstring( "Parameter(".itos(index,0).")='".name."'")

    ->sw(params(index)_type)

    sw(unknown):
        ! dump the unknown data
        printstring(" UNKNOWN")
        printstring(" name="); printstring(name)
        printstring(" value="); printstring(itos(value,0))
        printstring(" offset="); printstring(itos(offset,0))
        -> esac

    sw(variable):
        ! dump the variable data
        dump tag var( value, "VAR" )
        -> esac

    sw(register):
        ! dump the register data
        printstring(" REG")
        printstring(" name="); printstring(name)
        -> esac

    sw(number):
        ! dump the number data
        printstring(" NUMBER")
        printstring(" value="); printstring(itos(value,0))
        -> esac

    sw(pointer):
        ! dump the pointer data
        printstring(" PTR")
        printstring(" name="); printstring("["); printstring(name)
        %if (offset # 0) %start
            %if (offset > 0) %then printsymbol('+')
            printstring(itos(offset,0))
        %finish
        printstring("]")
        -> esac

    esac:
    newline
%end

%integer %function name2reg(%string(255) s)
    %integer r,i
    %string(255) u

    u = s
    to upper(u)
    r = -1
    %if (r = -1) %start
        %for i = EAX,1,EDI %cycle
            %if (u = reg32name(i)) %then r = i
        %repeat
    %finish

    %if (r = -1) %start
        %for i = AX,1,DI %cycle
            %if (u = reg16name(i)) %then r = i
        %repeat
    %finish

    %if (r = -1) %start
        %for i = AL,1,BH %cycle
            %if (u = reg8name(i)) %then r = i
        %repeat
    %finish

    %if (r = -1) %start
        %for i = FR0,1,FR7 %cycle
            %if (u = regfloatname(i)) %then r = i
        %repeat
    %finish

    %result = r
%end

%routine parseMachineCode( %string(255) code )
    %string(255) s,t
    %string(255) parameters
    %byte ch
    %byte insbflag;  ! inside square bracket sequence
    %byte hashflag;  ! expecting a number to follow
    %byte plusFlag;  ! indicates a pointer +ve constant offset
    %byte minusFlag; ! indicates a pointer -ve constant offset
    %switch c(0:127)
    %label esac, default
    %integer i,j,k,n,plen,tag,r

    code -> instruction.("_").parameters

    s = ""

    %if (length(parameters) > 0) %start
        ! parameters is a non-empty string so at least one parameter
        params count = 1
        plen = length(parameters)
        ! clear all the parse state flags
        insbFlag  = 0
        hashFlag  = 0
        plusFlag  = 0
        minusFlag = 0

        i = 1
        %while (i <= plen) %cycle

            t = ""
            ch = charno( parameters, i)
            -> c(ch) %if (ch < 128); ! this is an ordinary ASCII char

            ! So, ch > 127, thus this "char" starts a tweaked "name"
            ! Danger! Beware if this is the last parameter
            ! We must not fall off the end of the parameters string
            %while (charno(parameters, i) > 127) %and (i <= plen) %cycle
                ! Append the converted char
                length(t) = length(t) + 1
                ! tweak appended "char" to be a legal 7-bit ASCII char
                ch = charno(parameters, i) - 128
                ! convert char to uppercase
                %if ('a' <= ch <= 'z') %then ch = ch - 'a' + 'A'

                ! remember the char
                charno( t, length(t)) = ch
                i = i + 1
            %repeat

            r = name2reg( "%".t )
            %if (r # -1) %start
                %if (insbflag = 0) %start
                    params( params count )_type  = register
                %finish
                params( params count )_value = r
            %finish

            t = "%".t
            params( params count )_name  = t
            s = s.t." "

            -> esac

     c(' '):
            ! a variable/pointer reference is prefixed by a space.
            n = (charno( parameters, i+1 )<<8) + charno( parameters, i+2 )

            ! now determine the variable name
            t = get symbol name( n )

            ! remember this parameter is a variable (and its tag)
            params( params count )_type = variable
            params( params count )_value = n
            params( params count )_name = t

            s = s.t

            i = i + 3
            -> esac
     c('N'):
            ! A number is prefixed by an ASCII 'N'
            n = 0
            n = n + charno( parameters, i+1 ); n = n << 8
            n = n + charno( parameters, i+2 ); n = n << 8
            n = n + charno( parameters, i+3 ); n = n << 8
            n = n + charno( parameters, i+4 )

            %if (hashflag = 1) %and (params(params count)_type = unknown) %start
                ! hashflag indicates this is a genuine integer
                hashFlag = 0

                ! remember this parameter is a number
                params( params count )_type  = number
                params( params count )_value = n
                params( params count )_name  = ""

                %if (n > 255) %start
                    t = "16_".int2ascii( n, 16, 0 )
                %finish %else %start
                    t = itos( n, 0 )
                %finish
                            
                s = s.t
                params( params count )_name  = t

            %finish %else %if (insbflag = 1) %start
                ! This represents an integer offset for a pointer

                ! remember this parameter is a pointer
                params( params count )_type = pointer

                %if (plusFlag = 1) %start
                    ! remember this pointer has a +ve offset
                    params( params count )_offset = n
                    t = itos( n, 0)
                %finish %else %if (minusFlag = 1) %start
                    ! remember this pointer has a -ve offset
                    params( params count )_offset = -n
                    ! however, negative sign (and or #) already output
                    t = itos( n, 0)
                %finish

                s = s.t
            %finish
            i = i + 5
            -> esac

     c('#'):
            ! let this char through
            ! BUT remember # is assumed to prefix a positive number
            hashFlag = 1
            -> default

     c(','):
            ! let this char through
            ! comma separates instruction parameters
            ! (or values between brackets)

            { ok, check to see if this is a parameter separator }
            %if (insbFlag = 0) %start
                { ok, we are not inside one of the "bracket" types }
                ! REMEMBER, the parameter type and value should have been
                ! determined previously
                ! note comma location in the s string
                params(params count)_scomma = length(s) + 1

                ! note comma location in the parameters string
                params(params count)_pcomma = i
                ! beware fence post error
                ! we are counting fence posts (,)
                ! and their locations
                ! So "last" fence post at end of parameters string

                ! we have an additional parameter
                params count = params count + 1
                ! BUT set the parameter type appropriately
                params( params count )_type = unknown
                params( params count )_offset = 0

            %finish
            -> default

     c('+'):
            ! pass this char( only allowed between [] brackets
            plusFlag = 1
            minusFlag = 0
            -> default

     c('-'):
            ! pass this char( only allowed between [] brackets
            plusFlag = 0
            minusFlag = 1
            -> default

     c('['):
            ! we are referencing an indirect variable
            ! initialise the name,value and offset
            params( params count )_type = pointer
            params( params count )_name  = ""
            params( params count )_value = 0
            params( params count )_offset = 0

            ! pass this char (opening square brackets)
            insbFlag = 1
            -> default

     c(']'):
            ! pass this char (closing square brackets)
            plusFlag = 0
            minusFlag = 0
            insbFlag = 0
            -> default

    default:
     c( * ):
            ! pass these chars
            ! chars > 127 are already dealt with
            ! So, this deals with remaining chars
            s = s.tostring( charno( parameters, i) )
            i = i + 1
            -> esac
       esac:

        %repeat

    %finish %else %start
        ! Oh, this instruction has no parameters
        params count = 0
    %finish

    %if (params count # 0) %start
        ! now to identify each instruction parameter inside the s string
        %for i = 1,1,params count %cycle

            %if (i = 1) %start
                params(i)_start = 1
            %finish %else %start
                params(i)_start = params(i-1)_scomma + 1
            %finish
            %if (i = params count) %start
                params(i)_end = length(s)
            %finish %else %start
                params(i)_end = params(i)_scomma - 1
            %finish
            params(i)_data = ""
            %for j = params(i)_start,1,params(i)_end %cycle
                params(i)_data = params(i)_data.toString( charno(s,j) )
            %repeat

        %repeat
    %finish

    ! JDM JDM debug show the iCode instruction
    %if (diagnose&16 # 0) %start
        ListICode
        spaces(4)
        printstring( instruction )

        %if (params count # 0) %start
            printsymbol( ' ' )
            ! now to show each parameter
            %for i = 1,1,params count %cycle
                %if (i > 1) %start
                    printsymbol( ',' )
                %finish
                printstring( params(i)_data )
            %repeat
        %finish
        newline
    %finish
    ! JDM JDM end debug

    ! JDM JDM debug show the instruction and decoded parameters
    %if (diagnose&8 # 0) %start
        spaces(4)
        printstring("INSTRUCTION=")
        printstring( instruction )

        %if (params count # 0) %start
            printsymbol( ' ' )
            ! now to show each parameter
            %for i = 1,1,params count %cycle
                dump parameter( i )
            %repeat
        %finish
        newline
    %finish
    ! JDM JDM end debug

%end

%integer %fn validateMachineCode( %string(5) instruction )
    %string(5) opNameX
    %integer i
    %integer opId,opIdx

    ! determine the opId for this instruction
    ! set a default "ILLEGAL" value for the opId
    ! Although Intel 386 has opCodes 0..255
    ! the count of opCode names is much less than 255
    ! so, we are safe to set opId and opIdx = 255
    opId = -1
    opIdx = -1
    %for i = NOP,1,JMP %cycle
        %if (instruction = opName(i)) %start
            opId = i
            opIdx = opGenericId(opId)
            %if (opIdx # -1) %start
                opNameX = instruction
            %finish %else %start
                opNameX = itos(opId,0)
            %finish
        %finish
    %repeat

    ! Currently, we are NOT allowing any floating point instructions
!    %for i = FILD,1,FLDPI %cycle
!       %if instruction = flopname(i) %then opId = i
!    %repeat
!    %if (opId < FILD) %start
!        instruction = opName(opId)
!    %finish %else %start
!        instruction = flopName(opId)
!    %finish

    ! use short form of %if statement (as an example)
    abort("MCODE has illegal/unknown instruction name") %if (opId = -1)

    %result = opId
%end

%routine abortmcode( %string(5) instruction, %string(255) message)
    abort( "ERROR: ".instruction." ".message )
%end

%routine insertMachineCode( %string(5) instruction, %integer opId )
    %integer opIdx
    %record(paramFm) target, source
    %string(255) message
                
    opIdx = opGenericId(opId)

    abortmcode( instruction, "is unknown" ) %if (opId = -1)

    %if (params count = 0) %start
        %if (NOP <= opId <= LEAVE) %start
            dumpsimple(opId)
        %finish %else %start
            selectoutput(listout)
            message = " is an unrecognised zero parameter instruction (or has missing parameter(s)"
            printstring( "ERROR: ".instruction )
            printstring( message )
            newline
            abortmcode( instruction, message )
        %finish
    %finish %else %if (params count = 1) %start
    %begin
        %switch dest(unknown:pointer)
        %label esac dest

        ! NB source,target usage
        ! source == source operand
        ! target == destination of result
        ! But source is also target (for single parameter)
        !
        ! INTEL assembler notation:
        !    "opcode params(1)"
        !    has format
        !    "opcode target"
        !     
        ! AT&T assembler notation:
        !    "opcode params(1)"
        !    has format
        !    "opcode target"
        !
        ! Current implementation uses INTEL assembler notation
        ! with added wrinkle of % as prefix to register
        target = params(1)

        -> dest(target_type)

    dest(unknown):
        abortmcode( instruction, "has 'UNKNOWN' target" )

    dest(variable):
        Stack Var( target_value )
        Operation(opIdx)
        -> esac dest

    dest(register):
        dumpur(opId, target_value)
        -> esac dest

    dest(number):
        abortmcode( instruction, "has 'NUMBER' target" )

    dest(pointer):
        abortmcode( instruction, "has 'POINTER' target" )

    esac dest:
    %end
    %finish %else %if (params count = 2) %start
    %begin
        %switch dest(unknown:pointer)
        %label esac dest

        ! NB source,target usage
        ! source == source operand
        ! target == destination of result
        !
        ! INTEL assembler notation:
        ! "opcode params(1),params(2)" == "opcode target,source"
        !    target == params(1)
        !    source == params(2)
        !
        ! AT&T assembler notation:
        ! "opcode params(1),params(2)" == "opcode source,target"
        !    source == params(1)
        !    target == params(2)
        !
        ! Current implementation uses INTEL assembler notation
        ! with added wrinkle of % as prefix to register
        target = params(1)
        source = params(2)

        ! Defend against various errors
        ! SOURCE + TARGET errors
        %if ((source_type = pointer) %and (target_type = pointer)) %or
            ((source_type = pointer) %and (target_type = variable)) %or
            ((source_type = variable) %and (target_type = pointer)) %or
            ((source_type = variable) %and (target_type = variable)) %start
            abortmcode( instruction, "cannot access memory for source AND target")
        %finish

        ! SOURCE errors
        %if (source_type = pointer) %and (AL <= source_value <= BH) %start
            abortmcode( instruction, "Source POINTER must use a 32-bit register" )
        %finish

        %if (source_type = unknown) %start
            abortmcode( instruction, "has 'UNKNOWN' source" )
        %finish

        ! TARGET errors
        %if (target_type = pointer) %and (AL <= target_value <= BH) %start
            abortmcode( instruction, "Target POINTER must use a 32-bit register" )
        %finish

        %if (target_type = unknown) %start
            abortmcode( instruction, "has 'UNKNOWN' target" )
        %finish

        ! If absolute target addressing is to be allowed
        !     then this may not be an error
        %if (target_type = number) %start
            abortmcode( instruction, "has 'NUMBER' target" )
        %finish

        -> dest(target_type)

    dest(unknown):
        -> esac dest

    dest(variable):
        %begin
            %switch srce(unknown:pointer)
            %label esac source

            -> srce(source_type)

        srce(unknown):
            -> esac source

        srce(variable):
            -> esac source

        srce(register):
            Stack Var( target_value )

            %if (top_base = ESP) %start
                ! it's a push
                %if (top_type = integer) %start
                    dumpur(PUSH, source_value)
                %finish %else %if (top_type = byte) %start
                    dumpur(PUSH, source_value)
                %finish
            %finish %else %if (top_type = general) %start
                dumpmr(opId, top_base!top_scope, top_disp, top_extdisp, source_value)
            %finish %else %if (top_type = integer) %start
                dumpmr(opId, top_base!top_scope, top_disp, top_extdisp, source_value)
            %finish %else %if (top_type = record) %start
                dumpmr(OpId, top_base!top_scope, top_disp, top_extdisp, source_value)
            %finish %else %if (top_type = byte) %start
                %if (EAX <= source_value <= EDI) %start
                    dumpmr(opId, top_base!top_scope, top_disp, top_extdisp, source_value )
                %finish %else %if (AL <= source_value <= BH) %start
                    dumpmr8(opId, top_base!top_scope, top_disp, top_extdisp, source_value )
                %finish %else %start
                    selectoutput(listout)
                    printstring(" **** Unknown SOURCE register id=".itos(source_value,0))
                    newline
                %finish
            %finish %else %start
                dumpmr(opId, top_base!top_scope, top_disp, top_extdisp, source_value)
            %finish
            pop rel
            -> esac source

        srce(number):
            Stack Var( target_value )
            Stack Var( target_value )
            Push Const( source_value )
            Operation(opIdx)
            Assign(1)
            -> esac source

        srce(pointer):
            -> esac source

        esac source:
        %end
        -> esac dest

    dest(register):
        %begin
            %switch srce(unknown:pointer)
            %label esac source

            -> srce(source_type)

        srce(unknown):
            -> esac source

        srce(variable):
            selectoutput(listout)
            Stack Var( source_value )

            %if (EAX <= target_value <= EDI) %start
                dumprv( opId, target_value, top )
            %finish %else %if (AL <= target_value <= BH) %start
                dumpr8v( opId, target_value, top )
            %finish %else %start
                selectoutput(listout)
                printstring(" **** Unknown TARGET register")
                newline
            %finish
            ! remove the temporary variable from the operation stack
            pop rel
            -> esac source

        srce(register):
            %if (EAX <= target_value <= EDI) %and (EAX <= source_value <= EDI) %start
                ! Operation on two 32-bit registers
                dumprr(opId, target_value, source_value )
            %finish %else %if (AL <= target_value <= BH) %and (AL <= source_value <= BH) %start
                ! Operation on two 8-bit registers
                dumprr8(opId, target_value, source_value )
            %finish %else %if (EAX <= target_value <= EDI) %and (AX <= source_value <= BX) %start
                ! Special operation on 32-bit target from 16-bit source registers
                dumprrx(opId, target_value, source_value )
            %finish %else %start
                selectoutput(listout)
                printstring(" **** Mis-match between SOURCE,TARGET registers")
                newline
            %finish
            -> esac source

        srce(number):
            dumprioffset(opId,target_value,0,source_value,0)
            -> esac source

        srce(pointer):
            selectoutput(listout)
            printstring(" EXPERIMENTAL IMPLEMENTATION")
            newline

            %if (EAX <= target_value <= EDI) %start
                dumprm(opId, target_value, source_value, source_offset, source_offset )
            %finish %else %if (AL <= target_value <= BH) %start
                dumprm8(opId, target_value, source_value, source_offset, source_offset )
            %finish %else %start
                selectoutput(listout)
                printstring(" **** Unknown TARGET register id=".itos(target_value,0))
                newline
            %finish
            -> esac source

        esac source:
        %end
        -> esac dest

    dest(number):
        -> esac dest

    dest(pointer):
        %begin
            %switch srce(unknown:pointer)
            %label esac source

            -> srce(source_type)

        srce(unknown):
            -> esac source

        srce(variable):
            -> esac source

        srce(register):
            %if (EAX <= source_value <= EDI) %start
                ! source is 32-bit register
                dumpmr(opId, target_value, target_offset, 0, source_value)
            %finish %else %if (AL <= source_value <= BH) %start
                ! source is 8-bit register
                dumpmr8(opId, target_value, target_offset, 0, source_value)
            %finish %else %start
                selectoutput(listout)
                printstring(" **** Unknown SOURCE register id=".itos(source_value,0))
                newline
            %finish
            -> esac source

        srce(number):
            selectoutput(listout)
            printstring(" EXPERIMENTAL IMPLEMENTATION")
            newline
            dumpmi(opId, target_value, target_offset, 0, source_value)
            -> esac source

        srce(pointer):
            -> esac source

        esac source:
        %end
        -> esac dest

    esac dest:
    %end
    %finish %else %start
        abortmcode( instruction, "has unexpected number ".itos(params count,0)." of parameters.")
    %finish

%end

!                                              >> MACHINE CODE <<
%routine Machine Code( %string(255) code )
    ! This is meant to insert a machine code fragment into the code stream
    %integer opId

    ! ok, lets go
    ! First, parse the machine code text
    parseMachineCode( code )

    ! Next, check that we have a valid instruction
    opId = validateMachineCode( instruction )

    ! Finally add the instruction to the output
    insertMachineCode( instruction, opId )
    
%end

%list
%endoffile
