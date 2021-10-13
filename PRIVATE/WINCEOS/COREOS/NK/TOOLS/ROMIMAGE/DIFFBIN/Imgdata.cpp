// ImgData.cpp

#include "diffbin.h"

static char c_szBINHeader[] = "B000FF\n";

CImageData::CImageData() :  m_pbImage(NULL),
                            m_dwImageLen(0),
                            m_pRomimageHeader(NULL),
                            m_pFirstSection(NULL),
                            m_cSections(0),
                            m_pTOC(NULL),
                            m_pbModules(NULL),
                            m_pbFiles(NULL),
                            m_ppE32Entries(NULL),
                            m_ppO32Entries(NULL),
                            m_pSectionDataList(NULL),
                            m_pCompressedRegions(NULL),
                            m_cCompressedRegions(0),
                            m_pbUncompressedImage(NULL),
                            m_cbUncompressedImage(0),
                            m_cbLargestUncompressedSection(0),
                            m_cModules(0),
                            m_cFiles(0)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{

} /* CImageData::CImageData()
   */


CImageData::~CImageData()
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    LocalFree(m_ppE32Entries);
    LocalFree(m_ppO32Entries);
    LocalFree(m_pCompressedRegions);
    LocalFree(m_pbUncompressedImage);
    delete [] m_pSectionDataList;

} /* CImageData::~CImageData()
   */

LPBYTE
CImageData::FindAddress(UINT32 ulAddr, BOOL fExact)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    BOOL romoffset = false;
    PROMIMAGE_SECTION pSection = NULL;
    ulAddr += m_dwRomOffset;

AGAIN:    
    pSection = m_pFirstSection;
    
    while (pSection->Address || (!pSection->Address && pSection->CheckSum))
    {
        if ((pSection->Address <= ulAddr) && 
            (pSection->Address + pSection->Size > ulAddr) &&
            (!fExact || (pSection->Address == ulAddr)))
        {
            return DataFromSection(pSection) + (ulAddr - pSection->Address);
        }

        pSection = NextSection(pSection);
    }

    if(!romoffset){
        romoffset = true;
        ulAddr -= m_dwRomOffset;
        goto AGAIN;
    }

    return NULL;

} /* CImageData::FindAddress()
   */

DWORD
CImageData::FindVirtualAddress(LPBYTE pb)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    int i = 0;
    CSectionData *pSection;

    while(pSection = GetSection(i++)) {
        if(pb >= pSection->GetDataPtr() && 
           pb <= pSection->GetDataPtr() + pSection->GetSectionHeader()->Size) 
        {
            return pSection->GetSectionHeader()->Address + (pb - pSection->GetDataPtr());
        }
    }

    return 0;

} /* CImageData::FindVirtualAddress()
   */

HRESULT
CImageData::Initialize(LPBYTE pbImage, DWORD dwLen)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    HRESULT                 hr              = NOERROR;
    UINT32                  i;
    DWORD                   dwTOCSection    = 0;
    LPDWORD                 pdwROMSignature = NULL;
    PROMIMAGE_SECTION       pSection        = NULL;
    DWORD                   dwSectionCount;
      
    // Set up internal pointers

    m_pbImage               = pbImage;
    m_dwImageLen            = dwLen;

    m_pRomimageHeader       = (ROMIMAGE_HEADER *)m_pbImage;
    m_pFirstSection         = (ROMIMAGE_SECTION *)(m_pbImage + sizeof(ROMIMAGE_HEADER));

    //
    // Verify the signature in the ROM header
    //
    CBR(memcmp(m_pRomimageHeader->Signature, c_szBINHeader, sizeof(c_szBINHeader)) == 0);

    //
    // Walk through the image and count the sections
    //
    pSection = m_pFirstSection;
    while(pSection->Address || (!pSection->Address && pSection->CheckSum))	{
        m_cSections++;
        pSection = NextSection(pSection);
    }

    CPR(m_pSectionDataList = new CSectionData[m_cSections]);

    dwSectionCount = 0;
    pSection = m_pFirstSection;
    while(pSection->Address || (!pSection->Address && pSection->CheckSum))	{

        m_pSectionDataList[dwSectionCount].SetSectionHeader(pSection);
        m_pSectionDataList[dwSectionCount++].SetDataPtr(DataFromSection(pSection));

        pSection = NextSection(pSection);
    }

    ASSERT(dwSectionCount == m_cSections);

    //
    // Find the TOC
    //
    pdwROMSignature = (LPDWORD)FindAddress(m_pRomimageHeader->PhysicalStartAddress + ROM_SIGNATURE_OFFSET, TRUE);

    CBR(pdwROMSignature);
    CBR(pdwROMSignature[0] == ROM_SIGNATURE);
    dwTOCSection = pdwROMSignature[1];

    m_dwRomOffset = ((m_pRomimageHeader->PhysicalStartAddress + m_pRomimageHeader->PhysicalSize) & 0xFF000000) - (DWORD) ((DWORD)dwTOCSection & 0xFF000000);
    //m_pTOC = FindAddress(dwTOCSection + dwRomOffset, TRUE);

    m_pTOC = FindAddress(dwTOCSection, TRUE);
    CBR(m_pTOC);
    ASSERT(m_pTOC);
    
    m_pbModules = NextSection(m_pTOC);
    ASSERT(m_pbModules);

    //
    // Initialze member variables for Modules
    //
    m_cModules = ((ROMHDR*)m_pTOC)->nummods;
