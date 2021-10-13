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
#ifndef _INC_MSDOS
#define _INC_MSDOS

#define FOPEN        0x01    /* file handle open */
#define FEOFLAG     0x02    /* end of file has been encountered */
#define FCRLF        0x04    /* CR-LF across read buffer (in text mode) */
#define FPIPE        0x08    /* file handle refers to a pipe */

#define FAPPEND     0x20    /* file handle opened O_APPEND */
#define FDEV        0x40    /* file handle refers to device */
#define FTEXT        0x80    /* file handle is in text mode */


#endif // _INC_MSDOS
