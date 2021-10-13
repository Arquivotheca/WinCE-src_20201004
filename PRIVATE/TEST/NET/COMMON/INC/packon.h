//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


Module Name:

    packon.h

Abstract:

    This file turns packing of structures on.  (That is, it disables
    automatic alignment of structure fields.)  An include file is needed
    because various compilers do this in different ways.

    The file packoff.h is the complement to this file.

--*/

#if ! (defined(lint) || defined(_lint))
#if ( _MSC_VER >= 800 )
#pragma warning(disable:4103)
#endif
#pragma pack(1)                 // x86, MS compiler; MIPS, MIPS compiler
#endif // ! (defined(lint) || defined(_lint))

