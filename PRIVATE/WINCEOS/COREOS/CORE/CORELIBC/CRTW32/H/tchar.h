//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
