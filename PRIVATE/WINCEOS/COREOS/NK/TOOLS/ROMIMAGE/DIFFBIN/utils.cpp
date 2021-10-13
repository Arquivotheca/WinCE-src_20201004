// utils.cpp

#include "diffbin.h"

HRESULT
SafeRealloc(LPVOID *pPtr, UINT32 cBytes)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    HRESULT         hr      = NOERROR;

    ASSERT(pPtr);

    if(*pPtr) {
        LPVOID pvTemp;
        CPR(pvTemp = LocalReAlloc(*pPtr, cBytes, LMEM_MOVEABLE));
        *pPtr = pvTemp;
    } else {
        CPR(*pPtr = LocalAlloc(LMEM_FIXED, cBytes));
    }

Error:
    return hr;

} /* SafeRealloc()
   */



/****************************************************************************
    Function used by the asserting variants of the macros (CHRA, CBRA, etc.)

    Returns TRUE to cause a DebugBreak().  The DebugBreak() actually happens
    in the macro so that the debugger actually stops in the line that failed
    instead of this funciton.

    It is still included in the retail builds so that the export table for
    ossvcs.dll will be the same between retail and debug.  However, it will
    not be exported in a SHIP_BUILD (and should go away during linking)

    (s): KKennedy
 ****************************************************************************/
BOOL OnAssertionFail(
    eCodeType eType, // in - type of the assertion
    DWORD dwCode, // return value for what failed
    const TCHAR* pszFile, // in - filename
    unsigned long ulLine, // in - line number
    const TCHAR* pszMessage // in - message to include in assertion report
    )
{
#ifdef DEBUG
    DWORD dwLastError = GetLastError();
    TCHAR szBuffer[1000];
    TCHAR szErrorCode[100];
    BOOL fAssert = TRUE;

    szErrorCode[0] = 0;
    switch(eType)
        {
        case eHRESULT:
            wsprintf(szErrorCode, TEXT("HRESULT=0x%08x"), dwCode);
            if(E_OUTOFMEMORY == dwCode)
                {
                // Don't assert on out of memory
                fAssert = FALSE;
                }
            break;
        case eBOOL:
            wsprintf(szErrorCode, TEXT("BOOL=0x%08x"), dwCode);
            break;
        case ePOINTER:
            wsprintf(szErrorCode, TEXT("Pointer=0x%08x"), dwCode);
            break;
        case eWINDOWS:
            if((ERROR_NOT_ENOUGH_MEMORY == dwLastError) || 
               (ERROR_DISK_FULL == dwLastError) || 
               (ERROR_HANDLE_DISK_FULL == dwLastError))
                {
                // Don't assert on out of memory
                fAssert = FALSE;
                }
            break;
        }

    wsprintf(szErrorCode + lstrlen(szErrorCode), TEXT(" GetLastError()=%d (0x%08x)"), dwLastError, dwLastError);
    wsprintf(szBuffer, TEXT("\r\nAssertion failure:\r\n\tFile: %s    Line: %d\r\n\t%s\r\n\t%s\r\n%s"),
            pszFile, ulLine, pszMessage, szErrorCode, fAssert ? TEXT("") : TEXT("\tNot asserting\r\n"));

    OutputDebugString(szBuffer);

    #ifdef DEBUG
    return(fAssert); // always assert right now
    #else
    return(FALSE); // this should never get called...
    #endif
#else 
    return FALSE;
#endif 
}
