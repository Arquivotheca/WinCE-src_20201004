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

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 5.03.0286 */
/* at Mon Aug 21 16:32:45 2006
 */
/* Compiler settings for .\dshowfix.idl:
    Oicf (OptLev=i2), W1, Zp8, env=Win32 (32b run), ms_ext, c_ext
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
//@@MIDL_FILE_HEADING(  )


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 440
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif // __RPCNDR_H_VERSION__

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif /*COM_NO_WINDOWS_H*/

#ifndef __dshowfix_h__
#define __dshowfix_h__

/* Forward Declarations */ 

#ifndef __IAMLatency_FWD_DEFINED__
#define __IAMLatency_FWD_DEFINED__
typedef interface IAMLatency IAMLatency;
#endif 	/* __IAMLatency_FWD_DEFINED__ */


#ifndef __IAMPushSource_FWD_DEFINED__
#define __IAMPushSource_FWD_DEFINED__
typedef interface IAMPushSource IAMPushSource;
#endif 	/* __IAMPushSource_FWD_DEFINED__ */


#ifndef __IMpeg2Demultiplexer_FWD_DEFINED__
#define __IMpeg2Demultiplexer_FWD_DEFINED__
typedef interface IMpeg2Demultiplexer IMpeg2Demultiplexer;
#endif 	/* __IMpeg2Demultiplexer_FWD_DEFINED__ */


#ifndef __IEnumStreamIdMap_FWD_DEFINED__
#define __IEnumStreamIdMap_FWD_DEFINED__
typedef interface IEnumStreamIdMap IEnumStreamIdMap;
#endif 	/* __IEnumStreamIdMap_FWD_DEFINED__ */


#ifndef __IMPEG2StreamIdMap_FWD_DEFINED__
#define __IMPEG2StreamIdMap_FWD_DEFINED__
typedef interface IMPEG2StreamIdMap IMPEG2StreamIdMap;
#endif 	/* __IMPEG2StreamIdMap_FWD_DEFINED__ */


/* header files for imported files */
#include "unknwn.h"
#include "objidl.h"
#include "strmif.h"

