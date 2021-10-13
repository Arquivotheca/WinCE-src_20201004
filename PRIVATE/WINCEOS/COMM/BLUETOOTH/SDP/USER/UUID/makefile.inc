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
O=$(_OBJDIR)

IID_Sources = \
    $O\bthapi_i.gc \
    $O\guid.gc

GUIDLIB_OPT=$(CDEFINES)

.SUFFIXES: .gc

.c{$O}.gc:
  copy $? $@

{$O}.gc{$O}.lib:
    set __guidlib_temp_dir=$(_OBJDIR)
    guidlib /verbose /Out:$@ /CPP_CMD:"$(HOSTCOMPILER) $(CFLAGS)" /CPP_OPT:"$(GUIDLIB_OPT) /I$(_COMMONSDKROOT)\inc" $?

$(O)\bthguid.lib: $(IID_Sources) $(IID_Sources:.gc=.lib)
   lib /verbose /out:$@ $(IID_Sources:.gc=.lib)

$(_COMMONSDKROOT)\lib\$(_CPUINDPATH)\bthguid.lib : $(O)\bthguid.lib
    copy $? $@
