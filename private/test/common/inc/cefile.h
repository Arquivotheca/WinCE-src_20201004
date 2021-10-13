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
//
//
// Module Name:
//
//      CEFile.cpp
//
// Abstract:
//
//      This module provides a simple C++ class to abstract file access
//      from CE devices.  Files can be accessed directly or forced to be
//      made local.
//
// Author:
//
//      Jonas Keating (jonask) April 5, 2001
//
//
//------------------------------------------------------------------------

#pragma once

#ifndef CEFILE_h
#define CEFILE_h

// @doc QAUTILS

//------------------------------------------------------------------------
// Define functions as import when building CEFile, and as export when this file
// is included by all other applications. We only use CEFILEAPI on C++ classes.
// For straight C functions, our DEF file will take care of exporting them.
//------------------------------------------------------------------------

//#ifndef CEFILEAPI
//   #define CEFILEAPI __declspec(dllimport)
//#endif

//------------------------------------------------------------------------
// Define EXTERN_C
//------------------------------------------------------------------------
#ifndef EXTERN_C
   #ifdef __cplusplus
      #define EXTERN_C extern "C"
   #else
      #define EXTERN_C
   #endif
#endif

//------------------------------------------------------------------------
// Specify 32 bit pack size to ensure everyone creates the correct size objects
//------------------------------------------------------------------------
#pragma pack(push)
#pragma pack(4)

//------------------------------------------------------------------------
// Actions for "Access"ing a file.
//------------------------------------------------------------------------
// Action Flags
#define AF_OPEN         0x0001
#define AF_MAKELOCAL    0x0002
#define AF_MAKEVISIBLE  0x0004
// Behavior flags
#define AF_NOCESHNET    0x0100
#define AF_VERBOSE      0x0200
#define AF_NODELETE     0x0400
#define AF_BEHAVE_FLAGS 0xFF00

//------------------------------------------------------------------------
// Types
//------------------------------------------------------------------------
DECLARE_HANDLE(HCEFILE);

//------------------------------------------------------------------------
// APIs for C interface (C++ applications should use the CEFile class)
//------------------------------------------------------------------------

// File creation and destruction
EXTERN_C HCEFILE        WINAPI  CEFileAccess(DWORD dwAction, const TCHAR *szSearchPath, const TCHAR *szFileName, const TCHAR *szAttrib);
EXTERN_C BOOL           WINAPI  CEFileRelease(HCEFILE hFile);

// File data manipulation
EXTERN_C DWORD          WINAPI  CEFileRead(HCEFILE hFile, BYTE *pbData, UINT cBytes);
EXTERN_C DWORD          WINAPI  CEFileWrite(HCEFILE hFile, BYTE *pbData, UINT cBytes);
EXTERN_C DWORD          WINAPI  CEFileReadLine(HCEFILE hFile, TCHAR *pszLine, UINT cChars);
EXTERN_C DWORD          WINAPI  CEFileWriteLine(HCEFILE hFile, const TCHAR *pszFormat, ...);
EXTERN_C DWORD          WINAPI  CEFileSeek(HCEFILE hFile, long lOffset, int nOrigin);
EXTERN_C const TCHAR*   WINAPI  CEFileGetName(HCEFILE hFile);


#ifdef __cplusplus
//------------------------------------------------------------------------
// CEFile - Interface for C++ applications
//------------------------------------------------------------------------

/*
//------------------------------------------------------------------------
@class This class provides generic file access to a file independent of the
source location or type of the file.

@comm The goal of this module is to abstract the details of file access
away from the host application.

Goals of the CEFile class:<nl>
1)  To abstract file access for both text and binary files away from the host 
    application, hiding the details of the underlying technology used (CESH, 
    network, local file, etc.)<nl>
2)  To provide a set file access operations which will always provide basic 
    file I/O operations while transparently adapting to use whatever functionality 
    is available on the current host system.<nl>
3)  Provide basic path searching for files.<nl>
4)  Provide basic formatted string input and output.<nl>
5)  Provide this functionality to both C and C++ clients.

Assumptions:<nl>
1)  The CESH file access operations will always be available (U_ropen, U_rread, 
    etc.).<nl>
//------------------------------------------------------------------------
*/

class CEFile
{
    // @access Public Members
public:
    // @cmember Constructor
    CEFile(void);
    // @cmember Destructor
    ~CEFile(void);

    // @cmember Main file creation function.  Tells the object to search for the
    // desirect file, and either copy it or open it as desired.
    HRESULT WINAPI  Access(DWORD dwAction, const TCHAR *szSearchPath, const TCHAR *szFileName, const TCHAR *szAttrib);
    // @cmember Indicates that the file will no longer be used.
    HRESULT WINAPI  Release(void);

