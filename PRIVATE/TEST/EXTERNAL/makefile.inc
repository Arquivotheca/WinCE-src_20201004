!if 0
Copyright (c) Microsoft Corporation.  All rights reserved.
!endif
!if 0
Use of this source code is subject to the terms of the Microsoft shared
source or premium shared source license agreement under which you licensed
this source code. If you did not accept the terms of the license agreement,
you are not authorized to use this source code. For the terms of the license,
please see the license agreement between you and Microsoft or, if applicable,
see the SOURCE.RTF on your install media or the root of your tools installation.
THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
!endif
!if "$(USING_PB_WORKSPACE_ENVIRONMENT)"=="1"

copylibs:
    copy "$(_PB_INSTALL_ROOT)\CEPB\WCETK\TUX\LIB\$(_TGTCPU)\retail\*" $(_PROJECTOAKROOT)\lib\$(_CPUINDPATH)
    copy "$(_PB_INSTALL_ROOT)\CEPB\WCETK\TUX\BIN\$(_TGTCPU)\retail\*" $(_PROJECTOAKROOT)\target\$(_CPUINDPATH)
    copy "$(_PB_INSTALL_ROOT)\CEPB\WCETK\TUX\LIB\$(_TGTCPU)\retail\*" $(_PUBLICROOT)\common\oak\lib\$(_CPUINDPATH)
    copy "$(_PB_INSTALL_ROOT)\CEPB\WCETK\TUX\BIN\$(_TGTCPU)\retail\*" $(_PUBLICROOT)\common\oak\target\$(_CPUINDPATH)

!else

copylibs:

!endif