// imgdata.h

#pragma once

#include "tt.h"

#define SignExtendShortToLong(x) ((LONG) ((SHORT) (x)))

class CImageData;

class CModuleData
{
public:
    CModuleData(CImageData *pImg, TOCentry *pTocEntry, e32_rom *pE32, o32_rom *pO32List);
    ~CModuleData() {}

    LPCSTR      GetFilename();
    TOCentry *  GetTOCEntry()       { return m_pTOCentry; }
    e32_rom *   GetE32()            { return m_pE32; }
    o32_rom *   GetO32(int iIndex)  
    { 
        if(iIndex < 0 || iIndex >= m_pE32->e32_objcnt) {
            return NULL;
        }
        return m_pO32List + iIndex;
    }

    CImageData *        GetParentImage() { return m_pImg; }

private:
    CImageData *        m_pImg;
    TOCentry *          m_pTOCentry;
    e32_rom *           m_pE32;
    o32_rom *           m_pO32List;

};

//
// SectionDivisions class
//
class CSecDiv
{
public:
    CSecDiv();
    ~CSecDiv();

    HRESULT     Add(LPBYTE pbDiv);
    BOOL        IsSecDiv(UCHAR* pbBuf);
    LPBYTE      GetSecDiv(int iIndex) 
    { 
        if(iIndex < 0 || iIndex >= m_cSections) {
            return NULL;
        } 
        return m_pSections[iIndex];
    }

private:
    DWORD           m_cSections;
    LPBYTE *        m_pSections;

};

#define RELOC_REALLOC_SIZE      64

#define RELOC_CMD_MASK          0xC0
#define RELOC_CMD_SHIFT         6
#define GetRelocCmd(x)          (((x) & RELOC_CMD_MASK) >> RELOC_CMD_SHIFT)
#define RELOC_SKIP_MASK         0x60
#define RELOC_SKIP_SHIFT        5
#define GetRelocSkip(x)         (((x) & RELOC_SKIP_MASK) >> RELOC_SKIP_SHIFT)
#define RELOC_COUNT_MASK        0x1F
#define RELOC_COUNT_SHIFT       0
#define GetRelocCount(x)        (((x) & RELOC_COUNT_MASK) >> RELOC_COUNT_SHIFT)

// The count field of a pattern command is 5 bits long, meaning it can encode
// 32 possible values (0-31).  Since we know that the minimum length of a 
// pattern is 2 (we wouldn't have created a pattern if it was just one entry)
// the count field encodes a value from 2-33.  So the maximum length of a
// pattern is 33 cycles.

#define MAX_COUNT               31
#define MAX_SKIP                3

#define RELOC_CMDTYPE_OFFSET                0x0
#define RELOC_CMDTYPE_SINGLE                0x1
#define RELOC_CMDTYPE_PATTERN               0x2
#define RELOC_CMDTYPE_PATTERN2              0x3

#define INVALID_CMD             0xFFFFFFFF

#define RELOC_SINGLE_CONTINUATION_BIT       0x20
#define RELOC_OFFSET_64K_ALIGN_BIT          0x20
#define RELOC_OFFSET_CONTINUATION_BIT       0x10
#define RELOC_VARINT_CONTINUATION_BIT       0x80

//
// Section Data class
// 
//    Stores information about each section in this image.
//

class CSectionData
{
public:
    CSectionData();
    ~CSectionData();

    HRESULT     SetSectionHeader(PROMIMAGE_SECTION prs) { m_pRomimageSection = prs; return NOERROR; };
    HRESULT     SetUncompressedSize(DWORD dwUncompressedSize) { m_dwUncompressedSize = dwUncompressedSize; return NOERROR; }
    HRESULT     SetCompressionCommands(COMPR_CMD *pcc, DWORD cCC) 
    { 
        if(m_pcc) {
            LocalFree(m_pcc); 
            m_pcc = NULL;
        }
        // Special case the entire region uncompressed case
        if(cCC == 1 && pcc->cBytesCompressed == pcc->cBytesUncompressed) {
            m_cCompressionCommands = 0;
            LocalFree(pcc);
        } else {
            m_pcc = pcc; 
            m_cCompressionCommands = cCC; 
        }
        return NOERROR; 
    }
    HRESULT     SetDataPtr(LPBYTE pbData) { m_pbSectionData = pbData; return NOERROR; }
    HRESULT     SetUncompressedDataPtr(LPBYTE pbData) { m_pbUncompressedSectionData = pbData; return NOERROR; }
    HRESULT     BeginReloc(DWORD dwFixupOffset);
    HRESULT     AddReloc(DWORD dwFixupLoc);

