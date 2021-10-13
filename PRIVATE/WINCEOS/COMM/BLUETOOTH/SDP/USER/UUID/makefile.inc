!if 0
Copyright (c) Microsoft Corporation.  All rights reserved.
!endif
!if 0
This source code is licensed under Microsoft Shared Source License
Version 1.0 for Windows CE.
For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
