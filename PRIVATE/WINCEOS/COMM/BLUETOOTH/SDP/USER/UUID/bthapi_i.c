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

#pragma warning( disable: 4049 )  /* more than 64k source lines */

/* this ALWAYS GENERATED file contains the IIDs and CLSIDs */

/* link this file in with the server and any clients */


 /* File created by MIDL compiler version 5.03.0286 */
/* Compiler settings for BthAPI.idl:
    Oicf (OptLev=i2), W1, Zp8, env=Win32 (32b run), ms_ext, c_ext
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
//@@MIDL_FILE_HEADING(  )

#if !defined(_M_IA64) && !defined(_M_AXP64)

#ifdef __cplusplus
extern "C"{
#endif 


#include <rpc.h>
#include <rpcndr.h>

#ifdef _MIDL_USE_GUIDDEF_

#ifndef INITGUID
#define INITGUID
#include <guiddef.h>
#undef INITGUID
#else
#include <guiddef.h>
#endif

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
        DEFINE_GUID(name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8)

#else // !_MIDL_USE_GUIDDEF_

#ifndef __IID_DEFINED__
#define __IID_DEFINED__

typedef struct _IID
{
    unsigned long x;
    unsigned short s1;
    unsigned short s2;
    unsigned char  c[8];
} IID;

#endif // __IID_DEFINED__

#ifndef CLSID_DEFINED
#define CLSID_DEFINED
typedef IID CLSID;
#endif // CLSID_DEFINED

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
        const type name = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}

#endif !_MIDL_USE_GUIDDEF_

MIDL_DEFINE_GUID(IID, IID_ISdpWalk,0x57134AE6,0x5D3C,0x462D,0xBF,0x2F,0x81,0x03,0x61,0xFB,0xD7,0xE7);


MIDL_DEFINE_GUID(IID, IID_ISdpNodeContainer,0x43F6ED49,0x6E22,0x4F81,0xA8,0xEB,0xDC,0xED,0x40,0x81,0x1A,0x77);


MIDL_DEFINE_GUID(IID, IID_ISdpSearch,0xD93B6B2A,0x5EEF,0x4E1E,0xBE,0xCF,0xF5,0xA4,0x34,0x0C,0x65,0xF5);


MIDL_DEFINE_GUID(IID, IID_ISdpStream,0xA6ECD9FB,0x0C7A,0x41A3,0x9F,0xF0,0x0B,0x61,0x7E,0x98,0x93,0x57);


MIDL_DEFINE_GUID(IID, IID_ISdpRecord,0x10276714,0x1456,0x46D7,0xB5,0x26,0x8B,0x1E,0x83,0xD5,0x11,0x6E);


MIDL_DEFINE_GUID(IID, IID_IBluetoothDevice,0x5BD0418B,0xD705,0x4766,0xB2,0x15,0x18,0x3E,0x4E,0xAD,0xE3,0x41);


MIDL_DEFINE_GUID(IID, IID_IBluetoothAuthenticate,0x5F0FBA2B,0x8300,0x429D,0x99,0xAD,0x96,0xA2,0x83,0x5D,0x49,0x01);


MIDL_DEFINE_GUID(IID, LIBID_BTHAPILib,0x00BC26C8,0x0A87,0x41d0,0x82,0xBA,0x61,0xFF,0x9E,0x0B,0x1B,0xB5);


MIDL_DEFINE_GUID(CLSID, CLSID_SdpNodeContainer,0xD5CA76C5,0x0DEE,0x4453,0x96,0xA1,0xE6,0x03,0xC2,0x40,0x17,0x66);


MIDL_DEFINE_GUID(CLSID, CLSID_SdpSearch,0x3B898402,0x857E,0x4e41,0x91,0x45,0xBC,0x35,0x43,0x1B,0x7B,0x4D);


MIDL_DEFINE_GUID(CLSID, CLSID_SdpWalk,0xED384010,0x59AE,0x44c7,0x8F,0xCA,0xF3,0xDF,0x22,0xCD,0xCD,0x28);


MIDL_DEFINE_GUID(CLSID, CLSID_SdpStream,0x249797FA,0x19DB,0x4dda,0x94,0xD4,0xE0,0xBC,0xD3,0x0E,0xA6,0x5E);


MIDL_DEFINE_GUID(CLSID, CLSID_SdpRecord,0xACD02BA7,0x9667,0x4085,0xA1,0x00,0xCC,0x6A,0xCA,0x96,0x21,0xD6);

#undef MIDL_DEFINE_GUID

#ifdef __cplusplus
}
#endif



#endif /* !defined(_M_IA64) && !defined(_M_AXP64)*/


