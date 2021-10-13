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

#include <windows.h>
#include <tchar.h>
#include "basebvt.h"
#include "fileio.h"

TCHAR    achBuffer[MAX_BUF_LEN] = { 0 };         // buffer to get back path

extern WCHAR* gpszDirName;
extern WCHAR* gpszFileName;


/*****************************************************************************
 *
 *    Description: 
 *
 *       FileIoTest creates and deletes a directory; creates, closes, opens, 
 *    reads, writes and deletes a file.
 *    Default dir name : iobvt.dir
 *    Default filename : iobvt.txt
 *
 *    To modify the test - use the following flags
 *    /d:dirName
 *    /f:fileName
 *
 *****************************************************************************/
TESTPROCAPI 
BaseFileIoTest(
    UINT uMsg, 
    TPPARAM /*tpParam*/, 
    LPFUNCTION_TABLE_ENTRY /*lpFTE*/
    ) 
{
    HANDLE hTest=NULL, hStep=NULL;
    TCHAR   acDirName[256] = { 0 };
    TCHAR   acFileName[256] = { 0 };

    if (uMsg != TPM_EXECUTE) {
        return TPR_NOT_HANDLED;
    }
    
    if (!gpszDirName)
    {
        wcscat_s( acDirName, _countof(acDirName), IOBVT_DIR);
    }
    else
    {
        wcscat_s( acDirName, _countof(acDirName), gpszDirName);
    }
    if (!gpszFileName)
    {
        wcscat_s( acFileName, _countof(acFileName), IOBVT_FILE);
    }
    else
    {
        wcscat_s( acFileName, _countof(acFileName), gpszFileName);
    }

    hTest = StartTest( TEXT("Base File I/O Test"));
    
    hStep = BEGINSTEP( hTest, hStep, TEXT("Check if dir can be created"));

    CHECKTRUE( BaseCreateDirectory(acDirName    ));

    ENDSTEP( hTest, hStep);

    hStep = BEGINSTEP( hTest, hStep, TEXT("Check if file can be created , if so, close the created file"));

    CHECKTRUE( BaseCreateCloseFile(acFileName));

    ENDSTEP( hTest, hStep);

    hStep = BEGINSTEP( hTest, hStep, TEXT("Perform actions on the file"));

    CHECKTRUE( BaseOpenReadWriteClose(acFileName));

    ENDSTEP( hTest, hStep);

    hStep = BEGINSTEP( hTest, hStep, TEXT("Check if the file can be deleted"));

    CHECKTRUE( BaseDeleteFile(acFileName));

    ENDSTEP( hTest, hStep);


    hStep = BEGINSTEP( hTest, hStep, TEXT("Check if dir can be deleted"));

    CHECKTRUE( BaseDeleteDirectory(acDirName ));

    ENDSTEP( hTest, hStep);

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}







/*****************************************************************************
*
*   Name    : BaseCreateDirectory (pszDirPath)
*
*   Purpose : This function calls the API CreateDirectory which creates a
*             directory.
*
*    Input  : pszDirPath - Pathname of the directory to be created.
*             and variation number
*
*   Exit    : none
*
*   Calls   : CreateDirectory
*
*
*
*****************************************************************************/

BOOL BaseCreateDirectory(LPTSTR lpDirPathName)
{

    BOOL         bActualResult = FALSE;

    bActualResult  = YCreateDirectory ( lpDirPathName,
                                        (LPSECURITY_ATTRIBUTES) NULL);
    if (!bActualResult)  {
        if (ERROR_ALREADY_EXISTS == GetLastError())  {
            LogDetail( TEXT("Directory Already Exists ... Continuing with test"));
            bActualResult = TRUE;
        }
    }
    return( bActualResult);
    
}


/*****************************************************************************
*
*   Name    : BaseCreateCloseFile (pszPath)
*
*   Purpose : This function calls the API CreateFile which creates a
*             new file and then close the file handle
*
*    Input  : pszPath - Pathname of the file to be created.
*             and variation number
*
*   Exit    : none
*
*   Calls   : CreateFile
*
*
*
*****************************************************************************/

