#ifndef __DDAUTOBLT_MAIN_H__
#define __DDAUTOBLT_MAIN_H__

#include <windows.h>

#include "config.h"

#include <stdlib.h>
#include <platform_config.h>
#include <tchar.h>

// This is for NT
#ifndef ASSERT
    #include "assert.h" 
    #define ASSERT assert
#endif

#include <ddraw.h>
#if DIRECTDRAW_VERSION>=0x0600
    #include <dvp.h>
#endif

#if CFG_PLATFORM_DREAMCAST
    #include <kat4cc.h>
    #include <mapledev.h>
#endif 

#include <hqalog.h>
#include <QATestUty/DebugUty.h>
#include <DDrawUty.h>

using DDrawUty::g_ErrorMap;

#endif
