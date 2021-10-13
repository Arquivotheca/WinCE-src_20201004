// utils.h

HRESULT SafeRealloc(LPVOID *pPtr, UINT32 cBytes);

BOOL OnAssertionFail(
    eCodeType eType, // in - type of the assertion
    DWORD dwCode, // return value for what failed
    const TCHAR* pszFile, // in - filename
    unsigned long ulLine, // in - line number
    const TCHAR* pszMessage // in - message to include in assertion report
    );
