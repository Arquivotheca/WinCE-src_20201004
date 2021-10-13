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

#ifndef _NK_PRIV_H_
#define _NK_PRIV_H_

// suspend or resume immediately if a thread resume bit is set
#define NKSuspendPriv PRIV_IMPLICIT_DECL(BOOL, SH_CURTHREAD, ID_THREAD_BLOCKSRWLOCK, (HTHREAD hThread))

// resume a suspended thread or set the resume wait bit
#define NKResumePriv PRIV_IMPLICIT_DECL(BOOL, SH_CURTHREAD, ID_THREAD_RESUMESRWLOCK, (HTHREAD hThread))


#endif // _NK_PRIV_H_
