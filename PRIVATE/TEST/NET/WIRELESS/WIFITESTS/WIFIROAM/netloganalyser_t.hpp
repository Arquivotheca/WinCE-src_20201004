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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// Definitions and declarations for the NetlogAnalyser_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_NetlogAnalyser_t_
#define _DEFINED_NetlogAnalyser_t_
#pragma once

#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <windows.h>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// This class, designed to be instantiated as a static object within the
// main test program, is used to periodically run an analysis of the WiFi
// packet exchanges to calculate the time taken to roam between Access Points.
//
class NetlogManager_t;
class NetlogAnalyser_t
{
private:

    // Basename of the netlog capture files:
    const TCHAR *m_pBasename;

    // Existing netlog-manager object:
    NetlogManager_t *m_pManager;

    // Number captures analysed:
    long m_NumberCaptures;

    // Last capture-file name:
    TCHAR m_CapFileName[MAX_PATH];

    // Copy and assignment are deliberately disabled:
    NetlogAnalyser_t(const NetlogAnalyser_t &src);
    NetlogAnalyser_t &operator = (const NetlogAnalyser_t &src);

protected:

public:
    
    // Initializes or cleans up static resources:
    static void StartupInitialize(void);
    static void ShutdownCleanup  (void);
    
    // Constructor and destructor:
    NetlogAnalyser_t(const TCHAR *pNetlogBasename);
    virtual
   ~NetlogAnalyser_t(void);

    // Called at the begining of each cycle. If there's an existing netlog
    // capture file, stops netlog and fires off a thread to analyse the
    // roaming times. In any case, if this isn't the first cycle we've
    // seen, starts another capture.
    void
    StartCapture(
        DWORD CycleNumber);
};

};
};
#endif /* _DEFINED_NetlogAnalyser_t_ */
// ----------------------------------------------------------------------------
