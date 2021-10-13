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


TESTPROCAPI CheckHardwareLock(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    TCHAR szDisk[MAX_PATH] = _T("");
    HANDLE hDisk = INVALID_HANDLE_VALUE;
    HANDLE hDetect = INVALID_HANDLE_VALUE;
    HANDLE hEnumeratePartition=INVALID_HANDLE_VALUE;
    HANDLE hOpenPartition=INVALID_HANDLE_VALUE;
    STOREINFO storeinfo;
    PARTINFO partinfo;
    DELETE_SECTOR_INFO delsectorinfo;
    BOOL FoundIMGFS=FALSE;
    SG_REQ sq_req,wsq_req;
    BYTE *readSectors=NULL;
    BYTE *writeSectors= NULL;
    DWORD cBytes=0,rBytes=0;
    
 // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }
    
    g_pKato->Log(LOG_DETAIL,TEXT("Starting Hardware Locking Test. \n"));

    //enumerate the stores find the IMGFS partition from there. 
    storeinfo.cbSize=sizeof(STOREINFO);
    hDetect = FindFirstStore(&storeinfo);
    do
    {
        if(hDetect!=INVALID_HANDLE_VALUE)
        {
            g_pKato->Log(LOG_DETAIL,TEXT("Trying to Open Store %s\n"),storeinfo.szDeviceName);
            hDisk=OpenStore(storeinfo.szDeviceName);
            
            if(INVALID_HANDLE_VALUE==hDisk)
            {
                g_pKato->Log(LOG_DETAIL,TEXT("WARNING: Could not OpenStore on %s GetLastError returned %d \n"),
                                      storeinfo.szDeviceName,GetLastError());
               
            }
            else
            {
                //allocate memory for Read and WriteSectorBuffers
                readSectors=new BYTE[storeinfo.dwBytesPerSector];
                if(!readSectors)
                {
                    g_pKato->Log(LOG_FAIL,TEXT("Error: Could not allocate Memory for readSector GetLastError Returned %d"),
                                         GetLastError());
                    goto Error;
                }
                writeSectors=new BYTE[storeinfo.dwBytesPerSector];
                if(!writeSectors)
                {
                    g_pKato->Log(LOG_FAIL,TEXT("Error: Could not allocate Memory for WriteSector GetLastError Returned %d"),
                                         GetLastError());
                    goto Error;
                
                }
                partinfo.cbSize=sizeof(PARTINFO);
                hEnumeratePartition=FindFirstPartition(hDisk,&partinfo);
                if(INVALID_HANDLE_VALUE!=hEnumeratePartition)
                {
                    do
                    {
                        hOpenPartition=OpenPartition(hDisk,partinfo.szPartitionName);
                        if(hOpenPartition==INVALID_HANDLE_VALUE)
                        {
                            g_pKato->Log(LOG_DETAIL,TEXT("WARNING: Could not do OpenPartition on %s GetLastError() Returned %d"),
                                                   partinfo.szPartitionName,GetLastError());
                                
                        }
                        else
                        {
                            
                            if((PART_IMGFS==partinfo.bPartType)||(PART_BOOTSECTION==partinfo.bPartType)
                                ||(PART_RAMIMAGE==partinfo.bPartType)||(PART_ROMIMAGE==partinfo.bPartType))
                            {
                                if(PART_IMGFS==partinfo.bPartType)
                                {
                                    g_pKato->Log(LOG_DETAIL,TEXT("Found IMGFS Partition\n"));
                                    FoundIMGFS=TRUE;
                                }
                                if(PART_BOOTSECTION==partinfo.bPartType)
                                {
                                    g_pKato->Log(LOG_DETAIL,TEXT("Found PART_BOOTSECTION\n"));
                                }
                                if(PART_RAMIMAGE==partinfo.bPartType)
                                {
                                    g_pKato->Log(LOG_DETAIL,TEXT("FOUND PART_RAMIMAGE (NK Partition)\n"));
                                }
                                if(PART_ROMIMAGE==partinfo.bPartType)
                                {
                                    g_pKato->Log(LOG_DETAIL,TEXT("FOUND PART_ROMIMAGE (NK Partition)\n"));
                                }
                                
                                
                                
                                //found the IMGFS Partition. This should be ReadOnly
                                //start from the first sector and make sure this partition is not writable.
                                //read all the Sectors 
                                for(UINT i=0; i<partinfo.snNumSectors;i++)
                                {
                                    if(0==i%1000)
                                    {
                                        g_pKato->Log(LOG_DETAIL,TEXT("Working on Sectors between %d and %d\n"),i,(i+1000));
                                    }
                                    memset(&sq_req,0,sizeof(sq_req));
                                    memset(readSectors,0,storeinfo.dwBytesPerSector);
                                    sq_req.sr_start=i;
                                    sq_req.sr_num_sec=1;
                                    sq_req.sr_num_sg=1;
                                    sq_req.sr_status=0;
                                    sq_req.sr_callback=NULL;
                                    sq_req.sr_sglist[0].sb_len=storeinfo.dwBytesPerSector;
                                    sq_req.sr_sglist[0].sb_buf=readSectors;
                                
                                    if(!DeviceIoControl(hOpenPartition,IOCTL_DISK_READ,&sq_req,sizeof(SG_REQ),NULL,0,&cBytes,NULL)&&
                                        !DeviceIoControl(hOpenPartition,DISK_IOCTL_READ,&sq_req,sizeof(SG_REQ),NULL,0,&cBytes,NULL))
                                    {
                                        g_pKato->Log(LOG_FAIL,TEXT("ERROR: Both IOCTL_DISK_READ and DISK_IOCTL_READ Failed on the device on sector %u \n "),i);
                                        
                                    }
                                    else
                                    {
                                        //g_pKato->Log(LOG_DETAIL,TEXT("Read Sector %u from the disk Attempting to write it back\n"));
                                        memset(&wsq_req,0,sizeof(wsq_req));
                                        memset(writeSectors,0,storeinfo.dwBytesPerSector);
                                        wsq_req.sr_start=i;
                                        wsq_req.sr_num_sec=1;
                                        wsq_req.sr_num_sg=1;
                                        wsq_req.sr_status=0;
                                        wsq_req.sr_callback=NULL;
                                        wsq_req.sr_sglist[0].sb_len=storeinfo.dwBytesPerSector;
                                        memcpy(writeSectors,readSectors,storeinfo.dwBytesPerSector);
                                        wsq_req.sr_sglist[0].sb_buf=writeSectors;
                                        //check if write works 
                                        if(!DeviceIoControl(hOpenPartition,IOCTL_DISK_WRITE,&wsq_req,sizeof(SG_REQ),NULL,0,&rBytes,NULL)&&
                                            !DeviceIoControl(hOpenPartition,DISK_IOCTL_WRITE,&wsq_req,sizeof(SG_REQ),NULL,0,&rBytes,NULL))
                                        {
                                            if(ERROR_SUCCESS!=wsq_req.sr_status)
                                            {
                                                //write not successful Lock Test successful
                                                //g_pKato->Log(LOG_DETAIL,TEXT("Success: Flash Sector %u is locked Write Failed\n"),i);
                                            }
                                            if(ERROR_WRITE_PROTECT!=wsq_req.sr_status)
                                            {
                                                g_pKato->Log(LOG_DETAIL,TEXT("WARNING: sr_status did not return ERROR_WRITE_PROTECT Returned code %d for sector %u\n"),
                                                                                                  wsq_req.sr_status,i);
                                            }
                                            
                                        }
                                        else
                                        {
                                            g_pKato->Log(LOG_FAIL,TEXT("ERROR: Flash not locked for Sector %u Write Successful\n"));
                                        }
                                        //check if incorrect write works
                                        if(!DeviceIoControl(hEnumeratePartition,IOCTL_DISK_WRITE,NULL,0,&wsq_req,sizeof(SG_REQ),&rBytes,NULL)&&
                                            !DeviceIoControl(hEnumeratePartition,DISK_IOCTL_WRITE,NULL,0,&wsq_req,sizeof(SG_REQ),&rBytes,NULL))
                                        {
                                              // g_pKato->Log(LOG_DETAIL,TEXT("Success: Flash Sector %u is locked Write Failed\n"),i);
                                        }
                                        else
                                        {
                                            g_pKato->Log(LOG_FAIL,TEXT("ERROR: Flash not locked for Sector %u Write Successful\n"));
                                            g_pKato->Log(LOG_DETAIL,TEXT("Flash Driver supports Writes Incorrectly\n"));
                                        }

                                        //make sure delete doesn't work either. 
                                        //We don't want the contents of the device to change in normal OS mode
                                        //initialize the DELETE_SECTOR_INFO to s tart from the top and try to delete one sector each time.
                                        delsectorinfo.cbSize=sizeof(DELETE_SECTOR_INFO);
                                        delsectorinfo.startsector =i;
                                        delsectorinfo.numsectors=1;
                                        if(!DeviceIoControl(hEnumeratePartition,IOCTL_DISK_DELETE_SECTORS,&delsectorinfo,sizeof(DELETE_SECTOR_INFO),NULL,0,NULL,0))
                                        {
                                            //g_pKato->Log(LOG_DETAIL,TEXT("Success: Could not delete sector %u\n"),i);
                                        }
                                        else
                                        {
                                            g_pKato->Log(LOG_FAIL,TEXT("ERROR: Deleted Sector %u Flash is not locked for IOCTL_DISK_DELETE_SECTOR\n"),i);
                                            g_pKato->Log(LOG_DETAIL,TEXT("Trying to write back the sector %u that we accidently deleted \n"),i);
                                            if(!DeviceIoControl(hOpenPartition,IOCTL_DISK_WRITE,&wsq_req,sizeof(SG_REQ),NULL,0,&rBytes,NULL)&&
                                                !DeviceIoControl(hOpenPartition,DISK_IOCTL_WRITE,&wsq_req,sizeof(SG_REQ),NULL,0,&rBytes,NULL))
                                            {
                                                   g_pKato->Log(LOG_DETAIL,TEXT("ERROR: Could not write the deleted sector. The device maybe corrupt now\n"),i);
                                                   goto Error;
                                            }
                                            else
                                            {
                                                g_pKato->Log(LOG_DETAIL,TEXT("Deleted Sector %u Restored. \n "),i);
                                                goto Error;
                                                
                                            }
                                            
                                        }
                                        
                                        
                                                                                                                       
                                        
                                    }
                                }
                                
                                
                                
                            }
                        }
                        //close this handle before enumerating the next partition. 
                        if(hOpenPartition!=INVALID_HANDLE_VALUE)
                        {
                            CloseHandle(hOpenPartition);
                        }

                    }while (FindNextPartition(hEnumeratePartition,&partinfo));
                    FindClosePartition(hEnumeratePartition);
                    hEnumeratePartition=INVALID_HANDLE_VALUE;//make sure we don't close this twice in cleanup code. 
                }
                else if(INVALID_HANDLE_VALUE==hEnumeratePartition)
                {
                    g_pKato->Log(LOG_DETAIL,TEXT("WARNING: Could not open Partition Handle GetLastError returned %d\n"),
                                    GetLastError());
                    
                }
                if(readSectors)
                {
                    delete []readSectors;
                    readSectors=NULL;
                }
                if(writeSectors)
                {
                    delete[] writeSectors;
                    writeSectors=NULL;
                }
                    
                
            }
            
            
        }
        if(INVALID_HANDLE_VALUE==hDetect)
        {
            g_pKato->Log(LOG_SKIP,TEXT("SKIPPING: No Mass storage device found bailing \n"));
            goto Error;
        }
          
        
    }while(FindNextStore(hDetect, &storeinfo));
    FindCloseStore(hDetect);
    hDetect=INVALID_HANDLE_VALUE;
    

    

    Error:
        if(INVALID_HANDLE_VALUE!=hDetect)
        {
           CloseHandle(hDetect);
        }
        if(INVALID_HANDLE_VALUE!=hEnumeratePartition)
        {
            FindClosePartition(hEnumeratePartition);
        }
        if(INVALID_HANDLE_VALUE!=hDisk)
        {
            FindCloseStore(hDetect);
        }
        if(INVALID_HANDLE_VALUE!=hOpenPartition)
        {
            CloseHandle(hOpenPartition);
        }
        if(writeSectors)
        {
            delete [] writeSectors;
            writeSectors=NULL;
        }
        if(readSectors)
        {
            delete[] readSectors;
            readSectors=NULL;
        }
        return GetTestResult(g_pKato);
        

    
    

}
