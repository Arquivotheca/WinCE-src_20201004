/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Copyright (c) 1998 Microsoft Corporation

Module Name:

	diffbin.cpp

Abstract:

	This program constructs a binary diff between two files.  It has
	been tailored to the NK image structure for the purpose of
	cutting image download times, but the basic algorithm can be
	applied to any two files.



	Brian Wahlert (t-brianw) 8-Jul-1998

Environment:

	Non-specific

Revision History:

	 8-Jul-1998 Brian Wahlert (t-brianw)    Created
    30-Jun-2001 Scott Shell (ScottSh)       Ported from nkcompr    

-------------------------------------------------------------------*/
#include "diffbin.h"

// Debugging Zones
DWORD   g_dwZoneMask        = 0x00000001;

HRESULT
IWriteFile(HANDLE hfOutput, LPBYTE pb, DWORD cb, DWORD *pdwBytesWritten)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    HRESULT         hr          = NOERROR;
    DWORD           dwWritten   = 0;

    ASSERT(pdwBytesWritten);

    CBR(WriteFile(hfOutput, pb, cb, &dwWritten, NULL));
    CBRA(cb == dwWritten);

    *pdwBytesWritten += dwWritten;
Error:
    return hr;

} /* IWriteFile()
   */

HRESULT 
WriteInt(HANDLE hfOutput, UINT32 i, UCHAR cBytes, DWORD *pdwBytesWritten)
/*---------------------------------------------------------------------------*\
 * Write the low-order "cBytes" bytes of "i" to file hfOutput
\*---------------------------------------------------------------------------*/
{
    return IWriteFile(hfOutput, (LPBYTE)&i, cBytes, pdwBytesWritten);

} /* WriteInt()
   */

#define VARINT_PAGE_ALIGN_BIT       0x40
#define VARINT_CONTINUATION_BIT     0x80

HRESULT
WriteVarInt(HANDLE hfOutput, UINT32 u, DWORD *pdwBytesWritten)
/*---------------------------------------------------------------------------*\
 * Writes a variable length integer using the minimal number of bytes necessary
\*---------------------------------------------------------------------------*/
{
    HRESULT     hr      = NOERROR;
    BYTE        b;

    ASSERT(pdwBytesWritten);

    do {
        b = u & 0x7f;
        if(u >>= 7) {
            b |= VARINT_CONTINUATION_BIT;
        }
        if(hfOutput) {
            CHR(IWriteFile(hfOutput, &b, 1, pdwBytesWritten));
        } else {
            *pdwBytesWritten++;
        }
    } while(u > 0);

Error:
    return hr;

} /* WriteVarInt()
   */

HRESULT
WriteAddrVarInt(HANDLE hfOutput, UINT32 u, DWORD *pdwBytesWritten)
/*---------------------------------------------------------------------------*\
 * Writes a section address
 * A section address is special in that a very large number of them are page
 * aligned, which means they end in 000.  Spending an extra bit to represent
 * that page alignment saves quite a few bytes;
\*---------------------------------------------------------------------------*/
{
    HRESULT     hr      = NOERROR;
    BYTE        b       = 0;

    ASSERT(pdwBytesWritten);

    // Check for page alignment
    if((u & 0xFFF) == 0) {
        // Page aligned!
        u >>= 12;
        b |= VARINT_PAGE_ALIGN_BIT;
    }

    b |= u & 0x3f;
    if(u >>= 6) {
        b |= VARINT_CONTINUATION_BIT;
    }
    if(hfOutput) {
        CHR(IWriteFile(hfOutput, &b, 1, pdwBytesWritten));
    } else {
        *pdwBytesWritten++;
    }

    // If this is going to be longer than 1 byte, the rest of the bytes
    // look just like a normal VarInt, so call that on the remainder.
    if(u) {
        CHR(WriteVarInt(hfOutput, u, pdwBytesWritten));
    }

Error:
    return hr;

} /* WriteVarInt()
   */

