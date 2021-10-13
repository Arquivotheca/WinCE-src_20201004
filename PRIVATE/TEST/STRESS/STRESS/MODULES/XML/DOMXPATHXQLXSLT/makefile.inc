!if 0
Copyright (c) Microsoft Corporation.  All rights reserved.
!endif
!if 0
This source code is licensed under Microsoft Shared Source License
Version 1.0 for Windows CE.
For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
!endif
#

$(_FLATRELEASEDIR)\s2_domxpathxqlxslt.xml: s2_domxpathxqlxslt.xml
    copy s2_domxpathxqlxslt.xml $(_FLATRELEASEDIR)

$(_RELEASEDIR)\s2_domxpathxqlxslt.xml: s2_domxpathxqlxslt.xml
    copy s2_domxpathxqlxslt.xml $(_RELEASEDIR)