//    m_cModules = SectionSize(m_pbModules) / sizeof(TOCentry);
    CPR(m_ppE32Entries = (e32_rom**)LocalAlloc(LMEM_FIXED, m_cModules * sizeof(e32_rom*)));
    CPR(m_ppO32Entries = (o32_rom**)LocalAlloc(LMEM_FIXED, m_cModules * sizeof(o32_rom*)));

    m_pbFiles = m_pbModules + m_cModules * sizeof(TOCentry);
    ASSERT(m_pbFiles);

    //
    // Initialize member variables for Files section
    //
    m_cFiles = ((ROMHDR*)m_pTOC)->numfiles;
//    m_cFiles = SectionSize(m_pbFiles) / sizeof(FILESentry);

    //
    // Find E32 and O32 for each Module
    //
    for (i = 0; i < m_cModules; ++i) {
        TOCentry *pEntry = GetModule(i);
        ASSERT(pEntry);

        m_ppE32Entries[i] = (e32_rom *)FindAddress(pEntry->ulE32Offset);
        m_ppO32Entries[i] = (o32_rom *)FindAddress(pEntry->ulO32Offset);
    }

    CHR(DumpImageData());

Error:
    return hr;

} /* CImageData::Initialize()
   */

char e32str[11][4] = {  // e32 image header data directory types
    "EXP",              // export table
    "IMP",              // import table
    "RES",              // resource table
    "EXC",              // exception table
    "SEC",              // security table
    "FIX",              // base relocation table
    "DEB",              // debug data
    "IMD",              // copyright
    "MSP"               // global ptr
};

void DumpHeader(e32_rom* e32) 
{
    int loop;
    RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("  e32_objcnt     : %8.8lx\n"),e32->e32_objcnt));
    RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("  e32_imageflags : %8.8lx\n"),e32->e32_imageflags));
    RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("  e32_entryrva   : %8.8lx\n"),e32->e32_entryrva));
    RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("  e32_vbase      : %8.8lx\n"),e32->e32_vbase));
    RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("  e32_subsysmajor: %8.8lx\n"),e32->e32_subsysmajor));
    RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("  e32_subsysminor: %8.8lx\n"),e32->e32_subsysminor));
    RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("  e32_stackmax   : %8.8lx\n"),e32->e32_stackmax));
    RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("  e32_vsize      : %8.8lx\n"),e32->e32_vsize));
    RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("  e32_subsys     : %8.8lx\n"),e32->e32_subsys));
    for (loop = 0; loop < 9; loop++)
        RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("  e32_unit:%3.3s	 : %8.8lx %8.8lx\n"),
    e32str[loop],e32->e32_unit[loop].rva,e32->e32_unit[loop].size));
}