BOOL BaseCreateCloseFile(LPTSTR lpPathName)
{
    HANDLE    hFile = NULL;
    DWORD     dwDesiredAccess=0, dwShareMode=0, dwFileAttributes=0;
    BOOL      bRc=TRUE;


    dwDesiredAccess = MY_READ_WRITE_ACCESS;
    dwShareMode     = SHARE_ALL;
    dwFileAttributes= FILE_ATTRIBUTE_NORMAL;

    hFile = YCreateFile( lpPathName,
                         dwDesiredAccess,
                         dwShareMode,
                         (LPSECURITY_ATTRIBUTES)NULL,
                         CREATE_ALWAYS,
                         dwFileAttributes,
                         (HANDLE)NULL);

    if (hFile != BAD_FILE_HANDLE)  {
          bRc = YCloseHandle (hFile);
    }

    return((hFile != BAD_FILE_HANDLE) && bRc);
}



/*****************************************************************************
*
*   Name    : BaseDeleteDirectory (pszDirPathName)
*
*   Purpose : This function calls the API RemoveDirectory which deletes a
*             directory.
*
*   Input   : pszDirPathName - Pathname of the directory to be deleted
*
*
*
*   Exit    : none
*
*   Calls   : RemoveDirectory
*
*
*
*****************************************************************************/

BOOL BaseDeleteDirectory(LPTSTR lpDirPathName)
{
    BOOL         bActualResult = FALSE;

    bActualResult  = YRemoveDirectory ( lpDirPathName);

    return( bActualResult);
}


/*****************************************************************************
*
*   Name    : BaseDeleteFile
*
*   Purpose : Delete an existing file
*
*   Entry   : Variation number, and pathname of the file to be deleted.
*
*   Exit    : none
*
*   Calls   : DeleteFile
*
*
*****************************************************************************/



BOOL BaseDeleteFile(LPTSTR lpPathName)
{

    BOOL         bActualResult = FALSE;

    bActualResult  = YDeleteFile ( lpPathName);

    return( bActualResult);
}



/*****************************************************************************
*
*   Name    : BaseOpenReadWriteClose
*
*   Purpose : Open an existing file
*             Write predefined data into it
*             Set file pointer to begining of file
*             Read the data from this file
*             Compare and check if data is same as the one written
*             Set file pointer again, back to begining of file
*             Close the file
*
*
*
*   Entry   : Variation number, and pathname of the file on which to do Rd Wr
*
*   Exit    : none
*
*   Calls   : OpenExistingFile,ReadWriteFile,CloseHandle
*
*
*****************************************************************************/



BOOL BaseOpenReadWriteClose(LPTSTR lpFilename)
{

    BOOL   bResult = FALSE;
    HANDLE hFileHandle = NULL;

    bResult = OpenExistingFile(lpFilename, &hFileHandle);

    if (bResult)  {
        bResult  = ReadWriteFile( hFileHandle, READ_WRITE_BUF);
    }

    if (bResult)  {
        bResult = YCloseHandle(hFileHandle);
    }
    
    return( bResult);
}


/*****************************************************************************
*
*   Name    : OpenExistingFile
*
*   Purpose : Open an existing file
*             and store the handle of this open file into phandle arg
*
*
*   Entry   : pathname of the file to be opened, and address at which the
*             handle to this file should be stored.
*
*   Exit    : TRUE if file opened, and FALSE if something goes wrong
*
*   Calls   : CreateFile with OPEN_EXISTING disposition
*
*
*****************************************************************************/




BOOL    OpenExistingFile(LPTSTR lpFilename,HANDLE *phFileHandle)
{

     *phFileHandle = YCreateFile( lpFilename,
                                 GENERIC_READ|GENERIC_WRITE,//dwDesiredAccess,
                                 SHARE_ALL,               //dwShareMode,
                                 (LPSECURITY_ATTRIBUTES)NULL, //lpsec
                                 OPEN_EXISTING,          //
                                 FILE_ATTRIBUTE_NORMAL, //dwFileAttributes,
                                 (HANDLE)NULL);

        
    return (*phFileHandle != BAD_FILE_HANDLE);
}


