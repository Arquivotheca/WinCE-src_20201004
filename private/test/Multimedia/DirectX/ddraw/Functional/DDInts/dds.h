////////////////////////////////////////////////////////////////////////////////
//
//  DDBVT TUX DLL
//  Copyright (c) 1996-1998, Microsoft Corporation
//
//  Module: dds.h
//          Common declarations for IDirectDrawSurface tests.
//
//  Revision History:
//      12/05/1997  lblanco     Created.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __DDS_H__
#define __DDS_H__

////////////////////////////////////////////////////////////////////////////////
// Macros

#define DDSI_TESTBACKBUFFER     1

////////////////////////////////////////////////////////////////////////////////
// Types

typedef INT (WINAPI *DDSTESTPROC)(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
struct DDSTESTINFO
{
    IDirectDraw         *m_pDD;
    IDirectDrawSurface  *m_pDDS;
    DDSURFACEDESC       *m_pddsd;
    LPCTSTR             m_pszName;
    DWORD               m_dwTestData;
};

////////////////////////////////////////////////////////////////////////////////
// Prototypes

ITPR    FillSurface(
            DWORD               dwCookie,
            IDirectDrawSurface  *pDDS,
            RECT                *prcFill = NULL);
ITPR    VerifySurfaceContents(
            DWORD               dwCookie,
            IDirectDrawSurface  *pDDS,
            RECT                *prcCurrent,
            RECT                *prcFilled);
INT     Help_DDSIterate(
            DWORD,
            DDSTESTPROC,
            LPFUNCTION_TABLE_ENTRY,
            DWORD dwTestData = 0);

////////////////////////////////////////////////////////////////////////////////

#endif // __DDS_H__
