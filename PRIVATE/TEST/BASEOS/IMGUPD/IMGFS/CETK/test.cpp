//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
////////////////////////////////////////////////////////////////////////////////
//
//  TUXTEST TUX DLL
//
//  Module: test.cpp
//          Contains the test functions.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"

// returns:

// TPR_FAIL if LOG_FAIL has occurred during the current test case

// TPR_ABORT if LOG_ABORT has occurred during the current test case

// TPR_SKIP if LOG_NOT_IMPLEMENTED has occurred during the current test case

// TPR_PASS otherwise

#define GetTestResult(pkato) \
    ((pkato)->GetVerbosityCount(LOG_ABORT) ? TPR_ABORT : \
     (pkato)->GetVerbosityCount(LOG_FAIL) ? TPR_FAIL : \
     (pkato)->GetVerbosityCount(LOG_NOT_IMPLEMENTED) ? TPR_SKIP : \
     (pkato)->GetVerbosityCount(LOG_SKIP) ? TPR_SKIP : \
     TPR_PASS)


/*
* memory map a PE ROM file.  This sets up the mapping and reads the
* corresponding values into the peromFileInfo data structure.  The
* file handles are returned as well so that the callee can close them
* when done with the mapping.
*
* First arg is the data structure to fill, second arg is the filename
* of the file to open, the third arg is the file handle, and the fourth
* are is the memory mapped file handle.
*
* Return true on success and false on failure.  Prints out messages to
* stderr on failure.
*/
BOOL memoryMapPEROM (sPEROMFileInfo * peromFileInfo, TCHAR szFileName[],
                     HANDLE & hExecutableFile, HANDLE & hMappedExecutableFile)
{
    void * mappedExe;

    ASSERT (peromFileInfo != NULL);
    ASSERT (szFileName != NULL);

    /*
    * for the returnval, assume success until proven otherwise.
    */
    BOOL returnVal = TRUE;

    /*
    * the file handles assume failure until proven otherwise.  Since we
    * track failures through the value for the file handles, we need to
    * set these here.  We don't have a success value to use for assuming
    * success first, so we use failure.
    */
    hExecutableFile = INVALID_HANDLE_VALUE;
    hMappedExecutableFile = INVALID_HANDLE_VALUE;

    /*
    * The two modes might seem contradictory, but they fall out of the
    * program structure.  Since returnVal is sent to false only on error
    * condition (in if statements), it makes sense to assume true.  The
    * file handle will be set to valid values on success by the file functions,
    * so these need to be failure to begin with to find the failures.  Yes,
    * it isn't ideal, but it should work.
    */

    /*
    * open the file for reading
    */
    hExecutableFile = CreateFile(szFileName, GENERIC_READ, 0, 0,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (hExecutableFile == NULL)
    {
        g_pKato->Log(LOG_FAIL, L"Error:  Couldn't open file %s.", szFileName);
        returnVal = FALSE;
        goto cleanAndReturn;
    }
    //Debug(L"File %s Handle %d",szFileName,hExecutableFile);
    /*
    * create a file mapping
    * this returns null on error, not INVALID_HANDLE_VALUE
    * set to INVALID_HANDLE_VALUE on error to track errors
    */
    //#if 1
    hMappedExecutableFile =
        CreateFileMapping (hExecutableFile,
        NULL, PAGE_READONLY, 0, 0, NULL);

    if (hMappedExecutableFile == NULL)
    {
        g_pKato->Log(LOG_FAIL, L"Error:  Couldn't create mapping object for %s.\n",
            szFileName);

        hMappedExecutableFile = INVALID_HANDLE_VALUE;

        returnVal = FALSE;
        goto cleanAndReturn;
    }

    /*
    * map this file into the memory space
    */
    mappedExe = (void *)MapViewOfFile (hMappedExecutableFile, FILE_MAP_READ,
        0, 0, 0);

    /*
    * check for errors
    */
    if (mappedExe == NULL)
    {
        g_pKato->Log(LOG_FAIL, L"Error:  Couldn't memory map %s.\n",
            szFileName);
        returnVal = FALSE;
        goto cleanAndReturn;
    }

    /*
    * get the file size (this needs to be in bytes, not blocks)
    */
    peromFileInfo->fileLength = GetFileSize (hExecutableFile, NULL);

    if (peromFileInfo->fileLength == INVALID_FILE_SIZE)
    {
        g_pKato->Log(LOG_FAIL, L"Error:  Couldn't get size of %s.\n",
            szFileName);
        returnVal = FALSE;
        goto cleanAndReturn;
    }

    /*
    * relmerge header is the first thing in the file.  Find it and
    * check magic.
    */
    sRelmergeHeader * relmergeHeader = (sRelmergeHeader *)
        mappedExe;

    if (relmergeHeader->relmergeMagic != RELMERGE_MAGIC)
    {
        g_pKato->Log(LOG_FAIL, L"Error: memoryMapPEROM: "
            L"Relmerge magic number is incorrect.  0x%x != 0x%x\n",
            relmergeHeader->relmergeMagic, RELMERGE_MAGIC);
        returnVal = FALSE;
        goto cleanAndReturn;
    }

    /*
    * jump past the relmerge header and start ripping start ripping out
    * the values that we are going to need and placing them into the
    * data structures.
    */
    peromFileInfo->fileData = (void *) (mappedExe);

    peromFileInfo->fileHeader = (e32_rom *)
        (((byte *) peromFileInfo->fileData) + (sizeof (sRelmergeHeader)));

    peromFileInfo->numberOfSections = peromFileInfo->fileHeader->e32_objcnt;

    /*
    * jump past the main header into the objects.  We cast the void *
    * to a byte because I don't know how wide void * is.  byte is guaranteed
    * to do what I want to do.  sizeof should returns bytes, so this should
    * give us a memory address that rests on the beginning of the sections
    * area.
    */
    peromFileInfo->sectionHeaders = (o32_rom *)
        (((byte *) peromFileInfo->fileHeader) + sizeof (e32_rom));
    //#endif
cleanAndReturn:

    if (returnVal == FALSE)
    {

        if (hExecutableFile != INVALID_HANDLE_VALUE)
        {
            if (CloseHandle (hExecutableFile) == 0)
            {
                g_pKato->Log(LOG_FAIL, L"Error: Couldn't close handle.\n");
            }

            hExecutableFile = INVALID_HANDLE_VALUE;
        }

        if (hMappedExecutableFile != INVALID_HANDLE_VALUE)
        {
            if (CloseHandle (hMappedExecutableFile) == 0)
            {
                g_pKato->Log(LOG_FAIL, L"Error: Couldn't close handle.\n");
            }

            hMappedExecutableFile = INVALID_HANDLE_VALUE;
        }
    }
    else
    {
        /* these had better be valid at this point */
        ASSERT (hExecutableFile != INVALID_HANDLE_VALUE);
        ASSERT (hMappedExecutableFile != INVALID_HANDLE_VALUE);
    }

    return (returnVal);

}


/*
This function finds the file in the *_PACKAGE_FILES and returns the correct path for the file
including the file name. So the file foo.dll in OEM_PACKAGE_FILES would be returned as
\release\OEM_PACKAGE_FILE\foo.dll
going with the assumption that only one file exists per package.

This function always return the first match found
*/
void FindRelFile(TCHAR * filename,TCHAR* path)
{

    HANDLE hfile=NULL;
    HANDLE filehandle=NULL;
    WIN32_FIND_DATA wFindData;
    WIN32_FIND_DATA filedata;
    TCHAR szpath[MAX_PATH]={NULL};

    //standard parameter check
    if(NULL==filename)
    {
        path=NULL;

        goto cleanup;
    }

    hfile=FindFirstFile(ALL_PACKAGE_FILES,&wFindData);//find package_file dir
    if(INVALID_HANDLE_VALUE!=hfile)
    {
        do
        {
            if(wFindData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
            {
                //create the path for file
                _snwprintf(szpath,MAX_PATH,TEXT("%s\\%s\\%s"),RELEASEDIR,wFindData.cFileName,filename);
                szpath[MAX_PATH-1]=L'\0';
                //find the file
                filehandle = FindFirstFile(szpath,&filedata);
                if(INVALID_HANDLE_VALUE!=filehandle)
                {
                    //found the file return thepath
                    _tcsncpy(path,szpath,MAX_PATH); //TODO: should we add filename on to this?
                    goto cleanup;
                }

            }


        }while(FindNextFile(hfile,&wFindData));

    }

cleanup:
    if(hfile)
        CloseHandle(hfile);
    if(filehandle)
        CloseHandle(filehandle);


}



////////////////////////////////////////////////////////////////////////////////
// TestProc
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI GetE32O32Header(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    HANDLE hfile=NULL;
    HANDLE cfHandle=NULL;
    HANDLE peromHandle=NULL;
    HANDLE peromMHandle=NULL;
    HANDLE binHandle=NULL;
    //WIN32_FIND_DATA wFindData; probably not needed
    WIN32_FIND_DATA filedata;
    TCHAR szPath[MAX_PATH]={NULL};
    TCHAR relPath[MAX_PATH]={NULL};
    TCHAR binPath[MAX_PATH]={NULL};
    e32_rom e32;
    o32_lite *po32=NULL;
    sPEROMFileInfo  peromFileInfo;
    DWORD retval=0;
    Module *Dummymodule=NULL;
    //    Module *RelmergeModule;
    DWORD dwRead;
    DWORD dwRead2;
    BYTE *pTemp = NULL;
    int FilesMatched = 0;
    int FilesNotMatched = 0;
    int iTestCaseID;
    BOOL fGetXIPPages = TRUE;

    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }
    if(lpFTE->dwUniqueID != NULL)
        iTestCaseID = lpFTE->dwUniqueID;
    else
        iTestCaseID = 0;

    g_pKato->Log(LOG_DETAIL,TEXT("Test message %d, TestcaseID %d"),uMsg,iTestCaseID);
    if(iTestCaseID == E32O32_testID)
        fGetXIPPages = FALSE;
    if(iTestCaseID == XIP_testID)
        fGetXIPPages = TRUE;

    //Find modules Extract e32Header

    hfile=FindFirstFile(ALL_IMGFS_FILES,&filedata);

    if(INVALID_HANDLE_VALUE!=hfile)
    {
        //PrintModuleInfo(&filedata);
        //get all the other files. and compare the e32 header.
        do
        {
            //see if the file is a rom module
        if(FILE_ATTRIBUTE_ROMMODULE & filedata.dwFileAttributes)
        {
            bool Worked = true;
            //create the file name to open the file
            _snwprintf(szPath,MAX_PATH,TEXT("%s\\%s"),IMGFS,filedata.cFileName);
            szPath[MAX_PATH-1]=L'\0';
            cfHandle=CreateFile(szPath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if(INVALID_HANDLE_VALUE==cfHandle)
            {
                g_pKato->Log(LOG_FAIL,TEXT("CreateFile failed for %s at line %d"),szPath,__LINE__);
                goto cleanup;
            }
            //find the file in the release Directory
            FindRelFile(filedata.cFileName,relPath);
            //extract the information out of the relmerge output file.
            if((memoryMapPEROM( &peromFileInfo, relPath, peromHandle, peromMHandle))&&(peromHandle != INVALID_HANDLE_VALUE)) //TODO: use peromMHandle!
            {
                peromMHandle = peromHandle;
            } else
            {
                DWORD dwErrorCode = GetLastError();
                g_pKato->Log(LOG_FAIL, "Error mapping File.%d\n",dwErrorCode);
                goto cleanup1;
            }

            // get the flags for this file from the DSM - dwFlags
            Dummymodule = new Module(filedata.cFileName, 0);
            if(Dummymodule == NULL)
            {
                goto cleanup;
            }

                //get the e32 Header from IMGFS
            if(DeviceIoControl(cfHandle,IOCTL_BIN_GET_E32,NULL,0,&e32,sizeof(e32_rom),&retval,NULL))
            {
                Dummymodule->SetE32Ptr(&e32);
                Dummymodule->SetOriginalVBase(peromFileInfo.fileHeader->e32_vbase);

                //compare the e32 header with perom file e32 header.

                //g_pKato->Log(LOG_DETAIL,TEXT("Comparing e32 header of file %s line: %d"),szPath,__LINE__);
                if(peromFileInfo.fileHeader == NULL)
                {
                    g_pKato->Log(LOG_FAIL, TEXT("fileHeader is NULL for  file %s peromfile value = %d IMGFS value = %d at line %d\n"),relPath,peromFileInfo.fileHeader,&e32, __LINE__);
                    }
                if(e32.e32_objcnt != peromFileInfo.fileHeader->e32_objcnt)
                {
                    g_pKato->Log(LOG_FAIL, TEXT("e32_objcnt mismatch for file %s peromfile value = %d IMGFS value = %d at line %d\n"),relPath,peromFileInfo.fileHeader->e32_objcnt,e32.e32_objcnt, __LINE__);
                }
                if(e32.e32_imageflags != peromFileInfo.fileHeader->e32_imageflags)
                {
                    g_pKato->Log(LOG_DETAIL, TEXT("e32_image   mismatch for file %s peromfile value = %x IMGFS value = %x at line %d\n"),relPath,peromFileInfo.fileHeader->e32_imageflags,e32.e32_imageflags, __LINE__);
                                        }
                if(e32.e32_entryrva != peromFileInfo.fileHeader->e32_entryrva)
                {
                    g_pKato->Log(LOG_DETAIL, TEXT("e32_entryrva mismatch for file %s peromfile value = %x IMGFS value = %x at line %d\n"),relPath,peromFileInfo.fileHeader->e32_entryrva,e32.e32_entryrva, __LINE__);
                }
                    //if(e32.e32_vbase != peromFileInfo.fileHeader->e32_vbase)
                    //{
                    //    g_pKato->Log(LOG_DETAIL, TEXT("e32_vbase  mismatch for file %s peromfile value = %x IMGFS value = %x at line %d\n"),relPath,peromFileInfo.fileHeader->e32_vbase,e32.e32_vbase, __LINE__);
                    //    goto cleanup;  //Confirm with arogers
                    //}
                if( e32.e32_subsysmajor != peromFileInfo.fileHeader->e32_subsysmajor)
                {
                    g_pKato->Log(LOG_FAIL, TEXT("e32_subsysmajor mismatch for file %s peromfile value = %x IMGFS value = %x at line %d\n"),relPath,peromFileInfo.fileHeader->e32_subsysmajor,e32.e32_subsysmajor, __LINE__);
                                        }
                    if(e32.e32_subsysminor != peromFileInfo.fileHeader->e32_subsysminor)
                {
                    g_pKato->Log(LOG_FAIL, TEXT("e32_subsysminor mismatch for file %s peromfile value = %x IMGFS value = %x at line %d\n"),relPath,peromFileInfo.fileHeader->e32_subsysminor,e32.e32_subsysminor, __LINE__);
                                        }
                if(e32.e32_stackmax != peromFileInfo.fileHeader->e32_stackmax)
                {
                    g_pKato->Log(LOG_FAIL, TEXT("e32_stackmax mismatch for file %s peromfile value = %x IMGFS value = %x at line %d\n"),relPath,peromFileInfo.fileHeader->e32_stackmax,e32.e32_stackmax, __LINE__);
                }
                if(e32.e32_vsize != peromFileInfo.fileHeader->e32_vsize)
                {
                        g_pKato->Log(LOG_DETAIL, TEXT("e32_vsize mismatch for file %s peromfile value = %x IMGFS value = %x at line %d\n"),relPath,peromFileInfo.fileHeader->e32_vsize,e32.e32_vsize, __LINE__);
                                            }
                }//end deviceiocontrol E32


                //get the o32Headers.
                po32=new o32_lite[e32.e32_objcnt];
                if(po32 == NULL)
                {
                    DWORD dwErrorCode = GetLastError();
                    g_pKato->Log(LOG_FAIL,L"could not allocate memory for O32 Headers in line %d, GetLastError() returned  %d ",__LINE__,dwErrorCode);
                    goto cleanup;
                    //g_pKato->Log(LOG_FAIL, "Error Reading File.%d\n",dwErrorCode);
                }
                else
                {
                    if (DeviceIoControl( cfHandle, IOCTL_BIN_GET_O32, NULL, 0, po32, sizeof(o32_lite) * e32.e32_objcnt, &retval, NULL))
                    {
                        //setup the module object
                    for (INT sec=0;sec<e32.e32_objcnt;sec++)
                    {
                        //OutputDebugString(L"setup the module object");
                        Section* psection;
                        o32_rom tempo32;

                        // create a new section
                            Dummymodule->AddSection((PHANDLE)&psection);

                        //copy o32_lite to o32_rom
                         tempo32.o32_vsize=po32[sec].o32_vsize;
                         tempo32.o32_rva=po32[sec].o32_rva;
                         tempo32.o32_psize=po32[sec].o32_psize;
                         tempo32.o32_dataptr=po32[sec].o32_dataptr;
                         tempo32.o32_realaddr=po32[sec].o32_realaddr;
                         tempo32.o32_flags=po32[sec].o32_flags;

                        //set the o32 Header
                        psection->SetO32Header(&tempo32);

                        //special Case the creloc
                        if (po32[sec].o32_rva == Dummymodule->GetE32Ptr()->e32_unit[FIX].rva)
                        {
                            Dummymodule->SetCrelocPtr (psection);
                            pTemp = new BYTE[po32[sec].o32_psize];
                            if(NULL==pTemp)
                            {
                                DWORD dwErrorCode = GetLastError();
                             g_pKato->Log(LOG_FAIL,L"could not allocate memory for Dummy Module in line %d, GetLastError() returned  %d ",__LINE__,dwErrorCode);
                             goto cleanup;
                            }
                            ReadFileWithSeek( cfHandle, pTemp, po32[sec].o32_psize, &dwRead, NULL,  po32[sec].o32_dataptr, 0);
                            psection->SetDataPtr(pTemp, po32[sec].o32_psize, FALSE);
                        }


                        }//end for

                        //buffer to keep the data for readfilewithseek
                        BYTE *pBuffer = (BYTE*)LocalAlloc(LPTR, 4096);
                        BYTE *pBuffer2 = (BYTE*)LocalAlloc(LPTR, 4096);
                        //BYTE *pBuffer = new BYTE[4096];
                        //BYTE *pBuffer2 = new BYTE[4096];

                        //for ever section get the data page by page unfix it up and compare it with the same file data.
                        if(e32.e32_objcnt != peromFileInfo.numberOfSections)
                        {
                            g_pKato->Log(LOG_FAIL, L"Section Count doesn't match.");
                         goto cleanup1;
                         }
                        for (INT i=0;i<e32.e32_objcnt;i++)
                        {
                        if (pBuffer && pBuffer2)
                        {
                            if(po32[i].o32_vsize != peromFileInfo.sectionHeaders[i].o32_vsize)
                            {
                                g_pKato->Log(LOG_FAIL, TEXT("o32_vsize of sections not the same size %d, %d"),po32[i].o32_vsize,peromFileInfo.sectionHeaders[i].o32_vsize);
                                VERIFY(NULL == LocalFree((HLOCAL)pBuffer));
                                VERIFY(NULL == LocalFree((HLOCAL)pBuffer2));
                                goto cleanup1;
                            }
                            if (fGetXIPPages == TRUE){
                                if(
                                    (e32.e32_imageflags & IMAGE_FILE_XIP)
                                    && !(po32[i].o32_flags & IMAGE_SCN_MEM_WRITE)
                                    && !(po32[i].o32_flags & IMAGE_SCN_COMPRESSED)
                                    ) {
                                        DWORD cbRet = 0;
                                        LPDWORD pdwXIPPageList = NULL;
                                        int j;
                                        g_pKato->Log(LOG_DETAIL,L"Testing XIP");
                                        if (!DeviceIoControl(cfHandle, IOCTL_BIN_GET_XIP_PAGES, &j, sizeof(DWORD), NULL, 0, &cbRet, NULL)) {
                                            g_pKato->Log(LOG_FAIL,L"        failed to get XIP page count!\n");
                                            Worked = false;
                                        }
                                        else {
                                            // got XIP page count
                                            g_pKato->Log(LOG_DETAIL,L"        this section supports XIP, %u pages\n", cbRet / sizeof(DWORD));
                                            pdwXIPPageList = (LPDWORD)LocalAlloc(LPTR, cbRet);
                                            //BYTE *pBuffer = (BYTE*)LocalAlloc(LPTR, 4096);
                                            VOID *pDataPage;
                                            BOOL fKMode = SetKMode(TRUE);
                                            if (pdwXIPPageList && pBuffer) {
                                                // get XIP page statically mapped virtual addresses
                                                if(!DeviceIoControl(cfHandle, IOCTL_BIN_GET_XIP_PAGES, &j, sizeof(DWORD), pdwXIPPageList, cbRet, NULL, NULL)) {
                                                    Debug(L"        failed to get XIP pages!\n");
                                                } else {
                                                    for( DWORD k=0; k < cbRet/sizeof(DWORD); k++) {
                                                        g_pKato->Log(LOG_DETAIL,L"            page %08u --> %08X\n", k, pdwXIPPageList[k]);
                                                        if (!ReadFileWithSeek(cfHandle, pBuffer, 4096, &dwRead, NULL, po32[j].o32_dataptr + (k * 4096), 0)) {
                                                            ASSERT(0);
                                                            break;
                                                        }
                                                        pDataPage = (VOID*)pdwXIPPageList[k];
                                                        if (!IsBadPtr(VERIFY_EXECUTE_FLAG | VERIFY_READ_FLAG, (BYTE*)pDataPage, 4096)) {
                                                            if (0 != memcmp(pBuffer, pDataPage, dwRead)) {
                                                                g_pKato->Log(LOG_FAIL,L"***** XIP Blocks mismatch");
                                                                ASSERT(0);
                                                                break;
                                                            }
                                                        } else {
                                                            g_pKato->Log(LOG_DETAIL,L"        page at 0x%p is not valid for read/execute!\n", pDataPage);
                                                            break;
                                                        }
                                                    }
                                                }
                                                VERIFY(NULL == LocalFree((HLOCAL)pdwXIPPageList));
                                                VERIFY(NULL == LocalFree((HLOCAL)pBuffer));
                                            }
                                            SetKMode(fKMode);
                                        }
                                    }else
                                {
                                    g_pKato->Log(LOG_SKIP, L"XIP not supported. e32 imageflags 0x%X     o32_flags 0x%X\n",e32.e32_imageflags,po32[i].o32_flags);
                                    //Worked = FALSE;
                                }
                            }
                            if(fGetXIPPages == FALSE)
                            {
                                for(DWORD j=0; j<po32[i].o32_vsize; j+=PAGE_SIZE)
                                {
                                    //set the buffer to zero
                                    memset( pBuffer, 0, PAGE_SIZE);
                                    memset( pBuffer2, 0, PAGE_SIZE);
                                    //get the data in the buffer.
                                    if (ReadFileWithSeek( cfHandle, pBuffer, PAGE_SIZE, &dwRead, NULL,  po32[i].o32_dataptr+j, 0))
                                    {
                                        //unfixup here.
                                        //if(
                                        Dummymodule->FixupBlob(pBuffer,dwRead,po32[i].o32_rva+j,(BYTE)i, TRUE, NULL,0);//== S_OK)
                                        //{
                                        if (ReadFileWithSeek(peromHandle, pBuffer2,dwRead, &dwRead2, NULL, peromFileInfo.sectionHeaders[i].o32_dataptr+j, 0)) //TODO: replace with data from perominfro ..
                                        {
                                            // dwRead2 value is not correct, it will read garbage beyond the end of the file
                                            if(dwRead <= dwRead2) //This is what normally happens
                                            {
                                                if (0 != memcmp(pBuffer, pBuffer2,dwRead))
                                                {
                                                    g_pKato->Log(LOG_FAIL, TEXT("***** Blocks mismatch.\n"));
                                                    Worked = false;
                                                }else
                                                {
                                                    //g_pKato->Log(LOG_DETAIL, "Blocks match.\n");
                                                }
                                            }else
                                            {
                                                //g_pKato->Log(LOG_PASS, "Blocks different size.\n");
                                                //Debug(L"Blocks different size cf %d perom %d", dwRead, dwRead2);
                                                g_pKato->Log(LOG_FAIL, "Blocks different size cf %d perom %d in line %d", dwRead, dwRead2,__LINE__);
                                                Worked = false;
                                            }
                                        }
                                        else
                                        {
                                            DWORD dwErrorCode = GetLastError();
                                            g_pKato->Log(LOG_FAIL, L"Error Reading Rel File.\n",dwErrorCode);
                                            Worked = false;
                                            VERIFY(NULL == LocalFree((HLOCAL)pBuffer));
                                            VERIFY(NULL == LocalFree((HLOCAL)pBuffer2));
                                            goto cleanup1;
                                        }
                                        /*}else
                                        {
                                        DWORD dwErrorCode = GetLastError();
                                        g_pKato->Log(LOG_FAIL, L"Error fixing up File.%d\n",dwErrorCode);
                                        delete[] pBuffer;
                                        delete[] pBuffer2;
                                        goto cleanup1;
                                        }*/

                                    }else
                                    {
                                        DWORD dwErrorCode = GetLastError();
                                        g_pKato->Log(LOG_FAIL, L"Error Reading Img File.%d\n",dwErrorCode);
                                        Worked = false;
                                        VERIFY(NULL == LocalFree((HLOCAL)pBuffer));
                                        VERIFY(NULL == LocalFree((HLOCAL)pBuffer2));
                                        goto cleanup1;
                                    }
                                }
                            }
                        }

                    }    //end for
                        VERIFY(NULL == LocalFree((HLOCAL)pBuffer));
                        VERIFY(NULL == LocalFree((HLOCAL)pBuffer2));
                        //delete[] pBuffer;
                        //delete[] pBuffer2;
                        delete Dummymodule;
                        Dummymodule=NULL;
                    }//end deviceiocontrol O32
                    else
                    {
                        DWORD dwErrorCode = GetLastError();
                        //Debug(L"Error DeviceIOControl. %d ",dwErrorCode);
                        g_pKato->Log(LOG_FAIL, "Error DeviceIOControl.%d in line %d\n",dwErrorCode,__LINE__);
                    }
                }
                if(Worked == true)
                {
                    g_pKato->Log(LOG_DETAIL, L"***** File %s Matches.\n",szPath);
                    FilesMatched ++;
                }else
                {
                   g_pKato->Log(LOG_FAIL, L"***** File %s MisMatched.\n",szPath);
                   FilesNotMatched ++;
                   Worked = true;
                }
            }//end if ROM file
            else
            {
                g_pKato->Log(LOG_DETAIL, L"Not a ROM File.\n");
            }
cleanup1:
            if(Dummymodule != NULL)
                delete Dummymodule;
            if(peromHandle)
            {
                CloseHandle(peromHandle);
                peromHandle=INVALID_HANDLE_VALUE;
            }
            if(peromMHandle)
            {
                CloseHandle(peromMHandle);
                peromMHandle=INVALID_HANDLE_VALUE;
            }
                    
        }while(FindNextFile(hfile,&filedata));
    }

    g_pKato->Log(LOG_DETAIL,L" %d files tested, %d matched, %d failed",FilesMatched+FilesNotMatched,FilesMatched, FilesNotMatched);
cleanup:
    if(hfile)
    {
        CloseHandle(hfile);
    }
    if(cfHandle)
        CloseHandle(cfHandle);
    if(peromHandle)
        CloseHandle(peromHandle);
    if(peromMHandle)
        CloseHandle(peromMHandle);
    if(po32)
        delete [] po32;
    if(pTemp)
        delete [] pTemp;

    /*if(FilesNotMatched != 0)
        return TPR_FAIL;
    else
        return TPR_PASS;*/

    return GetTestResult(g_pKato);
}


