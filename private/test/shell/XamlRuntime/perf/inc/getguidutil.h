//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#include <GUIDGenerator.h>

////////////////////////////////////////////////////////////////////////////////
//
//  GetGUID
//
//   Converts the test description into a unique GUID
//
////////////////////////////////////////////////////////////////////////////////
inline GUID GetGUID( const WCHAR* lpTestDescription )
{
    GUID testGUID;
    if( GenerateGUID((TCHAR*)lpTestDescription, (BYTE*)&testGUID, 16) == false )
    {
        g_pKato->Log(LOG_DETAIL, L"GenerateGUID returned false");
    }
    return( testGUID );
}

