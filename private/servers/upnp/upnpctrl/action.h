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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#ifndef __ACTION__
#define __ACTION__

#include "vector.hxx"
#include "Argument.h"
#include "StateVar.h"
#include "sax.h"
#include "HttpRequest.h"

class Action : ce::SAXContentHandler
{
public:
    Action(LPCWSTR pwszName, LPCWSTR pwszNamespace);

    void AddInArgument(const Argument& arg)
        {m_InArguments.push_back(arg); }

    void AddOutArgument(const Argument& arg)
        {m_OutArguments.push_back(arg); }

    int GetInArgumentsCount()
        {return m_InArguments.size(); }

    Argument& GetInArgument(int index)
        {return m_InArguments[index]; }

    int GetOutArgumentsCount()
        {return m_OutArguments.size(); }

    Argument& GetOutArgument(int index)
        {return m_OutArguments[index]; }

    LPCWSTR GetName()
        {return m_strName; }

    LPCWSTR GetNamespace()
        {return m_strNamespace; }

    LPCSTR GetSoapActionName()
        {return m_strSoapActionName; }

    int GetFaultCode()
    {
        wchar_t *p;
        return wcstol(m_strFaultCode, &p, 10);
    }
    
    LPCWSTR GetFaultDescription()
        {return m_strFaultDescription; }

    LPCWSTR CreateSoapMessage();
    HRESULT ParseSoapResponse(HttpRequest& request);
    void BindArgumentsToStateVars(ce::vector<StateVar>::const_iterator itBegin, ce::vector<StateVar>::const_iterator itEnd);

// ISAXContentHandler
private:
    virtual HRESULT STDMETHODCALLTYPE characters( 
        /* [in] */ const wchar_t __RPC_FAR *pwchChars,
        /* [in] */ int cchChars);

private:
    ce::wstring             m_strName;
    ce::wstring             m_strNamespace;
    ce::wstring             m_strSoapMessage;
    ce::string              m_strSoapActionName;
    ce::wstring             m_strFaultCode;
    ce::wstring             m_strFaultDescription;
    ce::vector<Argument>    m_InArguments;
    ce::vector<Argument>    m_OutArguments;
};

#endif // __ACTION__
