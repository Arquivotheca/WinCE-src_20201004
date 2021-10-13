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
// Support.cpp
#pragma code_seg("LCODE")

#include "NDISVER.h"

extern "C"{
    #include    <ndis.h>
}
#include <memory.h> 
#include "Support.h"
#include <string.h>

//===========================================================================
    void dumpMem (void *p, int count, int size)
//===========================================================================
// 
// Description: Dump memory using DbgPrint(), beginning at address p.  May be 
//              dumped as bytes, words or dwords based on the size argument.
//    
//      Inputs: p    - pointer to memory to dump. 
//              len  - number of bytes to dump.
//              size - size to print (may be 1, 2, or 4 -- byte, word, dword).
//    
//     Returns: nothing.
//    
//      (01/26/01)
//---------------------------------------------------------------------------
{
    if ((size != 2) && (size != 4)) {
        size = 1;
        }
    PUCHAR  ptr = (PUCHAR)p;
    int printCount = 0;
    for (int i = 0; i < count; i++) {
        switch (size) {
            case 1:
                DbgPrint("%02X ", *ptr);
                break;
            case 2:
                DbgPrint("%04X ", *((PUSHORT)ptr));
                break;
            case 4:
                DbgPrint("%08X ", *((PULONG)ptr));
                break;
            }
        printCount += size;
        if ((printCount % 16) == 0) {
            DbgPrint("\n");
            printCount = 0;
            }
        ptr += size;
        }
    DbgPrint("\n");
}

//===========================================================================
    void* NdisMalloc (unsigned int size)
//===========================================================================
// 
// Description: 
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{ 
    void    *p;
    NDIS_PHYSICAL_ADDRESS HighestAcceptableMax = NDIS_PHYSICAL_ADDRESS_CONST(-1,-1);

#if NDISVER == 3
    NdisAllocateMemory(&p, size, 0, HighestAcceptableMax);
#else   
    NdisAllocateMemoryWithTag(&p, size, 'oriA');
#endif  
    return p;
}

//===========================================================================
    char* NumStr (ULONG value, ULONG base)
//===========================================================================
// 
// Description: Convert the specified value to a string using the specified
//              base.  The returned string is static and is overwritten on 
//              each call to this function.
//        7
//      Inputs: value   - value to convert to a string.
//              base    - base to use when converting value.
//    
//     Returns: Pointer to static string containing string representation of
//              the specified value.
//    
//      (10/26/00)
//---------------------------------------------------------------------------
{
    // 
    // 33 bytes is the largest this buffer needs to be 
    // (i.e. value==0xFFFFFFFF, base == 2)
    // 
    static char buffer[33]; 
    char *buf = buffer;
    *buf = '\0';
    // 
    // Calculate the highest power of 10 <= value.
    // 
    ULONG   val     = value;
    ULONG   divisor = 1;
    while (val >= base) {
        val     /= base;
        divisor *= base;
        }
    // 
    // Do the conversion
    // 
    ULONG digit;
    while (divisor >= 1) {
        digit = value / divisor;
        if (digit > 9) {
            *buf++ = (char)((digit - 10) + 'A');
            }
        else {
            *buf++ = (char)(digit + '0');
            }
        value %= divisor;
        divisor /= base;
        }
    *buf = '\0';
    return(buffer);
}

