!if 0
Copyright (c) Microsoft Corporation.  All rights reserved.
!endif
!if 0
This source code is licensed under Microsoft Shared Source License
Version 1.0 for Windows CE.
For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
!endif
$(SDK_INC_PATH)\mssoap.h : ..\..\inc\idl\mssoap.h
    copy $(_PRIVATEROOT)\bat\pbcopyc.txt+..\..\inc\idl\mssoap.h $(SDK_INC_PATH)\mssoap.h

$(SDK_INC_PATH)\WISC.h : ..\..\inc\idl\WISC.h
    copy $(_PRIVATEROOT)\bat\pbcopyc.txt+..\..\inc\idl\WISC.h $(SDK_INC_PATH)\WISC.h

$(SDK_INC_PATH)\XHSC.h : ..\..\inc\idl\XHSC.h
    copy $(_PRIVATEROOT)\bat\pbcopyc.txt+..\..\inc\idl\XHSC.h $(SDK_INC_PATH)\XHSC.h

$(SDK_INC_PATH)\isaresponse.h : ..\..\inc\idl\isaresponse.h
    copy $(_PRIVATEROOT)\bat\pbcopyc.txt+..\..\inc\idl\isaresponse.h $(SDK_INC_PATH)\isaresponse.h


$(SDK_INC_PATH)\mssoap.idl : mssoap.idl
    copy mssoap.idl $(SDK_INC_PATH)\mssoap.idl

$(SDK_INC_PATH)\WISC.idl : WISC.idl
    copy WISC.idl $(SDK_INC_PATH)\WISC.idl

$(SDK_INC_PATH)\XHSC.idl : XHSC.idl
    copy XHSC.idl $(SDK_INC_PATH)\XHSC.idl

$(SDK_INC_PATH)\isaresponse.idl : isaresponse.idl
    copy isaresponse.idl $(SDK_INC_PATH)\isaresponse.idl



$(SDK_INC_PATH)\conn.idl : conn.idl
    copy conn.idl $(SDK_INC_PATH)\conn.idl

$(SDK_INC_PATH)\soapreader.idl : soapreader.idl
    copy soapreader.idl $(SDK_INC_PATH)\soapreader.idl

$(SDK_INC_PATH)\soapserializer.idl : soapserializer.idl
    copy soapserializer.idl $(SDK_INC_PATH)\soapserializer.idl

$(SDK_INC_PATH)\soapmsg.idl : soapmsg.idl
    copy soapmsg.idl $(SDK_INC_PATH)\soapmsg.idl

$(SDK_INC_PATH)\wsdl.idl : wsdl.idl
    copy wsdl.idl $(SDK_INC_PATH)\wsdl.idl

$(SDK_INC_PATH)\soaperr.idl : soaperr.idl
    copy soaperr.idl $(SDK_INC_PATH)\soaperr.idl

mssoapproxy_p : mssoap_p.c
    copy mssoap_p.c ..\prxystub

isaresponse_p : isaresponse_p.c
    copy isaresponse_p.c ..\prxystub

dlldata : dlldata.c
    copy dlldata.c ..\prxystub

