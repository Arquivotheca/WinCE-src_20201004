////////////////////////////////////////////////////////////////////////////////
//
//  DDBVT TUX DLL
//  Copyright (c) 1996-1998, Microsoft Corporation
//
//  Module: ifacetbl.h
//          Interface table for DirectX. Each entry in the table is a DirectX
//          interface. Each entry is represented by a macro which, properly
//          declared, causes the table to generate an array of pointers to all
//          IIDs, an array to pointers to descriptions to each IID, or an
//          enumerated type that can be used as an index into the two arrays.
//          It also creates strings the same way that text.h does (which means
//          that these names should not be duplicated in text.h). This file is
//          meant to be included from globals.h only.
//
//  Revision History:
//      07/09/1997  lblanco     Created.
//
////////////////////////////////////////////////////////////////////////////////

#if TEST_IDD
KNOWN_INTERFACE(IDirectDraw)
#endif // TEST_IDD
#if TEST_IDDCC
KNOWN_INTERFACE(IDirectDrawColorControl)
#endif // TEST_IDDCC
#if TEST_IDDP
KNOWN_INTERFACE(IDirectDrawPalette)
#endif // TEST_IDDP
#if TEST_IDDS
KNOWN_INTERFACE(IDirectDrawSurface)
#endif // TEST_IDDS
#if TEST_IDDC
KNOWN_INTERFACE(IDirectDrawClipper)
#endif // TEST_IDDC
