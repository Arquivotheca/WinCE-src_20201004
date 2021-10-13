!if "$(NOPASS0)"=="1"
copylib:
    copy $(_PUBLICROOT)\common\oak\lib\$(_CPUINDPATH)\netmain.* $(_PROJECTOAKROOT)\lib\$(_CPUINDPATH)

!else

copylib:
    copy $(_PUBLICROOT)\common\oak\lib\$(_CPUINDPATH)\netmain.* $(_PROJECTOAKROOT)\lib\$(_CPUINDPATH)

!endif