/*
int
UnicodeStringToString(PUCHAR dest, PUNICODE_STRING pUnicodeString, UINT len, int WinNT )
{
    UINT        i, j;
    UINT        UnicodeStrLen = pUnicodeString->Length;

    if( WinNT )
        UnicodeStrLen /= 2;

    if( len >  UnicodeStrLen )
        len = UnicodeStrLen;
    
    NdisZeroMemory(dest, len);
    
    for(i=j=0; i< len; i++ ){
        dest[j] = WinNT ? *(PUCHAR)(pUnicodeString->Buffer+i) :
                *((PUCHAR)(pUnicodeString->Buffer)+i);
        if( dest[j] )
            ++j;
    }       
    return j;
}
*/   
/*
typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
#ifdef MIDL_PASS
    [size_is(MaximumLength / 2), length_is((Length) / 2) ] USHORT * Buffer;
#else // MIDL_PASS
    PWSTR  Buffer;
#endif // MIDL_PASS
} UNICODE_STRING;
typedef UNICODE_STRING *PUNICODE_STRING;
#define UNICODE_NULL ((WCHAR)0) // winnt
*/
int
UnicodeStringToString(PUCHAR dest, PUNICODE_STRING pUnicodeString, UINT len, int WinNT )
{
    UINT        i, j;
    UINT        UnicodeStrLen = pUnicodeString->Length;

    if( WinNT )
        UnicodeStrLen /= 2;

    if( len >  UnicodeStrLen )
        len = UnicodeStrLen;
    
    NdisZeroMemory(dest, len);
    
    //for(i=j=0; i< len; i++ ){
    for(i=j=0; j< len && i<UnicodeStrLen; i++ ){
        dest[j] = WinNT ? *(PUCHAR)(pUnicodeString->Buffer+i) :
                *((PUCHAR)(pUnicodeString->Buffer)+i);
        if( dest[j] )
            ++j;
    }       
    return j;
}
   

void
NdisStringToWideNdisString( 
    PNDIS_STRING    dest,
    PNDIS_STRING    src
    )
{   
    UCHAR   *ptr = (PUCHAR)src->Buffer; 
    
    for( dest->Length=0; dest->Length < src->Length; ptr++ ){
        if( ' ' <= *ptr && 0x7F >= *ptr )
            dest->Buffer[ dest->Length++ ] = *ptr;              
    }
    dest->MaximumLength = dest->Length;         
}

//===========================================================================
    void NdisInitString (PNDIS_STRING Destination, PUCHAR Source)
//===========================================================================
// 
// Description: This was defined as a macro in ndis.h (NdisInitializeString) 
//              in the NT 3.51 DDK.  It became a function as of the NT4 DDK.  
//              Apparently, NDIS5 under Win98 does not define this function so 
//              we made it a function local to the driver.  Be sure to call 
//              NdisFreeString on the allocated string.
//    
//      Inputs: Destination - destination string
//              Source      - source string
//    
//     Returns: nothing.
//---------------------------------------------------------------------------
{                           
    NDIS_PHYSICAL_ADDRESS HighestAcceptableMax = NDIS_PHYSICAL_ADDRESS_CONST(-1,-1);

    PNDIS_STRING    _D  = Destination;                                      
    UCHAR           *_S = Source;                                                   
    WCHAR           *_P;                                                                

    _D->Length          = (strlen((char*)_S)) * sizeof(WCHAR);                              
    _D->MaximumLength   = _D->Length + sizeof(WCHAR);                         
    
    NdisAllocateMemory((PVOID *)&(_D->Buffer), _D->MaximumLength, 0, HighestAcceptableMax);  
    
    _P = _D->Buffer;                                                    
    
    while(*_S != '\0') {                                                                       
        *_P = (WCHAR)(*_S);                                             
        _S++;                                                           
        _P++;                                                           
        }
    *_P = UNICODE_NULL;                                                 
}


/*
//#if 0
#if (NDISVER < 5) || (NDISVER == 41)
void
NdisInitializeString( 
    PNDIS_STRING Destination, 
    PUCHAR Source 
    )           
{                           
    NDIS_PHYSICAL_ADDRESS HighestAcceptableMax = NDIS_PHYSICAL_ADDRESS_CONST(-1,-1);

    PNDIS_STRING    _D  = Destination;                                      
    UCHAR           *_S = Source;                                                   
    WCHAR           *_P;                                                                

    _D->Length = (strlen((char*)_S)) * sizeof(WCHAR);                              
    _D->MaximumLength = _D->Length + sizeof(WCHAR);                         
    
    NdisAllocateMemory((PVOID *)&(_D->Buffer), _D->MaximumLength, 0, 
        HighestAcceptableMax);  
    
    _P = _D->Buffer;                                                    
    
    while(*_S != '\0')                                                  
    {                                                                       
        *_P = (WCHAR)(*_S);                                             
        _S++;                                                           
        _P++;                                                           
    }                                                                   
    *_P = UNICODE_NULL;                                                 
}
#endif
*/