HRESULT
WriteDataToken(HANDLE hfOutput, UINT32 cWords, DWORD *pdwBytesWritten)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    HRESULT         hr          = NOERROR;
    UCHAR           chWrite;

    // pack "iType" and "cWords" into between one and
	// four words
	UCHAR cBytes = 0;
	UINT32 i;
	chWrite = TOKEN_DATA | (UCHAR)(cWords << 3);
    if (cWords < (1 << 5)) {
		cBytes = 0;
    } else if (cWords < (1 << 13)) {
		cBytes = 1;
    } else if (cWords < (1 << 21)) {
		cBytes = 2;
    } else {
		cBytes = 3;
    }
	
    chWrite |= cBytes << 1;
    CHR(IWriteFile(hfOutput, &chWrite, 1, pdwBytesWritten));

    for (i = 0; i < cBytes; ++i) {
		chWrite = (UCHAR)(cWords >> (5 + 8 * i));
        CHR(IWriteFile(hfOutput, &chWrite, 1, pdwBytesWritten));
	}

Error:
    return hr;

} /* WriteDataToken()
   */

HRESULT
WriteCopyToken(HANDLE hfOutput, UINT32 cWords, ADDRESS iOffset, 
               DWORD cComprRgns, DWORD *pdwBytesWritten)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    HRESULT         hr          = NOERROR;
    UCHAR           chWrite;
	UCHAR           cBytes = 0;
	UINT32          i;

	chWrite = TOKEN_COPY;
	chWrite |= ((iOffset.iOffset != NO_COMPRESS) << 1);
	chWrite |= (UCHAR)(cWords << 4);

    if (cWords < (1 << 4)) {
		cBytes = 0;
    } else if (cWords < (1 << 12)) {
		cBytes = 1;
    } else if (cWords < (1 << 20)) {
		cBytes = 2;
    } else {
		cBytes = 3;
    }
	chWrite |= cBytes << 2;
    CHR(IWriteFile(hfOutput, &chWrite, 1, pdwBytesWritten));

    for (i = 0; i < cBytes; ++i) {
		chWrite = (UCHAR)(cWords >> (4 + 8 * i));
        CHR(IWriteFile(hfOutput, &chWrite, 1, pdwBytesWritten));
	}

    if (iOffset.iOffset == NO_COMPRESS) {
        CHR(WriteInt(hfOutput, iOffset.iAddr, 3, pdwBytesWritten));

    } else {
        if (cComprRgns < (1 << 8)) {
            CHR(WriteInt(hfOutput, iOffset.iAddr, 1, pdwBytesWritten));
        } else if (cComprRgns < (1 << 16)) {
            CHR(WriteInt(hfOutput, iOffset.iAddr, 2, pdwBytesWritten));
        } else {
            CHR(WriteInt(hfOutput, iOffset.iAddr, 3, pdwBytesWritten));
        }
        CHR(WriteInt(hfOutput, iOffset.iOffset, 3, pdwBytesWritten));
	}

Error:
    return hr;

} /* WriteCopyToken()
   */

HRESULT
ComputeRuns(CRunList *pRunList, UINT32 *pcRunLen, UINT32* pcOffset, 
            LPBYTE pbSection, UINT32 cLen)
/*---------------------------------------------------------------------------*\
 * Here we go through all of the common substrings and condense that 
 * information down into a list of matching runs
\*---------------------------------------------------------------------------*/
{
    HRESULT     hr              = NOERROR;
	UINT32      iChar           = 0;
    BOOL        fLastRunData    = FALSE;

	while (iChar < cLen) {
		pcRunLen[iChar] = (pcRunLen[iChar] == 0) ? 1 : pcRunLen[iChar];

		if (pcRunLen[iChar] >= HASH_BYTES) {
            // If we have a long enough run, let's make it a copy
			fLastRunData = FALSE;
            CHR(pRunList->Insert(RUNTYPE_COPYTOKEN, pcRunLen[iChar], pcOffset[iChar]));

        } else {
            // Okay, this run is not long enough to make it a copy. Let's
            // make a data run instead... 
			if (fLastRunData) {
                // If the last run was a data run as well, let's just add on
                // to the previous run
                CHR(pRunList->AddToLastRunLength(pcRunLen[iChar]));
            } else {
                // The last run was a copy run, so we need to start a new data
                // run here.
                CHR(pRunList->Insert(RUNTYPE_DATATOKEN, pcRunLen[iChar], (DWORD)pbSection + iChar));
                fLastRunData = TRUE;
            }
		}
		iChar += pcRunLen[iChar];
	}

Error:
    return hr;

} /* ComputeRuns()
   */

