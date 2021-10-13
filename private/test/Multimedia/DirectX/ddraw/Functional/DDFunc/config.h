// ============================================================================
//
//  DDrawFunc TUX DLL
//  Copyright (c) 1999, Microsoft Corporation
//
//  Module: config.h
//          Defines configuration settings for the Function test
//
//  Revision History:
//      01/11/2000  JonasK  Coppied from DSound
//
// ============================================================================

/* 
  ============================================================================
  TEST CONFIGURATION FLAGS
  ============================================================================
  
  Test Inclusion Flags
  --------------------
  CFG_TEST_DirectDrawAPI : top-level DDraw API tests
  CFG_TEST_IDirectDraw : IDirectDraw tests
  
*/

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <DDConfig.h>

// Default Settings
// ----------------
// Overall interface testing configuration
#define CFG_TEST_DirectDrawAPI          1
#define CFG_TEST_IDirectDraw            1
#define CFG_TEST_IDirectDraw2           1
#define CFG_TEST_IDirectDraw4           1
#define CFG_TEST_IDirectDrawPalette     1
#define CFG_TEST_IDirectDrawSurface     1
#define CFG_TEST_IDirectDrawSurface2    1
#define CFG_TEST_IDirectDrawSurface3    1
#define CFG_TEST_IDirectDrawSurface4    1
#define CFG_TEST_IDirectDrawSurface5    1
#define CFG_TEST_IDirectDrawColorControl 1
#define CFG_TEST_IDirectDrawClipper     1
// Specific, targeted testing
#define CFG_TEST_Blt                    1
#define CFG_TEST_Refcounting            0
#define CFG_TEST_ZBuffer                0

#if defined(DEBUG)
#   define CFG_TEST_INVALID_PARAMETERS  1
#else
#   define CFG_TEST_INVALID_PARAMETERS  0
#endif

/*
  ============================================================================
  PLATFORM SPECIFIC TEST CONFIGURATION FLAGS
  ============================================================================
  
  Override default test configuration flags with platform
  specific test configuration flags here.
*/

// Dreamcast Settings
// ------------------
#if CFG_PLATFORM_DREAMCAST
#define CFG_TEST_IDirectDrawSurface5    0
#define CFG_TEST_IDirectDrawColorControl 0
#define CFG_TEST_IDirectDrawClipper     0
#define CFG_TEST_Refcounting            1
#define CFG_TEST_ZBuffer                1
#endif

// ASTB Settings
// -------------
#if CFG_PLATFORM_ASTB
#endif

// DX Pack Settings
// ----------------
#if CFG_PLATFORM_DXPACK
#endif

// Mariner Settings
// ----------------
#if CFG_PLATFORM_MARINER
#endif

// Desktop Settings
// ----------------
#if CFG_PLATFORM_DESKTOP
#endif

// For backward compatability
#define QAHOME_OVERLAY          CFG_DirectDraw_Overlay
#define QAHOME_REFCOUNTING      CFG_TEST_Refcounting
#define QAHOME_INVALIDPARAMS    CFG_TEST_INVALID_PARAMETERS

#define TEST_APIS       CFG_TEST_DirectDrawAPI
#define TEST_IDD        CFG_TEST_IDirectDraw
#define TEST_IDD2       CFG_TEST_IDirectDraw2
#define TEST_IDD4       CFG_TEST_IDirectDraw4
#define TEST_IDDP       CFG_TEST_IDirectDrawPalette
#define TEST_IDDS       CFG_TEST_IDirectDrawSurface
#define TEST_IDDS2      CFG_TEST_IDirectDrawSurface2
#define TEST_IDDS3      CFG_TEST_IDirectDrawSurface3
#define TEST_IDDS4      CFG_TEST_IDirectDrawSurface4
#define TEST_IDDS5      CFG_TEST_IDirectDrawSurface5
#define TEST_IDDCC      CFG_TEST_IDirectDrawColorControl
#define TEST_IDDC       CFG_TEST_IDirectDrawClipper
#define TEST_BLT        CFG_TEST_Blt
#define TEST_ZBUFFER    CFG_TEST_ZBuffer

#define NEED_INITDDS    (TEST_IDDS || TEST_IDDS2 || TEST_IDDS3 || \
                         TEST_IDDCC || TEST_IDDP || TEST_BLT)
#define NEED_INITDD     (TEST_IDD || TEST_IDD2 || NEED_INITDDS)
#define NEED_CREATEDDP  (TEST_IDDP || TEST_IDDS)

#define SUPPORT_IDDCC_METHODS   0

#endif // header sentinel
