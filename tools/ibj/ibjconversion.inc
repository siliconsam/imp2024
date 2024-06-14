%endoflist

%external %integer     %fn %spec hex2byte    ( %string(255) s, %integer t )
%external %integer     %fn %spec hex2integer ( %string(255) s, %integer t )
%external %integer     %fn %spec hex2short   ( %string(255) s, %integer t )
%external %string(127) %fn %spec hex2name    ( %string(255) s, %integer t )
%external %string(255) %fn %spec hex2code    ( %string(255) s, %integer t )
%external %string(1)   %fn %spec itype2hex   ( %integer x )
%external %string(2)   %fn %spec byte2hex    ( %integer x )
%external %string(4)   %fn %spec short2hex   ( %integer x )
%external %string(4)   %fn %spec integer2hex ( %integer x )
%external %string(255) %fn %spec name2hex    ( %string(127) s )

%list
%endoffile
