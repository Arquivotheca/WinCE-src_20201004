!if 0
Copyright (c) Microsoft Corporation.  All rights reserved.
!endif
!if 0
This source code is licensed under Microsoft Shared Source License
Version 1.0 for Windows CE.
For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
!endif
$(_COMMONSDKROOT)\inc\bthapi.h: $(PASS0_HEADERDIR)\bthapi.h
    copy $(_PRIVATEROOT)\bat\pbcopyc.txt+$? $@

{$(PASS0_HEADERDIR)}.h{$(_COMMONSDKROOT)\inc}.h:
    copy $** $@

{$O}.h{$(_COMMONSDKROOT)\inc}.h:
    copy $** $@

{}.h{$(_COMMONSDKROOT)\inc}.h:
    copy $** $@

{}.idl{$(_COMMONSDKROOT)\inc}.idl:
    copy $** $@