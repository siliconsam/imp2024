%endoflist
{----------------------------------------------------------------------------}
    ! Simple trig library functions to demonstrate
    ! how to cross-link with the C library functions
    ! Copyright 2002 NB Information Limited
    !
    ! Declare useful trignometry functions
    !
{----------------------------------------------------------------------------}
    %external %long %real %fn %spec C Lib Cos %c
                                    %alias "cos" %c
                                    ( %longreal r)
    %external %long %real %fn %spec C Lib Sine %c
                                    %alias "sin" %c
                                    ( %longreal r)
{----------------------------------------------------------------------------}
%list
%endoffile
