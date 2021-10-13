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
// Definitions and declarations for the QADialogPrompter_t class.
//
// ----------------------------------------------------------------------------

#pragma once

#include <QADialog_t.hpp>

namespace CENetworkQA {

// ----------------------------------------------------------------------------
// 
// Tells QADialog_t how to deal with question/answer interaction.
// 
// Applications must derive from this and supply the SendPrompt and 
// WaitResponse methods to tell QADialog_t how to interact with the
// user via the debug log.
//
class QADialogPrompter_t
{
private:

    // Object running the dialog.
    const QADialog_t &m_rDialog;
    
public:

    // Constructor.
    QADialogPrompter_t(const QADialog_t &rDialog);

    // Destructor.
    virtual 
   ~QADialogPrompter_t(void);

    // Gets the object running the dialog.
    const QADialog_t &
    GetDialog(void) const { return m_rDialog; }

    // Initializes the object (if necessary) and makes sure it's ready.
    // False indicates there's a problem - use GetLastError for explanation.
    virtual bool
    IsValid(void) = 0;
    
    // Sends the user a prompt and waits for a response.
    virtual QADialog_t::ResponseCode 
    RunDialog(
        LPCTSTR pPrompt,            // prompt to show user
        HANDLE *pEventHandles,      // OK/Cancel-button events
        long    MaxWaitTimeMS) = 0; // time to wait for user or events
    
protected:

    // Sends the specified message to the log. 
    void 
    Log(DWORD Verbosity, LPCTSTR pFormat, ...) const;
    void 
    LogV(DWORD Verbosity, LPCTSTR pFormat, va_list varArgs) const; 
};

};
// ----------------------------------------------------------------------------
