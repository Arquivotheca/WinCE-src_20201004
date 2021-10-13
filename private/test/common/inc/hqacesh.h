//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#ifdef UNDER_CE

#define _O_RDONLY   0x0000  /* open for reading only */
#define _O_WRONLY   0x0001  /* open for writing only */
#define _O_RDWR     0x0002  /* open for reading and writing */
#define _O_APPEND   0x0008  /* writes done at eof */
#define _O_SEQUENTIAL 0x0020

#define _O_CREAT    0x0100  /* create and open file */
#define _O_TRUNC    0x0200  /* open and truncate */
#define _O_EXCL     0x0400  /* open only if file doesn't already exist */

#define SEEK_SET      0
#define SEEK_END      2

int     pp_open(WCHAR *pFileName, UINT uiMode);
int     pp_read(int h, BYTE * pBuffer, int iCount);
int     pp_write(int h, BYTE * pBuffer, int iCount);
int     pp_lseek(int h, int iOffset, int iMode);
int     pp_close(int h);

#endif // UNDER_CE

