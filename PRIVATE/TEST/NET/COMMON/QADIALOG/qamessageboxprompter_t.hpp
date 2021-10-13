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
// Definitions and declarations for the QAMessageBoxPrompter_t class.
//
// ----------------------------------------------------------------------------

#pragma once

#include <CoreDllLoader_t.hpp>
#include <QADialogPrompter_t.hpp>

namespace CENetworkQA {

// ----------------------------------------------------------------------------
// 
// Tells QADialog_t how to deal with question/answer interaction
// using a MessageBox window.
//
class QAMessageBoxPrompter_t : public QADialogPrompter_t
{
private:

    // Handle of "coredll.dll":
    CoreDllLoader_t m_CoreDll;

    // Pointer to MessageBox procedure:
    CoreDllLoader_t::pMessageBox_t m_pMessageBoxProc;

public:

    // Constructor.
    QAMessageBoxPrompter_t(const QADialog_t &rDialog);

    // Destructor.
    virtual
   ~QAMessageBoxPrompter_t(void);
    
    // Initializes the object (if necessary) and makes sure it's ready.
    // False indicates there's a problem - use GetLastError for an explanation.
    virtual bool
    IsValid(void);
    
    // Sends the user a prompt and waits for a response.
    virtual QADialog_t::ResponseCode 
    RunDialog(
        LPCTSTR pPrompt,        // prompt to show user
        HANDLE *pEventHandles,  // OK/Cancel-button events
        long    MaxWaitTimeMS); // time to wait for user or events
};

};
// ----------------------------------------------------------------------------
