////////////////////////////////////////////////////////////////////////////////
//
//  DDBVT TUX DLL
//  Copyright (c) 1996-1998, Microsoft Corporation
//
//  Module: dds5.h
//          Common declarations for IDirectDrawSurface5 tests.
//
//  Revision History:
//      12/05/1997  lblanco     Created.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __DDS5_H__
#define __DDS5_H__
#include "dds.h"

////////////////////////////////////////////////////////////////////////////////
// Types

struct DDS5TESTINFO
{
    IDirectDraw         *m_pDD;
    IDirectDrawSurface *m_pDDS5;
    DDSURFACEDESC      *m_pddsd;
    LPCTSTR             m_pszName;
    DWORD               m_dwTestData;
};

////////////////////////////////////////////////////////////////////////////////
// Prototypes

INT     Help_DDSIterate(
            DWORD,
            DDSTESTPROC,
            LPFUNCTION_TABLE_ENTRY,
            DWORD dwTestData);

////////////////////////////////////////////////////////////////////////////////

#endif // __DDS5_H__