#pragma warning( disable: 4049 )  /* more than 64k source lines */

/* this ALWAYS GENERATED file contains the IIDs and CLSIDs */

/* link this file in with the server and any clients */


 /* File created by MIDL compiler version 5.03.0286 */
/* Compiler settings for BthAPI.idl:
    Oicf (OptLev=i2), W1, Zp8, env=Win64 (32b run,appending), ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
//@@MIDL_FILE_HEADING(  )

#if defined(_M_IA64) || defined(_M_AXP64)

#ifdef __cplusplus
extern "C"{
#endif 


#include <rpc.h>
#include <rpcndr.h>

#ifdef _MIDL_USE_GUIDDEF_

#ifndef INITGUID
#define INITGUID
#include <guiddef.h>
#undef INITGUID
#else
#include <guiddef.h>
#endif

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
        DEFINE_GUID(name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8)

#else // !_MIDL_USE_GUIDDEF_

#ifndef __IID_DEFINED__
#define __IID_DEFINED__

typedef struct _IID
{
    unsigned long x;
    unsigned short s1;
    unsigned short s2;
    unsigned char  c[8];
} IID;

#endif // __IID_DEFINED__

#ifndef CLSID_DEFINED
#define CLSID_DEFINED
typedef IID CLSID;
#endif // CLSID_DEFINED

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
        const type name = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}

#endif !_MIDL_USE_GUIDDEF_

MIDL_DEFINE_GUID(IID, IID_ISdpWalk,0x57134AE6,0x5D3C,0x462D,0xBF,0x2F,0x81,0x03,0x61,0xFB,0xD7,0xE7);


MIDL_DEFINE_GUID(IID, IID_ISdpNodeContainer,0x43F6ED49,0x6E22,0x4F81,0xA8,0xEB,0xDC,0xED,0x40,0x81,0x1A,0x77);


MIDL_DEFINE_GUID(IID, IID_ISdpSearch,0xD93B6B2A,0x5EEF,0x4E1E,0xBE,0xCF,0xF5,0xA4,0x34,0x0C,0x65,0xF5);


MIDL_DEFINE_GUID(IID, IID_ISdpStream,0xA6ECD9FB,0x0C7A,0x41A3,0x9F,0xF0,0x0B,0x61,0x7E,0x98,0x93,0x57);


MIDL_DEFINE_GUID(IID, IID_ISdpRecord,0x10276714,0x1456,0x46D7,0xB5,0x26,0x8B,0x1E,0x83,0xD5,0x11,0x6E);


MIDL_DEFINE_GUID(IID, IID_IBluetoothDevice,0x5BD0418B,0xD705,0x4766,0xB2,0x15,0x18,0x3E,0x4E,0xAD,0xE3,0x41);


MIDL_DEFINE_GUID(IID, IID_IBluetoothAuthenticate,0x5F0FBA2B,0x8300,0x429D,0x99,0xAD,0x96,0xA2,0x83,0x5D,0x49,0x01);


MIDL_DEFINE_GUID(IID, LIBID_BTHAPILib,0x00BC26C8,0x0A87,0x41d0,0x82,0xBA,0x61,0xFF,0x9E,0x0B,0x1B,0xB5);


MIDL_DEFINE_GUID(CLSID, CLSID_SdpNodeContainer,0xD5CA76C5,0x0DEE,0x4453,0x96,0xA1,0xE6,0x03,0xC2,0x40,0x17,0x66);


MIDL_DEFINE_GUID(CLSID, CLSID_SdpSearch,0x3B898402,0x857E,0x4e41,0x91,0x45,0xBC,0x35,0x43,0x1B,0x7B,0x4D);


MIDL_DEFINE_GUID(CLSID, CLSID_SdpWalk,0xED384010,0x59AE,0x44c7,0x8F,0xCA,0xF3,0xDF,0x22,0xCD,0xCD,0x28);


MIDL_DEFINE_GUID(CLSID, CLSID_SdpStream,0x249797FA,0x19DB,0x4dda,0x94,0xD4,0xE0,0xBC,0xD3,0x0E,0xA6,0x5E);


MIDL_DEFINE_GUID(CLSID, CLSID_SdpRecord,0xACD02BA7,0x9667,0x4085,0xA1,0x00,0xCC,0x6A,0xCA,0x96,0x21,0xD6);

#undef MIDL_DEFINE_GUID

#ifdef __cplusplus
}
#endif



#endif /* defined(_M_IA64) || defined(_M_AXP64)*/