    // @cmember Read the given number of bytes from the file into the buffer.
    DWORD   WINAPI  Read(BYTE *pbData, UINT cBytes);
    // @cmember Write the given number of bytest from the buffer to the file.
    DWORD   WINAPI  Write(BYTE *pbData, UINT cBytes);
    // @cmember Reads a line of unicode text from the file into the buffer.
    // Assumes eol characters in the file.
    DWORD   WINAPI  ReadLine(TCHAR *pszLine, UINT cChars);
    // @cmember Writes a line of formatted text to the file.
    DWORD   WINAPIV WriteLine(const TCHAR *pszFormat, ...);

    // @cmember Seeks to the given offset within the file.
    DWORD   WINAPI  Seek(long lOffset, int nOrigin);

    // @cmember Retrieve the name of the currently accessed file.
    const TCHAR* WINAPI GetName(void);

private:
    // Different supported file sources
    enum CEFILETYPE { ftUnknown, ftCESH, ftLocal, ftNetwork, ftCount };
    static const TCHAR *m_rgKeyPrefix[ftCount];
    TCHAR m_szFileName[_MAX_PATH];
    CEFILETYPE m_filetype;
    HANDLE m_hFile;
    DWORD m_dwAction;
    DWORD m_dwBehavior;
        
    void Init(void);
    CEFILETYPE DetectFileType(TCHAR *szKeyword, DWORD *pdwCount);
    HRESULT Open(DWORD dwAction, const TCHAR *szAttrib);
    HRESULT Close(void);
    DWORD GetFileMode(const TCHAR *szMode, DWORD dwRead, DWORD dwWrite, DWORD dwAppend);
    DWORD WriteLineV(const TCHAR *pszFormat, va_list pArgs);

// Friend functions (necessary since the WriteLineV method is private).
    friend DWORD WINAPI CEFileWriteLine(HCEFILE hFile, const TCHAR *pszFormat, ...);
};

#endif // __cplusplus

#pragma pack(pop) // restore packing size to previous state

/*************************************************************************
* @comm For working examples of usage of the CEFile Library, including
* the C-Language wrappers and correct link lines, review the CECopy
* and ScreenCap utilities under %_PRIVATEROOT%\\test\\tools.
*/

/*************************************************************************
* @ex This is an example of basic file reading.  It doesn't know where
* the config file is going to come from, but it knows that it is
* either in the image (local), in the flatrelease directory (cesh),
* or on the given network share.
* It doesn't care what the source is -- it only needs to be able
* to read lines of text from the file. |
Test()
{
    const TCHAR szSearchPath = TEXT("cesh;local;\\cedxmedia\mediafiles");
    CFile ConfigFile;

    // Only ask to open the file, don't force it to be local.  This
    // searches the entire given path, including across CESH, the local
    // windows directory, and the given network share, and opens the
    // file directly from wherever it is found.  This means
    // that our returned file handle can be used with our file read
    // and write functions, but that they may be using cesh or going 
    // across the netowork under the hood.
    if (SUCCEEDED(ConfigFile.Access(AF_OPEN, szSearchPath, TEXT("config.ini"), TEXT("r"))))
    {
        TCHAR szBuffer[256];
        DWORD dwRead;
        
        // Read a unicode line of text
        while (0 != (dwRead = ConfigFile.ReadLine(szBuffer, 256)))
        {
            // Do something useful with the line of text
            ParseCfgLine(szBuffer);
        }
        
        // This is unnecessary, since the destructor will force a close
        // of the file.
        //ConfigFile.Release();
    }
    else
    {
        <error>
    }
}
*/

/*************************************************************************
* @ex This uses AF_MAKELOCAL to ensure that a file is made available on the
* the local machine, but it does NOT open the file.
* It searches a number of network shares and the cd-rom drive to find
* the file. |
Test()
{
    const TCHAR szSearchPath = TEXT("\\cedxmedia\mediafiles;")
                               TEXT("\\cedxmedia\wmttestmedia;")
                               TEXT("cdrom");
    CFile MediaFile;
    
    // Only ask to make the file local, do not open it.  This means
    // that 
    if (SUCCEEDED(MediaFile.Access(AF_MAKELOCAL, szSearchPath, TEXT("audio\132_16bit_stereo_signed_byte_swapped.au"), TEXT("r"))))
    {
        // Allow DirectShow to create the FilterGraph for this media file
        hr = g_pGB->RenderFile(MediaFile.GetName(), NULL);
        if (FAILED(hr))
        {
            <error>
        }
        ...

        // Because we asked for "AF_MAKELOCAL" when we called "Access",
        // calling "Release" here will actually delete the temporary
        // file, rendering it unusable.  Subsequent calls to "GetName"
        // will return an empty string.
        MediaFile.Release();
    }
    else
    {
        <error>
    }
}
*/

