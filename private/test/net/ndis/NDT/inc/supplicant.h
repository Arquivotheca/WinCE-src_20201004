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
// Supplicant.h : Declaration of the CSupplicant

#ifndef __SUPPLICANT_H_
#define __SUPPLICANT_H_
                                                                     
/////////////////////////////////////////////////////////////////////////////
// CSupplicant
class CSupplicant  
{
    public:
	CSupplicant()
	{
	}

// ISupplicant
public:
//	STDMETHOD(get_DebugLevel)(/*[out, retval]*/ LONG *pVal);
//	STDMETHOD(put_DebugLevel)(/*[in]*/ LONG newVal);
	BOOL WaitAuthenticationComplete(/*[in]*/ LONG timeout);
       BOOL StartAuthenticator(/*[in]*/ char* password,/*[in]*/ NDIS_802_11_SSID ssid);
	BOOL CloseSupplicant();
	BOOL OpenSupplicant(/*[in]*/ LPTSTR szAdapterName);
	void (get_GroupKeyIndex)(/*[out, retval]*/ LONG *pVal);
       static DWORD __stdcall RecvThread(LPVOID pThis);
	VOID FinalRelease();
	HRESULT FinalConstruct();
private:
   THREADINFO     m_threadInfo;
   HANDLE         m_thread;
   DWORD       m_threadID;
};

#endif //__SUPPLICANT_H_
