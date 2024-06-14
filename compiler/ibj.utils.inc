
    ! remembers the stream# of the IBJ output stream
    %external %routine %spec setibjoutstream( %integer stream )

    ! outputs n as a hex number to the specified hex digits
    %external %routine %spec writehex(%integer n, places)

    ! flushes out the IBJ buffer to the IBJ output stream
    %external %routine %spec writeifrecord( %integer type )

    ! puts a normal code byte into the code pipe
    %external %routine %spec putcodebyte(%integer b)

    ! puts a symbol tag (2-bytes) into the code pipe
    %external %routine %spec putcodetag(%integer t)

    ! puts a 16-bit integer (2-bytes) into the code pipe
    %external %routine %spec putcodeword(%integer t)

    ! puts a 32-bit integer (4-bytes) into the code pipe
    %external %routine %spec putcodelong(%integer t)

    ! utility to copy an IMP string into the IBJ buffer
    %external %routine %spec putcodestring(%string(255)%name s)

%endoffile
