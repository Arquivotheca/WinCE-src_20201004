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

// We need this dummy file here to force the real TCHAR.H to *not* be included.
// The CRT has a funky scheme to generate ansi & wide versions of funcs from 
// the same C files just by enabling & disabling _UNICODE. We averted a 
// conflict by renaming _UNICODE in this context to CRT_UNICODE. However
// if we get the real TCHAR included we end up with further conflicts that
// are hard to resolve. So this dummy is used to avoid that.
//
// Note: This dir must be *first* in the INCLUDE path
//
