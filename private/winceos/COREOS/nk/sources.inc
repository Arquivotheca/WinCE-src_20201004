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
THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
!endif

# VS11 porting: platform, nk and fulllibc should be built with /arch:IA32
# because we do not have FP registers initialized during early boot and
# kernel should not be using SSE2 instruction set due to performance reasons.

!IF "$(_TGTCPUFAMILY)" == "x86"
C_ARCH_FLAGS=/arch:IA32
!ENDIF