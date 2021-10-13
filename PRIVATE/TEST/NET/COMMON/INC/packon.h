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

