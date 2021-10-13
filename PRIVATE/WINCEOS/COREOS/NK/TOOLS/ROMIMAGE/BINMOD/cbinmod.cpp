//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "cbinmod.h"

#include "..\compress\compress.h"

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

CBinMod::CBinMod(bool log2file)
{
  hFile = INVALID_HANDLE_VALUE;
  fpLog = NULL;

  pTOC = 0;
  dwImageStart = 0;
  dwImageLength = 0;
  dwNumRecords = 0;
  dwROMOffset = 0;
  
  memset(Records, 0, sizeof(Records));

  if(log2file){
    char path[MAX_PATH];
    char tmp[MAX_PATH];    

    if(GetTempPath(sizeof(path), path) == ERROR_PATH_NOT_FOUND)
    {
      printf("Error: GetTempPath() failed\n");
      goto exit;
    }

    if(GetTempFileName(path, "binmod", 0, tmp) == 0)
    {
      printf("Error: GetTempFileName() failed\n");
      goto exit;
    }

    printf("Logging to %s\n", tmp);
    fpLog = fopen(tmp, "w");
    if(!fpLog)
    {
      printf("Error: fopen() failed\n");
      goto exit;
    }
  }
exit:
  return;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

CBinMod::~CBinMod()
{
  if(hFile != INVALID_HANDLE_VALUE)
  { 
    my_CloseHandle(hFile);
    hFile = INVALID_HANDLE_VALUE;
  }

  if(fpLog){
    fclose(fpLog);
    fpLog = NULL;
  }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

bool CBinMod::Init(string _image)
{
  image = _image;
  
  if(my_GetFileAttributes(image.c_str()) == INVALID_FILE_ATTRIBUTES){
    dprintf("Error: Could not find image '%s'\n", image.c_str());
    return false;
  }

  hFile = my_CreateFile(image.c_str(), 
                       GENERIC_READ | GENERIC_WRITE, 
                       FILE_SHARE_READ | FILE_SHARE_WRITE, 
                       NULL, 
                       OPEN_EXISTING, 
                       0, 
                       0);

  if(hFile == INVALID_HANDLE_VALUE) {
    dprintf("Error opening %s\n", image.c_str());
    goto Error;
  }
  
  DWORD cb;
  INT fRet;

  fRet = my_ReadFile(hFile, pBuffer, 7, &cb, NULL);
  
  if (!fRet || cb != 7) {
    dprintf("ERROR: Reading %s (%d)\n", image.c_str(), __LINE__);
    goto Error;
  }
  
  if(memcmp(pBuffer, "B000FF\x0A", 7 )) {
    dprintf("Missing initial signature (BOOOFF\x0A). Not a BIN file\n");
    goto Error;
  }

  //
  // Read the image header
  // 
  fRet = my_ReadFile(hFile, &dwImageStart, sizeof(DWORD), &cb, NULL);

  if (!fRet || cb != sizeof(DWORD)) {
  		dprintf("ERROR: Can't find Image start\n");
      goto Error;
  }

  fRet = my_ReadFile(hFile, &dwImageLength, sizeof(DWORD), &cb, NULL);

  if (!fRet || cb != sizeof(DWORD)) {
  		dprintf("ERROR: Can't find Image length\n");
      goto Error;
  }

  //
  // Now read the records.
  //
	DWORD dwRecAddr, dwRecLen, dwRecChk;
  while (1) {       
      //
      // Record address
      //
      fRet = my_ReadFile(hFile, &dwRecAddr, sizeof(DWORD), &cb, NULL);

      if (!fRet || cb != sizeof(DWORD)) {
          break;
      }

      //
      // Record length
      //
      fRet = my_ReadFile(hFile, &dwRecLen, sizeof(DWORD), &cb, NULL);

      if (!fRet || cb != sizeof(DWORD)) {
          break;
      }

      //
      // Record checksum
      //
      fRet = my_ReadFile(hFile, &dwRecChk, sizeof(DWORD), &cb, NULL);

      if (!fRet || cb != sizeof(DWORD)) {
          break;
      }

      Records[dwNumRecords].dwStartAddress = dwRecAddr;
      Records[dwNumRecords].dwLength       = dwRecLen;
      Records[dwNumRecords].dwChecksum     = dwRecChk;
      Records[dwNumRecords].dwFilePointer  = my_SetFilePointer(hFile, 0, NULL, FILE_CURRENT);

      dwNumRecords++;
      
      if (dwRecAddr == 0) {
          break;
      }
      
      my_SetFilePointer(hFile, dwRecLen, NULL, FILE_CURRENT);
  }

  //
  // Find pTOC
  //
  fRet = AccessBinFile(pBuffer, dwImageStart + 0x40, 8, 0, false);
  if (fRet) {
      pTOC = *((PDWORD)(pBuffer + 4));
  } else {
  		dprintf("ERROR: Couldn't find pTOC\n");
      goto Error;
  }

  return true;

Error:
  image.erase();
  
  return false;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

bool CBinMod::Replace(string _file)
{
  if(my_GetFileAttributes(_file.c_str()) == INVALID_FILE_ATTRIBUTES){
    dprintf("Could not find file '%s'\n", _file.c_str());
    return false;
  }

  bool ret = false;

	ROMHDR Toc = {0};
  DWORD state = 0;
  string name;
    
NextRegion:
  if (!ComputeRomOffset(state)) {
    goto EndOfRegions;
	}

  string::size_type idx = _file.rfind('\\');
  if(idx == string::npos) 
    name = _file;
  else
    name = _file.substr(idx+1); // start just after the \

	// read toc
	if(!AccessBinFile((PBYTE)&Toc, pTOC, sizeof(ROMHDR), dwROMOffset, false)){
		dprintf("Couldn't read TOC data\n");
		goto exit;
	}

  FILESentry *pFileList = new FILESentry[Toc.numfiles];

  if(!pFileList){
    dprintf("Error allocated space for files list containing %d files\n", Toc.numfiles);
    goto exit;
  }

  DWORD files_section_offset = pTOC + sizeof(ROMHDR) + sizeof(TOCentry) * Toc.nummods;

  // read FILESEntry list
  if(!AccessBinFile((BYTE*)pFileList, 
                    files_section_offset,
                    sizeof(FILESentry) * Toc.numfiles, 
                    dwROMOffset, 
                    false)){
    dprintf("Couldn't read file list\n");
    goto exit;
  }
  
  for(int i = 0; i < (int)Toc.numfiles; i++){
    if(!AccessBinFile(pBuffer, (DWORD)pFileList[i].lpszFileName, 0, dwROMOffset, false)){
      dprintf("Couldn't read file name for file %d\n", i);
      goto exit;
    }

    if(strcmp((char*)pBuffer, name.c_str()) == 0)
    {
      bool compressed = false;
      if(pFileList[i].nRealFileSize != pFileList[i].nCompFileSize) compressed = true;

      DWORD size = 0;
      if(compressed)
        size = pFileList[i].nCompFileSize;
      else
        size = pFileList[i].nRealFileSize;
      
      dprintf("Found '%s' in the image! - located at %08x, size %08x %s\n", name.c_str(), pFileList[i].ulLoadOffset, size, compressed ? "compressed" : "uncompressed");

      BYTE *new_data = NULL;
      DWORD csize = 0;
      DWORD rsize = 0;
      if(!GetNewFileData(&new_data, &csize, &rsize, _file.c_str(), compressed)){
        dprintf("Error: failed processing new file\n");
        goto exit;
      }

      if(compressed)
        dprintf("New file compressed = %08x, uncompressed = %08x\n", csize, rsize);
      else
        dprintf("New file uncompressed = %08x\n", rsize);

      // if sizes are the same or smaller just overwrite the data and zero pad
      if(size >= csize){
        BYTE *buffer = new BYTE[size];
        if(!buffer){
          dprintf("Error: failed allocating buffer\n");
          goto exit;
        }

        memset(buffer, 0, size);
        memcpy(buffer, new_data, csize);
        
        if(!AccessBinFile(buffer, pFileList[i].ulLoadOffset, size, dwROMOffset, true)){
          dprintf("Error: failed to write new data to image\n");
          goto exit;
        }          

        delete[] buffer;
      }
      else{      // allocate new location for data?
        dprintf("File too large for origional location, searching for new space...\n");
        
        BYTE *buffer = new BYTE[csize];
        if(!buffer){
          dprintf("Error: failed allocating buffer\n");
          goto exit;
        }

        memcpy(buffer, new_data, csize);
        
        pFileList[i].ulLoadOffset = FindHole(csize, &Toc);
        if(!pFileList[i].ulLoadOffset){
          dprintf("Error: Couldn't find space in the image for file, replace aborted\n");
          dprintf(" The image is in a possibly inconsistent state and should not be used!!!!\n");
          goto exit;
        }

        if(!AccessBinFile(buffer, pFileList[i].ulLoadOffset, csize, dwROMOffset, true)){
          dprintf("Error: failed to write new data to image\n");
          dprintf(" The image is in a possibly inconsistent state and should not be used!!!!\n");
          goto exit;
        }          

        delete[] buffer;
      }

      dprintf("New data written to image!\n");

      pFileList[i].nCompFileSize = csize;
      pFileList[i].nRealFileSize = rsize;
      
      if(!AccessBinFile((BYTE*)pFileList, files_section_offset, sizeof(FILESentry) * Toc.numfiles, dwROMOffset, true)){
        dprintf("Error: failed updating TOC!\n");
        dprintf(" The image is in a possibly inconsistent state and should not be used!!!!\n");
        goto exit;
      }

      dprintf("TOC updated!\n");

      delete[] new_data;

      ret = true;
      
      goto exit;
    }
  }

  goto NextRegion;

EndOfRegions:
  dprintf("Error: File not found in image\n");

exit:
  return ret;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

bool CBinMod::GetNewFileData(BYTE **buffer, DWORD *csize, DWORD *rsize, const char *file_name, bool compressed){
  bool ret = false;
  
  HANDLE hInput = INVALID_HANDLE_VALUE;
	hInput = my_CreateFile(file_name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);
	if(hInput == INVALID_HANDLE_VALUE){
    dprintf("Error opening %s\n", file_name);
    goto exit;
	}

  *csize = *rsize = my_GetFileSize(hInput, NULL);
  if(*rsize == INVALID_FILE_SIZE){
    dprintf("Error: failed to get file size information for '%s'\n", file_name);
    goto exit;
  }

  *buffer = new BYTE[*rsize];
  if(!*buffer){
    dprintf("Error: failed allocating buffer\n");
    goto exit;
  }

  DWORD cb = 0;
  if(!my_ReadFile(hInput, *buffer, *rsize, &cb, NULL) || cb != *rsize){
    dprintf("Error: failed reading '%s'\n", file_name);
    goto exit;
  }

  if(compressed){
    // compress the data
    BYTE *compressed_data = new BYTE[*rsize];
    DWORD compressed_size;
    DWORD page_size = 0x1000;

    if(!compressed_data){
      dprintf("Error: failed allocating buffer\n");
      goto exit;
    }

    char *m_name = "compress.dll";
    HMODULE m_hcomp = LoadLibrary(m_name);
    if(!m_hcomp){
      dprintf("Error: LoadLibrary() failed to load '%s': %d\n", m_name, GetLastError());
      return false;
    }

    CECOMPRESS cecompress = (CECOMPRESS)GetProcAddress(m_hcomp, "CECompress");
    if(!cecompress){
      dprintf("Error: GetProcAddress() failed to find 'CECompress' in '%s': %d\n", m_name, GetLastError());
      FreeLibrary(m_hcomp);
      return false;
    }

    dprintf("Assuming pagesize of %08x for compression!\n", page_size);

    compressed_size = cecompress(*buffer, 
                                 *rsize, 
                                 compressed_data, 
                                 *rsize - 1, 
                                 1, 
                                 page_size);


    FreeLibrary(m_hcomp);
  
    // if success
    if(compressed_size != -1){
      *csize = compressed_size;
      memcpy(*buffer, compressed_data, *csize);
    }
  }

  ret = true;
  
exit:
  return ret;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


bool CBinMod::Extract(string _file)
{
  if(image.empty())
    return false;

  if(GetFileAttributes(_file.c_str()) != INVALID_FILE_ATTRIBUTES){
    dprintf("File already exists '%s', extraction aborted.\n", _file.c_str());
    return false;
  }

  bool ret = false;
  ROMHDR TOC = {0};
  DWORD state = 0;
  string name;
    
NextRegion:
  if (!ComputeRomOffset(state)) {
    goto EndOfRegions;
	}

  string::size_type idx = _file.rfind('\\');
  if(idx == string::npos) 
    name = _file;
  else
    name = _file.substr(idx+1); // start just after the \

	// read toc
	if(!AccessBinFile((PBYTE)&TOC, pTOC, sizeof(ROMHDR), dwROMOffset, false)){
		dprintf("Couldn't read TOC data\n");
		goto exit;
	}

  FILESentry *pFileList = new FILESentry[TOC.numfiles];

  if(!pFileList){
    dprintf("Error allocated space for files list containing %d files\n", TOC.numfiles);
    goto exit;
  }

  DWORD files_section_offset = pTOC + sizeof(ROMHDR) + sizeof(TOCentry) * TOC.nummods;

  // read FILESEntry list
  if(!AccessBinFile((BYTE*)pFileList, 
                    files_section_offset,
                    sizeof(FILESentry) * TOC.numfiles, 
                    dwROMOffset, 
                    false)){
    dprintf("Couldn't read file list\n");
    goto exit;
  }
  
  for(int i = 0; i < (int)TOC.numfiles; i++){
    if(!AccessBinFile(pBuffer, (DWORD)pFileList[i].lpszFileName, 0, dwROMOffset, false)){
      dprintf("Couldn't read file name for file %d\n", i);
      if(i)
        dprintf("previous file was %s\n", pBuffer);
      goto exit;
    }

    if(strcmp((char*)pBuffer, name.c_str()) == 0)
    {
      bool compressed = false;
      if(pFileList[i].nRealFileSize != pFileList[i].nCompFileSize) compressed = true;

      DWORD size = 0;
      if(compressed)
        size = pFileList[i].nCompFileSize;
      else
        size = pFileList[i].nRealFileSize;
      
      dprintf("Found '%s' in the image! - located at %08x, size %08x %s\n", name.c_str(), pFileList[i].ulLoadOffset, size, compressed ? "compressed" : "uncompressed");

      BYTE *cbuffer = new BYTE[pFileList[i].nCompFileSize];
      if(!cbuffer){
        dprintf("Error: failed allocating buffer\n");
        goto exit;
      }
      
      if(!AccessBinFile((BYTE*)cbuffer, 
                        pFileList[i].ulLoadOffset,
                        pFileList[i].nCompFileSize, 
                        dwROMOffset, 
                        false)){
        dprintf("Error: failed reading reading file\n");
        goto exit;
      }

      if(compressed){
        // compress the data
        BYTE *uncompressed_data = new BYTE[pFileList[i].nRealFileSize];
        DWORD uncompressed_size;
        DWORD page_size = 0x1000;

        if(!uncompressed_data){
          dprintf("Error: failed allocating buffer\n");
          goto exit;
        }

        char *m_name = "compress.dll";
        HMODULE m_hcomp = LoadLibrary(m_name);
        if(!m_hcomp){
          dprintf("Error: LoadLibrary() failed to load '%s': %d\n", m_name, GetLastError());
          return false;
        }

        CEDECOMPRESS cedecompress = (CEDECOMPRESS)GetProcAddress(m_hcomp, "CEDecompress");
        if(!cedecompress){
          dprintf("Error: GetProcAddress() failed to find 'CEDecompress' in '%s': %d\n", m_name, GetLastError());
          FreeLibrary(m_hcomp);
          return false;
        }

        dprintf("Assuming pagesize of %08x for decompression!\n", page_size);

        uncompressed_size = cedecompress(cbuffer, 
                                     pFileList[i].nCompFileSize, 
                                     uncompressed_data, 
                                     pFileList[i].nRealFileSize, 
                                     0,
                                     1, 
                                     page_size);


        FreeLibrary(m_hcomp);
      
        // if success
        if(uncompressed_size != pFileList[i].nRealFileSize){
          dprintf("Error: decompression failed\n");
          goto exit;
        }

        cbuffer = uncompressed_data;
      }

      // open and write file
      HANDLE hOut = CreateFile(_file.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, 0);
      if(hOut == INVALID_HANDLE_VALUE){
        dprintf("failed to open %s for output\n", _file.c_str());
        goto exit;
      }

      DWORD cb = 0;
      if(!WriteFile(hOut, cbuffer, pFileList[i].nRealFileSize, &cb, NULL)){
        dprintf("Error: failed to write file data\n");
        goto exit;
      }

      dprintf("File extracted!\n");

      ret = true;
      goto exit;
      break;
    }
  }

  goto NextRegion;

EndOfRegions:
  dprintf("Error: File not found in image\n");

exit:
  return ret;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

bool CBinMod::AccessBinFile(PBYTE buffer, DWORD dwAddress, DWORD dwLength, DWORD dwROMOffsetRead, bool bWrite)
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

  for (i = 0; i < dwNumRecords; i++) {
    if(Records[i].dwStartAddress <= dwAddress &&
       Records[i].dwStartAddress + Records[i].dwLength >= (dwAddress + dwLength))
    {
      
      //
      // Offset into the record.
      //
      dwDiff = dwAddress - Records[i].dwStartAddress;

      my_SetFilePointer(hFile, Records[i].dwFilePointer + dwDiff, NULL, FILE_BEGIN);

  		if(bWrite)
      {
	  		if(dwLength)
        {
		  		fRet = my_WriteFile(hFile, buffer, dwLength, &dwBytes, NULL);
  
	  			if(!fRet || dwBytes != dwLength)
          {
		  			return false;
			  	}
  			}
        else
        {
	  			do
	  			{
		  			fRet = my_WriteFile(hFile, buffer, 1, &dwBytes, NULL); 
			  	}
          while (*buffer++);
  			}

	  		if(!ReWriteCheckSum(dwAddress))
	  		{
          dprintf("Error: Failed writing new checksum\n");
			    return false;
	  		}
		  } 
      else 
      {
        if (dwLength)
        {
  				fRet = my_ReadFile(hFile, buffer, dwLength, &dwBytes, NULL);
  			
  				if (!fRet || dwBytes != dwLength)
          {
  					return false;
  				}
  			} 
        else
        {
  				do 
          {
  					fRet = my_ReadFile(hFile, buffer, 1, &dwBytes, NULL);
  				}
          while (*buffer++);
  			}
  		}
  
      return true;
    }
  }

  return false;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

bool CBinMod::ReWriteCheckSum(DWORD dwAddress)
{
  DWORD dwBytes;
  DWORD i, j;
  UCHAR c;
  DWORD dwCheckSum = 0;

  for(i = 0; i < dwNumRecords; i++)
  {
    if(Records[i].dwStartAddress <= dwAddress &&
       Records[i].dwStartAddress + Records[i].dwLength > dwAddress)
    {
      
      dprintf("Original record checksum: 0x%x\n", Records[i].dwChecksum);

      // point to record
      my_SetFilePointer(hFile, Records[i].dwFilePointer, NULL, FILE_BEGIN);

      // calculate checksum
      for (j = 0; j < Records[i].dwLength; j++) {
        if (!my_ReadFile(hFile, &c, 1, &dwBytes, NULL) || (dwBytes != 1)) {
          return false;
        }

        dwCheckSum += c;
      }

      dprintf("New record checksum: 0x%x\n", dwCheckSum);

      // Now write the checksum
      my_SetFilePointer(hFile, Records[i].dwFilePointer - 4, NULL, FILE_BEGIN);

      if (!my_WriteFile(hFile, &dwCheckSum, 4, &dwBytes, NULL) || (dwBytes != 4)){
        return false;
      }

      return true;
    }
  }

  return false;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

bool CBinMod::ComputeRomOffset(DWORD &state)
{
  DWORD i;
  bool fFoundIt = false;
  bool fRet;
  DWORD dwROMOffsetRead;
  DWORD header[2] = {0};
  ROMHDR *dwpTOC = 0;

  for (i = state; i < dwNumRecords; i++)
  {
    //
    // no pTOC and not an 8 byte record... skip
    //
    if(!dwpTOC)
    {
      if(Records[i].dwLength != 8)
        continue;

      // 
      // check for signature and pTOC
      // 
      fRet = AccessBinFile((BYTE*)header, Records[i].dwStartAddress, 8, 0, false);

      if(!fRet || header[0] != ROM_SIGNATURE)
          continue;

      //
      // set pTOC and save current search and start record search over
      //
      dwpTOC = (ROMHDR*)header[1];
      state = i + 1;
      i = 0;
    }
    
    //
    // Check for potential TOC records
    //
    if (Records[i].dwLength == sizeof(ROMHDR)) 
    {
      
      //
      // If this _IS_ the TOC record, compute the ROM Offset.
      //
      dwROMOffsetRead = Records[i].dwStartAddress - (DWORD)dwpTOC;

      dprintf("Checking record #%d for potential TOC (ROMOFFSET = 0x%08X)\n", i, dwROMOffsetRead);
      //
      // Read out the record to verify. (unadjusted)
      //
      fRet = AccessBinFile(pBuffer, Records[i].dwStartAddress, sizeof(ROMHDR), 0, false);

      if (fRet)
      {
        ROMHDR *pTOCLoc = (ROMHDR *)pBuffer;

        if(pTOCLoc->physfirst > (DWORD)dwpTOC || pTOCLoc->physlast < (DWORD)dwpTOC){
//          dprintf("NOTICE! Record %d looked like a TOC at 0x%08x except Phys first = 0x%08X, and Phys last = 0x%08X\r\n", i, dwpTOC, pTOC->physfirst, pTOC->physlast);
            continue;
        }
    
        if((pTOCLoc->physfirst >= (dwImageStart - dwROMOffsetRead)) &&
           (pTOCLoc->physlast  <= (dwImageStart - dwROMOffsetRead) + dwImageLength))
        {

          //
          // Extra sanity check...
          //
          if((DWORD)(HIWORD(pTOCLoc->dllfirst) << 16) <= pTOCLoc->dlllast && 
             (DWORD)(LOWORD(pTOCLoc->dllfirst) << 16) <= pTOCLoc->dlllast)
          { 

						dprintf("Found pTOC  = 0x%08x\n", (DWORD)dwpTOC);
            fFoundIt = true;
            break;
          } 
          else 
          {
            dprintf("NOTICE! Record %d looked like a TOC except DLL first = 0x%08X, and DLL last = 0x%08X\r\n", i, pTOCLoc->dllfirst, pTOCLoc->dlllast);
          }
        }
      }
    }

    if(i == dwNumRecords - 1)
    {
      i = state - 1;
      dwpTOC = 0;
    }
  }

  if (fFoundIt)
  {
    dwROMOffset = dwROMOffsetRead;
    pTOC = (DWORD)dwpTOC;

    dprintf("rom offset = 0x%08X\n", dwROMOffset);
    
    return true;
  } 

  dwROMOffset = 0;

  return false;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

DWORD CBinMod::FindHole(DWORD size, ROMHDR *pToc){
  for(DWORD i = dwNumRecords - 3; i; i--)
  {
    if(Records[i].dwStartAddress > pToc->physfirst + dwROMOffset &&
       Records[i].dwStartAddress + Records[i].dwLength < pToc->physlast + dwROMOffset)
    {
      if(Records[i + 1].dwStartAddress - (Records[i].dwStartAddress + Records[i].dwLength) >= size)
      {
        dprintf("Found probable hole at end of record %d\n", i);

        // shift end of image down to allow appending to this record.

        DWORD insertion_point = SetFilePointer(hFile, 
                                               Records[i].dwFilePointer + Records[i].dwLength,
                                               NULL,
                                               FILE_BEGIN);

        if(insertion_point == INVALID_SET_FILE_POINTER){
          dprintf("Error: failed setting file pointer\n");
          goto exit;
        }
        
        DWORD shift_size = my_GetFileSize(hFile, NULL) - insertion_point;
        BYTE *buffer = (BYTE*)new BYTE[shift_size];
        if(!buffer){
          dprintf("Error: failed allocating buffer\n");
          goto exit;
        }

        DWORD cb = 0;
        if(!my_ReadFile(hFile, buffer, shift_size, &cb, NULL)){
          dprintf("Error: failed reading shift data\n");
          goto exit;
        }

        DWORD shift_point = my_SetFilePointer(hFile, 
                                           insertion_point + size,
                                           NULL,
                                           FILE_BEGIN);
        
        if(shift_point == INVALID_SET_FILE_POINTER){
          dprintf("Error: failed setting file pointer\n");
          goto exit;
        }

        if(!my_WriteFile(hFile, buffer, shift_size, &cb, NULL)){
          dprintf("Error: failed to write shifted data\n");
          goto exit;
        }

        DWORD insertion_address = Records[i].dwStartAddress + Records[i].dwLength + dwROMOffset;

        // checksum will get updated with data write but length needs to be fudged or write will fail.
        Records[i].dwLength += size;

        DWORD record_point = my_SetFilePointer(hFile,
                                            Records[i].dwFilePointer - 3 * sizeof(DWORD),
                                            NULL,
                                            FILE_BEGIN);

        if(record_point == INVALID_SET_FILE_POINTER){
          dprintf("Error: failed setting file pointer\n");
          goto exit;
        }
 
        if(!my_WriteFile(hFile, &Records[i], 3 * sizeof(DWORD), &cb, NULL)){
          dprintf("Error: failed writing record header\n");
          goto exit;
        }

        return insertion_address;
      }
    }
  }

exit:
  return 0;
}

void CBinMod::dprintf(char *message, ...){
  va_list ap;
  va_start(ap, message);

  if(fpLog)
    vfprintf(fpLog, message, ap);
  else
    vprintf(message, ap);

  va_end(ap);
}

