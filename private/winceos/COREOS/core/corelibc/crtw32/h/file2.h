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
/***
*file2.h - auxiliary file structure used internally by file run-time routines
*
*
*Purpose:
*   This file defines the auxiliary file structure used internally by
*   the file run time routines.
*
*   [Internal]
*
****/

#ifndef _INC_FILE2
#define _INC_FILE2

#ifdef __cplusplus
extern "C" {
#endif

/* Additional _iobuf[]._flag values
 *
 *  _IOSETVBUF - Indicates file was buffered via a setvbuf (or setbuf call).
 *       Currently used ONLY in _filbuf.c, _getbuf.c, fseek.c and
 *       setvbuf.c, to disable buffer resizing on "random access"
 *       files if the buffer was user-installed.
 */

#define _IOYOURBUF  0x0100
#define _IOSETVBUF  0x0400
#define _IOFEOF     0x0800
#define _IOFLRTN    0x1000
#define _IOCTRLZ    0x2000
#define _IOCOMMIT   0x4000


/* General use macros */

#define inuse(s)    ((s)->_flag & (_IOREAD|_IOWRT|_IORW))
#define mbuf(s)     ((s)->_flag & _IOMYBUF)
#define bigbuf(s)   ((s)->_flag & (_IOMYBUF|_IOYOURBUF))
#define anybuf(s)   ((s)->_flag & (_IOMYBUF|_IONBF|_IOYOURBUF))

#ifdef __cplusplus
}
#endif

#endif  /* _INC_FILE2 */