HRESULT
HashOldImage(CImageData *pOldImg, CHashTable *pHT, CSecDiv *pSecDiv)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    HRESULT             hr              = NOERROR;
	UINT32              iSecDiv         = 0;
    LPBYTE              pbOldImage      = pOldImg->GetUncompressedDataPtr();
    DWORD               cbOldImage      = pOldImg->GetUncompressedImageLength();
    LPBYTE              pbCurSecDiv     = NULL;
    int                 i;

	RETAILMSG(ZONE_VERBOSE, (TEXT("\nHashing old image file\n")));
	RETAILMSG(ZONE_PROGRESS, (TEXT("Bytes hashed, out of %u:         "), cbOldImage));

    CBRA(pbCurSecDiv = pSecDiv->GetSecDiv(iSecDiv));

	for (i = 0; i <= cbOldImage - HASH_BYTES; ++i) {
        if (i % 10000 == 0) {
			RETAILMSG(ZONE_PROGRESS, (TEXT("\b\b\b\b\b\b\b\b%8u"), i));
        }

		if (pbCurSecDiv - (pbOldImage + i) < HASH_BYTES) {
            if (pSecDiv->GetSecDiv(iSecDiv) == pbOldImage + i) {
                iSecDiv++;
                CBRA(pbCurSecDiv = pSecDiv->GetSecDiv(iSecDiv));

                // Used to be: iSecDiv = (iSecDiv + 1) % cSecDiv;
                // Don't see why we should ever loop around, hence why bother with the %?
                // Just to handle the top boundary case?  (now handled inside GetSecDiv)
            }
        } else {
			pHT->Insert(pbOldImage + i, i);
        }
	}

	RETAILMSG(ZONE_PROGRESS, (TEXT("\b\b\b\b\b\b\b\b%8u\n"), cbOldImage));

Error:
    return hr;

} /* HashOldImage()
   */

HRESULT
ComputeSubstrings(CImageData *pOldImg, CImageData *pNewImg,  CSecDiv *pSecDiv, 
                  CRunList *pRunList)