void
DelayUS( int delay )        // delay i useconds
{
#ifndef CE_FLAG_USE_NDISM_SLEEP
    if ( delay >= 50 ){
        for( ;delay > 0; delay -=50 )
            NdisStallExecution( 50 );
    }
    if( delay > 0 )
        NdisStallExecution( delay );
#else

	NdisMSleep(delay);

#endif
}

void
DelayUS100( int delay )
{
    for( delay<<=1; delay; delay-- )
        NdisStallExecution( 50 );
}

void
DelayMS( int delay )
{
#ifndef CE_FLAG_USE_NDISM_SLEEP
    for( delay*=20; delay; delay-- )
        NdisStallExecution( 50 );
#else
	/*
	//stall a maximum of 50 ms at a time
	if( delay > 50 ) 
	{
		for ( ;delay > 50; delay -=50 )
		{
			RETAILMSG (1, (TEXT("Delay = [%d]\r\n"), delay));
			NdisStallExecution( (UINT)(50 * 1000) );
		}
	}
	if( delay > 0 )
		NdisStallExecution( delay * 1000 );
	*/

	RETAILMSG (0, (TEXT("Delay = [%d]\r\n"), delay));
	NdisMSleep(delay * 1000);
	RETAILMSG (0, (TEXT("Delay = [%d] done!\r\n"), delay));

#endif
}

BOOLEAN 
GetMacAddressEntry( 
    NDIS_STRING     *Entry, 
    UCHAR           *mac, 
    NDIS_HANDLE     ConfigHandle
    )
{
    UCHAR   tmp[13];
    NdisZeroMemory(mac, 6 );
    int len = GetConfigEntry( ConfigHandle, Entry, tmp, 12 );
    return len ? StringToMac(mac, tmp) : FALSE;
}

BOOLEAN
StringToMac( 
    UCHAR _mac[6], 
    UCHAR *buf 
    )
{
    for( int i=0; i<12; i+=2 ){
        
        char    cMost = AsciiByteToHex(buf[i] );
        char    cLeast = AsciiByteToHex(buf[i+1] );
        
        if( (char)0xFF == cMost || (char)0xFF==cLeast ){
            NdisZeroMemory(_mac, 6 );
            return FALSE;
        }
       _mac[ i/2 ] = (cMost<<4) | cLeast;
    }               
    return TRUE;
}

UCHAR 
AsciiByteToHex( 
    UCHAR abyte 
    )
{
    return  abyte >= '0' && abyte <= '9' ? abyte - 0x30 :
            abyte >= 'A' && abyte <= 'F' ? abyte - 0x37 :
            abyte >= 'a' && abyte <= 'f' ? abyte - 0x57 :
            0xFF;
}

UCHAR 
AsciiWordToHex( 
    USHORT aword 
    )
{
    return  (AsciiByteToHex(aword >> 8) & 0xF) | (AsciiByteToHex(aword & 0xFF)<<4);
}

//===========================================================================
    BOOLEAN GetConfigEntry (NDIS_HANDLE         ConfigHandle,
                            NDIS_STRING         *EntryString,
                            unsigned int        *intValue,
                            NDIS_PARAMETER_TYPE type)
//===========================================================================
// 
// Description: 
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{
    NDIS_STATUS                     Status;
    PNDIS_CONFIGURATION_PARAMETER   ReturnedValue;// The value read from the registry.

    NdisReadConfiguration(&Status, &ReturnedValue, ConfigHandle, EntryString, type/*NdisParameterHexInteger*/);
    
    if (NDIS_STATUS_SUCCESS == Status) {
        *intValue = ReturnedValue->ParameterData.IntegerData;
        return TRUE;
        }
    return FALSE;
}


