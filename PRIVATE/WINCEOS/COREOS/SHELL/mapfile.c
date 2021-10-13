//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*---------------------------------------------------------------------------*\
 *
 *  module: mapfile.c
 *
\*---------------------------------------------------------------------------*/


#include "windows.h"
#include "string.h"
#include "stdlib.h"
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////////////
// Binary search support function

PVOID
bsearch( 
   const void *key,
   const void *base,
   size_t      num,
   size_t      width,
   int ( __cdecl *compare ) ( const void *, const void *) 
)
{
	PVOID pResult = NULL;
	DWORD dwLower, dwUpper, dwMiddle;
	int   compareResult;
	PBYTE pMiddleArrayElement;

	if (num == 0)
		return NULL;

	dwLower = 0;
	dwUpper = num - 1;

	while (dwLower <= dwUpper)
	{
		// Bisect [dwLower..dwUpper] with dwMiddle
		dwMiddle = dwLower + (dwUpper - dwLower) / 2;
		pMiddleArrayElement = (PBYTE)base + dwMiddle * width;

		compareResult = compare(key, pMiddleArrayElement);
		if (compareResult == 0)
		{
			// Match found, return it.
			pResult = pMiddleArrayElement;
			break;
		}
		else if (compareResult < 0)
		{
			// key is less than pCurArrayElement, so dwMiddle - 1
			// is our new upper bound
			if (dwMiddle == 0)
				break;
			dwUpper = dwMiddle - 1;
		}
		else
		{
			// key is greater than pCurArrayElement, so dwMiddle + 1
			// is our new lower bound
			dwLower = dwMiddle + 1;
		}
	}
	return pResult;
}

//
////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////
// Support functions for reading text file
//

#define TEXT_FILE_BUFFER_SIZE 4096

typedef struct
{
	HANDLE hFile;
	BOOL   bEndOfFileReached;
	PBYTE  pInputBuffer;
	DWORD  cbInputBuffer;
	DWORD  dwInputBufferOffset;
} TEXTFILE, *PTEXTFILE;