HRESULT
CImageData::DumpImageData()
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    DWORD           nCount       = 0;
    int             i;

    // Dump out ROMIMAGE sections
    RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("ROMIMAGE Sections\n")));
	RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("Index  Address     Size        Checksum\n"))); 

    PROMIMAGE_SECTION pSection = m_pFirstSection;
    LPBYTE pbBuf = (LPBYTE)pSection;

	while (pSection->Address || (!pSection->Address && pSection->CheckSum)) {
		pbBuf += sizeof(ROMIMAGE_SECTION);

        RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("0x%04d   0x%08x  0x%08x  0x%08x\n"), nCount++, pSection->Address, pSection->Size, pSection->CheckSum));

		pbBuf += pSection->Size;
		pSection = (PROMIMAGE_SECTION)pbBuf;
	}

    RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("\npTOC=0x%08lx  pModules=0x%08lx  pFiles=0x%08lx\n"), 
        FindVirtualAddress(m_pTOC), FindVirtualAddress(m_pbModules), FindVirtualAddress(m_pbFiles)));
    
    // Print out Modules TOC
	RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("\nModules TOC\n")));
	RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("  #  Filename                  FileAttr    FileSize    LoadOffset\n")));

    for (i = 0; i < m_cModules; ++i)
	{
        TOCentry *pEntry = GetModule(i);
        LPSTR szFilename = (LPSTR)FindAddress((DWORD)pEntry->lpszFileName);

        RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("%3d  %25s 0x%08x  0x%08x  0x%08x\n"), 
			i, szFilename, pEntry->dwFileAttributes, pEntry->nFileSize, pEntry->ulLoadOffset));
	}

    // Print out Files TOC
	RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("\nFiles TOC\n")));
	RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("  #  Filename                  FileAttr    RealSize    CompSize    LoadOffset  Compressed?\n")));
    for (i = 0; i < m_cFiles; i++) 
    {
		FILESentry *pEntry = GetFile(i);
        LPSTR szFilename = (LPSTR)FindAddress((DWORD)pEntry->lpszFileName);

		RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("%3d  %25s 0x%08x  0x%08x  0x%08x  0x%08x  %s\n"), 
            i, szFilename, pEntry->dwFileAttributes, pEntry->nRealFileSize,pEntry->nCompFileSize,pEntry->ulLoadOffset,
			((pEntry->dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED) &&
			(pEntry->nRealFileSize > pEntry->nCompFileSize)) ? "Yes" : "No"));
	}

    // Print out E32 and O32s for each module
    for (i = 0; i < m_cModules; ++i) {
        CModuleData *pMod = GetModuleData(i);
        
		RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("\nModule %d:  e32=0x%08lx  o32=0x%08lx\n"), i, 
            FindVirtualAddress((LPBYTE)pMod->GetE32()), FindVirtualAddress((LPBYTE)pMod->GetO32(0))));
		DumpHeader(pMod->GetE32());

        RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("  o32_vsize   o32_rva     o32_psize   o32_dataptr o32_realsize  o32_flags\n")));
        for(int j = 0; j < pMod->GetE32()->e32_objcnt; j++) {
            o32_rom *o32 = pMod->GetO32(j);
	        RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("  0x%08lx  0x%08lx  0x%08lx  0x%08lx  0x%08lx    0x%08lx\n"),
                o32->o32_vsize, o32->o32_rva, o32->o32_psize, o32->o32_dataptr, o32->o32_realaddr,o32->o32_flags));
        }
    }
	RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("\n")));

    return NOERROR;

} /* CImageData::DumpImageData()
   */

////////////////////////////////////////////////////////////
// SortCompressedRegions
//   Sort the compressed regions by address
////////////////////////////////////////////////////////////
void SortCompressedRgns(COMPR_RGN* pcr, UINT32 cComprRgns)
{
	UINT16 i, j, k;
	for (i = 1; i < cComprRgns; ++i)
	{
		j = 0;
		while (pcr[i].iAddress > pcr[j].iAddress)
			++j;
		if (i > j)
		{
			COMPR_RGN temp = pcr[i];
			for (k = i; k > j; --k)
				pcr[k] = pcr[k - 1];
			pcr[j] = temp;
		}
	}
}

UINT32 
CImageData::TokenLen(Run *pRun)
/*---------------------------------------------------------------------------*\
 * Return the length of the token for "iType", "dwRunLength" and "iOffset," 
 * including, for DATA tokens, the actual data
\*---------------------------------------------------------------------------*/
{
    DWORD       dwLen       = 0;
    
    switch(pRun->eType) {
        case RUNTYPE_SECTIONHEADER:
            dwLen += sizeof(ROMIMAGE_SECTION);
            break;

        case RUNTYPE_DATATOKEN:
            // A data run takes up the amount of data in the run itself, plus
            // the length of the type and length.  Type and length are bit
            // backed together, and will take between 1 and 4 bytes to encode,
            // depending on the length.

            if (pRun->dwRunLength < (1 << 5)) {
			    dwLen += 1;

            } else if (pRun->dwRunLength < (1 << 13)) {
			    dwLen += 2;

            } else if (pRun->dwRunLength < (1 << 21)) {
			    dwLen += 3;

            } else {
			    dwLen += 4;
            }
            // Intentional Fallthrough

        case RUNTYPE_RAWDATA:
        case RUNTYPE_ZEROBLOCK:
            dwLen += pRun->dwRunLength;
            break;

        case RUNTYPE_COPYTOKEN:
            if (pRun->dwRunLength < (1 << 4)) {
			    dwLen += 4;

            } else if (pRun->dwRunLength < (1 << 12)) {
			    dwLen += 5;

            }  else if (pRun->dwRunLength < (1 << 20)) {
			    dwLen += 6;

            } else {
			    dwLen += 7;

            }

            if (m_tt.Lookup(pRun->dwOffset).iOffset != NO_COMPRESS) {
                if (m_cCompressedRegions < (1 << 8)) {
			        dwLen += 1;

                } else if (m_cCompressedRegions < (1 << 16)) {
			        dwLen += 2;

                } else {
			        dwLen += 3;

                }
            }
            break;

        case RUNTYPE_COMPRESSIONCOMMANDS:
            {
                CSectionData *pSection = (CSectionData*)pRun->dwOffset;
                dwLen += pSection->GetCountCompressionCommands() * 6 + 3;
            }
            break;

        default:
            ASSERT(0);
            break;
	}

    return dwLen;

} /* CImageData::TokenLen()
   */

