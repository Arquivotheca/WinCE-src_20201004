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

#pragma once
#include <string>
#include "auto_xxx.hxx"

namespace FileUtils
{
    /**
     * Checks whether a file exists or not
     * @param file The filename to check existence of
     * @return true if exists, false otherwise
     */
    bool FileExists(LPCWSTR file);

    /**
     * Checks whether a directory exists or not
     * @param dir The directory to check existence of
     * @return true if exists, false otherwise
     */
    bool DirectoryExists(LPCWSTR dir);

    /**
     * Dumps binary data to a file.
     * Creates a new file if it doesn't exist; clobbers existing file if
     * it does exist
     * @param file The filename to dump to
     * @param bits The bits to dump
     * @param count The number of bytes pointed to by bits to dump
     * @return true upon success (i.e., all bits written), false otherwise
     */
    bool DumpBitsToFile(LPCWSTR file, LPBYTE bits, DWORD count);

    /**
     * Copies a file from a URL or UNC path to local directory.
     * @param filename The filename to copy locally--must begin
                       with: ('http://', 'file://', or '\\').
       @param destDir The local directory to store the file in.
       @param threadSpecific Whether to include the thread id
                             as part of the resultant filename or not.
       @return The resultant filename.
    */
    std::wstring CopyLocal(LPCWSTR filename, LPCWSTR destDir,
        bool threadSpecific = false);
};

namespace ce
{
    typedef ce::auto_xxx<LPCWSTR, BOOL (__stdcall*)(LPCWSTR), DeleteFile, NULL> temp_file;
}