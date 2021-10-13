!if "$(_WINCEPBIDE)"=="1"

copylibs:
    copy $(_WINCEROOT)\OTHERS\WCETK\TUX\LIB\$(_TGTCPUFAMILY)\retail\* $(_PROJECTOAKROOT)\lib\$(_CPUINDPATH)
    copy $(_WINCEROOT)\OTHERS\WCETK\TUX\BIN\$(_TGTCPUFAMILY)\retail\* $(_PROJECTOAKROOT)\target\$(_CPUINDPATH)
    copy $(_WINCEROOT)\OTHERS\WCETK\$(_TGTCPUFAMILY)\retail\* $(_PROJECTOAKROOT)\target\$(_CPUINDPATH)
    copy $(_WINCEROOT)\OTHERS\WCETK\TUX\LIB\$(_TGTCPUFAMILY)\retail\* $(_PUBLICROOT)\common\oak\lib\$(_CPUINDPATH)
    copy $(_WINCEROOT)\OTHERS\WCETK\TUX\BIN\$(_TGTCPUFAMILY)\retail\* $(_PUBLICROOT)\common\oak\target\$(_CPUINDPATH)
    copy $(_WINCEROOT)\OTHERS\WCETK\$(_TGTCPUFAMILY)\retail\* $(_PUBLICROOT)\common\oak\target\$(_CPUINDPATH)

!else

copylibs:

!endif