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

#include <windows.h>
#include <assert.h>
#include <vector.hxx>

//****************************
// Utility functions 
//****************************

inline WORD ReadWord (PBYTE pData)
{
    return (pData[0] << 8) | pData[1];
}

inline DWORD ReadDword (PBYTE pData)
{
    return (pData[0] << 24) | (pData[1] << 16) | (pData[2] << 8) | pData[3];
}

//****************************
// Protocols 
//****************************

#define PROTOCOL_ETHERNET		1
#define PROTOCOL_ARP			2

#define PROTOCOL_EAPOL			10
#define PROTOCOL_EAP			11

#define PROTOCOL_IPV4			20
#define PROTOCOL_ICMP			21
#define PROTOCOL_UDP			22
#define PROTOCOL_TCP			23
#define PROTOCOL_DHCP			24


//********************************************
// Generic Buffer, points to a chunk of data in a frame 
//********************************************

typedef struct _BUFFER
{
    _BUFFER ( BYTE * _pData, DWORD _cbSize): 
        pData (_pData), cbSize (_cbSize)
    {
    };

     _BUFFER ( ) 
    {
        Zero();
    };   

     void Zero ()
    {
        memset (this, 0, sizeof(_BUFFER));
    }

    void Advance (DWORD n)
    {
        assert (pData && n <= cbSize);
        pData += n;
        cbSize -= n;
    };
    
    BYTE * pData;          // pointer to buffer
    DWORD cbSize;        // size of buffer
} BUFFER, * PBUFFER;

//**********************************************
// ProtocolStack_t holds a frame's recognized protocols
// It doesn't copy any data, so all buffer pointers must be valid
// throughout the life of this object
//**********************************************

class ProtocolStack_t
{
public:
    ProtocolStack_t();
    
    void SetFrameData (_int64 TimeStamp, DWORD FrameNumber);
    void GetFrameData (_int64 * pTimeStamp, DWORD * pFrameNumber) const;
    
    HRESULT PushProtocolInstance (DWORD protocol, const BUFFER& buffer);
    HRESULT GetProtocolInstance (DWORD protocol, PBUFFER buffer, int instance = 1) const;
    
private:
    typedef std::pair <DWORD, BUFFER> ProtocolInstance_t;
    typedef ce::vector<ProtocolInstance_t> ProtocolInstanceList_t;
    ProtocolInstanceList_t m_Protocols;

    _int64 m_TimeStamp;
    DWORD m_FrameNumber;
};


//**********************************************
// FrameParser_t is the parser engine
//**********************************************

class FrameParser_t
{
public:
    HRESULT ParseFromCapFile (const wchar_t * FileName);

    virtual void HandleProtocol (const ProtocolStack_t& Stack, int protocol) {};

private:
    HRESULT ProcessEthernet (ProtocolStack_t& Stack, const BUFFER& buffer);
    HRESULT ProcessEapol (ProtocolStack_t& Stack, const BUFFER& buffer);
    HRESULT ProcessIPv4 (ProtocolStack_t& Stack, const BUFFER& buffer);
    HRESULT ProcessUdp (ProtocolStack_t& Stack, const BUFFER& buffer);
    HRESULT ProcessTcp (ProtocolStack_t& Stack, const BUFFER& buffer);
};