/*************************************************************************
* @ex This is typical example of reading of a local data file.  It does not 
* matter if the original file was located in the image, on a network share,
* or across CESH.  The file is copied to the local machine and then opened
* for read.  It could be opened for write as well, but that would only
* change the local file instance, and any changes made would not be
* reflected in the original file. |
Test()
{
    CFile ImageFile;

    // Open a file for reading, and make sure that it's put on
    // the local machine for speed.
    if (SUCCEEDED(ImageFile.Access(AF_MAKELOCAL | AF_OPEN, TEXT("local;cesh"), TEXT("image.bmp"), TEXT("r"))))
    {
        ...
        // Read the data we care about
        ImageFile.Read((BYTE*)&bfh, sizeof(bfh));
        ImageFile.Read((BYTE*)&bmi, sizeof(bmi));
        DWORD dwImageDataSize = bmi.bmiHeader.biWidth * bmi.bmiHeader.biHeight * (bmi.bmiHeader.biBitCount / 8);
        ...
        ImageFile.Read((BYTE*)lpbBits, dwImageDataSize);
        ...

        // Release the file -- this closes the file and, if it was copied
        // across CESH, deletes it from the local machine.
        ImageFile.Release();
    }
    else
    {
        <error>
    }
}
*/

/*************************************************************************
* @ex Simple example of writing a data file to the host's FLATRELEASEDIR |
Test()
{
    CFile ImageFile;

    <...Get bitmap data...>

    // Open a file for write across cesh
    if (SUCCEEDED(ImageFile.Access(AF_OPEN, TEXT("cesh"), TEXT("screen.bmp"), TEXT("w"))))
    {
        // Save the data
        ImageFile.Write((BYTE*)&bfh, sizeof(bfh));
        ImageFile.Write((BYTE*)&bmi, sizeof(bmi));
        ImageFile.Write((BYTE*)lpbBits, dwImageDataSize);

        // Release the file
        ImageFile.Release();
    }
    else
    {
        <error>
    }
}
*/

/*************************************************************************
* Example of copying a file to the local machine and then repeately
* opening, reading, and closing it.
* Opens a local file, reads a record from it, and then closes the file.
* The truth is that since we know that the file is local we could directly
* use the Windows file APIs. |
ReadRecord(TCHAR szLocalFile, MY_RECORD *prec)
{
    CFile LocalFile;

    // Open the local file.  No search path necessary, since we know
    // that we've got a fully-qualified path to a file on the local
    // machine.
    if (SUCCEEDED(LocalFile(AF_OPEN, NULL, szLocalFile, TEXT("rw"))))
    {
        // Read the file record
        LocalFile.Read(prec, sizeof(*prec));

        // Close the local file
        LocalFile.Release();
    }
}
*/

/*************************************************************************
* @ex Creates a local copy of a file (from either the network or CESH)
* and then uses that local copy to fill an array of records. |
Test()
{
    CFile LocalFile;

    // Get a local copy of the file, either across CESH or from our
    // network share.
    if (SUCCEEDED(LocalFile.Access(AF_MAKELOCAL, TEXT("cesh;\\myserver\myshare"), TEXT("myrecord.bin"), TEXT("r"))))
    {
        MY_RECORD rgRec[10];

        // Read the file into each of the records (OK, it makes more
        // sense to read into one record and then just copy the
        // records, but I'm trying to illustrate a point here.  :-)
        // Any changes this subroutine makes to the file will
        // remain until we release our handle to the file.
        for (int i = 0; i < 10; i++)
        {
            ReadRecord(LocalFile.GetName(), &rgRec[i]);
        }

        // Delete the local file
        LocalFile.Release();
    }
    else
    {
        <error>
    }
}
*/

/*************************************************************************
* @ex Example using the C-language wrapper functions instead of the
* CEFile class itself.  Basic code to copy a file. |
Test()
{
    HCEFILE fileSrc = NULL, fileDest = NULL;
    
    if (NULL != (fileSrc = CEFileAccess(AF_OPEN, szSrcPath, szSrc, TEXT("r"))))
    {
        if (NULL != (fileDest = CEFileAccess(AF_OPEN, szDestPath, szDest, TEXT("w"))))
        {
            BYTE rgData[1024];
            DWORD dwRead;
            
            DebugOut(TEXT("Copying \"%s\" (\"%s\") to \"%s\" (\"%s\")"), szSrc, CEFileGetName(fileSrc), szDest, CEFileGetName(fileDest));

            while (0 != (dwRead = CEFileRead(fileSrc, rgData, countof(rgData))))
            {
                DWORD dwWrote;
                dwWrote = CEFileWrite(fileDest, rgData, dwRead);
                if (dwRead != dwWrote) DebugOut(TEXT("Mismatched Read/Write sizes"));
            }
            DebugOut(TEXT("Copy finished"));
            
            // Close the destination file
            CEFileRelease(fileDest);
            fileDest = NULL;
        }
        else
        {
            DebugOut(TEXT("Unable to open destination file \"%s\""), szDest);
        }
        // Close the source file
        CEFileRelease(fileSrc);
        fileSrc = NULL;
    }
    else
    {
        DebugOut(TEXT("Unable to find source file \"%s\""), szSrc);
    }
}
*/


#endif // CEFILE_h