    ROMIMAGE_SECTION * GetSectionHeader() { return m_pRomimageSection; }
    DWORD       GetUncompressedSize() { return m_dwUncompressedSize; }
    LPBYTE      GetDataPtr() { return m_pbSectionData; }
    LPBYTE      GetUncompressedDataPtr() { return m_pbUncompressedSectionData; }
    DWORD       GetCountCompressionCommands() { return m_cCompressionCommands; }
    COMPR_CMD * GetCompressionCommand(int iIndex) 
    { 
        if(iIndex < 0 || iIndex >= m_cCompressionCommands) {
            return NULL;
        }
        return &m_pcc[iIndex];
    }

    HRESULT     GetRelocCmdBuffer(LPBYTE *ppbRelocCmds, LPDWORD pdwLen) 
    { 
        HRESULT         hr      = NOERROR;
        ASSERT(ppbRelocCmds);
        ASSERT(pdwLen);

        CHR(FlushPrevCmd());

        *ppbRelocCmds = m_pbRelocCommands;
        *pdwLen = m_cbRelocCommands;

    Error:
        return hr;
    }

private:
    HRESULT     FlushPrevCmd();
    HRESULT     FlushPrevCmdVarInt(UINT32 u);
    HRESULT     AddRelocCmd(LPBYTE pb, DWORD cb)
    {
        HRESULT         hr          = NOERROR;

        if (m_cbRelocCommands + cb > m_cbRelocBufferAlloced) {
            CHR(SafeRealloc((LPVOID*)&m_pbRelocCommands, (m_cbRelocBufferAlloced + cb + RELOC_REALLOC_SIZE) * sizeof(BYTE)));
            m_cbRelocBufferAlloced += cb + RELOC_REALLOC_SIZE;
        }

        ASSERT(m_cbRelocCommands + cb <= m_cbRelocBufferAlloced);

        memcpy(m_pbRelocCommands + m_cbRelocCommands, pb, cb);
	    m_cbRelocCommands += cb;

    Error:
        return hr;

    }

    // Contains Address, length and checksum
    // Note: The length contained in the section header
    // is the actual length of data for this section. It
    // is therefore the compressed size of the section if
    // the section contains any compressed regions.
    PROMIMAGE_SECTION       m_pRomimageSection;

    // Pointer into the image for the data for this section
    LPBYTE                  m_pbSectionData;

    // Pointer into the uncompressed copy of the image to the data for this
    // section
    LPBYTE                  m_pbUncompressedSectionData;

    // Total uncompressed size of the section
    DWORD                   m_dwUncompressedSize;

    // Compression commands needed to regenerate the compressed
    // version of this section
    COMPR_CMD *             m_pcc;
    DWORD                   m_cCompressionCommands;

    // Current offset for reloc fixups
    DWORD                   m_dwCurrentRelocOffset;
    LPBYTE                  m_pbRelocCommands;
    DWORD                   m_cbRelocCommands;
    DWORD                   m_cbRelocBufferAlloced;
    DWORD                   m_dwPrevCmd;
    DWORD                   m_dwPrevCmdData;
    DWORD                   m_dwSecondtoLastCmdData;
};

__inline 
CSectionData::CSectionData() : m_pRomimageSection(NULL), m_dwUncompressedSize(0), 
                               m_pcc(NULL), m_cCompressionCommands(0), 
                               m_dwCurrentRelocOffset(0), m_cbRelocCommands(0),
                               m_pbRelocCommands(NULL), m_cbRelocBufferAlloced(0),
                               m_dwPrevCmd(INVALID_CMD), m_dwPrevCmdData(0),
                               m_dwSecondtoLastCmdData(0)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
} /* CSectionData::CSectionData()
   */

__inline 
CSectionData::~CSectionData()
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    LocalFree(m_pcc);
    LocalFree(m_pbRelocCommands);

} /* CSectionData::~CSectionData()
   */


// This class stores image about a BIN file
class CImageData
{
public:
    
    CImageData();
    ~CImageData();

    HRESULT         Initialize(LPBYTE pbImage, DWORD dwLen);

    TOCentry *      GetModule(int iModuleIndex) { return &(((TOCentry *)m_pbModules)[iModuleIndex]); }
    FILESentry*     GetFile(int iFilesIndex) { return &(((FILESentry *)m_pbFiles)[iFilesIndex]); }
    e32_rom *       GetE32(int iModuleIndex) { return m_ppE32Entries[iModuleIndex]; }
    o32_rom *       GetO32List(int iModuleIndex) { return m_ppO32Entries[iModuleIndex]; }

    LPBYTE          GetDataPtr() { return m_pbImage; }
    DWORD           GetDataLen() { return m_dwImageLen; }

    CModuleData *   GetModuleData(int iModuleIndex)
    {
        if((iModuleIndex < 0) || (iModuleIndex >= m_cModules)) {
            return NULL;
        }
        
        TOCentry *pEntry = GetModule(iModuleIndex);
        e32_rom *pe32 = GetE32(iModuleIndex);
        o32_rom *po32 = GetO32List(iModuleIndex);

        if(pEntry && pe32 && po32) {
            return new CModuleData(this, pEntry, pe32, po32);
        } else {
            return NULL;
        }
    }

