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

#endif // header sentinel
