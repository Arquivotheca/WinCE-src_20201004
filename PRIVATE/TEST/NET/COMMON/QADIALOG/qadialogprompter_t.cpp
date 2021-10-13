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
// Implentation of the QADialogPrompter_t classes.
//
// ----------------------------------------------------------------------------

#include <QADialogPrompter_t.hpp>
#include <assert.h>

using namespace CENetworkQA;

// ----------------------------------------------------------------------------
//
// Constructor.
//
QADialogPrompter_t::
QADialogPrompter_t(const QADialog_t &rDialog)
    : m_rDialog(rDialog)
{
    // nothing to do.
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
QADialogPrompter_t::
~QADialogPrompter_t(void)
{
    // nothing to do.
}

// ----------------------------------------------------------------------------
//
// Sends the specified message to the debug log.
//
void
QADialogPrompter_t::
Log(DWORD Verbosity, LPCTSTR pFormat, ...) const
{
    va_list va;
    va_start(va, pFormat);
    m_rDialog.LogV(Verbosity, pFormat, va);
    va_end(va);
}
void
QADialogPrompter_t::
LogV(DWORD Verbosity, LPCTSTR pFormat, va_list va) const
{
    m_rDialog.LogV(Verbosity, pFormat, va);
}

// ----------------------------------------------------------------------------