////////////////////////////////////////////////////////////
// FindCompressedRegions
//   Find the regions that were compressed before
//   being packed into the image.  
////////////////////////////////////////////////////////////
HRESULT
CImageData::FindCompressedRegions()
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    HRESULT         hr      = NOERROR;
	UINT32          i;
    UINT32          j;

    m_cCompressedRegions = 0;

    //
    // Find compressed files
    //
    for (i = 0; i < m_cFiles; i++) 
    {
		FILESentry *pEntry = GetFile(i);

        CBR(pEntry);

		if ((pEntry->dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED) && (pEntry->nRealFileSize > pEntry->nCompFileSize))
		{
            if (m_cCompressedRegions % NUM_COMPRESSED == 0) {
                CHR(SafeRealloc((LPVOID*)&m_pCompressedRegions, (m_cCompressedRegions + NUM_COMPRESSED) * sizeof(COMPR_RGN)));
            }
			m_pCompressedRegions[m_cCompressedRegions].iAddress = pEntry->ulLoadOffset;
			m_pCompressedRegions[m_cCompressedRegions].cBytesCompressed = pEntry->nCompFileSize;
			m_pCompressedRegions[m_cCompressedRegions].cBytesUncompressed = pEntry->nRealFileSize;
			m_cCompressedRegions++;
		}
	}

    // 
    // Find compressed sections within Modules
    //
    for(i = 0; i < m_cModules; i++) {
        o32_rom *rgO32 = GetO32List(i);
        if(rgO32) {
            e32_rom *pE32 = GetE32(i);
            CBRA(pE32);

            for(j = 0; j < pE32->e32_objcnt; j++) {
				if ((rgO32[j].o32_flags & IMAGE_SCN_COMPRESSED) &&
					(rgO32[j].o32_vsize > rgO32[j].o32_psize))
				{
                    if (m_cCompressedRegions % NUM_COMPRESSED == 0) {
                        CHR(SafeRealloc((LPVOID*)&m_pCompressedRegions, (m_cCompressedRegions + NUM_COMPRESSED) * sizeof(COMPR_RGN)));
                    }
					m_pCompressedRegions[m_cCompressedRegions].iAddress = rgO32[j].o32_dataptr;
					m_pCompressedRegions[m_cCompressedRegions].cBytesCompressed = rgO32[j].o32_psize;
					m_pCompressedRegions[m_cCompressedRegions].cBytesUncompressed = rgO32[j].o32_vsize;
					m_cCompressedRegions++;
				}
            }
        }
    }

	SortCompressedRgns(m_pCompressedRegions, m_cCompressedRegions);
	
	RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("\nCompressed regions\n")));
    for (i = 0; i < m_cCompressedRegions; ++i) {
		RETAILMSG(ZONE_PARSE_VERBOSE, (TEXT("Addr: 0x%08X, Comp: 0x%08X, Uncomp: 0x%08X\n"), 
                  m_pCompressedRegions[i].iAddress, m_pCompressedRegions[i].cBytesCompressed, m_pCompressedRegions[i].cBytesUncompressed));
    }
	
Error:
    return hr;

} /* CImageData::FindCompressedRegions()
   */

HRESULT
CImageData::GenerateDecompressedImage(CSecDiv *pSecDiv)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    HRESULT             hr              = NOERROR;
    UCHAR*              pbBuf           = m_pbImage;
	UINT32              cLen            = m_dwImageLen;
    COMPR_CMD*          pcc             = NULL;
	UINT32				cbBufNew		= m_dwImageLen * 4;
	LPBYTE              pbBufNew        = NULL;
	LPBYTE              pbBufNewOrig;
	LPBYTE				pbBufNewEnd;
	PROMIMAGE_SECTION   pSectionHeader;
    DWORD               dwSectionCount;

typedef DWORD (*CECOMPRESS)(LPBYTE  lpbSrc, DWORD cbSrc, LPBYTE lpbDest, DWORD cbDest, WORD wStep, DWORD dwPagesize);
typedef DWORD (*CEDECOMPRESS)(LPBYTE  lpbSrc, DWORD cbSrc, LPBYTE  lpbDest, DWORD cbDest, DWORD dwSkip, WORD wStep, DWORD dwPagesize);