//===========================================================================
    BOOLEAN GetConfigEntry (NDIS_HANDLE         ConfigHandle,
                            NDIS_STRING         *EntryString,
                            unsigned short      *shortValue,
                            NDIS_PARAMETER_TYPE type)
//===========================================================================
// 
// Description: 
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{
    NDIS_STATUS                     Status;
    PNDIS_CONFIGURATION_PARAMETER   ReturnedValue;// The value read from the registry.

    NdisReadConfiguration(&Status, &ReturnedValue, ConfigHandle, EntryString, type /*NdisParameterHexInteger*/);
    
    if (NDIS_STATUS_SUCCESS == Status) {
        *shortValue = (USHORT)ReturnedValue->ParameterData.IntegerData;
        return TRUE;
        }
    return FALSE;
}

//===========================================================================
    BOOLEAN GetConfigEntry (NDIS_HANDLE         ConfigHandle,
                            NDIS_STRING         *EntryString,
                            UCHAR               *ucharValue,
                            NDIS_PARAMETER_TYPE type)
//===========================================================================
// 
// Description: 
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{
    NDIS_STATUS                     Status;
    PNDIS_CONFIGURATION_PARAMETER   ReturnedValue;// The value read from the registry.

    NdisReadConfiguration(&Status, &ReturnedValue, ConfigHandle, EntryString, type /*NdisParameterHexInteger*/);
    
    if (NDIS_STATUS_SUCCESS == Status) {
        *ucharValue = (UCHAR)ReturnedValue->ParameterData.IntegerData;
        return TRUE;
        }
    return FALSE;
}

//===========================================================================
    int GetConfigEntry (NDIS_HANDLE     ConfigHandle,
                        NDIS_STRING     *EntryString,
                        UCHAR           *strValue,  
                        UINT            nByte)
//===========================================================================
// 
// Description: 
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{
    NDIS_STATUS                     Status;
    PNDIS_CONFIGURATION_PARAMETER   ReturnedValue;// The value read from the registry.

    NdisReadConfiguration(&Status, &ReturnedValue, ConfigHandle, EntryString, NdisParameterString);

    if ((NDIS_STATUS_SUCCESS != Status) || (NULL == ReturnedValue->ParameterData.StringData.Buffer)) {
        return 0;
        }

    return UnicodeStringToString(strValue, &(ReturnedValue->ParameterData.StringData), nByte, OSType); 
}


//===========================================================================
    BOOLEAN GetConfigEntry (NDIS_HANDLE ConfigHandle,
                            NDIS_STRING *EntryString,
                            NDIS_STRING *strValue)
//===========================================================================
// 
// Description: 
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{
    NDIS_STATUS                     Status;
    PNDIS_CONFIGURATION_PARAMETER   ReturnedValue;// The value read from the registry.

    NdisReadConfiguration(&Status, &ReturnedValue, ConfigHandle, EntryString, NdisParameterString);

    if ((NDIS_STATUS_SUCCESS != Status) || (NULL == ReturnedValue->ParameterData.StringData.Buffer)) {
        return FALSE;
        }

    NdisStringToWideNdisString(strValue, &ReturnedValue->ParameterData.StringData); 

    return TRUE;
}