PTEXTFILE
TextFileOpen(
	PWSTR wszFileName)
{
	PTEXTFILE pFile;

	pFile = (PTEXTFILE)LocalAlloc(LPTR, sizeof(*pFile) + TEXT_FILE_BUFFER_SIZE);
	if (pFile)
	{
		pFile->hFile = CreateFile(wszFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
		if (INVALID_HANDLE_VALUE == pFile->hFile)
		{
			LocalFree(pFile);
			pFile = NULL;
		}
		else
		{
			pFile->pInputBuffer = (PBYTE)(pFile + 1);
		}
	}
	return pFile;
}

void
TextFileClose(
    PTEXTFILE pFile)
{
	if (pFile)
	{
		CloseHandle(pFile->hFile);
		LocalFree(pFile);
	}
}

DWORD
TextFileGetNextLine(
    IN  OUT PTEXTFILE pFile,
	OUT     PCHAR     szLineBuffer,
	IN      DWORD     cbLineBuffer)
//
//  Return the next line from the file.
//  The returned line is null terminated.
//  If no lines remain, return an error.
//
{
	DWORD dwResult = NO_ERROR;
	DWORD bytesToRead,
		  bytesRead,
		  bytesToCopy;
	PCHAR pchNewline,
		  pchNext;

	while (dwResult == NO_ERROR)
	{
		pchNext = pFile->pInputBuffer + pFile->dwInputBufferOffset;

		// If there is a line in the input buffer, return it
		pchNewline = memchr(pchNext, '\n', pFile->cbInputBuffer);

		if (!pchNewline
		&&  pFile->cbInputBuffer == TEXT_FILE_BUFFER_SIZE)
		{
			// The buffer is full but contains no newline,
			// (i.e. line longer than buffer size), so treat current buffer as a line
			pchNewline = pFile->pInputBuffer + TEXT_FILE_BUFFER_SIZE - 1;
		}

		if (pchNewline)
		{
			bytesToCopy = pchNewline - pchNext;

			ASSERT(bytesToCopy < pFile->cbInputBuffer);

			// Remove the bytes we will copy and the '\n' from the input buffer
			pFile->dwInputBufferOffset += bytesToCopy + 1;
			pFile->cbInputBuffer -= bytesToCopy + 1;

			ASSERT(pFile->dwInputBufferOffset <= TEXT_FILE_BUFFER_SIZE);
			ASSERT(pFile->cbInputBuffer < TEXT_FILE_BUFFER_SIZE);

			// If the user provided buffer is too small, truncate the returned line
			if (bytesToCopy >= cbLineBuffer)
				bytesToCopy = cbLineBuffer - 1;
			memcpy(szLineBuffer, pchNext, bytesToCopy);
			szLineBuffer[bytesToCopy] = '\0';
			break;
		}
		else if (!pFile->bEndOfFileReached)
		{
			// Try to read in more data from file to the input buffer

			// Shift remaining data in input buffer to start of buffer
			memcpy(pFile->pInputBuffer, pFile->pInputBuffer + pFile->dwInputBufferOffset, pFile->cbInputBuffer);
			pFile->dwInputBufferOffset = 0;

			// Read in data to fill to end of buffer
			bytesToRead = TEXT_FILE_BUFFER_SIZE - pFile->cbInputBuffer;
			if (FALSE == ReadFile(pFile->hFile, pFile->pInputBuffer + pFile->cbInputBuffer, bytesToRead, &bytesRead, NULL))
			{
				pFile->bEndOfFileReached = TRUE;
				dwResult = GetLastError();
			}
			else if (bytesRead == 0)
			{
				pFile->bEndOfFileReached = TRUE;
				dwResult = ERROR_NO_MORE_ITEMS;
			}
			else
			{
				pFile->cbInputBuffer += bytesRead;
				ASSERT(pFile->cbInputBuffer <= TEXT_FILE_BUFFER_SIZE);
			}
		}
		else
		{
			dwResult = ERROR_NO_MORE_ITEMS;
		}
	}

	return dwResult;
}

//
////////////////////////////////////////////////////////////////////////////////////////

/*

Format of map file symbol table section:
  Address         Publics by Value              Rva+Base     Lib:Object

 0001:0000001c       ??_C@_1DA@OPLK@?$AA?9?$AAC?$AAT?$AAE?$AAp?$AAI?$AAn?$AAi?$AAt?$AAi?$AAa?$AAl?$AAi?$AAz?$AAe?$AA?$CI?$AA?$CJ?$AA?5?$AAE?$AAn@ 1000101c     cxport_ALL:cxport.obj

 */

#define MAXSYMBOLNAME 64

#define ADDRESS_MASK 0x01FFFFFF

typedef struct
{
	DWORD address;
	DWORD length;
	CHAR  name[MAXSYMBOLNAME];
}
MAPSYMBOL, *PMAPSYMBOL;

typedef struct _MAPFILE
{
	struct _MAPFILE *next;

	WCHAR      wszModuleName[MAX_PATH]; // e.g. L"coredll.dll"

	// The symbol table is sorted with the smallest address
	// first and the highest address last.
	PMAPSYMBOL pSymbolTable;
	DWORD      sizeSymbolTable;

	DWORD      numSymbolTableEntries;
} MAPFILE, *PMAPFILE;

#define SYMBOL_TABLE_GROWTH_INCREMENT 100

PMAPFILE g_MapFileList = NULL;



int
SymbolCompare(
   IN  const void *pSym1,
   IN  const void *pSym2)
//
// Compare the addresses in two symbol table elements.
//
{
	PMAPSYMBOL pSymbol1 = (PMAPSYMBOL)pSym1; 
	PMAPSYMBOL pSymbol2 = (PMAPSYMBOL)pSym2; 

	return pSymbol1->address - pSymbol2->address;
}

void
MapfileFree(
	IN  PMAPFILE pMapFile)
{
	LocalFree(pMapFile->pSymbolTable);
	LocalFree(pMapFile);
}

DWORD
MapfileLoad(
	IN  PWSTR     wszModuleName, // e.g. L"coredll.dll"
	OUT PMAPFILE  *ppMapFile)
//
//  Open the mapfile for the specified module, and
//  read in its table of function name / address values.
//
{
	WCHAR    wszMapFileName[MAX_PATH];
	PWCHAR   pDot;
	PMAPFILE pNewMapFile;
	PTEXTFILE ptfMapFile;
	CHAR     szLineBuffer[512];
	DWORD    dwResult = NO_ERROR;
	PMAPSYMBOL pNewSymbolTable,
		       pCurSymbol,
			   pPrevSymbol;
	PCHAR     szToken, pOrigName;

	pNewMapFile = (PMAPFILE)LocalAlloc(LPTR, sizeof(MAPFILE));
	if (NULL == pNewMapFile)
	{
		dwResult = ERROR_OUTOFMEMORY;
	}
	else
	{
		wcscpy(pNewMapFile->wszModuleName, wszModuleName);

		//
		// Build the mapfile name, e.g. L"\Release\coredll.map"
		// 
		wsprintf(wszMapFileName, L"\\Release\\%s", wszModuleName);
		pDot = wcschr(wszMapFileName, L'.');
		if (pDot == NULL)
			pDot = wszMapFileName + wcslen(wszMapFileName);
		wcscpy(pDot, L".map");

		//
		// Open the mapfile
		//
		ptfMapFile = TextFileOpen(wszMapFileName);
		if (NULL == ptfMapFile)
		{
			dwResult = GetLastError();
		}
		else
		{
			// Skip over lines preceding the symbol table
			while (NO_ERROR == TextFileGetNextLine(ptfMapFile, szLineBuffer, sizeof(szLineBuffer)))
			{
				PSTR szFirstWord;

				szFirstWord = szLineBuffer;
				while (*szFirstWord == ' ')
					szFirstWord++;

				if (strncmp(szFirstWord, "Address", 7) == 0)
				{
					// Skip blank line following the "Address" line...
					TextFileGetNextLine(ptfMapFile, szLineBuffer, sizeof(szLineBuffer));
					break;
				}
			}

			// Load all the symbols
			while (NO_ERROR == TextFileGetNextLine(ptfMapFile, szLineBuffer, sizeof(szLineBuffer)))
			{
				// Grow the symbol table if needed
				if (pNewMapFile->numSymbolTableEntries == pNewMapFile->sizeSymbolTable)
				{
					pNewMapFile->sizeSymbolTable += SYMBOL_TABLE_GROWTH_INCREMENT;
					pNewSymbolTable = (PMAPSYMBOL)LocalAlloc(LPTR, sizeof(MAPSYMBOL) * pNewMapFile->sizeSymbolTable);
					if (pNewSymbolTable == NULL)
					{
						dwResult = ERROR_OUTOFMEMORY;
						break;
					}
					memcpy(pNewSymbolTable, pNewMapFile->pSymbolTable, sizeof(MAPSYMBOL) * pNewMapFile->numSymbolTableEntries);
					LocalFree(pNewMapFile->pSymbolTable);
					pNewMapFile->pSymbolTable = pNewSymbolTable;
				}

				//
				// Each line is of the format:
				//  0001:00001a75       _dllentry                  10002a75 f   cxport_ALL:cxport.obj
				//

				// Get and check the '0001:00001a75' field
				szToken = strtok(szLineBuffer, " ");
				if (strlen(szToken) < 13 || szToken[4] != ':')
					continue;

				pCurSymbol = &pNewMapFile->pSymbolTable[pNewMapFile->numSymbolTableEntries];
				pPrevSymbol = NULL;
				if (pNewMapFile->numSymbolTableEntries > 0)
					pPrevSymbol = pCurSymbol - 1;

				// Get and save the symbol name
				pOrigName = szToken = strtok(NULL, " ");
				if (szToken == NULL)
					continue;
				strncpy(pCurSymbol->name, szToken, MAXSYMBOLNAME);
				pCurSymbol->name[MAXSYMBOLNAME-1] = '\0';

				// Get the address
				szToken = strtok(NULL, " ");
				if (szToken == NULL)
					continue;
				pCurSymbol->address = strtol(szToken, NULL, 16) & ADDRESS_MASK;

				// Use the address to set the size of the previous symbol table entry, if needed
				if (pPrevSymbol
				&&  pPrevSymbol->length == 0)
				{
					pPrevSymbol->length = pCurSymbol->address - pPrevSymbol->address;
				}

				// Ignore symbols whose first character in their name is a '?'
				if (pCurSymbol->name[0] == '?') {
                    PCHAR   pClass, pEndClass;
                    
                    // some decorated C++ name
                    pOrigName++;    // Move past ?

                    // Look for the delim
                    for (pClass = pOrigName; ('\0' != *pClass) && ('@' != *pClass); pClass++) {
                        ;
                    }
                    pEndClass = pClass;
                    if ('@' == *pClass) {
                        *pClass = '\0';
                        pClass++;
                        for (pEndClass = pClass; ('\0' != *pEndClass) && ('@' != *pEndClass); pEndClass++) {
                            ;
                        }
                        *pEndClass = '\0';
                    }
                    if (pClass != pEndClass) {
                        strncpy (pCurSymbol->name, pClass, MAXSYMBOLNAME);
                        pCurSymbol->name[MAXSYMBOLNAME-1] = '\0';
                        strncat (pCurSymbol->name, "::", MAXSYMBOLNAME-strlen(pCurSymbol->name));
                        pCurSymbol->name[MAXSYMBOLNAME-1] = '\0';
                        strncat (pCurSymbol->name, pOrigName, MAXSYMBOLNAME-strlen(pCurSymbol->name));
                        pCurSymbol->name[MAXSYMBOLNAME-1] = '\0';
                    } else {
                        strncpy (pCurSymbol->name, pOrigName, MAXSYMBOLNAME);
                        pCurSymbol->name[MAXSYMBOLNAME-1] = '\0';
                    }
                }

				// We are only interested in function symbols, which have " f " after the address
				szToken = strtok(NULL, " ");
				if (!szToken || strcmp(szToken, "f") != 0)
					continue;

				// Add the new entry to the table
				pNewMapFile->numSymbolTableEntries++;
			}

			TextFileClose(ptfMapFile);
		}
	}

	if (dwResult != NO_ERROR)
	{
		MapfileFree(pNewMapFile);
		pNewMapFile = NULL;
	}
	else
	{
		// Trim the symbol table unused entries
		pNewSymbolTable = (PMAPSYMBOL)LocalReAlloc(pNewMapFile->pSymbolTable, sizeof(MAPSYMBOL) * pNewMapFile->numSymbolTableEntries, 0);
		if (pNewSymbolTable)
		{
			pNewMapFile->sizeSymbolTable = pNewMapFile->numSymbolTableEntries;
			pNewMapFile->pSymbolTable = pNewSymbolTable;
		}

		// Sort the symbol table
		qsort( pNewMapFile->pSymbolTable, pNewMapFile->numSymbolTableEntries, sizeof(MAPSYMBOL), SymbolCompare );

	}

	*ppMapFile = pNewMapFile;

	return dwResult;
}

int
compareAddress(
   IN  const void *pKey,
   IN  const void *pArray)
//
// Compare an address against an element of the symbol table.
// If the address is less than the symbol table element start, return -1.
// If the address falls in the range for the symbol table element, return 0.
// Otherwise return 1.
//
{
	DWORD      dwAddress = *(PDWORD)pKey;
	PMAPSYMBOL pSymbol = (PMAPSYMBOL)pArray; 

	if (dwAddress < pSymbol->address)
		return -1;
	else if (dwAddress <= pSymbol->address + pSymbol->length - 1)
		return 0;
	else
		return 1;
}



PSTR
MapfileLookupAddress(
	IN  PWSTR wszModuleName,
	IN  DWORD dwAddress)
//
//  Return the function name at the specified address of the specified module if known.
//  Return "" if unknown.
//
{
	PMAPFILE   pMapFile;
	PMAPSYMBOL pSymbol;
	DWORD      dwResult;
	PSTR       szSymbolName  = "";

	// Use already loaded
	for (pMapFile = g_MapFileList; pMapFile; pMapFile = pMapFile->next)
	{
		if (wcscmp(pMapFile->wszModuleName, wszModuleName) == 0)
			break;
	}

	if (pMapFile == NULL)
	{
		dwResult = MapfileLoad(wszModuleName, &pMapFile);
		if (dwResult == NO_ERROR)
		{
			pMapFile->next = g_MapFileList;
			g_MapFileList = pMapFile;
		}
	}

	if (pMapFile)
	{
		pSymbol = (PMAPSYMBOL) bsearch(&dwAddress, 
										pMapFile->pSymbolTable, 
										pMapFile->numSymbolTableEntries, 
										sizeof(pMapFile->pSymbolTable[0]),
										compareAddress);

		if (pSymbol)
			szSymbolName = pSymbol->name;
	}

	return szSymbolName;
}