#define COMPRESS "compress.dll"
#define CECOMPRESSOR "CECompress"
#define CEDECOMPRESSOR "CEDecompress"

  HMODULE hcomp = LoadLibrary(COMPRESS);
  if(!hcomp){
    fprintf(stderr, "Error: LoadLibrary() failed to load '%s': %d\n", COMPRESS, GetLastError());
    return false;
  }

  CEDECOMPRESS cedecompress = (CEDECOMPRESS)GetProcAddress(hcomp, CEDECOMPRESSOR);
  if(!cedecompress){
    fprintf(stderr, "Error: GetProcAddress() failed to find 'CEDecompress' in '%s': %d\n", CEDECOMPRESSOR, GetLastError());
    return false;
  }

    CPR(pbBufNew = (LPBYTE)LocalAlloc(LMEM_FIXED, cbBufNew));
    pbBufNewOrig = pbBufNew;
	pbBufNewEnd  = pbBufNewOrig + cbBufNew;

	CHR(FindCompressedRegions());
	
	pSectionHeader = m_pFirstSection;
    dwSectionCount = 0;

	while (pSectionHeader->Address || (!pSectionHeader->Address && pSectionHeader->CheckSum))
	{
    	UINT32      cBytesUncompressedTotal  = 0;
		UINT32      cComprRgnsSec            = 0;
		UINT32      cComprRgnsSecMax         = 0;
		UINT32      cBytesUncompressed;
        UINT32      cBytesCompressed;
		UINT32      iRgn    = 0;
		UINT32      iOffset = 0;
		ADDRESS     addr;

        ASSERT(pcc == NULL);

		pbBuf = DataFromSection(pSectionHeader);

        CHR(m_pSectionDataList[dwSectionCount].SetUncompressedDataPtr(pbBufNew));

        if(m_cCompressedRegions > 0) 
        {
            while ((iRgn < m_cCompressedRegions - 1) && (m_pCompressedRegions[iRgn].iAddress < pSectionHeader->Address)) {
			        ++iRgn;
            }
        }

		while (iOffset < pSectionHeader->Size)
		{
            UINT32 uAddr = pSectionHeader->Address + iOffset;
  			if ((m_cCompressedRegions > 0) && uAddr == m_pCompressedRegions[iRgn].iAddress)
  			{
                addr.iAddr = iRgn;
  				addr.iOffset = 0;

  				CHR(m_tt.Insert(pbBufNew - pbBufNewOrig, addr));

  				cBytesCompressed = m_pCompressedRegions[iRgn].cBytesCompressed;
  				cBytesUncompressed = cedecompress(pbBuf + iOffset, cBytesCompressed, pbBufNew, pbBufNewEnd - pbBufNew, 0, 1, 0x1000);
  				
                  CBRA(cBytesUncompressed != CEDECOMPRESS_FAILED);

                  RETAILMSG(ZONE_DECOMPRESS, (TEXT("  %u bytes decompressed to %u\n"), cBytesCompressed, cBytesUncompressed));

  				pbBufNew += cBytesUncompressed;

  				++iRgn;
  			}
  			else
  			{
                addr.iAddr = uAddr;
                addr.iOffset = NO_COMPRESS;

  				CHR(m_tt.Insert(pbBufNew - pbBufNewOrig, addr));
  				
                if(m_cCompressedRegions > 0) 
                {
    				cBytesUncompressed = cBytesCompressed = min(pSectionHeader->Size, m_pCompressedRegions[iRgn].iAddress - pSectionHeader->Address) - iOffset;
                } else 
                {
    				cBytesUncompressed = cBytesCompressed = pSectionHeader->Size - iOffset;
                }
          
				// Check to make sure we won't go off the end of the buffer
				CBRA((pbBufNew + cBytesCompressed)<pbBufNewEnd);

  				memcpy(pbBufNew, pbBuf + iOffset, cBytesCompressed);

  				pbBufNew += cBytesCompressed;
  			}

			if (cComprRgnsSec == cComprRgnsSecMax) {
				cComprRgnsSecMax += NUM_COMPRESSED_SCN;
                CHR(SafeRealloc((LPVOID*)&pcc, cComprRgnsSecMax * sizeof(COMPR_CMD)));
			}
			pcc[cComprRgnsSec].cBytesCompressed = cBytesCompressed;
			pcc[cComprRgnsSec].cBytesUncompressed = cBytesUncompressed;
			++cComprRgnsSec;

            iOffset += cBytesCompressed;
            cBytesUncompressedTotal += cBytesUncompressed;

            if(pSecDiv) {
                CHR(pSecDiv->Add(pbBufNew));
				*pbBufNew++ = SEC_DIV_CHAR;
            }
		}

        CHR(m_pSectionDataList[dwSectionCount].SetUncompressedSize(cBytesUncompressedTotal));
        CHR(m_pSectionDataList[dwSectionCount].SetCompressionCommands(pcc, cComprRgnsSec));

        // Handed off to the SectionData List...
        pcc = NULL;

		m_cbLargestUncompressedSection = max(m_cbLargestUncompressedSection, cBytesUncompressedTotal);

		pSectionHeader = NextSection(pSectionHeader);
        dwSectionCount++;
	}

    ASSERT(dwSectionCount == m_cSections);

	m_cbUncompressedImage = pbBufNew - pbBufNewOrig;
    RETAILMSG(ZONE_VERBOSE, (TEXT("Size of uncompressed image: %u\n"), m_cbUncompressedImage));

    m_pbUncompressedImage = pbBufNewOrig;
    pbBufNewOrig = NULL;

    CHR(SafeRealloc((LPVOID*)&m_pbUncompressedImage, m_cbUncompressedImage));

