!if 0
Copyright (c) Microsoft Corporation.  All rights reserved.
!endif
!if 0
Use of this source code is subject to the terms of the Microsoft
premium shared source license agreement under which you licensed
this source code. If you did not accept the terms of the license
agreement, you are not authorized to use this source code.
For the terms of the license, please see the license agreement
signed by you and Microsoft.
THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
!endif

!if "$(NOPASS0)"=="1"

copy_files:

!else

copy_files:
   xcopy xml\*.xml $(_RELEASEDIR)\xamlruntime\test\perf\ScreenTransition\*.xml /D /Y /F
!if "$(WINCEREL)"=="1"
   xcopy xml\*.xml $(_FLATRELEASEDIR)\xamlruntime\test\perf\ScreenTransition\*.xml /D /Y /F
!endif

   xcopy ..\..\data\images\ScreenTransition\* ..\..\Data\Xaml\ScreenTransition\ScreenTransition\* /D /Y /F

!endif

del_files:
   rd /S /Q ..\..\Data\Xaml\ScreenTransition\ScreenTransition