    int             FindModuleByName(LPCSTR szModuleName)
    {
        if(szModuleName) {
            for(int i = 0; i < m_cModules; i++) {
                TOCentry *pEntry = GetModule(i);
                if(pEntry) {
                    LPSTR szFilename = (LPSTR)FindAddress((UINT32)pEntry->lpszFileName);
                    if(!stricmp(szModuleName, szFilename)) {
                        return i;
                    }
                }
            }
        }
        return -1;
    }

    DWORD           GetModuleCount()     { return m_cModules; }
    DWORD           GetFileCount()       { return m_cFiles; }

    LPBYTE          FindAddress(UINT32 ulAddr, BOOL fExact = FALSE);
    DWORD           FindVirtualAddress(LPBYTE pb);
    HRESULT         FindCompressedRegions();
    HRESULT         GenerateDecompressedImage(CSecDiv *pSecDiv);

    HRESULT         DumpImageData();

    UINT32          TokenLen(Run *pRun);

    DWORD           GetSectionCount()    { return m_cSections; }
    CSectionData *  GetSection(int iIndex) 
    {
        if(iIndex < 0 || iIndex >= m_cSections) {
            return NULL;
        }
        return &m_pSectionDataList[iIndex];
    }

    HRESULT         FindDataPtr(DWORD dwVirtualAddr, DWORD dwLen, CSectionData **ppSection, LPBYTE *ppbData);

    CTranslationTable * GetTranslationTable() { return &m_tt; }

    LPBYTE          GetUncompressedDataPtr() { return m_pbUncompressedImage; }
    DWORD           GetUncompressedImageLength() { return m_cbUncompressedImage; }
    DWORD           GetMaxBytesUncompressed() { return m_cbLargestUncompressedSection; }

    DWORD           GetCompressedRegionCount() { return m_cCompressedRegions; }
    COMPR_RGN *     GetCompressedRegion(int iIndex)
    {
        if(iIndex < 0 || iIndex >= m_cCompressedRegions) {
            return NULL;
        }
        return &m_pCompressedRegions[iIndex];
    }

    ROMIMAGE_HEADER *    GetHeader() { return m_pRomimageHeader; }

private:

    PBYTE                   m_pbImage;
    DWORD                   m_dwImageLen;
    ROMIMAGE_HEADER *       m_pRomimageHeader;
    ROMIMAGE_SECTION *      m_pFirstSection;
    DWORD                   m_cSections;
    DWORD                   m_dwRomOffset;
    LPBYTE                  m_pTOC;
    LPBYTE                  m_pbModules;
    LPBYTE                  m_pbFiles;
    e32_rom**               m_ppE32Entries;
    o32_rom**               m_ppO32Entries;
    CSectionData *          m_pSectionDataList;
    DWORD                   m_cbLargestUncompressedSection;


    // Translation Table for translating addresses from compressed to uncompressed
    CTranslationTable       m_tt;

    // List of compressed regions for this entire image
    COMPR_RGN *             m_pCompressedRegions;
    DWORD                   m_cCompressedRegions;

    // Uncompressed version of this image
    LPBYTE                  m_pbUncompressedImage;
    DWORD                   m_cbUncompressedImage;

    DWORD                   m_cModules;
    DWORD                   m_cFiles;

};

__inline PROMIMAGE_SECTION
SectionFromPtr(LPBYTE pbBeginningOfSection)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    // Preserve a NULL if we get one...
    if(pbBeginningOfSection == NULL) {
        return NULL;
    }
    return (PROMIMAGE_SECTION)(pbBeginningOfSection - sizeof(ROMIMAGE_SECTION));

} /* SectionFromPtr()
   */

__inline LPBYTE
DataFromSection(PROMIMAGE_SECTION pSection)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    return (LPBYTE)pSection + sizeof(ROMIMAGE_SECTION);

} /* DataFromSection()
   */

__inline DWORD
SectionSize(LPBYTE pbBeginningOfSection)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    PROMIMAGE_SECTION pSection = SectionFromPtr(pbBeginningOfSection);
    return pSection->Size;

} /* SectionSize()
   */

__inline LPBYTE          
NextSection(LPBYTE pbBeginningOfSection)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    return pbBeginningOfSection + SectionSize(pbBeginningOfSection) + sizeof(ROMIMAGE_SECTION);

} /* NextSection()
   */

__inline PROMIMAGE_SECTION
NextSection(PROMIMAGE_SECTION pSection)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    return (PROMIMAGE_SECTION) (DataFromSection(pSection) + pSection->Size);

} /* NextSection()
   */


__inline LPCSTR       
CModuleData::GetFilename() 
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    return (LPCSTR)m_pImg->FindAddress((UINT32)m_pTOCentry->lpszFileName); 

} /* CModuleData::GetFilename()
   */