Error:
    LocalFree(pcc);
    LocalFree(pbBufNewOrig);

	if (FAILED(hr))
	{
		fprintf(stderr, "Error: GenerateDecompressedImage failed : 0x%x\n", hr);
	}
	return hr;

} /* CImageData::GenerateDecompressedImage()
   */


HRESULT
CImageData::FindDataPtr(DWORD dwVirtualAddr, DWORD dwLen, 
                        CSectionData **ppSection, LPBYTE *ppbData)
/*---------------------------------------------------------------------------*\
 * Find some data within the image...
 *      dwVirtualAddr       - Virtual address that we're looking for.  This is 
 *                            a real address in flash, which means that it's 
 *                            a "compressed" address.  This is an address that
 *                            you could actually go read from flash.  
 *                            If you need the address of some uncompressed data
 *                            You have to call this function with the base 
 *                            address of the compression block and offset into
 *                            it the appropriate amount.
 *      dwLen               - Length of the data section.  Note that the entire
 *                            data section must be either compressed or 
 *                            uncompressed and can not cross a section boundary.
 *                            If the data is compressed, this is the COMPRESSED
 *                            length of the data.
 *      ppbData             - return pointer for the address in memory.
\*---------------------------------------------------------------------------*/
{
    HRESULT         hr          = NOERROR;
    int             i, j;  
    DWORD           dwAddr;
    LPBYTE          pb;

    ASSERT(ppSection);
    ASSERT(ppbData);

    for(i = 0; i < m_cSections; i++) {
        CSectionData *pSection = GetSection(i);
        ROMIMAGE_SECTION *pSectionHdr = pSection->GetSectionHeader();
        
        if( dwVirtualAddr >= pSectionHdr->Address && 
            dwVirtualAddr + dwLen <= pSectionHdr->Address + pSectionHdr->Size)
        {
            // It's in here somewhere... Now we have to see if it was compressed...
            dwAddr = pSectionHdr->Address;
            pb = pSection->GetUncompressedDataPtr();

            if(pSection->GetCountCompressionCommands()) {
                for(j = 0; j < pSection->GetCountCompressionCommands(); j++) {
                    COMPR_CMD *pCmd = pSection->GetCompressionCommand(j);

                    if(dwVirtualAddr >= dwAddr && dwVirtualAddr + dwLen <= dwAddr + pCmd->cBytesCompressed) {
                        // Got it!
                        *ppbData = (dwVirtualAddr - dwAddr) + pb;
                        *ppSection = pSection;
                        goto Error;
                    }
                    dwAddr += pCmd->cBytesCompressed;
                    pb += pCmd->cBytesUncompressed;
                }
            } else {
                // This entire section is uncompressed, so we must have it...
                *ppbData = (dwVirtualAddr - dwAddr) + pb;
                *ppSection = pSection;
                goto Error;
            }
        }
    }

    hr = E_FAIL;

Error:
    return hr;

} /* CImageData::FindDataPtr()
   */


CModuleData::CModuleData(CImageData *pImg, TOCentry *pTocEntry, e32_rom *pE32, 
                         o32_rom *pO32List) :    
      m_pImg(pImg), m_pTOCentry(pTocEntry), m_pE32(pE32), m_pO32List(pO32List)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
} /* CModuleData::CModuleData()
   */

CSecDiv::CSecDiv() : m_cSections(0), m_pSections(NULL)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
} /* CSecDiv::CSecDiv()
   */

CSecDiv::~CSecDiv()
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    LocalFree(m_pSections);

} /* CSecDiv::~CSecDiv()
   */

