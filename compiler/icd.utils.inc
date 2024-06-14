%endoflist

    %external %integer     %function %spec get maxtag
    %external              %routine  %spec set symbol data( %integer tag, %string(255) name, %integer tf, size, scope)
    %external %string(255) %function %spec get symbol name( %integer tag )
    %external %string(15)  %function %spec get own String( %integer flags )
    %external %string(63)  %function %spec get flags string( %integer flags )
    %external %string(255) %function %spec get type def( %integer type,size )
    %external %integer     %function %spec get entry type( %integer type, size )
    %external %string(15)  %function %spec get typeof String( %integer type )
    %external %string(15)  %function %spec get type name( %integer type)
    %external %string(15)  %function %spec get external form name( %integer form )
    %external %string(15)  %function %spec get internal form name( %integer form )
    %external %string(15)  %function %spec getICodeName( %integer code )
    %external %integer     %function %spec ReadTag
    %external %integer     %function %spec ReadTagComma
    %external %integer     %function %spec ReadInteger
    %external %integer     %function %spec ReadByte
    %external %longreal    %function %spec ReadReal
    %external %string(255) %function %spec ReadString
    %external %string(255) %function %spec Get Ascii( %integer terminator)
    %external %integer     %function %spec ReadICode
    %external %integer     %function %spec lookahead icode
    %external              %routine  %spec next iCode

%list
%endoffile
