// runlist.cpp

#include "diffbin.h"

CRunList::CRunList() : m_pRunList(NULL), m_cRuns(0), m_cAllocated(0)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{

} /* CRunList::CRunList()
   */

CRunList::~CRunList()
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    LocalFree(m_pRunList);
    m_pRunList = NULL;

} /* CRunList::~CRunList()
   */

HRESULT         
CRunList::Clear()
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    m_cRuns = 0;

    return NOERROR;

} /* CRunList::Clear()
   */

HRESULT         
CRunList::Initialize(CImageData *pImgData) 
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    m_pImgData = pImgData;

    return NOERROR;

} /* CRunList::Initialize()
   */

HRESULT         
CRunList::Insert(RunType eType, DWORD dwRunLength, DWORD dwOffset)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    HRESULT         hr       = NOERROR;

    if (m_cRuns >= m_cAllocated) {
        CHR(SafeRealloc((LPVOID*)&m_pRunList, (m_cAllocated + RUNLIST_ENTRIES) * sizeof(Run)));
        m_cAllocated += RUNLIST_ENTRIES;
    }
	m_pRunList[m_cRuns].eType = eType;
	m_pRunList[m_cRuns].dwRunLength = dwRunLength;
	m_pRunList[m_cRuns].dwOffset = dwOffset;
	m_cRuns++;

Error:
    return hr;

} /* CRunList::Insert()
   */

HRESULT
CRunList::AddToLastRunLength(DWORD dwRunLengthAdder)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    HRESULT         hr      = NOERROR;

    CBR(m_cRuns > 0);

    CBR(m_pRunList[m_cRuns-1].eType == RUNTYPE_DATATOKEN);

    m_pRunList[m_cRuns-1].dwRunLength += dwRunLengthAdder;

Error:
    return hr;

} /* CRunList::AddToLastRunLength()
   */

HRESULT
CRunList::GetTotalLength(DWORD *pdwTotalLength)
/*---------------------------------------------------------------------------*\
 * Return the total length of all the tokens in this run list.
 * This is how much space these tokens will take up in a diff file.
\*---------------------------------------------------------------------------*/
{
    HRESULT         hr      = NOERROR;
    DWORD           cBytes  = 0;
	DWORD           dwIndex;
    Run *           pRun    = m_pRunList;

    if(!pdwTotalLength) {
        CHR(E_INVALIDARG);
    }

    for (dwIndex = 0; dwIndex < m_cRuns; dwIndex++, pRun++) {
		cBytes += m_pImgData->TokenLen(pRun);
    }

    *pdwTotalLength = cBytes;

Error:
    return hr;

} /* CRunList::GetTotalLength()
   */

HRESULT
CRunList::CopyRuns(CRunList *pSrc)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    HRESULT         hr      = NOERROR;

    if (m_cRuns + pSrc->m_cRuns >= m_cAllocated) {
        CHR(SafeRealloc((LPVOID*)&m_pRunList, (m_cRuns + pSrc->m_cRuns + RUNLIST_ENTRIES) * sizeof(Run)));
        m_cAllocated = m_cRuns + pSrc->m_cRuns + RUNLIST_ENTRIES;
    }

    memcpy(m_pRunList + m_cRuns, pSrc->m_pRunList, pSrc->m_cRuns * sizeof(Run));
    m_cRuns += pSrc->m_cRuns;

Error:
    return hr;

} /* CRunList::CopyRuns()
   */
