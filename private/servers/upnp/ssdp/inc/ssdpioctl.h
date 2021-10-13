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
#pragma once

#include <psl_marshaler.hxx>

#define UPNP_IOCTL_INVOKE                       2000
#define SSDP_IOCTL_INVOKE                       2001

#define SSDP_IOCTL_FIRST                        1


#define SSDP_IOCTL_REGISTERNOTIFICATION         1
#define SSDP_IOCTL_DEREGISTERNOTIFICATION       2
#define SSDP_IOCTL_GETSUBSCRIPTIONID            3
#define SSDP_IOCTL_REGISTER_SERVICE             7
#define SSDP_IOCTL_DEREGISTER_SERVICE           8
#define SSDP_IOCTL_LAST                         10


#define UPNP_IOCTL_FIRST                        20
#define UPNP_IOCTL_ADD_DEVICE                   21
#define UPNP_IOCTL_REMOVE_DEVICE                22
#define UPNP_IOCTL_PUBLISH_DEVICE               23
#define UPNP_IOCTL_UNPUBLISH_DEVICE             24
#define UPNP_IOCTL_SUBMIT_PROPERTY_EVENT        25
#define UPNP_IOCTL_SET_RAW_CONTROL_RESPONSE     26
#define UPNP_IOCTL_GET_UDN                      27
#define UPNP_IOCTL_GET_SCPD_PATH                28
#define UPNP_IOCTL_UPDATE_EVENTED_VARIABLES     29
#define UPNP_IOCTL_LAST                         30
#define UPNP_IOCTL_INIT_RECV_INVOKE_REQUEST     31
#define UPNP_IOCTL_RECV_INVOKE_REQUEST          32
#define UPNP_IOCTL_CANCEL_RECV_INVOKE_REQUEST   33


typedef DWORD (*PFUPNPCALLBACK)(PVOID pvContext, PBYTE pbInBuf, DWORD cbInBuf);

//
// ce::psl_data_wrapper specialization for PFUPNPCALLBACK
//
template<>
class ce::psl_data_wrapper<PFUPNPCALLBACK>
{
public:
    void marshal(PFUPNPCALLBACK x)
        {data = x; }
    
    PFUPNPCALLBACK unmarshal(HANDLE)
        {return data; }

private:
    PFUPNPCALLBACK data;
};