/*

//===========================================================================
    int StrLen (char *str)
//===========================================================================
// 
// Description: Return the length of the specified string.
//    
//      Inputs: str - pointer to a string.
//    
//     Returns: length of the string.
//---------------------------------------------------------------------------
{
    int i = 0;
    while (str && *(str+i)) {
        ++i;
        }
    return i;
}

//===========================================================================
    char* StrCpy (char *dest, char *src)
//===========================================================================
// 
// Description: Copy the source string to the destination string.  No 
//              overflow checking is performed.
//    
//      Inputs: dest    - pointer to destination string
//              src     - pointer to source string
//    
//     Returns: Pointer to the destination string.
//---------------------------------------------------------------------------
{
    char *ptr = dest;
    while (*dest++ = *src++);
    return ptr; 
}

//===========================================================================
    char* StrCat (char *dest, char *src)
//===========================================================================
// 
// Description: Append the source string to the destination string.  No 
//              overflow checking is performed.
//    
//      Inputs: dest    - pointer to destination string
//              src     - pointer to source string
//    
//     Returns: Pointer to the destination string.
//    
//      (10/25/00)
//---------------------------------------------------------------------------
{
    char *ptr = dest + StrLen(dest);
    return(StrCpy(ptr, src));
}

//===========================================================================
    int StrCmp (char *str1, char *str2)
//===========================================================================
// 
// Description: 
//    
//      Inputs: 
//    
//     Returns: 
//    
//      (11/14/00)
//---------------------------------------------------------------------------
{
    int len1 = StrLen(str1);
    int len2 = StrLen(str2);

    int ret = memcmp(str1, str2, len1 < len2 ? len1 : len2);

    if (ret == 0) {
        if (len1 < len2) {
            ret = -1;
            }
        else if (len2 > len1) {
            ret = 1;
            }
        }
    return(ret);
}

//===========================================================================
    void* MemCpy (void *dest, void *src, size_t size)
//===========================================================================
// 
// Description: Copies byte between buffers.
//    
//      Inputs: dest    - pointer to destination buffer.
//              src     - pointer to source buffer.
//              size    - number of bytes to copy.
//    
//     Returns: dest
//    
//      (10/26/00)
//---------------------------------------------------------------------------
{
    char *p1 = (char*)dest;
    char *p2 = (char*)src;
    for (size_t i = 0; i < size; i++) {
        *p1++ = *p2++;
        }
    return(dest);
}

//===========================================================================
    void* MemSet (void *dest, int c, size_t count)
//===========================================================================
// 
// Description: Set count bytes beginning at the byte pointed to by dest to c.
//    
//      Inputs: 
//    
//     Returns: 
//    
//      (11/10/00)
//---------------------------------------------------------------------------
{
    char *p1 = (char*)dest;
    for (size_t i = 0; i < count; i++) {
        *p1++ = (char)c;
        }
    return(dest);
}

*/


/*
//===========================================================================
    int memcmp (void *buf1, void *buf2, size_t count)
//===========================================================================
// 
// Description: Compare characters in two buffers
//    
//      Inputs: buf1    - pointer to first buffer.
//              buf2    - pointer to second buffer.
//              count   - number of chars to compare.
//    
//     Returns: -1  (buf1 < buf2)
//               0  (buf1 == buf2)
//               1  (buf1 > buf2)
//    
//      (10/26/00)
//---------------------------------------------------------------------------
{
    int retval = 0;
    char *b1 = (char*)buf1;
    char *b2 = (char*)buf2;
    for (size_t i = 0; i < count; i++) {
        char c1 = *b1++;
        char c2 = *b2++;
        if (c1 < c2) {
            return(-1);
            }
        else if (c1 > c2) {
            return(1);
            }
        }
    return(0);
}

//===========================================================================
    void* memcpy (void *dest, void *src, size_t size)
//===========================================================================
// 
// Description: Copies byte between buffers.
//    
//      Inputs: dest    - pointer to destination buffer.
//              src     - pointer to source buffer.
//              size    - number of bytes to copy.
//    
//     Returns: dest
//    
//      (10/26/00)
//---------------------------------------------------------------------------
{
    char *p1 = (char*)dest;
    char *p2 = (char*)src;
    for (size_t i = 0; i < size; i++) {
        *p1++ = *p2++;
        }
    return(dest);
}

//===========================================================================
    void* memset (void *dest, int c, size_t count)
//===========================================================================
// 
// Description: Set count bytes beginning at the byte pointed to by dest to c.
//    
//      Inputs: 
//    
//     Returns: 
//    
//      (11/10/00)
//---------------------------------------------------------------------------
{
    char *p1 = (char*)dest;
    for (size_t i = 0; i < count; i++) {
        *p1++ = (char)c;
        }
    return(dest);
}
*/