/*---------------------------------------------------------------------------*\
 * This is the heart of the program.  We determine all substrings that imgOld 
 * and imgNew have in common.  When we're done, "piOffset[i]" is the offset 
 * into imgOld of the beginning of the longest substring that matches imgNew 
 * beginning at offset i.  "pcMatched[i]" is the length of that matching 
 * substring.
\*---------------------------------------------------------------------------*/
{
	UINT32*             piCurMatched;
	UINT32*             pcCurMatched;
	UINT32              cCurMatched;
	UINT32*             piPrevMatched;
	UINT32*             pcPrevMatched;
	UINT32              cPrevMatched;
	UINT32              iPrevMatched;
    UINT32              j;
    UINT32              k;
	UINT32*             pcMatched;
	UINT32*             piPlace;

    HRESULT             hr                  = NOERROR;
	UINT32              i;
    DWORD               dwSectionIndex;
    DWORD               dwSectionTokenLen   = 0;
    LPBYTE              pbOldImage          = pOldImg->GetUncompressedDataPtr();
    LPBYTE              pbNewImage          = pNewImg->GetUncompressedDataPtr();
    DWORD               cbNewImage          = pNewImg->GetUncompressedImageLength();
    CRunList            SectionRunList;           
	CHashTable          HashTable;

    // $REVIEW: This should be pOldImg, right?!?
    CHR(SectionRunList.Initialize(pOldImg));

    CHR(HashTable.Initialize(1 << LOG_NUM_BUCKETS));

    CHR(HashOldImage(pOldImg, &HashTable, pSecDiv));

    //
    // Allocate and initialize data structures
    //
    
    CPR(piCurMatched = (UINT32*)LocalAlloc(LMEM_FIXED, HashTable.GetMaxBucketSize() * sizeof(UINT32)));
	CPR(pcCurMatched = (UINT32*)LocalAlloc(LMEM_FIXED, HashTable.GetMaxBucketSize() * sizeof(UINT32)));
	cCurMatched = 0;

	CPR(piPrevMatched = (UINT32*)LocalAlloc(LMEM_FIXED, HashTable.GetMaxBucketSize() * sizeof(UINT32)));
	CPR(pcPrevMatched = (UINT32*)LocalAlloc(LMEM_FIXED, HashTable.GetMaxBucketSize() * sizeof(UINT32)));
	cPrevMatched = 0;
	iPrevMatched = 0;

	CPR(pcMatched = (UINT32*)LocalAlloc(LMEM_FIXED, pNewImg->GetMaxBytesUncompressed() * sizeof(UINT32)));
	CPR(piPlace = (UINT32*)LocalAlloc(LMEM_FIXED, pNewImg->GetMaxBytesUncompressed() * sizeof(UINT32)));

	RETAILMSG(ZONE_VERBOSE, (TEXT("\nComputing common substrings\n")));
	RETAILMSG(ZONE_PROGRESS, (TEXT("Bytes finished, out of %u:         "), cbNewImage));

    for(dwSectionIndex = 0; dwSectionIndex < pNewImg->GetSectionCount(); dwSectionIndex++) {

        CSectionData *  pSection = pNewImg->GetSection(dwSectionIndex);
        CBRA(pSection);

        LPBYTE pbSection = pSection->GetUncompressedDataPtr();
        UINT32 cSecLen   = pSection->GetUncompressedSize();

        CHR(pRunList->Insert(RUNTYPE_SECTIONHEADER, 0, (DWORD)pSection));

        CHR(SectionRunList.Clear());
        
		cCurMatched = 0;
		cPrevMatched = 0;
		iPrevMatched = 0;

		if (cSecLen >= HASH_BYTES)
		{
			FillMemory(pcMatched, cSecLen * sizeof(UINT32), 0);
			for (j = cSecLen - HASH_BYTES; j <= cSecLen - HASH_BYTES; j -= (HASH_BYTES / 2))
			{
				HashBucketExt hbe = HashTable.GetBucket(pbSection + j);
				UINT32* pcTemp;
				UINT32* piTemp;
				for (i = hbe.phb->cCount - 1; i < hbe.phb->cCount; --i)
				{
					UINT32 iOffset = hbe.phb->piOffsets[i];
					if (hbe.fOneChar || !memcmp(pbOldImage + iOffset, pbSection + j, HASH_BYTES))
					{
						piCurMatched[cCurMatched] = iOffset;
                        while ((iPrevMatched < cPrevMatched) && (piPrevMatched[iPrevMatched] > iOffset + HASH_BYTES / 2)) {
							++iPrevMatched;
                        }
                        if ((iPrevMatched < cPrevMatched) && (piPrevMatched[iPrevMatched] == iOffset + HASH_BYTES / 2)) {
							pcCurMatched[cCurMatched] = max(HASH_BYTES, pcPrevMatched[iPrevMatched] + HASH_BYTES / 2);
                        } else {
							k = 0;
							while ((k < HASH_BYTES / 2) &&
								   (HASH_BYTES + j + k < cSecLen) &&
								   (!pSecDiv->IsSecDiv(pbOldImage + iOffset + HASH_BYTES + k)) &&
                                   (pbOldImage[iOffset + HASH_BYTES + k] == pbSection[HASH_BYTES + j + k])) {
								++k;
                            }
							pcCurMatched[cCurMatched] = HASH_BYTES + k;
						}
						if ((pcMatched[j] < HASH_BYTES / 2) || (pcCurMatched[cCurMatched] > pcMatched[j] - HASH_BYTES / 2))
						{
							k = 1;
							while ((k < HASH_BYTES / 2) &&
								   (j >= k) &&
								   (!pSecDiv->IsSecDiv(pbOldImage + iOffset - k)) &&
								   (pbOldImage[iOffset - k] == pbSection[j - k]))
							{
								if (pcCurMatched[cCurMatched] + k > pcMatched[j - k])
								{
									pcMatched[j - k] = pcCurMatched[cCurMatched] + k;
									piPlace[j - k] = iOffset - k;
								}
								++k;
							}
						}
						if (pcCurMatched[cCurMatched] > pcMatched[j])
						{
							pcMatched[j] = pcCurMatched[cCurMatched];
							piPlace[j] = piCurMatched[cCurMatched];
						}
						++cCurMatched;
					}
				}
				// switch prev and cur lines
				pcTemp = pcCurMatched;
				pcCurMatched = pcPrevMatched;
				pcPrevMatched = pcTemp;
				
				piTemp = piPrevMatched;
				piPrevMatched = piCurMatched;
				piCurMatched = piTemp;
				
				cPrevMatched = cCurMatched;
				iPrevMatched = 0;
				cCurMatched = 0;

                if ((pbSection - pbNewImage + cSecLen - j) % 10000 < HASH_BYTES / 2) {
					RETAILMSG(ZONE_PROGRESS, (TEXT("\b\b\b\b\b\b\b\b%8u"), (pbSection - pbNewImage + cSecLen - j) / 10000 * 10000));
                }
			}
			CHR(ComputeRuns(&SectionRunList, pcMatched, piPlace, pbSection, cSecLen));
		}

        CHR(SectionRunList.GetTotalLength(&dwSectionTokenLen));

        RETAILMSG(ZONE_VERBOSE, (TEXT("ComputeSubstrings: Addr=0x%08lx  Len=0x%08lx  RunLen=0x%08lx\n"), 
            pSection->GetSectionHeader()->Address, pSection->GetSectionHeader()->Size, dwSectionTokenLen));
        
        // Now that we've got a compressed version of this section, let's see 
        // if it actually bought us anything at all.
        if ((cSecLen < HASH_BYTES) || (dwSectionTokenLen >= pSection->GetSectionHeader()->Size)) {

            // Either this section was too small to compress, or the compressed
            // size of the image was worse than the original.  Let's leave the 
            // original data section here.
            // Note that since we are using GetDataPtr, we are getting the original
            // version of the section that does not have fixups applied, so we do 
            // not need to write the fixup sections
            CHR(pSection->SetUncompressedSize(ALL_DATA));

            pRunList->Insert(RUNTYPE_RAWDATA, pSection->GetSectionHeader()->Size, (DWORD)pSection->GetDataPtr());

        } else {
            // The compression saved us something.  Use it!
            
            // Copy the runs that make up the compressedversion of this section
            pRunList->CopyRuns(&SectionRunList);

            // Insert the fixup commands
            CHR(pRunList->Insert(RUNTYPE_FIXUPCOMMANDS, 0, (DWORD)pSection));

            // Insert the compression commands
            CHR(pRunList->Insert(RUNTYPE_COMPRESSIONCOMMANDS, 0, (DWORD)pSection));
		}
	}

	RETAILMSG(ZONE_PROGRESS, (TEXT("\b\b\b\b\b\b\b\b%8u\n\n"), cbNewImage));

    // Insert zero'd section header to mark the end of the file
    CHR(pRunList->Insert(RUNTYPE_ZEROBLOCK, sizeof(ROMIMAGE_SECTION), 0));

Error:

	LocalFree(pcCurMatched);
	LocalFree(pcPrevMatched);
	LocalFree(piCurMatched);
	LocalFree(piPrevMatched);
	LocalFree(pcMatched);
	LocalFree(piPlace);

    return hr;
	
} /* ComputeSubstrings()
   */