//
// ce::copy_in specialization for SSDP_MESSAGE*
//
template<>
class ce::copy_in<SSDP_MESSAGE*>
    : public ce::ptr_copy_traits_base<SSDP_MESSAGE>
{
public:
    typedef ce::ptr_copy_traits_base<SSDP_MESSAGE> _Mybase;
    
    copy_in(HANDLE hClientProcess)
        : _Mybase(hClientProcess)
    {
    }
    
    void copy_arg_in(SSDP_MESSAGE* pMsg)
    {
        assert(m_pServerPtr == NULL);
        
        if(pMsg)
        {
            HRESULT hr = m_MappedClientPtr.Marshal(pMsg,detail::size_of<SSDP_MESSAGE>::value,ARG_I_PTR,FALSE);
            if  SUCCEEDED(hr)
            {
                m_LocalCopy = static_cast<SSDP_MESSAGE*>(m_MappedClientPtr.ptr());
            }
            else
            {
                assert(false);       
            }

            // allocate storage on the heap.
            m_pAltHeaders   = new ce::copy_in<LPCSTR>(m_hClientProcess);
            m_pContent      = new ce::copy_in<LPCSTR>(m_hClientProcess);
            m_pLocHeader    = new ce::copy_in<LPCSTR>(m_hClientProcess);
            m_pNls          = new ce::copy_in<LPCSTR>(m_hClientProcess);
            m_pSid          = new ce::copy_in<LPCSTR>(m_hClientProcess);
            m_pType         = new ce::copy_in<LPCSTR>(m_hClientProcess);
            m_pUSN          = new ce::copy_in<LPCSTR>(m_hClientProcess);

            // check allocation succeeded
            if (!m_pAltHeaders       ||
                !m_pContent          ||
                !m_pLocHeader        ||
                !m_pNls              ||
                !m_pSid              ||
                !m_pType             ||
                !m_pUSN )
            {
                return ;
            }
            // map nested pointers and copy the values
            m_pAltHeaders->copy_arg_in(verify_ptr(m_LocalCopy->szAltHeaders, sizeof(*m_LocalCopy->szAltHeaders), m_hClientProcess));
            m_pContent->copy_arg_in   (verify_ptr(m_LocalCopy->szContent, sizeof(*m_LocalCopy->szContent), m_hClientProcess));
            m_pLocHeader->copy_arg_in (verify_ptr(m_LocalCopy->szLocHeader, sizeof(*m_LocalCopy->szLocHeader), m_hClientProcess));
            m_pNls->copy_arg_in       (verify_ptr(m_LocalCopy->szNls, sizeof(*m_LocalCopy->szNls), m_hClientProcess));
            m_pSid->copy_arg_in       (verify_ptr(m_LocalCopy->szSid, sizeof(*m_LocalCopy->szSid), m_hClientProcess));
            m_pType->copy_arg_in      (verify_ptr(m_LocalCopy->szType, sizeof(*m_LocalCopy->szType), m_hClientProcess));
            m_pUSN->copy_arg_in       (verify_ptr(m_LocalCopy->szUSN, sizeof(*m_LocalCopy->szUSN), m_hClientProcess));
            
            // points members to the copied values
            m_LocalCopy->szAltHeaders = const_cast<LPSTR>(static_cast<LPCSTR>(*m_pAltHeaders));
            m_LocalCopy->szContent    = const_cast<LPSTR>(static_cast<LPCSTR>(*m_pContent));
            m_LocalCopy->szLocHeader  = const_cast<LPSTR>(static_cast<LPCSTR>(*m_pLocHeader));
            m_LocalCopy->szNls        = const_cast<LPSTR>(static_cast<LPCSTR>(*m_pNls));
            m_LocalCopy->szSid        = const_cast<LPSTR>(static_cast<LPCSTR>(*m_pSid));
            m_LocalCopy->szType       = const_cast<LPSTR>(static_cast<LPCSTR>(*m_pType));
            m_LocalCopy->szUSN        = const_cast<LPSTR>(static_cast<LPCSTR>(*m_pUSN));
            
            m_pServerPtr = m_LocalCopy;
        }
    }
    
    void copy_arg_out()
    {
    }
    
private:
    SSDP_MESSAGE        *m_LocalCopy;

    // ARM Compiler workaround :
    // Bug 1: This object gets copied on the stack.
    // Bug 2 (112193): If all arguments to a function >=  256 then we get bad function linkage.

    // This object is very large, and causes a function to hit Bug 2.
    // solution, create most of this object on the heap.
    ce::auto_ptr<ce::copy_in<LPCSTR> > m_pAltHeaders;
    ce::auto_ptr<ce::copy_in<LPCSTR> > m_pContent;
    ce::auto_ptr<ce::copy_in<LPCSTR> > m_pLocHeader;
    ce::auto_ptr<ce::copy_in<LPCSTR> > m_pNls;
    ce::auto_ptr<ce::copy_in<LPCSTR> > m_pSid;
    ce::auto_ptr<ce::copy_in<LPCSTR> > m_pType;
    ce::auto_ptr<ce::copy_in<LPCSTR> > m_pUSN;
};


