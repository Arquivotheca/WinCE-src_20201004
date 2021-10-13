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

#if ! defined (__cmdpipeHXX__)
#define __cmdpipeHXX__  1

#define PIPE_PROCESSED  1
#define PIPE_DELETED    2
#define PIPE_NOOUTPUT   4

struct PipeDescriptor {
    unsigned int uiPipeFlags;

    TCHAR   *lpCommand;
    TCHAR   *lpFileName;

    int     iCommandSize;

    struct PipeDescriptor *pNext;
    struct PipeDescriptor *pPrev;
};

int cmdpipe_CheckForPipes (PipeDescriptor *&);
int cmdpipe_Recharge (PipeDescriptor *pPD);
int cmdpipe_Release (PipeDescriptor *pPD);

#endif  /* __cmdpipeHXX__ */

