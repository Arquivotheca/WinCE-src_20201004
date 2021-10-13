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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#pragma once

// Initialzie the canonicalizer API.
LRESULT InitializeCanonicalizerAPI();

// Returns newly allocated path if successful, NULL if probe or
// canonicalization failed.  If lpszPathName is NULL this function will
// return NULL.  Free the buffer using SafeFreeCanonicalPath.
LPWSTR SafeGetCanonicalPathW (
    __in_z LPCWSTR lpszPathName,           // Caller buffer, NULL-terminated with MAX_PATH maximum length
    __out_opt size_t* pcchCanonicalPath = NULL
    );

// Symmetric function to free the path from SafeGetCanonicalPathW.
VOID SafeFreeCanonicalPath (LPWSTR lpszCPathName);

// Returns true if the string passed in is equivalent to L"\\"
BOOL IsRootPathName (const WCHAR* pPath);

// Convert trailing *.* to *
void SimplifySearchSpec (__inout_z WCHAR* pSearchSpec);

// determine if a path is, or is a sub-dir of, the root directory
BOOL IsPathInSystemDir(LPCWSTR lpszPath);

// determine if a path is in the root directory, but not a sub-dir
BOOL IsPathInRootDir(LPCWSTR lpszPath);

// determine if a name is a console name (\reg: or \con)
BOOL IsConsoleName (const WCHAR* pName);

// determine if a name ends with : (like \VOL:)
BOOL IsPsuedoDeviceName (const WCHAR* pName);

// determine if a name is a legacy driver name (like COM1:)
BOOL IsLegacyDeviceName (const WCHAR* pName);

// currently a no-op; always returns FALSE
BOOL IsSecondaryStreamName (const WCHAR* pName);
