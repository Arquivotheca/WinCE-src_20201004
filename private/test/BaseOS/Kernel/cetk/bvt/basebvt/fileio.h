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

#define IOBVT_DIR         TEXT("iobvt.dir")
#define IOBVT_FILE        TEXT("iobvt.txt")

#define MAX_BUF_LEN        260        // max buffer length, max path length
                                      // this should always be an even no

#define READ_WRITE_BUF     256     // size of read/write buffer

#define BAD_FILE_HANDLE     INVALID_HANDLE_VALUE    // bad file handle

#define MY_READ_WRITE_ACCESS    GENERIC_READ | GENERIC_WRITE

#define FAIL_IF_ALREADY_EXISTS    TRUE     // FailIfExists flag defines for
#define PASS_IF_ALREADY_EXISTS    FALSE    // CreateFile

#define NO_SHARE             0        // Share mode for exclusive access
#define ROOT_PATH_LEN        3        // Length of root path name

#define SHARE_ALL            FILE_SHARE_READ | FILE_SHARE_WRITE

#define SIZE(a) (sizeof(a)/sizeof(a[0]))

BOOL BaseCreateDirectory(LPTSTR acDirName);
BOOL BaseCreateCloseFile(LPTSTR acFileName);
BOOL BaseOpenReadWriteClose(LPTSTR acFileName);
BOOL BaseDeleteFile(LPTSTR acFileName);
BOOL BaseDeleteDirectory(LPTSTR acDirName);
BOOL OpenExistingFile(LPTSTR lpFilename,HANDLE *phFileHandle);
BOOL ReadWriteFile(HANDLE hFileHandle,DWORD dw);
BOOL SetWriteBuffer (LPBYTE ,DWORD, DWORD);
