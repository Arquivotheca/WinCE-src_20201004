//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//------------------------------------------------------------------------------
//
//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//  ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//  PARTICULAR PURPOSE.
//  
//------------------------------------------------------------------------------
#include <windows.h>
#include <stdio.h>
#include <tchar.h>

#include <pehdr.h>
#include <romldr.h>

#define BUFFER_SIZE  64*1024
BYTE pBuffer[BUFFER_SIZE];

HANDLE hFile;
DWORD g_pTOC = 0;
DWORD  g_dwImageStart;
DWORD  g_dwImageLength;
DWORD  g_dwNumRecords;
DWORD  g_dwROMOffset;

typedef struct _RECORD_DATA {
    DWORD dwStartAddress;
    DWORD dwLength;
    DWORD dwChecksum;
    DWORD dwFilePointer;
} RECORD_DATA, *PRECORD_DATA;

#define MAX_RECORDS 2048
RECORD_DATA g_Records[MAX_RECORDS];


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
ReWriteCheckSum(DWORD dwAddress)
{
    DWORD dwBytes;
    DWORD i, j;
    UCHAR c;
    DWORD dwCheckSum = 0;

    for (i = 0; i < g_dwNumRecords; i++) {
        if (g_Records[i].dwStartAddress <= dwAddress &&
            g_Records[i].dwStartAddress + g_Records[i].dwLength > dwAddress) {
            
            printf("Original record checksum: 0x%x\n", g_Records[i].dwChecksum);

            // point to record
            SetFilePointer(hFile, g_Records[i].dwFilePointer, NULL, FILE_BEGIN);

            // calculate checksum
            for (j = 0; j < g_Records[i].dwLength; j++) {
                if (!ReadFile(hFile, &c, 1, &dwBytes, NULL) || (dwBytes != 1)) return FALSE;

                dwCheckSum += c;
            }

            printf("New record checksum: 0x%x\n", dwCheckSum);

            // Now write the checksum
            SetFilePointer(hFile, g_Records[i].dwFilePointer - 4, NULL, FILE_BEGIN);

            if (!WriteFile(hFile, &dwCheckSum, 4, &dwBytes, NULL) || (dwBytes != 4)) return FALSE;

            return TRUE;
        }
    }

    return FALSE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
AccessBinFile(
    PBYTE pBuffer,
    DWORD dwAddress,
    DWORD dwLength,
    DWORD dwROMOffsetRead,
	BOOL  bWrite
    )
{
    DWORD dwBytes;
    BOOL  fRet;
    DWORD i;
    DWORD dwDiff;


  	FlushFileBuffers(hFile);

    //
    // Adjust if needed
    //
    dwAddress += dwROMOffsetRead;

    for (i = 0; i < g_dwNumRecords; i++) {
        if (g_Records[i].dwStartAddress <= dwAddress &&
            g_Records[i].dwStartAddress + g_Records[i].dwLength >= (dwAddress + dwLength)) {
            
            //
            // Offset into the record.
            //
            dwDiff = dwAddress - g_Records[i].dwStartAddress;

            SetFilePointer(hFile, g_Records[i].dwFilePointer + dwDiff, NULL, FILE_BEGIN);

			if (bWrite) {
				if (dwLength) {
					fRet = WriteFile(hFile, pBuffer, dwLength, &dwBytes, NULL);

					if (!fRet || dwBytes != dwLength) {
						return FALSE;
					}
				} else {
					do {
						fRet = WriteFile(hFile, pBuffer, 1, &dwBytes, NULL); 
					} while (*pBuffer++);
				}

				if(!ReWriteCheckSum(dwAddress))
				    return FALSE;
			} else {
				if (dwLength) {
					fRet = ReadFile(hFile, pBuffer, dwLength, &dwBytes, NULL);
				
					if (!fRet || dwBytes != dwLength) {
						return FALSE;
					}
				} else {
					do {
						fRet = ReadFile(hFile, pBuffer, 1, &dwBytes, NULL);
					} while (*pBuffer++);
				}
			}

            return TRUE;
        }
    }

    return FALSE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
ComputeRomOffset(DWORD pTOC)
{
    DWORD i;
    BOOL fFoundIt = FALSE;
    BOOL fRet;
    DWORD dwROMOffsetRead;

    for (i = 0; i < g_dwNumRecords; i++) {
        //
        // Check for potential TOC records
        //
        if (g_Records[i].dwLength == sizeof(ROMHDR)) {
            
            //
            // If this _IS_ the TOC record, compute the ROM Offset.
            //
            dwROMOffsetRead = g_Records[i].dwStartAddress - pTOC;

            //
            // Read out the record to verify. (unadjusted)
            //
            fRet = AccessBinFile(pBuffer, g_Records[i].dwStartAddress, sizeof(ROMHDR), 0, FALSE);
    
            if (fRet) {
                ROMHDR *pTOC = (ROMHDR *)pBuffer;
            
                if ((pTOC->physfirst == (g_dwImageStart - dwROMOffsetRead)) &&
                    (pTOC->physlast  == (g_dwImageStart - dwROMOffsetRead) + g_dwImageLength)) {

                    //
                    // Extra sanity check...
                    //
                    if (pTOC->dllfirst <= pTOC->dlllast && pTOC->dlllast == 0x02000000) { 
                        fFoundIt = TRUE;
                        break;
                    } else {
                        printf ("NOTICE! Record %d looked like a TOC except DLL first = 0x%08X, and DLL last = 0x%08X\r\n", pTOC->dllfirst, pTOC->dlllast);
                    }
                }
            }
        }
    }

    if (fFoundIt) {
        g_dwROMOffset = dwROMOffsetRead;
    } else {
        g_dwROMOffset = 0;
    }

    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
WritePID(ROMPID *pPID)
{
	if (AccessBinFile(pBuffer, g_pTOC, sizeof(ROMHDR), g_dwROMOffset, FALSE)) {
		ROMHDR* pTOC = (ROMHDR *)pBuffer;

		if (pTOC->pExtensions) {
			if (!AccessBinFile((PBYTE)pPID, (DWORD)pTOC->pExtensions, sizeof(ROMPID) - 4, g_dwROMOffset, TRUE)) {
				printf("Couldn't write TOC Extensions\n");
				return FALSE;
			}
		}
	} else {
		printf("Couldn't read TOC data\n");
		return FALSE;
	}

	return TRUE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int OpenBin(LPCTSTR lpFileName){
	DWORD dwRecAddr, dwRecLen, dwRecChk;
    DWORD dwRead;
    BOOL  fRet;

	hFile = CreateFile(lpFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 
                       NULL, OPEN_EXISTING, 0, 0);

    if (hFile == INVALID_HANDLE_VALUE) {
        printf ("Error opening %s\n", lpFileName);
        return FALSE;
    }
    
    fRet = ReadFile(hFile, pBuffer, 7, &dwRead, NULL);
    
    if (!fRet || dwRead != 7) {
        printf("ERROR: Reading %s (%d)\n", lpFileName, __LINE__);
        goto Error;
    }
    
    if (memcmp( pBuffer, "B000FF\x0A", 7 )) {
        printf("Missing initial signature (BOOOFF\x0A). Not a BIN file\n");
        goto Error;
    }

    //
    // Read the image header
    // 
    fRet = ReadFile(hFile, &g_dwImageStart, sizeof(DWORD), &dwRead, NULL);

    if (!fRet || dwRead != sizeof(DWORD)) {
		printf("ERROR: Can't find Image start\n");
        goto Error;
    }

    fRet = ReadFile(hFile, &g_dwImageLength, sizeof(DWORD), &dwRead, NULL);

    if (!fRet || dwRead != sizeof(DWORD)) {
		printf("ERROR: Can't find Image length\n");
        goto Error;
    }

    //
    // Now read the records.
    //
    while (1) {       
        //
        // Record address
        //
        fRet = ReadFile(hFile, &dwRecAddr, sizeof(DWORD), &dwRead, NULL);

        if (!fRet || dwRead != sizeof(DWORD)) {
            break;
        }

        //
        // Record length
        //
        fRet = ReadFile(hFile, &dwRecLen, sizeof(DWORD), &dwRead, NULL);

        if (!fRet || dwRead != sizeof(DWORD)) {
            break;
        }

        //
        // Record checksum
        //
        fRet = ReadFile(hFile, &dwRecChk, sizeof(DWORD), &dwRead, NULL);

        if (!fRet || dwRead != sizeof(DWORD)) {
            break;
        }

        g_Records[g_dwNumRecords].dwStartAddress = dwRecAddr;
        g_Records[g_dwNumRecords].dwLength       = dwRecLen;
        g_Records[g_dwNumRecords].dwChecksum     = dwRecChk;
        g_Records[g_dwNumRecords].dwFilePointer  = SetFilePointer(hFile, 0, NULL, FILE_CURRENT);

        g_dwNumRecords++;
        
        if (dwRecAddr == 0) {
            break;
        }
        
        SetFilePointer(hFile, dwRecLen, NULL, FILE_CURRENT);
    }

    //
    // Find pTOC
    //
    fRet = AccessBinFile(pBuffer, g_dwImageStart + 0x40, 8, 0, FALSE);
    if (fRet) {
        g_pTOC = *((PDWORD)(pBuffer + 4));
    } else {
		printf("ERROR: Couldn't find pTOC\n");
        goto Error;
    }

    if (!ComputeRomOffset(g_pTOC)) {
		goto Error;
	}

    return TRUE;

Error:
    return FALSE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int CloseBin(){
    CloseHandle(hFile);
    
    return TRUE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int StampFile(LPCTSTR lpFileName, LPCTSTR section_name, LPCTSTR section_type, LPCTSTR input_file){
    BOOL ret = FALSE;
    HANDLE hInput = INVALID_HANDLE_VALUE;
    DWORD type = strtol(section_type, NULL, 16);
    
	if(!OpenBin(lpFileName)){
        printf ("Error opening %s\n", lpFileName);
        goto EXIT;
    }

	hInput = CreateFile(input_file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);
	if(hInput == INVALID_HANDLE_VALUE){
        printf ("Error opening %s\n", input_file);
	    goto EXIT;
	}

	// find correct extension
	if(!AccessBinFile(pBuffer, g_pTOC, sizeof(ROMHDR), g_dwROMOffset, FALSE)){
		printf("Couldn't read TOC data\n");
		goto EXIT;
	}

    {
		ROMHDR* pTOC = (ROMHDR *)pBuffer;
		ROMPID pid;

		if (pTOC->pExtensions) {
			if (!AccessBinFile((PBYTE)&pid, (DWORD)pTOC->pExtensions, sizeof(ROMPID), g_dwROMOffset, FALSE)) {
				printf("Couldn't read TOC Extensions\n");
				goto EXIT;
			}

			while(pid.pNextExt){
			    DWORD ExtAddress = (DWORD)pid.pNextExt;
			    
      			if (!AccessBinFile((PBYTE)&pid, ExtAddress, sizeof(ROMPID), g_dwROMOffset, FALSE)) {
      				printf("Couldn't read TOC Extensions\n");
      				goto EXIT;
      			}

      			if(strcmp(pid.name, section_name) == 0 && pid.type == type) {
      			    DWORD size = 0, cb = 0;
      			    BYTE *buffer;
      			    
      			    
      			    printf("Found extension to update\n"
      			           "  Name: %s\n"
      			           "  Type: %08x\n"
      			           "  pData: %08x\n"
      			           "  Length: %08x\n"
      			           "  Reserved: %08x\n"
      			           "  Next: %08x\n",
      			           pid.name, pid.type, pid.pdata, pid.length, pid.reserved, pid.pNextExt);

                    size = GetFileSize(hInput, NULL);

                    if(pid.reserved < size){
                        printf("Error: input file was too large to fit in extension\n"
                               "    Extension size: %08x\n"
                               "    Input File size: %08x\n",
                               pid.reserved,
                               size);
                        goto EXIT;
                    }

                    buffer = malloc(size + sizeof(ROMPID));
                    if(!buffer){
                        printf("Error: failed allocating memory for buffer\n");
                        goto EXIT;
                    }

                    pid.length = size;

                    memcpy(buffer, &pid, sizeof(pid));

                    if(!ReadFile(hInput, buffer + sizeof(ROMPID), size, &cb, NULL)){
                        printf("Error: failed reading input file\n");
                        goto EXIT;
                    }

                    if(!AccessBinFile(buffer, ExtAddress, size + sizeof(ROMPID), g_dwROMOffset, TRUE)){
        				printf("Error: Failed updating extension\n");
        				goto EXIT;
                    }
                    // copy contents to file at right spot (AccessBinFile?)

      			    ret = TRUE;
      			    goto EXIT;
      			}
			}
		}
    }

    printf("No extensions found with name '%s' and type '%08x'\n",
           section_name, type);
EXIT:
    CloseHandle(hInput);
    CloseBin();
    return ret;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int StampPID(LPCTSTR lpFileName, ROMPID *pPID) {
	if(!OpenBin(lpFileName)){
        printf ("Error opening %s\n", lpFileName);
        return FALSE;
    }
 
	if (!WritePID(pPID)) {
		goto Error;
	}

    CloseBin();
	return TRUE;

Error:
    CloseBin();
	return FALSE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
	TCHAR *iFileName = "nk.bin";
	ROMPID Pid = {
		{ 0x11111111, 0x22222222, 0x33333333, 0x44444444, 0x55555555, 0x66666666, 0x77777777, 0x88888888, 0x99999999, 0xaaaaaaaa },
		NULL
	};

  printf("Stampbin V1.1 built " __DATE__ " " __TIME__ "\n\n");
    
	if(getenv("d_break"))
	  DebugBreak();

  if(argc >= 3 && _tcsicmp(argv[1], "-i") == 0){
    iFileName = argv[2];
    argc -= 2;
    argv = &argv[2];
	}

{
  int i;
  
  for(i = 0; i < argc; i++)
    printf("%d) %s\n", i, argv[i]);
}
  if(argc == 1){
    if(!StampPID(iFileName, &Pid)) {
      printf("ERROR!\n");
	    exit(1);
    }
  }
	else{
    if(argc != 4){
      printf("\nUsage: %s [-i filename] [name type inputfile]\n\n"
             "    When issued without any parameters it will stamp a default pid to the image.\n\n"
             "    Otherwise it will search for an extension with the specified name and type\n"
             "    and then copy the contents of the file into that region.  The file must be less\n"
             "    then or equal to the size of the region.\n",
             argv[0]);
      exit(1);
    }
    if(!StampFile(iFileName, argv[1], argv[2], argv[3])) {
  	  printf("ERROR!\n");
      exit(1);
  	}
	}

    return 0;
}
