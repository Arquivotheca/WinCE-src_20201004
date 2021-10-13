//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// Copyright 2001, Cisco Systems, Inc.  All rights reserved.
// No part of this source, or the resulting binary files, may be reproduced,
// transmitted or redistributed in any form or by any means, electronic or
// mechanical, for any purpose, without the express written permission of Cisco.
//
// Support.h
#ifndef __support_h__
#define __support_h__
//#include "Core.h"
//
#ifndef u8_16_u32
#define u8_16_u32
typedef unsigned short  FID; 
typedef unsigned char   u8;
typedef unsigned short  u16;
typedef unsigned long   u32;
typedef u8              MacAddr[6];
#endif


inline ULONG MIN (ULONG l1, ULONG l2) 
{
    return (l1 < l2) ? l1 : l2;
}

inline ULONG MAX (ULONG l1, ULONG l2) 
{
    return (l1 > l2) ? l1 : l2;
}

inline LONG MIN (LONG l1, LONG l2) 
{
    return (l1 < l2) ? l1 : l2;
}

inline LONG MAX (LONG l1, LONG l2) 
{
    return (l1 > l2) ? l1 : l2;
}

void dumpMem (void *p, int count, int size);

typedef enum {
    OS_WIN95,
    OS_WINNT,
}WINDOWSOS;

extern WINDOWSOS    OSType;     
struct CSTACK{
    #define         STACKENGTH  1024    
//  CSTACK(){ ix = buf; end = buf+ STACKENGTH;}
    UINT            buf[STACKENGTH ];   
    UINT            *ix;
    UINT            *end;
};

inline void ResetStack(CSTACK &s)
{
    s.ix = s.buf; s.end = s.buf+ STACKENGTH;        
}
inline BOOLEAN push(CSTACK &s, UINT var)
{   
    if( s.ix != s.end ){
        *s.ix++ = var;
        return TRUE;
    }
    
    return FALSE;
}

inline BOOLEAN PopPeak(CSTACK &s, UINT & var)
{
    if( s.ix != s.buf ){
        var = *(s.ix-1);
        return TRUE;
    }
    return FALSE;
}

inline UINT pop(CSTACK &s )
{
    if( s.ix != s.buf ){
        return *--s.ix;
    }
    return 0xFFFFFFFF;
}

struct CQ{
    #define         QLENGTH     4   
    CQ(){   out = in = buf;end  = buf + QLENGTH;}
    UINT            buf[QLENGTH ];  
    UINT            *in;
    UINT            *out;
    UINT            *end;
};

void *  NdisMalloc( unsigned int size );

#define tolower( a )    (((a)|0x20))    

char*   NumStr          (ULONG value, ULONG base);


int
UnicodeStringToString(
    UCHAR *dest, 
    UNICODE_STRING *pUnicodeString, 
    UINT len, 
    int WinNT
    );

#ifdef NdisStringToWideNdisString
#undef NdisStringToWideNdisString
#endif  
void
NdisStringToWideNdisString( 
    PNDIS_STRING    dest,
    PNDIS_STRING    src
    );

void NdisInitString (PNDIS_STRING Destination, UCHAR *Source);  

//#if 0
/*
#if (NDISVER < 5) || (NDISVER == 41)
#ifdef NdisInitializeString
#undef NdisInitializeString 
#endif
void
NdisInitializeString( 
    PNDIS_STRING Destination, 
    UCHAR *Source 
    );  
#endif
*/


inline 
void            
CQReset(CQ & q)
{   
    q.out   = q.in = q.buf;
    q.end   = q.buf + QLENGTH;
}
    
inline
UINT
CQGetNext(CQ & q )
{
    UINT    *r = q.out;
    if( q.out == q.in )     
        return NULL; // empty
    
    if( ++q.out == q.end )  
        q.out = q.buf; 
    return *r;
}


inline
UINT        
CQGetNextND(CQ & q)
{
    return q.out==q.in ? NULL : *q.out;
}

inline
void                
CQUpdate(CQ & q)
{
    if(++q.out == q.end )
        q.out = q.buf;
}

inline
BOOLEAN             
CQStore( CQ & q, UINT v )
{
    UINT *tmp   = q.in; 
    *tmp        = v;
                        
    if (++tmp == q.end )    
        tmp = q.buf;
    if( tmp == q.out )  
        return FALSE;
    q.in = tmp;         
    return TRUE;
}

inline
BOOLEAN             
CQIsFull(CQ & q)
{
    UINT *tmp   = q.in; 
                        
    if (++tmp == q.end )    
        tmp = q.buf;
    
    return tmp == q.out;
}

inline
BOOLEAN             
CQIsEmpty(CQ & q)
{
    return q.in == q.out;
}

void    
DelayUS( 
    int delay 
    );      // delay in useconds

void
DelayUS100( 
    int delay 
    );

void
DelayMS( 
    int delay 
    );


BOOLEAN 
GetMacAddressEntry( 
    NDIS_STRING *Entry, 
    UCHAR *MacPtr, 
    NDIS_HANDLE ConfigHandle
    ); 

BOOLEAN 
StringToMac( 
    UCHAR *mac, 
    UCHAR *buf 
    );

UCHAR   
AsciiByteToHex( 
    UCHAR abyte 
    );
UCHAR 
AsciiWordToHex( 
    USHORT aword 
    );


BOOLEAN             
GetConfigEntry( 
    NDIS_HANDLE     ConfigHandle,
    NDIS_STRING     *EntryString,
    UINT            *intValue,
    NDIS_PARAMETER_TYPE type=NdisParameterHexInteger
    );


BOOLEAN
GetConfigEntry( 
    NDIS_HANDLE     ConfigHandle,
    NDIS_STRING     *EntryString,
    USHORT          *shortValue,
    NDIS_PARAMETER_TYPE type=NdisParameterHexInteger
    );

BOOLEAN             
GetConfigEntry( 
    NDIS_HANDLE     ConfigHandle,
    NDIS_STRING     *EntryString,
    UCHAR           *ucharValue,
    NDIS_PARAMETER_TYPE type=NdisParameterHexInteger
    );

int                 
GetConfigEntry( 
    NDIS_HANDLE     ConfigHandle,
    NDIS_STRING     *EntryString,
    UCHAR           *strValue,  
    UINT            nByte
    );


BOOLEAN
GetConfigEntry( 
    NDIS_HANDLE     ConfigHandle,
    NDIS_STRING     *EntryString,
    NDIS_STRING     *strValue   
    );


//---------------------------------------------------------------------------
// Simple timer class.
//---------------------------------------------------------------------------
#if (NDISVER >= 5) && (NDISVER != 41)
class Timer 
{
public:
    Timer () 
    {
        reset();
    }

    void reset (void) 
    {
        NdisGetCurrentSystemTime(&startTime);
    }

    LONGLONG elapsedMS (void) 
    {
        return(elapsedKns() / 10000);
    }
    
    LONGLONG elapsedUS (void) 
    {
        return(elapsedKns() / 10);
    }

    LONGLONG elapsedKns (void) 
    {
        LARGE_INTEGER endTime;
        NdisGetCurrentSystemTime(&endTime);
        return (LONGLONG)(endTime.QuadPart - startTime.QuadPart);
    }

private:
    LARGE_INTEGER   startTime;  // start ticks (in Kns)
};
#endif


#endif
