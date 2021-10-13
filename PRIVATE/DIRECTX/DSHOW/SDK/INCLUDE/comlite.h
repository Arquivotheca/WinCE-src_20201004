//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
//==========================================================================;
//
//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
//  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
//  PURPOSE.
//
//
//--------------------------------------------------------------------------;

/******************************Module*Header*******************************\
* Module Name: ComLite.h
*
* This header file is to provide a migration path for uses of ActiveMovie
* betas 1 and 2.
*
\**************************************************************************/


#ifndef _INC_COMLITE_
#define _INC_COMLITE_

#define QzInitialize            CoInitialize
#define QzUninitialize          CoUninitialize
#define QzFreeUnusedLibraries   CoFreeUnusedLibraries

#define QzGetMalloc             CoGetMalloc
#define QzTaskMemAlloc          CoTaskMemAlloc
#define QzTaskMemRealloc        CoTaskMemRealloc
#define QzTaskMemFree           CoTaskMemFree
#define QzCreateFilterObject    CoCreateInstance
#define QzCLSIDFromString       CLSIDFromString
#define QzStringFromGUID2       StringFromGUID2

#endif  // _INC_COMLITE_
