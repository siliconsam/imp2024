%endoflist
{----------------------------------------------------------------------------}
    ! External Library routines implemented in C
    ! N.B. These routines need an alias name referring to the actual C routine
    !      Also the alias name may need to be changed for different operating systems
    ! imprtl routines implemented in C to access C standard file handles,error status,write a double length real
    %external %routine     %spec prim exit            %c
                           %alias "_exit"              %c
                           (%integer status)
    %external %integer %fn %spec prim geterr          %c
                           %alias "_geterrno"
    %external %integer %fn %spec prim geterrhandle    %c
                           %alias "_getstderr"
    %external %integer %fn %spec prim getinhandle     %c
                           %alias "_getstdin"
    %external %integer %fn %spec prim getouthandle    %c
                           %alias "_getstdout"
    %external %integer %fn %spec prim openfile        %c
                           %alias "_fopen"             %c
                           (%integer mode address, %integer file name address)
    %external %integer %fn %spec prim tell            %c
                           %alias "_ftell"             %c
                           (%integer handle)
    %external %routine     %spec prim file rewind     %c
                           %alias "_rewind"            %c
                           (%integer handle)
    %external %routine     %spec prim file seek       %c
                           %alias "_fseek"             %c
                           (%integer handle, displacement, pos)
    %external %routine     %spec prim close           %c
                           %alias "_fclose"            %c
                           (%integer handle)
    %external %integer %fn %spec prim readbytes       %c
                           %alias "_fread"             %c
                           (%integer handle, count, itemsz, %byte %name buffer)
    %external %integer %fn %spec prim writebytes      %c
                           %alias "_fwrite"            %c
                           (%integer handle, count, itemsz, %byte %name buffer)
    %external %integer %fn %spec prim eof             %c
                           %alias "_feof"              %c
                           (%integer handle)
    %external %integer %fn %spec prim get char        %c
                           %alias "_fgetc"             %c
                           (%integer handle)
    %external %routine     %spec prim put char        %c
                           %alias "_fputc"             %c
                           (%integer handle,ch)
    %external %routine     %spec prim flush           %c
                           %alias "_fflush"            %c
                           (%integer handle)
{----------------------------------------------------------------------------}
%list
%endoffile