////////////////////////////////////////////////////////////
// BuildFile
//   Output the compressed data to the file "name"
////////////////////////////////////////////////////////////
HRESULT
BuildFile(CRunList *pRunList, CImageData *pImgOld, CImageData *pImgNew,  
          LPCSTR szOutputFilename, DWORD *pdwOutputLength)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    HRESULT             hr                      = NOERROR;
    HANDLE              hfOutput                = INVALID_HANDLE_VALUE;
    DWORD               dwWritten               = 0;
    DWORD               cComprRgns;
    DWORD               dwPhysicalStartAddr;
	UINT32              i, j;
    ROMIMAGE_HEADER     rh;
    DWORD               cbRelocCmds             = 0;
    LPBYTE              pbRelocCmds             = NULL;
    DWORD               cbSectionHeaders        = 0;
    DWORD               cbCompressedRgnTable    = 0;
    DWORD               cbCompressionCommands   = 0;
    DWORD               cbHdr                   = 0;
    DWORD               cbVerification          = 0;
    DWORD               cbMisc                  = 0;
    DWORD               cbData                  = 0;
    DWORD               cbCopy                  = 0;
    DWORD               cbFixups                = 0;
    DWORD               dwTemp;

    dwPhysicalStartAddr = pImgOld->GetHeader()->PhysicalStartAddress;

    //
    // Open Output file
    //

    CBR((hfOutput = CreateFile(szOutputFilename, GENERIC_READ | GENERIC_WRITE,
                               0, NULL, CREATE_ALWAYS, 0, NULL)) != INVALID_HANDLE_VALUE);

    //
    // Write DIFF Header
    //

    memcpy(&rh, pImgNew->GetHeader(), sizeof(ROMIMAGE_HEADER));

	rh.Signature[0] = COMPRESSED_INDICATOR;
    CHR(IWriteFile(hfOutput, (LPBYTE)&rh, sizeof(ROMIMAGE_HEADER), &dwWritten));

	CHR(WriteInt(hfOutput, pImgNew->GetUncompressedImageLength(), 3, &dwWritten));

    cbHdr = dwWritten;

    //
    // Write Old BIN verification data
    //
    for(i = 0; i < pImgOld->GetSectionCount(); i++) {
        CSectionData *pSection = pImgOld->GetSection(i);
        CHR(IWriteFile(hfOutput, (LPBYTE)pSection->GetSectionHeader(), sizeof(ROMIMAGE_SECTION), &dwWritten));
    }

    {
        // Write a section full of 0's to mark the end.
        ROMIMAGE_SECTION section;
        memset(&section, 0, sizeof(section));
        CHR(IWriteFile(hfOutput, (LPBYTE)&section, sizeof(section), &dwWritten));
    }

    cbVerification = dwWritten - cbHdr;

    //
    // Write Compressed Region Table
    //

    cComprRgns = pImgOld->GetCompressedRegionCount();
    CHR(WriteInt(hfOutput, cComprRgns, 3, &dwWritten));

    for (i = 0; i < cComprRgns; ++i) {
        COMPR_RGN *pcr = pImgOld->GetCompressedRegion(i);

        CBRA(pcr);
        
        // pcr.iAddress holds the complete 32 bit address
        DWORD dwAddr = pcr->iAddress;

        dwAddr -= dwPhysicalStartAddr;

        // Make sure that the address we're trying to write is within 16MB of the start...
//        ASSERT((dwAddr & 0x00FFFFFF) == dwAddr);

        CHR(WriteInt(hfOutput, dwAddr, 3, &dwWritten));
        CHR(WriteInt(hfOutput, pcr->cBytesCompressed, 3, &dwWritten));
        CHR(WriteInt(hfOutput, pcr->cBytesUncompressed, 3, &dwWritten));
    }

    cbCompressedRgnTable = dwWritten - cbVerification - cbHdr;

    //
    // Write Token stream
    //
    for(i = 0; i < pRunList->GetRunCount(); i++) {
        Run *pRun = pRunList->GetRun(i);

        switch (pRun->eType)
        {
            case RUNTYPE_SECTIONHEADER:
                {
                    dwTemp = dwWritten;
                    ROMIMAGE_SECTION *pSection = ((CSectionData*)pRun->dwOffset)->GetSectionHeader();
                    CHR(WriteAddrVarInt(hfOutput, pSection->Address, &dwWritten));
                    CHR(WriteVarInt(hfOutput, pSection->Size, &dwWritten));
                    CHR(WriteVarInt(hfOutput, pSection->CheckSum, &dwWritten));
                    CHR(WriteVarInt(hfOutput, ((CSectionData*)pRun->dwOffset)->GetUncompressedSize(), &dwWritten));
                    cbSectionHeaders += dwWritten - dwTemp;
                }
                break;

            case RUNTYPE_DATATOKEN:
                dwTemp = dwWritten;
                CHR(WriteDataToken(hfOutput, pRun->dwRunLength, &dwWritten));
                cbData += dwWritten - dwTemp;
                // Intentional Fallthrough

            case RUNTYPE_RAWDATA:
                dwTemp = dwWritten;
                CHR(IWriteFile(hfOutput, (LPBYTE)pRun->dwOffset, pRun->dwRunLength, &dwWritten));
                cbData += dwWritten - dwTemp;
                break;

            case RUNTYPE_COPYTOKEN:
                dwTemp = dwWritten;
                {
                    ADDRESS addr = pImgOld->GetTranslationTable()->Lookup(pRun->dwOffset);
                    if(addr.iOffset == NO_COMPRESS) {
                        addr.iAddr -= dwPhysicalStartAddr;

                        // Make sure that the address we're trying to write is within 16MB of the start...
                        ASSERT((addr.iAddr & 0x00FFFFFF) == addr.iAddr);
                    }

                    CHR(WriteCopyToken(hfOutput, pRun->dwRunLength, addr, cComprRgns, &dwWritten));
                }
                cbCopy += dwWritten - dwTemp;
                break;

            case RUNTYPE_FIXUPCOMMANDS:
                {
                    CSectionData *pSection = (CSectionData*)pRun->dwOffset;
                    dwTemp = dwWritten;

                    CHR(pSection->GetRelocCmdBuffer(&pbRelocCmds, &cbRelocCmds));

                    CHR(WriteVarInt(hfOutput, cbRelocCmds, &dwWritten));
                    CHR(IWriteFile(hfOutput, pbRelocCmds, cbRelocCmds, &dwWritten));

                    cbFixups += dwWritten - dwTemp;
                }
                break;
            
            case RUNTYPE_COMPRESSIONCOMMANDS:
                {
                    CSectionData *pSection = (CSectionData*)pRun->dwOffset;
                    dwTemp = dwWritten;

                    CHR(WriteVarInt(hfOutput, pSection->GetCountCompressionCommands(), &dwWritten));
                    for(j = 0; j < pSection->GetCountCompressionCommands(); j++) {
                        COMPR_CMD *pCmd = pSection->GetCompressionCommand(j);
                        CBRA(pCmd);
                        CHR(WriteVarInt(hfOutput, pCmd->cBytesCompressed, &dwWritten));
                        CHR(WriteVarInt(hfOutput, pCmd->cBytesUncompressed, &dwWritten));
                    }
                    cbCompressionCommands += dwWritten - dwTemp;
                }
                break;

            case RUNTYPE_ZEROBLOCK:
                dwTemp = dwWritten;
                for(j = 0; j < pRun->dwRunLength; j++) {
                    DWORD dwZero = 0;
                    CHR(IWriteFile(hfOutput, (LPBYTE)&dwZero, 1, &dwWritten));
                }
                cbMisc += dwWritten - dwTemp;;
                break;

        }
    } 

    RETAILMSG(ZONE_VERBOSE, (TEXT("\nImage size breakdown:\n")));
    RETAILMSG(ZONE_VERBOSE, (TEXT("Header                       %8d\n"), cbHdr));
    RETAILMSG(ZONE_VERBOSE, (TEXT("Verification Data            %8d\n"), cbVerification));
    RETAILMSG(ZONE_VERBOSE, (TEXT("Compressed Region Table      %8d\n"), cbCompressedRgnTable));
    RETAILMSG(ZONE_VERBOSE, (TEXT("Section Headers              %8d\n"), cbSectionHeaders));
    RETAILMSG(ZONE_VERBOSE, (TEXT("Compression Commands         %8d\n"), cbCompressionCommands));
    RETAILMSG(ZONE_VERBOSE, (TEXT("Fixup Commands               %8d\n"), cbFixups));
    RETAILMSG(ZONE_VERBOSE, (TEXT("Miscellaneous                %8d\n"), cbMisc));
    RETAILMSG(ZONE_VERBOSE, (TEXT("Data Commands                %8d\n"), cbData));
    RETAILMSG(ZONE_VERBOSE, (TEXT("Copy Commands                %8d\n"), cbCopy));
    RETAILMSG(ZONE_VERBOSE, (TEXT("\n")));

    if(pdwOutputLength) {
        *pdwOutputLength = GetFileSize(hfOutput, NULL);
    }

    RETAILMSG(ZONE_VERBOSE, (TEXT("Total Size                   %8d\n"), dwWritten));