//
// ce::copy_in specialization for UPNPDEVICEINFO*
//
template<>
class ce::copy_in<UPNPDEVICEINFO*>
    : public ce::ptr_copy_traits_base<UPNPDEVICEINFO>
{
public:
    typedef ce::ptr_copy_traits_base<UPNPDEVICEINFO> _Mybase;
    
    copy_in(HANDLE hClientProcess)
        : _Mybase(hClientProcess),
          m_DeviceDescription(hClientProcess),
          m_DeviceName(hClientProcess),
          m_UDN(hClientProcess)
    {
    }
    
    void copy_arg_in(UPNPDEVICEINFO* pDevInfo)
    {
        assert(m_pServerPtr == NULL);
        
        if(pDevInfo)
        {
            HRESULT hr = m_MappedClientPtr.Marshal(pDevInfo,detail::size_of<UPNPDEVICEINFO>::value,ARG_I_PTR,FALSE);
            if  SUCCEEDED(hr)
            {
                m_LocalCopy = static_cast<UPNPDEVICEINFO*>(m_MappedClientPtr.ptr());
            }
            else
            {
                assert(false);
            }
        

            // map nested pointers and copy the values
            m_DeviceDescription.copy_arg_in(verify_ptr(m_LocalCopy->pszDeviceDescription, sizeof(*m_LocalCopy->pszDeviceDescription), m_hClientProcess));
            m_DeviceName.copy_arg_in(verify_ptr(m_LocalCopy->pszDeviceName, sizeof(*m_LocalCopy->pszDeviceName), m_hClientProcess));
            m_UDN.copy_arg_in(verify_ptr(m_LocalCopy->pszUDN, sizeof(*m_LocalCopy->pszUDN), m_hClientProcess));
            

            // points members to copied values
            m_LocalCopy->pszDeviceDescription = const_cast<LPWSTR>(static_cast<LPCWSTR>(m_DeviceDescription));
            m_LocalCopy->pszDeviceName = const_cast<LPWSTR>(static_cast<LPCWSTR>(m_DeviceName));
            m_LocalCopy->pszUDN = const_cast<LPWSTR>(static_cast<LPCWSTR>(m_UDN));
            
            m_pServerPtr = m_LocalCopy;
        }
    }
    
    void copy_arg_out()
    {
    }
    
private:
    UPNPDEVICEINFO*          m_LocalCopy;
    ce::copy_in<LPCWSTR>    m_DeviceDescription;
    ce::copy_in<LPCWSTR>    m_DeviceName;
    ce::copy_in<LPCWSTR>    m_UDN;
};



//
// ce::copy_in specialization for UPNPPARAM*
//
template<>
class ce::copy_in<UPNPPARAM>
    : ce::copy_traits_base
{
public:
    typedef ce::copy_traits_base _Mybase;
    
    copy_in(HANDLE hClientProcess)
        : _Mybase(hClientProcess),
          m_Name(hClientProcess),
          m_Value(hClientProcess)
    {
    }
    
    void copy_arg_in(UPNPPARAM DevInfo)
    {
        m_LocalCopy = DevInfo;
        
        // map nested pointers and copy the values
        m_Name.copy_arg_in  (verify_ptr(m_LocalCopy.pszName, sizeof(*m_LocalCopy.pszName), m_hClientProcess));
        m_Value.copy_arg_in (verify_ptr(m_LocalCopy.pszValue, sizeof(*m_LocalCopy.pszValue), m_hClientProcess));
        
        // points members to copied values
        m_LocalCopy.pszName  = const_cast<LPWSTR>(static_cast<LPCWSTR>(m_Name));
        m_LocalCopy.pszValue = const_cast<LPWSTR>(static_cast<LPCWSTR>(m_Value));
    }
    
    void copy_arg_out()
    {
    }
    
public:
    operator UPNPPARAM()
    {
        return m_LocalCopy;
    }
    
private:
    UPNPPARAM               m_LocalCopy;
    ce::copy_in<LPCWSTR>    m_Name;
    ce::copy_in<LPCWSTR>    m_Value;
};


extern "C" {
BOOL WINAPI
SDP_IOControl(
    DWORD  dwOpenData, 
    DWORD  dwCode, 
    PBYTE  pBufIn,
    DWORD  dwLenIn, 
    PBYTE  pBufOut, 
    DWORD  dwLenOut,
    PDWORD pdwActualOut
    );
}
