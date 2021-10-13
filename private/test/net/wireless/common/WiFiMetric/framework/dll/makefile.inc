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

UPDATE:

!else

UPDATE: ..\..\testscript\*.*
    if not exist $(_RELEASEDIR)\WQMConfigs mkdir $(_RELEASEDIR)\WQMConfigs
    copy ..\..\testscript\*.xml $(_RELEASEDIR)\WQMConfigs 
    copy ..\..\testscript\*.txt $(_RELEASEDIR)\WQMConfigs 
    copy ..\..\testscript\*.reg $(_RELEASEDIR)\WQMConfigs 
    copy ..\..\docs\WiFiMetric_CTK.docx $(_RELEASEDIR)
    copy ..\..\..\EnrollDll\enroll.cfg $(_RELEASEDIR)\eaptls.cfg
!if "$(WINCEREL)"=="1"
    if not exist $(_FLATRELEASEDIR)\WQMConfigs mkdir $(_FLATRELEASEDIR)\WQMConfigs
    copy ..\..\testscript\*.xml $(_FLATRELEASEDIR)\WQMConfigs 
    copy ..\..\testscript\*.txt $(_FLATRELEASEDIR)\WQMConfigs 
    copy ..\..\testscript\*.reg $(_FLATRELEASEDIR)\WQMConfigs 
    copy ..\..\docs\WiFiMetric_CTK.docx $(_FLATRELEASEDIR)
    copy ..\..\..\EnrollDll\enroll.cfg $(_FLATRELEASEDIR)\eaptls.cfg
!endif

!endif

