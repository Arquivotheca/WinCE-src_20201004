//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+----------------------------------------------------------------------------
//
// 
// File:    typemapr.cpp
// 
// Contents:
//
//  implementation file 
//
//		ITypeMapper classes  implemenation
//	
//
//-----------------------------------------------------------------------------

#ifndef _UTCTIME_H_
    #define _UTCTIME_H_

    HRESULT adjustTime(DOUBLE *pdblDate, long lSecondsToAdjust);
    HRESULT VariantTimeToUTCTime(DOUBLE dblVariantTime, DOUBLE *pdblUTCTime);
    HRESULT UTCTimeToVariantTime(DOUBLE dblUTCTime, DOUBLE *pdblVariantTime);

    extern const   long c_secondsInDay;
#endif