#ifdef __cplusplus
extern "C"{
#endif 

void __RPC_FAR * __RPC_USER MIDL_user_allocate(size_t);
void __RPC_USER MIDL_user_free( void __RPC_FAR * ); 

/* interface __MIDL_itf_dshowfix_0000 */
/* [local] */ 

//  {afb6c280-2c41-11d3-8a60-0000f81e0e4a}
DEFINE_GUID(CLSID_MPEG2Demultiplexer,
0xafb6c280, 0x2c41, 0x11d3, 0x8a, 0x60, 0x00, 0x00, 0xf8, 0x1e, 0x0e, 0x4a);
// 138AA9A4-1EE2-4c5b-988E-19ABFDBC8A11
DEFINE_GUID(MEDIASUBTYPE_MPEG2_TRANSPORT_STRIDE,
0x138aa9a4, 0x1ee2, 0x4c5b, 0x98, 0x8e, 0x19, 0xab, 0xfd, 0xbc, 0x8a, 0x11);
// {3ae86b20-7be8-11d1-abe6-00a0c905f375}
DEFINE_GUID(CLSID_MMSPLITTER,
0x3ae86b20, 0x7be8, 0x11d1, 0xab, 0xe6, 0x00, 0xa0, 0xc9, 0x05, 0xf3, 0x75);






enum _AM_PUSHSOURCE_FLAGS
    {	AM_PUSHSOURCECAPS_INTERNAL_RM	= 0x1,
	AM_PUSHSOURCECAPS_NOT_LIVE	= 0x2,
	AM_PUSHSOURCECAPS_PRIVATE_CLOCK	= 0x4,
	AM_PUSHSOURCEREQS_USE_STREAM_CLOCK	= 0x10000,
	AM_PUSHSOURCEREQS_USE_CLOCK_CHAIN	= 0x20000
    };


extern RPC_IF_HANDLE __MIDL_itf_dshowfix_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_dshowfix_0000_v0_0_s_ifspec;

#ifndef __IAMLatency_INTERFACE_DEFINED__
#define __IAMLatency_INTERFACE_DEFINED__

/* interface IAMLatency */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IAMLatency;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("62EA93BA-EC62-11d2-B770-00C04FB6BD3D")
    IAMLatency : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetLatency( 
            /* [in] */ REFERENCE_TIME __RPC_FAR *prtLatency) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IAMLatencyVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IAMLatency __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IAMLatency __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IAMLatency __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetLatency )( 
            IAMLatency __RPC_FAR * This,
            /* [in] */ REFERENCE_TIME __RPC_FAR *prtLatency);
        
        END_INTERFACE
    } IAMLatencyVtbl;

    interface IAMLatency
    {
        CONST_VTBL struct IAMLatencyVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IAMLatency_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IAMLatency_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IAMLatency_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IAMLatency_GetLatency(This,prtLatency)	\
    (This)->lpVtbl -> GetLatency(This,prtLatency)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IAMLatency_GetLatency_Proxy( 
    IAMLatency __RPC_FAR * This,
    /* [in] */ REFERENCE_TIME __RPC_FAR *prtLatency);


void __RPC_STUB IAMLatency_GetLatency_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IAMLatency_INTERFACE_DEFINED__ */


#ifndef __IAMPushSource_INTERFACE_DEFINED__
#define __IAMPushSource_INTERFACE_DEFINED__

/* interface IAMPushSource */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IAMPushSource;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("F185FE76-E64E-11d2-B76E-00C04FB6BD3D")
    IAMPushSource : public IAMLatency
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetPushSourceFlags( 
            /* [out] */ ULONG __RPC_FAR *pFlags) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetPushSourceFlags( 
            /* [in] */ ULONG Flags) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetStreamOffset( 
            /* [in] */ REFERENCE_TIME rtOffset) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetStreamOffset( 
            /* [out] */ REFERENCE_TIME __RPC_FAR *prtOffset) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetMaxStreamOffset( 
            /* [out] */ REFERENCE_TIME __RPC_FAR *prtMaxOffset) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetMaxStreamOffset( 
            /* [in] */ REFERENCE_TIME rtMaxOffset) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IAMPushSourceVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IAMPushSource __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IAMPushSource __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IAMPushSource __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetLatency )( 
            IAMPushSource __RPC_FAR * This,
            /* [in] */ REFERENCE_TIME __RPC_FAR *prtLatency);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetPushSourceFlags )( 
            IAMPushSource __RPC_FAR * This,
            /* [out] */ ULONG __RPC_FAR *pFlags);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *SetPushSourceFlags )( 
            IAMPushSource __RPC_FAR * This,
            /* [in] */ ULONG Flags);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *SetStreamOffset )( 
            IAMPushSource __RPC_FAR * This,
            /* [in] */ REFERENCE_TIME rtOffset);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetStreamOffset )( 
            IAMPushSource __RPC_FAR * This,
            /* [out] */ REFERENCE_TIME __RPC_FAR *prtOffset);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetMaxStreamOffset )( 
            IAMPushSource __RPC_FAR * This,
            /* [out] */ REFERENCE_TIME __RPC_FAR *prtMaxOffset);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *SetMaxStreamOffset )( 
            IAMPushSource __RPC_FAR * This,
            /* [in] */ REFERENCE_TIME rtMaxOffset);
        
        END_INTERFACE
    } IAMPushSourceVtbl;

    interface IAMPushSource
    {
        CONST_VTBL struct IAMPushSourceVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IAMPushSource_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IAMPushSource_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IAMPushSource_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IAMPushSource_GetLatency(This,prtLatency)	\
    (This)->lpVtbl -> GetLatency(This,prtLatency)


#define IAMPushSource_GetPushSourceFlags(This,pFlags)	\
    (This)->lpVtbl -> GetPushSourceFlags(This,pFlags)

#define IAMPushSource_SetPushSourceFlags(This,Flags)	\
    (This)->lpVtbl -> SetPushSourceFlags(This,Flags)

#define IAMPushSource_SetStreamOffset(This,rtOffset)	\
    (This)->lpVtbl -> SetStreamOffset(This,rtOffset)

#define IAMPushSource_GetStreamOffset(This,prtOffset)	\
    (This)->lpVtbl -> GetStreamOffset(This,prtOffset)

#define IAMPushSource_GetMaxStreamOffset(This,prtMaxOffset)	\
    (This)->lpVtbl -> GetMaxStreamOffset(This,prtMaxOffset)

#define IAMPushSource_SetMaxStreamOffset(This,rtMaxOffset)	\
    (This)->lpVtbl -> SetMaxStreamOffset(This,rtMaxOffset)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IAMPushSource_GetPushSourceFlags_Proxy( 
    IAMPushSource __RPC_FAR * This,
    /* [out] */ ULONG __RPC_FAR *pFlags);


void __RPC_STUB IAMPushSource_GetPushSourceFlags_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IAMPushSource_SetPushSourceFlags_Proxy( 
    IAMPushSource __RPC_FAR * This,
    /* [in] */ ULONG Flags);


void __RPC_STUB IAMPushSource_SetPushSourceFlags_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IAMPushSource_SetStreamOffset_Proxy( 
    IAMPushSource __RPC_FAR * This,
    /* [in] */ REFERENCE_TIME rtOffset);


void __RPC_STUB IAMPushSource_SetStreamOffset_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IAMPushSource_GetStreamOffset_Proxy( 
    IAMPushSource __RPC_FAR * This,
    /* [out] */ REFERENCE_TIME __RPC_FAR *prtOffset);


void __RPC_STUB IAMPushSource_GetStreamOffset_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IAMPushSource_GetMaxStreamOffset_Proxy( 
    IAMPushSource __RPC_FAR * This,
    /* [out] */ REFERENCE_TIME __RPC_FAR *prtMaxOffset);


void __RPC_STUB IAMPushSource_GetMaxStreamOffset_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IAMPushSource_SetMaxStreamOffset_Proxy( 
    IAMPushSource __RPC_FAR * This,
    /* [in] */ REFERENCE_TIME rtMaxOffset);


void __RPC_STUB IAMPushSource_SetMaxStreamOffset_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IAMPushSource_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_dshowfix_0252 */
/* [local] */ 


enum _AM_INTF_SEARCH_FLAGS
    {	AM_INTF_SEARCH_INPUT_PIN	= 0x1,
	AM_INTF_SEARCH_OUTPUT_PIN	= 0x2,
	AM_INTF_SEARCH_FILTER	= 0x4
    };


extern RPC_IF_HANDLE __MIDL_itf_dshowfix_0252_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_dshowfix_0252_v0_0_s_ifspec;

#ifndef __IMpeg2Demultiplexer_INTERFACE_DEFINED__
#define __IMpeg2Demultiplexer_INTERFACE_DEFINED__

/* interface IMpeg2Demultiplexer */
/* [unique][uuid][local][object] */ 


EXTERN_C const IID IID_IMpeg2Demultiplexer;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("436eee9c-264f-4242-90e1-4e330c107512")
    IMpeg2Demultiplexer : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE CreateOutputPin( 
            /* [in] */ AM_MEDIA_TYPE __RPC_FAR *pMediaType,
            /* [in] */ LPWSTR pszPinName,
            /* [out] */ IPin __RPC_FAR *__RPC_FAR *ppIPin) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetOutputPinMediaType( 
            /* [in] */ LPWSTR pszPinName,
            /* [in] */ AM_MEDIA_TYPE __RPC_FAR *pMediaType) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE DeleteOutputPin( 
            /* [in] */ LPWSTR pszPinName) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IMpeg2DemultiplexerVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IMpeg2Demultiplexer __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IMpeg2Demultiplexer __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IMpeg2Demultiplexer __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *CreateOutputPin )( 
            IMpeg2Demultiplexer __RPC_FAR * This,
            /* [in] */ AM_MEDIA_TYPE __RPC_FAR *pMediaType,
            /* [in] */ LPWSTR pszPinName,
            /* [out] */ IPin __RPC_FAR *__RPC_FAR *ppIPin);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *SetOutputPinMediaType )( 
            IMpeg2Demultiplexer __RPC_FAR * This,
            /* [in] */ LPWSTR pszPinName,
            /* [in] */ AM_MEDIA_TYPE __RPC_FAR *pMediaType);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *DeleteOutputPin )( 
            IMpeg2Demultiplexer __RPC_FAR * This,
            /* [in] */ LPWSTR pszPinName);
        
        END_INTERFACE
    } IMpeg2DemultiplexerVtbl;

    interface IMpeg2Demultiplexer
    {
        CONST_VTBL struct IMpeg2DemultiplexerVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMpeg2Demultiplexer_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IMpeg2Demultiplexer_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IMpeg2Demultiplexer_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IMpeg2Demultiplexer_CreateOutputPin(This,pMediaType,pszPinName,ppIPin)	\
    (This)->lpVtbl -> CreateOutputPin(This,pMediaType,pszPinName,ppIPin)

#define IMpeg2Demultiplexer_SetOutputPinMediaType(This,pszPinName,pMediaType)	\
    (This)->lpVtbl -> SetOutputPinMediaType(This,pszPinName,pMediaType)

#define IMpeg2Demultiplexer_DeleteOutputPin(This,pszPinName)	\
    (This)->lpVtbl -> DeleteOutputPin(This,pszPinName)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IMpeg2Demultiplexer_CreateOutputPin_Proxy( 
    IMpeg2Demultiplexer __RPC_FAR * This,
    /* [in] */ AM_MEDIA_TYPE __RPC_FAR *pMediaType,
    /* [in] */ LPWSTR pszPinName,
    /* [out] */ IPin __RPC_FAR *__RPC_FAR *ppIPin);


void __RPC_STUB IMpeg2Demultiplexer_CreateOutputPin_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IMpeg2Demultiplexer_SetOutputPinMediaType_Proxy( 
    IMpeg2Demultiplexer __RPC_FAR * This,
    /* [in] */ LPWSTR pszPinName,
    /* [in] */ AM_MEDIA_TYPE __RPC_FAR *pMediaType);


void __RPC_STUB IMpeg2Demultiplexer_SetOutputPinMediaType_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IMpeg2Demultiplexer_DeleteOutputPin_Proxy( 
    IMpeg2Demultiplexer __RPC_FAR * This,
    /* [in] */ LPWSTR pszPinName);


void __RPC_STUB IMpeg2Demultiplexer_DeleteOutputPin_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IMpeg2Demultiplexer_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_dshowfix_0253 */
/* [local] */ 

#define MPEG2_PROGRAM_STREAM_MAP                 0x00000000
#define MPEG2_PROGRAM_ELEMENTARY_STREAM          0x00000001
#define MPEG2_PROGRAM_DIRECTORY_PES_PACKET       0x00000002
#define MPEG2_PROGRAM_PACK_HEADER                0x00000003
#define MPEG2_PROGRAM_PES_STREAM                 0x00000004
#define MPEG2_PROGRAM_SYSTEM_HEADER              0x00000005
#define SUBSTREAM_FILTER_VAL_NONE                0x10000000
typedef /* [public][public] */ struct __MIDL___MIDL_itf_dshowfix_0253_0001
    {
    ULONG stream_id;
    DWORD dwMediaSampleContent;
    ULONG ulSubstreamFilterValue;
    int iDataOffset;
    }	STREAM_ID_MAP;



extern RPC_IF_HANDLE __MIDL_itf_dshowfix_0253_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_dshowfix_0253_v0_0_s_ifspec;

#ifndef __IEnumStreamIdMap_INTERFACE_DEFINED__
#define __IEnumStreamIdMap_INTERFACE_DEFINED__

/* interface IEnumStreamIdMap */
/* [unique][uuid][local][object] */ 


EXTERN_C const IID IID_IEnumStreamIdMap;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("945C1566-6202-46fc-96C7-D87F289C6534")
    IEnumStreamIdMap : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Next( 
            /* [in] */ ULONG cRequest,
            /* [size_is][out][in] */ STREAM_ID_MAP __RPC_FAR *pStreamIdMap,
            /* [out] */ ULONG __RPC_FAR *pcReceived) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Skip( 
            /* [in] */ ULONG cRecords) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Reset( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Clone( 
            /* [out] */ IEnumStreamIdMap __RPC_FAR *__RPC_FAR *ppIEnumStreamIdMap) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IEnumStreamIdMapVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IEnumStreamIdMap __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IEnumStreamIdMap __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IEnumStreamIdMap __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Next )( 
            IEnumStreamIdMap __RPC_FAR * This,
            /* [in] */ ULONG cRequest,
            /* [size_is][out][in] */ STREAM_ID_MAP __RPC_FAR *pStreamIdMap,
            /* [out] */ ULONG __RPC_FAR *pcReceived);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Skip )( 
            IEnumStreamIdMap __RPC_FAR * This,
            /* [in] */ ULONG cRecords);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Reset )( 
            IEnumStreamIdMap __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Clone )( 
            IEnumStreamIdMap __RPC_FAR * This,
            /* [out] */ IEnumStreamIdMap __RPC_FAR *__RPC_FAR *ppIEnumStreamIdMap);
        
        END_INTERFACE
    } IEnumStreamIdMapVtbl;

    interface IEnumStreamIdMap
    {
        CONST_VTBL struct IEnumStreamIdMapVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IEnumStreamIdMap_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IEnumStreamIdMap_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IEnumStreamIdMap_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IEnumStreamIdMap_Next(This,cRequest,pStreamIdMap,pcReceived)	\
    (This)->lpVtbl -> Next(This,cRequest,pStreamIdMap,pcReceived)

#define IEnumStreamIdMap_Skip(This,cRecords)	\
    (This)->lpVtbl -> Skip(This,cRecords)

#define IEnumStreamIdMap_Reset(This)	\
    (This)->lpVtbl -> Reset(This)

#define IEnumStreamIdMap_Clone(This,ppIEnumStreamIdMap)	\
    (This)->lpVtbl -> Clone(This,ppIEnumStreamIdMap)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IEnumStreamIdMap_Next_Proxy( 
    IEnumStreamIdMap __RPC_FAR * This,
    /* [in] */ ULONG cRequest,
    /* [size_is][out][in] */ STREAM_ID_MAP __RPC_FAR *pStreamIdMap,
    /* [out] */ ULONG __RPC_FAR *pcReceived);


void __RPC_STUB IEnumStreamIdMap_Next_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IEnumStreamIdMap_Skip_Proxy( 
    IEnumStreamIdMap __RPC_FAR * This,
    /* [in] */ ULONG cRecords);


void __RPC_STUB IEnumStreamIdMap_Skip_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IEnumStreamIdMap_Reset_Proxy( 
    IEnumStreamIdMap __RPC_FAR * This);


void __RPC_STUB IEnumStreamIdMap_Reset_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IEnumStreamIdMap_Clone_Proxy( 
    IEnumStreamIdMap __RPC_FAR * This,
    /* [out] */ IEnumStreamIdMap __RPC_FAR *__RPC_FAR *ppIEnumStreamIdMap);


void __RPC_STUB IEnumStreamIdMap_Clone_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IEnumStreamIdMap_INTERFACE_DEFINED__ */


#ifndef __IMPEG2StreamIdMap_INTERFACE_DEFINED__
#define __IMPEG2StreamIdMap_INTERFACE_DEFINED__

/* interface IMPEG2StreamIdMap */
/* [unique][uuid][local][object] */ 


EXTERN_C const IID IID_IMPEG2StreamIdMap;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("D0E04C47-25B8-4369-925A-362A01D95444")
    IMPEG2StreamIdMap : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE MapStreamId( 
            /* [in] */ ULONG ulStreamId,
            /* [in] */ DWORD MediaSampleContent,
            /* [in] */ ULONG ulSubstreamFilterValue,
            /* [in] */ int iDataOffset) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE UnmapStreamId( 
            /* [in] */ ULONG culStreamId,
            /* [in] */ ULONG __RPC_FAR *pulStreamId) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE EnumStreamIdMap( 
            /* [out] */ IEnumStreamIdMap __RPC_FAR *__RPC_FAR *ppIEnumStreamIdMap) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IMPEG2StreamIdMapVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IMPEG2StreamIdMap __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IMPEG2StreamIdMap __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IMPEG2StreamIdMap __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *MapStreamId )( 
            IMPEG2StreamIdMap __RPC_FAR * This,
            /* [in] */ ULONG ulStreamId,
            /* [in] */ DWORD MediaSampleContent,
            /* [in] */ ULONG ulSubstreamFilterValue,
            /* [in] */ int iDataOffset);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *UnmapStreamId )( 
            IMPEG2StreamIdMap __RPC_FAR * This,
            /* [in] */ ULONG culStreamId,
            /* [in] */ ULONG __RPC_FAR *pulStreamId);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *EnumStreamIdMap )( 
            IMPEG2StreamIdMap __RPC_FAR * This,
            /* [out] */ IEnumStreamIdMap __RPC_FAR *__RPC_FAR *ppIEnumStreamIdMap);
        
        END_INTERFACE
    } IMPEG2StreamIdMapVtbl;

    interface IMPEG2StreamIdMap
    {
        CONST_VTBL struct IMPEG2StreamIdMapVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMPEG2StreamIdMap_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IMPEG2StreamIdMap_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IMPEG2StreamIdMap_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IMPEG2StreamIdMap_MapStreamId(This,ulStreamId,MediaSampleContent,ulSubstreamFilterValue,iDataOffset)	\
    (This)->lpVtbl -> MapStreamId(This,ulStreamId,MediaSampleContent,ulSubstreamFilterValue,iDataOffset)

#define IMPEG2StreamIdMap_UnmapStreamId(This,culStreamId,pulStreamId)	\
    (This)->lpVtbl -> UnmapStreamId(This,culStreamId,pulStreamId)

#define IMPEG2StreamIdMap_EnumStreamIdMap(This,ppIEnumStreamIdMap)	\
    (This)->lpVtbl -> EnumStreamIdMap(This,ppIEnumStreamIdMap)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IMPEG2StreamIdMap_MapStreamId_Proxy( 
    IMPEG2StreamIdMap __RPC_FAR * This,
    /* [in] */ ULONG ulStreamId,
    /* [in] */ DWORD MediaSampleContent,
    /* [in] */ ULONG ulSubstreamFilterValue,
    /* [in] */ int iDataOffset);


void __RPC_STUB IMPEG2StreamIdMap_MapStreamId_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IMPEG2StreamIdMap_UnmapStreamId_Proxy( 
    IMPEG2StreamIdMap __RPC_FAR * This,
    /* [in] */ ULONG culStreamId,
    /* [in] */ ULONG __RPC_FAR *pulStreamId);


void __RPC_STUB IMPEG2StreamIdMap_UnmapStreamId_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IMPEG2StreamIdMap_EnumStreamIdMap_Proxy( 
    IMPEG2StreamIdMap __RPC_FAR * This,
    /* [out] */ IEnumStreamIdMap __RPC_FAR *__RPC_FAR *ppIEnumStreamIdMap);


void __RPC_STUB IMPEG2StreamIdMap_EnumStreamIdMap_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IMPEG2StreamIdMap_INTERFACE_DEFINED__ */


/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