HRESULT
CSecDiv::Add(LPBYTE pbSec)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    HRESULT         hr          = NOERROR;

    if (m_cSections % NUM_SECTIONS == 0) {
        CHR(SafeRealloc((LPVOID*)&m_pSections, (m_cSections + NUM_SECTIONS) * sizeof(LPBYTE)));
    }
	m_pSections[m_cSections] = pbSec;
	m_cSections++;

Error:
    return hr;

} /* CSecDiv::Add()
   */

BOOL 
CSecDiv::IsSecDiv(UCHAR* pbBuf)
/*---------------------------------------------------------------------------*\
 * Return TRUE iff "pbBuf" is a division between sections in the old image file
\*---------------------------------------------------------------------------*/
{
	UINT32 iSec = 0;

    if (*pbBuf != SEC_DIV_CHAR) {
		return FALSE;
    }

    while ((iSec < m_cSections) && (m_pSections[iSec] < pbBuf)) {
		++iSec;
    }

	return ((iSec < m_cSections) && (m_pSections[iSec] == pbBuf));

} /* CSecDiv::IsSecDiv()
   */


HRESULT     
CSectionData::BeginReloc(DWORD dwFixupOffset)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    HRESULT             hr          = NOERROR;
    DWORD               dwOffset;

    // dwFixupOffset is the amount that we fixed up to do the Reverse fixup.
    // the instruction we need to write into the file to do the proper fixup
    // is the instruction for -dwFixupOffset;

    dwOffset = -dwFixupOffset;

    if(dwOffset != m_dwCurrentRelocOffset) {
        m_dwCurrentRelocOffset = dwOffset;
        CHR(FlushPrevCmd());
        m_dwPrevCmd = RELOC_CMDTYPE_OFFSET << RELOC_CMD_SHIFT;
        m_dwPrevCmdData = dwOffset;
    }

Error:
    return hr;

} /* CSectionData::BeginReloc()
   */

DWORD
GetNextPattern(DWORD dwCmd, DWORD dwData)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    ASSERT(GetRelocCmd(dwCmd) & RELOC_CMDTYPE_PATTERN);

    return dwData + ((GetRelocSkip(dwCmd) + 1) * (GetRelocCount(dwCmd) + 2)) * sizeof(DWORD);

} /* GetNextPattern()
   */

DWORD
GetEndOfPattern(DWORD dwCmd, DWORD dwData)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    ASSERT(GetRelocCmd(dwCmd) & RELOC_CMDTYPE_PATTERN);

    return dwData + ((GetRelocSkip(dwCmd) + 1) * (GetRelocCount(dwCmd) + 1)) * sizeof(DWORD);

} /* GetEndOfPattern()
   */

HRESULT
CSectionData::FlushPrevCmdVarInt(UINT32 u)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
    HRESULT     hr      = NOERROR;
    BYTE        b;

    do {
        b = u & 0x7f;
        if(u >>= 7) {
            b |= RELOC_VARINT_CONTINUATION_BIT;
        }
        CHR(AddRelocCmd(&b, 1));
    } while(u > 0);

Error:
    return hr;

} /* CSectionData::FlushPrevCmdVarInt()
   */


HRESULT
CSectionData::FlushPrevCmd()
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    HRESULT         hr          = NOERROR;
    BYTE            b;

    if(m_dwPrevCmd != INVALID_CMD) {
        DWORD dwAddr = m_dwPrevCmdData - m_dwSecondtoLastCmdData;

        switch(GetRelocCmd(m_dwPrevCmd)) {
            case RELOC_CMDTYPE_OFFSET:
                b = m_dwPrevCmd;

                // We don't use the diff for an Offset command
                dwAddr = m_dwPrevCmdData;

                // Check for 64k alignment
                if((dwAddr & 0xFFFF) == 0) {
                    // 64k aligned!
                    dwAddr >>= 16;
                    b |= RELOC_OFFSET_64K_ALIGN_BIT;
                }

                b |= dwAddr & 0x0f;
                if(dwAddr >>= 4) {
                    b |= RELOC_OFFSET_CONTINUATION_BIT;
                }
                CHR(AddRelocCmd(&b, 1));
                if(dwAddr) {
                    CHR(FlushPrevCmdVarInt(dwAddr));
                }
                // We want the command after an OFFSET command
                // to diff against 0.
                m_dwPrevCmdData = 0;
                break;

            case RELOC_CMDTYPE_SINGLE:
                b = m_dwPrevCmd;
                b |= dwAddr & 0x1f;
                if(dwAddr >>= 5) {
                    b |= RELOC_SINGLE_CONTINUATION_BIT;
                }
                CHR(AddRelocCmd(&b, 1));
                if(dwAddr) {
                    CHR(FlushPrevCmdVarInt(dwAddr));
                }
                break;

            case RELOC_CMDTYPE_PATTERN:
            case RELOC_CMDTYPE_PATTERN2:
                b = m_dwPrevCmd;
                CHR(AddRelocCmd(&b, 1));
                CHR(FlushPrevCmdVarInt(dwAddr));
                // Update the previous address so that the next command will
                // be from the diff of the last entry in the pattern, isntead
                // of the first.  This will minimize the distance to the next
                // fixup.
                m_dwPrevCmdData = GetEndOfPattern(m_dwPrevCmd, m_dwPrevCmdData);
                break;

            default:
                ASSERT(0);
                break;
        }

        m_dwSecondtoLastCmdData = m_dwPrevCmdData;
        m_dwPrevCmdData = 0;
        m_dwPrevCmd = INVALID_CMD;
    }

