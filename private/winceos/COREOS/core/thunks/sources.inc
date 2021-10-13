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

COMMON_INCLUDES=..\..\inc;..\..\..\inc;..\..\..\gwe\inc
COMMON_CDEFINES=-DCOREDLL -DWINCEMACRO -DFAREAST
NOMIPS16CODE=1

#WARNLEVEL=4

COMMON_SOURCES=    \
 ..\tafdfunc.c     \
 ..\tpmfunc.c      \
 ..\tcommctr.cpp   \
 ..\textfile.c     \
 ..\timm.cpp       \
 ..\tkfuncs.cpp    \
 ..\tkmarshal.cpp  \
 ..\tkprofile.cpp  \
 ..\tpm.cpp        \
 ..\tnled.cpp      \
 ..\tservices.cpp  \
 ..\tshellap.c     \
 ..\ttapi.c        \
 ..\tdevice.cpp    \
 ..\twinbase.cpp   \
 ..\twindbas.cpp   \
 ..\twindbas2.cpp  \
 ..\tfsmain.cpp    \
 ..\tfsreplbit.cpp \
 ..\tfspass.cpp    \
 ..\twingdi.cpp    \
 ..\twingwes.cpp   \
 ..\twinreg.cpp    \
 ..\twinuser.cpp   \
 ..\tregclass.cpp  \
 ..\twnet.c        \
 ..\tsystemparametersinfo.cpp \
 ..\teventlog.cpp  \
 ..\tlassd.cpp     \
 ..\tcredapi.cpp   \
 ..\tadb.cpp       \
 ..\deviceid_proxy.cpp \
 ..\tstoreapi.cpp  \
 ..\tevnt.cpp     \

