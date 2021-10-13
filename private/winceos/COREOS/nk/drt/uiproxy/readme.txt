!if 0
Copyright (c) Microsoft Corporation.  All rights reserved.
!endif
!if 0
This source code is licensed under Microsoft Shared Source License
Version 1.0 for Windows CE.
For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
!endif

tst\...
Contains a test app to load a test driver (drv) into kernel.
Exe: ut_uiproxytst.exe

drv\...
Contains a test driver which is loaded in kernel and then calls the 
API: CeCallUserProc with option to load the ut_uiproxydll.dll and 
call an entry point (StrReverse) in it.
Dll: ut_uiproxydrv.dll

dll\...
Contains a test dll which is loaded by a ui proxy driver when the
kernel mode driver calls the API: CeCallUserProc.
Dll: ut_uiproxydll.dll

To run all these:

a) build -c from this folder to build all the components
b) run ut_uiproxytst.exe to register and activate the kernel mode driver
