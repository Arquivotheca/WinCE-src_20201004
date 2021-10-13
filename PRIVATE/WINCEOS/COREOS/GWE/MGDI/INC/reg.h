//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*


*/

#ifndef __REG_H__
#define __REG_H__

//
// GDI_REG_KEY is used to generate registry key names all based
// on the same MGDI-specific path.  Usage example:
//
//      g_fv1PaletteIndex = FRegKeyExists(GDI_REG_KEY("V1PALETTEINDEX"));
//
#define GDI_REG_KEY(str)    TEXT("SYSTEM\\GDI\\") TEXT(str)

//
// Return TRUE iff the given registry key exists.  Callers should
// usually use the GDI_REG_KEY macro to construct the key's name.
//
BOOL FRegKeyExists(WCHAR *pwszKey);

// Obtains a DWORD value from the registry.
BOOL FGetRegDword(WCHAR *pwszKey, WCHAR *pwszValueName, DWORD *pdwValue);

// Obtains a string value from the registry. Returns true iff it succesffuly
// read a string from the registry.

BOOL FGetRegSZ(WCHAR *pwszKey, WCHAR *pwszValueName, WCHAR *pwszBuf, DWORD *pcbBufSize );

#endif  // !__REG_H__
