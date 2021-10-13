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
#pragma once

//Calculate the standard date and daylight date for a specificed timezone in a specified year
//Return value:
//    pstStandardDate: The local time of result standard time start date in SYSTEMTIME format
//    pstDaylightDate: The local time of result daylight time start date in SYSTEMTIME format
//    pftStandardDate: The system time of result standard time start date in FILETIME format
//    pftDaylightDate: The system time of result daylight time start date in FILETIME format
BOOL GetStandardDate(WORD wYear, const TIME_ZONE_INFORMATION* pTZI, SYSTEMTIME* pstStandardDate, FILETIME* pftStandardDate);
BOOL GetDaylightDate(WORD wYear, const TIME_ZONE_INFORMATION* pTZI, SYSTEMTIME* pstDaylightDate, FILETIME* pftDaylightDate);

//Verify if the given timezone support DST
BOOL DoesTzSupportDST(const TIME_ZONE_INFORMATION *ptzi);
