!if "$(USING_PB_WORKSPACE_ENVIRONMENT)"=="1"

copylibs:
    md $(_PROJECTOAKROOT)\target\$(_CPUINDPATH)    
    copy $(_WINCEROOT)\OTHERS\WCETK\TUX\LIB\$(_TGTCPU)\retail\* $(_PROJECTOAKROOT)\lib\$(_CPUINDPATH)
    copy $(_WINCEROOT)\OTHERS\WCETK\TUX\BIN\$(_TGTCPU)\retail\* $(_PROJECTOAKROOT)\target\$(_CPUINDPATH)
    copy $(_WINCEROOT)\OTHERS\WCETK\$(_TGTCPU)\retail\* $(_PROJECTOAKROOT)\target\$(_CPUINDPATH)
    copy $(_WINCEROOT)\OTHERS\WCETK\TUX\LIB\$(_TGTCPU)\retail\* $(_PUBLICROOT)\common\oak\lib\$(_CPUINDPATH)
    copy $(_WINCEROOT)\OTHERS\WCETK\TUX\BIN\$(_TGTCPU)\retail\* $(_PUBLICROOT)\common\oak\target\$(_CPUINDPATH)
    copy $(_WINCEROOT)\OTHERS\WCETK\$(_TGTCPU)\retail\* $(_PUBLICROOT)\common\oak\target\$(_CPUINDPATH)

!else

copylibs:

!endif