Error:
    if(hfOutput != INVALID_HANDLE_VALUE) {
        CloseHandle(hfOutput);
        hfOutput = NULL;
    }

    return hr;

} /* BuildFile()
   */

HRESULT
BuildUncompressedFile(LPCSTR szInFileName, LPCSTR szOutFileName)
/*---------------------------------------------------------------------------*\
 * This is the fallback.  If we don't like the results of the compression, 
 * i.e. the savings were small, then we can create an uncompressed file.  
 * This file is identical to the NK image.
\*---------------------------------------------------------------------------*/
{
    CopyFile(szInFileName, szOutFileName, FALSE);
    return NOERROR;

} /* BuildUncompressedFile()
   */


void
DiffBin(LPSTR szFile1, LPSTR szFile2, LPSTR szOutputFile)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    HRESULT         hr              = NOERROR;
    DWORD           dwOutputLength  = 0;
    CRunList        RunList;
    CSecDiv         SecDiv;
    CMappedFile     mfOld(szFile1);
    CMappedFile     mfNew(szFile2);
    CImageData      imgOld;
    CImageData      imgNew;

    hr = mfOld.Open(GENERIC_READ, OPEN_EXISTING, FILE_SHARE_READ);
    if(FAILED(hr)) {
        RETAILMSG(1, (TEXT("\nError opening file '%s'\n\n"), szFile1));
        goto Error;
    }

    hr = mfNew.Open(GENERIC_READ, OPEN_EXISTING, FILE_SHARE_READ);
    if(FAILED(hr)) {
        RETAILMSG(1, (TEXT("\nError opening file '%s'\n\n"), szFile2));
        goto Error;
    }

    CHR(imgOld.Initialize((LPBYTE)mfOld.GetDataPtr(), mfOld.GetDataSize()));
    CHR(imgNew.Initialize((LPBYTE)mfNew.GetDataPtr(), mfNew.GetDataSize()));

    CHR(imgOld.GenerateDecompressedImage(&SecDiv));
    CHR(imgNew.GenerateDecompressedImage(NULL));
    
    CHR(ApplyReverseFixups(&imgOld, &imgNew));

    CHR(RunList.Initialize(&imgOld));

    CHR(ComputeSubstrings(&imgOld, &imgNew, &SecDiv, &RunList));

    CHR(BuildFile(&RunList, &imgOld, &imgNew, szOutputFile, &dwOutputLength));
	
