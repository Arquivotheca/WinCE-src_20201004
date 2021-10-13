////////////////////////////////////////////////////////////////////////////////
//
//  DDBVT TUX DLL
//  Copyright (c) 1996-1998, Microsoft Corporation
//
//  Module: main.h
//          Header for all files in the project.
//
//  Revision History:
//      09/12/96    lblanco     Created for the TUX DLL Wizard.
//      07/01/99    a-shende    re-enabling invalid tests.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __MAIN_H__
#define __MAIN_H__

////////////////////////////////////////////////////////////////////////////////
// Included files

#include <windows.h>
#include <stdlib.h>
#include <math.h>
#include <tchar.h>
#include <ddraw.h>
#ifdef KATANA
#include <kat4cc.h>
#endif // KATANA
#include <kato.h>
#include <tux.h>
#include "ddqahelp.h"
#include "hqalog.h"
#include "hqaexcpt.h"

////////////////////////////////////////////////////////////////////////////////
// Flags

#ifdef UNDER_CE
    #define QAHOME_OVERLAY              1
    #define QAHOME_REFCOUNTING          1
    #ifdef DEBUG
        #define QAHOME_INVALIDPARAMS    1
    #else // DEBUG
        #define QAHOME_INVALIDPARAMS    0
    #endif // QAHOME_INVALIDPARAMS
#else // UNDER_CE
    #define QAHOME_INVALIDPARAMS        1
    #define QAHOME_REFCOUNTING          1
#endif // UNDER_CE


////////////////////////////////////////////////////////////////////////////////

#endif // __MAIN_H__
