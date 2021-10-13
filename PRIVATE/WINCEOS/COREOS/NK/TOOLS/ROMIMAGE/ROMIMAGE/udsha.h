//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once

#ifndef __UDSHA_H__
#define __UDSHA_H__

//------------------------------------------------------------------------------
//
//  Microsoft Unified DRM
//
//  Microsoft Confidential
//  Copyright, 1999 Microsoft Corporation.  All Rights Reserved.
//
//  file:   udsha.h
//
//  Contents: Black Box for Purpose Built device
//
//  Revision History:
//      10/10/99  jmanfer - created 
// -@- 11/08/00 (mikemarr)  - moved magic #define constants to source file
//                          - renamed class from Sha to UDSha1
//                          - member data and Transform changed to private
//
//------------------------------------------------------------------------------

class CUDSha1{
public:
    enum {DIGESTSIZE= 20, BLOCKSIZE= 64};
    bool Init();
    bool Update(BYTE*, int);
    bool Final(BYTE[DIGESTSIZE]);

private:
    bool Transform(BYTE*);

private:
    unsigned m_rgDigest[DIGESTSIZE / sizeof(DWORD)]; // ABCDE
    unsigned m_uCount[2];                           // bit Length
    int      m_iBufLen;
    BYTE     m_rgBuffer[BLOCKSIZE];
};

#endif // #ifndef __UDSHA_H__

