
CDEFINES = $(CDEFINES)  -DCE_NO_EXCEPTIONS 


# There's just too much UI to be lean an mean, I suppose
#NOT_LEAN_AND_MEAN=1

#global includes and libs can be handled here
INCLUDES=\
    ..\e_include\inc;   \
    ..\inc\common;      \
    ..\inc\version;     \
    $(_PUBLICROOT)\common\oak\inc; \
    $(INCLUDES)