/*****************************************************************************
*
*   Name    : ReadWriteFile
*
*   Purpose : Set write buffer with pre defined data
*             write nCount bytes into the open file
*             set file pointer to the begining of this file
*             Read nCount bytes into read buffer
*             compare the 2 buffer contents for nCount bytes
*             Set file pointer back to the begining of the file
*
*
*
*
*   Entry   : handle of the open file with read/write access
*             number of bytes to be written and read from this file.
*
*
*   Exit    : TRUE if all these operations go fine,else FALSE if something goes wrong
*
*   Calls   : SetWriteBuffer,WriteFile,SetFilePointer,ReadFile,strcmpn
*
*
*****************************************************************************/



BOOL ReadWriteFile (HANDLE  hFileHandle,
                    DWORD   nNumberOfBytes)
{
    DWORD dwNumOfBytesRead = 0;
    BYTE  aszWBuffer[READ_WRITE_BUF] = { 0 };
    BYTE  aszRBuffer[READ_WRITE_BUF] = { 0 };  
    DWORD dwActualResult = 0, nCount = 0, dwNumOfBytesWritten = 0;
    int   nRc = 0;
    BOOL  brc = FALSE;


    nCount = nNumberOfBytes;        // store no of bytes in temp variable

    // write operation into the file; setup write buffer

    SetWriteBuffer ( aszWBuffer, sizeof (aszWBuffer), 0L);

    while (nCount > READ_WRITE_BUF) {

        brc = YWriteFile ( hFileHandle,
                           aszWBuffer,
                           READ_WRITE_BUF,
                           &dwNumOfBytesWritten,
                           NULL);


        if ((dwNumOfBytesWritten == 0) || (!brc))
          return (FALSE);

        nCount -= READ_WRITE_BUF;            // dec no of bytes
    }
    brc = YWriteFile( hFileHandle,
                      aszWBuffer,
                      nCount,
                      &dwNumOfBytesWritten,
                      NULL);


    if ((dwNumOfBytesWritten == 0) || (!brc))
      return (FALSE);


    dwActualResult = YSetFilePointer( hFileHandle,
                                      (LONG)0,
                                      (PLONG)NULL,
                                      FILE_BEGIN);

    if ((dwActualResult == -1))  {
         return (FALSE);
    }

    nCount = nNumberOfBytes;     // store no of bytes in temp variable

    while (nCount > READ_WRITE_BUF) {
        brc = YReadFile( hFileHandle,
                         aszRBuffer,
                         READ_WRITE_BUF,
                         &dwNumOfBytesRead,
                         NULL);

       
        if ((dwNumOfBytesRead == 0) || (!brc))
            return (FALSE);

        nRc = memcmp ( aszRBuffer, aszWBuffer, READ_WRITE_BUF);
        if (nRc != 0) {
            return (FALSE);
        }
        nCount -= READ_WRITE_BUF;        // decrement no of bytes
    }

    brc = YReadFile ( hFileHandle,
                      (LPVOID)aszRBuffer,
                      nCount,
                      &dwNumOfBytesRead,
                      NULL);

    if ((dwNumOfBytesRead == 0) || (!brc))
        return (FALSE);



     nRc = memcmp( aszRBuffer, aszWBuffer, nCount);
     if (nRc != 0)  {
        return (FALSE);
    }

    dwActualResult = SetFilePointer ( hFileHandle,
                                      (LONG)0,
                                      (PLONG)NULL,
                                      FILE_BEGIN);

         
    return ( dwActualResult == 0);       // everything worked fine and as expected
}


/*****************************************************************************
*
*   Name    : SetWriteBuffer
*
*   Purpose : Writes predefined data into a buffer that is passed to this
*           routine.  It appends a null terminated character at the end
*
*   Entry   : lpBuf - a buffer in which to fill up the data
*           dwBufSize - buffer size
*
*   Exit    : None
*
*   Calls   : None
*
****************************************************************************/

BOOL SetWriteBuffer (LPBYTE lpBuf, DWORD dwBufSize, DWORD offset )
{
    DWORD dwIndex = 0;

    __try  {
        while ( dwIndex < dwBufSize)
           *lpBuf++ = (CHAR)( 'a' + ( ( dwIndex++ + offset ) % 26));
    } __except( EXCEPTION_EXECUTE_HANDLER)  {
        return( FALSE);
    }
    return( TRUE);
    
}