Error:
    return hr;

} /* CSectionData::FlushPrevCmd()
   */

HRESULT     
CSectionData::AddReloc(DWORD dwFixupLoc)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    HRESULT             hr          = NOERROR;

    if(m_dwPrevCmd != INVALID_CMD) {
        // There is a previous command.  Let's see if we can extend it...
        DWORD dwPrevCmdData;
        DWORD dwCmdData;

        // We can only make patterns if we have DWORD aligned fixups...
        // If this fixup (or the previous one) is NOT DWORD aligned, then we
        // have to make this into a Single
        if(((dwFixupLoc & 0x0003) != 0) || ((m_dwPrevCmdData & 0x0003) != 0)) {
            goto MakeSingle;
        }

        dwPrevCmdData = m_dwPrevCmdData >> 2;
        dwCmdData = dwFixupLoc >> 2;

        switch(GetRelocCmd(m_dwPrevCmd)) {
            case RELOC_CMDTYPE_OFFSET:
                // Nope, can't extend an offset command
                goto MakeSingle;

            case RELOC_CMDTYPE_SINGLE:
                {
                    // First check for pattern:
                    DWORD dwSkip = dwCmdData - dwPrevCmdData - 1;
                    if(dwSkip <= MAX_SKIP) {
                        // We can build a pattern out of these two commands
                        // Let's go modify the previous command to be a 
                        // pattern command.
                        DWORD dwNewCmd = RELOC_CMDTYPE_PATTERN << RELOC_CMD_SHIFT;
                        dwNewCmd |= dwSkip << RELOC_SKIP_SHIFT;
                        // The count field should be set to 0, since a 0 in 
                        // the count field means 2 times through the pattern.
                        
                        // m_dwPrevCmdData is already set to the fixup address
                        // of the first item in the pattern, so we are ready
                        // to go... Just set that up as the new "last cmd" and
                        // return.

                        m_dwPrevCmd = dwNewCmd;
                        goto Error;
                    } else {
                        // This entry is too far away from the last entry
                        // to make a pattern entry.  Let's just make a single
                        // entry instead...

                        goto MakeSingle;
                    }
                }
                break;

            case RELOC_CMDTYPE_PATTERN:
            case RELOC_CMDTYPE_PATTERN2:
                {
                    if(dwFixupLoc == GetNextPattern(m_dwPrevCmd, m_dwPrevCmdData)) {
                        // Awesome... this fixup fits nicely into our pattern.  Let's
                        // just extend the pattern one more count...
                        DWORD dwNewCmd = RELOC_CMDTYPE_PATTERN << RELOC_CMD_SHIFT;
                        dwNewCmd |= GetRelocSkip(m_dwPrevCmd) << RELOC_SKIP_SHIFT;
                        dwNewCmd |= (GetRelocCount(m_dwPrevCmd) + 1) << RELOC_COUNT_SHIFT;

                        m_dwPrevCmd = dwNewCmd;

                        if(GetRelocCount(m_dwPrevCmd) == MAX_COUNT) {
                            // That's the end of life for this pattern... Let's flush it
                            // so we'll start over with a single next time...

                            CHR(FlushPrevCmd());
                        }
                        goto Error;
                    } else {
                        // Nope, we don't fit into the previous pattern.  Just 
                        // make a single...
                        goto MakeSingle;
                    }
                }
                break;

            default:
                ASSERT(0);
                CHR(E_FAIL);
        }

    }

MakeSingle:
    CHR(FlushPrevCmd());

    m_dwPrevCmd = RELOC_CMDTYPE_SINGLE << RELOC_CMD_SHIFT;
    m_dwPrevCmdData = dwFixupLoc;

Error:
    return hr;

} /* CSectionData::AddReloc()
   */
