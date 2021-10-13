//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
// runlist.h

#define RUNLIST_ENTRIES     10

enum RunType {
    RUNTYPE_SECTIONHEADER,                  // RunLength = 0,       Offset = CSectionData *
    RUNTYPE_DATATOKEN,                      // RunLength = Length,  Offset = LPBYTE (pointer to actual data)
    RUNTYPE_RAWDATA,                        // RunLength = Length,  Offset = LPBYTE (pointer to actual data)
    RUNTYPE_COPYTOKEN,                      // RunLength = Length,  Offset = Offset into Old image to copy from
    RUNTYPE_FIXUPCOMMANDS,                  // RunLength = 0,       Offset = CSectionData *
    RUNTYPE_COMPRESSIONCOMMANDS,            // RunLength = 0,       Offset = CSectionData *
    RUNTYPE_ZEROBLOCK,                      // RunLength = Length,  Offset = 0
};

struct Run
{
    RunType         eType;
    DWORD           dwRunLength;
    DWORD           dwOffset;
};

// forward definition
class CImageData;

class CRunList
{
public:
    CRunList();
    ~CRunList();

    HRESULT         Initialize(CImageData *pImgData);
    HRESULT         Clear();
    HRESULT         Insert(RunType eType, DWORD dwRunLength, DWORD dwOffset);
    HRESULT         GetTotalLength(DWORD *pdwTotalLength);
    HRESULT         AddToLastRunLength(DWORD dwRunLengthAdder);
    HRESULT         CopyRuns(CRunList *pSrc);

    DWORD           GetRunCount() { return m_cRuns; }
    Run *           GetRun(int iIndex)
    {
        if(iIndex < 0 || iIndex >= m_cRuns) {
            return NULL;
        }
        return &m_pRunList[iIndex];
    }

private:
    Run *           m_pRunList;
    DWORD           m_cRuns;
    DWORD           m_cAllocated;
    CImageData *    m_pImgData;


};