Error:
	return;

} /* DiffBin()
   */


void
usage()
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
	printf("Usage:\n");
	printf("  diffbin <infile1> <infile2> <outfile> /z:<zonemask>\n");
	printf("  infile1: Original image file\n");
	printf("  infile2: New image file\n");
	printf("  outfile: Compressed version of new image file\n");
    printf("  zonemask: Hex value of zones to enable:\n");
    printf("            ZONE_VERBOSE            00000001\n");
    printf("            ZONE_PARSE_VERBOSE      00000002\n");
    printf("            ZONE_FIXUP              00000004\n");
    printf("            ZONE_FIXUP_VERBOSE      00000008\n");
    printf("            ZONE_PROGRESS           00000010\n");
    printf("            ZONE_DECOMPRESS         00000020\n");

	exit(-1);

} /* usage()
   */


////////////////////////////////////////////////////////////
// main
//   The entry point to the program
////////////////////////////////////////////////////////////
int 
main(int argc, char* argv[])
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    if(getenv("d_break"))
      DebugBreak();
    
    if (argc < 4) {
        usage();
	  }

    for(int i=4; i < argc; i++) {
        if(argv[i][0] != '-' && argv[i][0] != '/') {
            usage();
        }
        switch(argv[i][1]) {
            case 'z':
            case 'Z':
                if(argv[i][2] != ':') {
                    usage();
                }
                g_dwZoneMask = strtoul(&argv[i][3], NULL, 16);
                break;

            default:
                usage();
                break;
        }
    }

    DiffBin(argv[1], argv[2], argv[3]);

    return 0;

} /* main()
   */


