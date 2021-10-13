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

!if "$(NOPASS0)"=="1"

copy_files:

!else

copy_files:
   xcopy xml\*.xml $(_RELEASEDIR)\xamlruntime\test\perf\ListPerf\*.xml /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix01.* $(_RELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix01.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix02.* $(_RELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix02.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix03.* $(_RELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix03.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix04.* $(_RELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix04.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix05.* $(_RELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix05.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix06.* $(_RELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix06.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix07.* $(_RELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix07.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix08.* $(_RELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix08.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix09.* $(_RELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix09.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix10.* $(_RELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix10.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix11.* $(_RELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix11.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix12.* $(_RELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix12.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix13.* $(_RELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix13.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix14.* $(_RELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix14.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix15.* $(_RELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix15.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix16.* $(_RELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix16.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix17.* $(_RELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix17.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix18.* $(_RELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix18.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix19.* $(_RELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix19.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix20.* $(_RELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix20.* /D /Y /F
!if "$(WINCEREL)"=="1"
   xcopy xml\*.xml $(_FLATRELEASEDIR)\xamlruntime\test\perf\ListPerf\*.xml /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix01.* $(_FLATRELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix01.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix02.* $(_FLATRELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix02.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix03.* $(_FLATRELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix03.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix04.* $(_FLATRELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix04.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix05.* $(_FLATRELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix05.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix06.* $(_FLATRELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix06.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix07.* $(_FLATRELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix07.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix08.* $(_FLATRELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix08.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix09.* $(_FLATRELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix09.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix10.* $(_FLATRELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix10.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix11.* $(_FLATRELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix11.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix12.* $(_FLATRELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix12.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix13.* $(_FLATRELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix13.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix14.* $(_FLATRELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix14.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix15.* $(_FLATRELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix15.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix16.* $(_FLATRELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix16.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix17.* $(_FLATRELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix17.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix18.* $(_FLATRELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix18.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix19.* $(_FLATRELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix19.* /D /Y /F
   xcopy ..\..\data\Images\ListPerf\JPEGs128x128\Pix20.* $(_FLATRELEASEDIR)\xamlruntime\test\perf\ListPerf\JPEGs128x128\XRPix20.* /D /Y /F
!endif

!endif
