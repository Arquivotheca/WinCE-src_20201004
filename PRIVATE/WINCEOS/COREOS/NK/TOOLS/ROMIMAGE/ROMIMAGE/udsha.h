//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
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

