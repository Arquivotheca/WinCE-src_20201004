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
//+---------------------------------------------------------------------------------
//
//
// File:
//      memutil.h
//
// Contents:
//
//      Memory utilities
//
//----------------------------------------------------------------------------------

#ifndef _MEMUTIL_H_INCLUDED
#define _MEMUTIL_H_INCLUDED

class CMemWStr
{
  private:
    WCHAR *m_pwstr;
    
  public:  
    CMemWStr() {m_pwstr = NULL;}
    ~CMemWStr() {reset(); }
    inline void reset() { if (m_pwstr) delete [] m_pwstr;  m_pwstr = NULL;}
    inline WCHAR * get() {return m_pwstr;}
    inline void set(WCHAR *pwstr) {reset() ; m_pwstr = pwstr;}
    inline BOOL isEmpty() { return (!m_pwstr);}
    inline WCHAR * transfer() { WCHAR *ptemp = m_pwstr; m_pwstr = NULL; return ptemp; }
    inline void alloc_copy(WCHAR *pwstr) { 
        reset() ; 
        if (pwstr)
        	{
	            m_pwstr = new WCHAR[wcslen(pwstr) + 1];
	        	if (m_pwstr)
	        		wcscpy(m_pwstr, pwstr);
        	}
        }
    inline void alloc_sz(UINT nwchars) { reset() ; m_pwstr = (WCHAR *) new WCHAR[nwchars]; }
    inline WCHAR ** operator&() { reset() ; return &m_pwstr; }
};


// A utility BSTR class that auto-cleans its memory
class CMemBSTR
{
  private:
    BSTR m_bstr;
    
  public:  
    CMemBSTR() {m_bstr = NULL;}
    ~CMemBSTR() {reset(); }
    inline void reset() { if (m_bstr) SysFreeString(m_bstr);  m_bstr = NULL;}
    inline BSTR get() {return m_bstr;}
    inline void set(BSTR bstr) {reset() ; m_bstr = bstr;}
    inline BOOL isEmpty() { return (!m_bstr);}
    inline BSTR transfer() { BSTR bstrtemp = m_bstr; m_bstr = NULL; return bstrtemp; }
    inline void alloc_copy(BSTR bstr) { reset() ; m_bstr = SysAllocString(bstr); }
    inline BSTR * operator&() { reset() ; return &m_bstr; }
};

#endif _MEMUTIL_H_INCLUDED
