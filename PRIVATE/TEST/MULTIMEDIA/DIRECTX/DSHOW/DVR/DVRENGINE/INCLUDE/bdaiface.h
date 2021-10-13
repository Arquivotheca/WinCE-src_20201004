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
/* at Mon Aug 21 16:32:47 2006
 */
/* Compiler settings for .\bdaiface.idl:
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

#ifndef __bdaiface_h__
#define __bdaiface_h__

/* Forward Declarations */ 

#ifndef __IBDA_NetworkProvider_FWD_DEFINED__
#define __IBDA_NetworkProvider_FWD_DEFINED__
typedef interface IBDA_NetworkProvider IBDA_NetworkProvider;
#endif 	/* __IBDA_NetworkProvider_FWD_DEFINED__ */


#ifndef __IBDA_EthernetFilter_FWD_DEFINED__
#define __IBDA_EthernetFilter_FWD_DEFINED__
typedef interface IBDA_EthernetFilter IBDA_EthernetFilter;
#endif 	/* __IBDA_EthernetFilter_FWD_DEFINED__ */


#ifndef __IBDA_IPV4Filter_FWD_DEFINED__
#define __IBDA_IPV4Filter_FWD_DEFINED__
typedef interface IBDA_IPV4Filter IBDA_IPV4Filter;
#endif 	/* __IBDA_IPV4Filter_FWD_DEFINED__ */


#ifndef __IBDA_IPV6Filter_FWD_DEFINED__
#define __IBDA_IPV6Filter_FWD_DEFINED__
typedef interface IBDA_IPV6Filter IBDA_IPV6Filter;
#endif 	/* __IBDA_IPV6Filter_FWD_DEFINED__ */


#ifndef __IBDA_DeviceControl_FWD_DEFINED__
#define __IBDA_DeviceControl_FWD_DEFINED__
typedef interface IBDA_DeviceControl IBDA_DeviceControl;
#endif 	/* __IBDA_DeviceControl_FWD_DEFINED__ */


#ifndef __IBDA_PinControl_FWD_DEFINED__
#define __IBDA_PinControl_FWD_DEFINED__
typedef interface IBDA_PinControl IBDA_PinControl;
#endif 	/* __IBDA_PinControl_FWD_DEFINED__ */


#ifndef __IBDA_SignalProperties_FWD_DEFINED__
#define __IBDA_SignalProperties_FWD_DEFINED__
typedef interface IBDA_SignalProperties IBDA_SignalProperties;
#endif 	/* __IBDA_SignalProperties_FWD_DEFINED__ */


#ifndef __IBDA_SignalStatistics_FWD_DEFINED__
#define __IBDA_SignalStatistics_FWD_DEFINED__
typedef interface IBDA_SignalStatistics IBDA_SignalStatistics;
#endif 	/* __IBDA_SignalStatistics_FWD_DEFINED__ */


#ifndef __IBDA_Topology_FWD_DEFINED__
#define __IBDA_Topology_FWD_DEFINED__
typedef interface IBDA_Topology IBDA_Topology;
#endif 	/* __IBDA_Topology_FWD_DEFINED__ */


#ifndef __IBDA_VoidTransform_FWD_DEFINED__
#define __IBDA_VoidTransform_FWD_DEFINED__
typedef interface IBDA_VoidTransform IBDA_VoidTransform;
#endif 	/* __IBDA_VoidTransform_FWD_DEFINED__ */


#ifndef __IBDA_NullTransform_FWD_DEFINED__
#define __IBDA_NullTransform_FWD_DEFINED__
typedef interface IBDA_NullTransform IBDA_NullTransform;
#endif 	/* __IBDA_NullTransform_FWD_DEFINED__ */


#ifndef __IBDA_FrequencyFilter_FWD_DEFINED__
#define __IBDA_FrequencyFilter_FWD_DEFINED__
typedef interface IBDA_FrequencyFilter IBDA_FrequencyFilter;
#endif 	/* __IBDA_FrequencyFilter_FWD_DEFINED__ */


#ifndef __IBDA_LNBInfo_FWD_DEFINED__
#define __IBDA_LNBInfo_FWD_DEFINED__
typedef interface IBDA_LNBInfo IBDA_LNBInfo;
#endif 	/* __IBDA_LNBInfo_FWD_DEFINED__ */


#ifndef __IBDA_AutoDemodulate_FWD_DEFINED__
#define __IBDA_AutoDemodulate_FWD_DEFINED__
typedef interface IBDA_AutoDemodulate IBDA_AutoDemodulate;
#endif 	/* __IBDA_AutoDemodulate_FWD_DEFINED__ */


#ifndef __IBDA_DigitalDemodulator_FWD_DEFINED__
#define __IBDA_DigitalDemodulator_FWD_DEFINED__
typedef interface IBDA_DigitalDemodulator IBDA_DigitalDemodulator;
#endif 	/* __IBDA_DigitalDemodulator_FWD_DEFINED__ */


#ifndef __IBDA_IPSinkControl_FWD_DEFINED__
#define __IBDA_IPSinkControl_FWD_DEFINED__
typedef interface IBDA_IPSinkControl IBDA_IPSinkControl;
#endif 	/* __IBDA_IPSinkControl_FWD_DEFINED__ */


#ifndef __IBDA_IPSinkInfo_FWD_DEFINED__
#define __IBDA_IPSinkInfo_FWD_DEFINED__
typedef interface IBDA_IPSinkInfo IBDA_IPSinkInfo;
#endif 	/* __IBDA_IPSinkInfo_FWD_DEFINED__ */


#ifndef __IEnumPIDMap_FWD_DEFINED__
#define __IEnumPIDMap_FWD_DEFINED__
typedef interface IEnumPIDMap IEnumPIDMap;
#endif 	/* __IEnumPIDMap_FWD_DEFINED__ */


#ifndef __IMPEG2PIDMap_FWD_DEFINED__
#define __IMPEG2PIDMap_FWD_DEFINED__
typedef interface IMPEG2PIDMap IMPEG2PIDMap;
#endif 	/* __IMPEG2PIDMap_FWD_DEFINED__ */


#ifndef __IFrequencyMap_FWD_DEFINED__
#define __IFrequencyMap_FWD_DEFINED__
typedef interface IFrequencyMap IFrequencyMap;
#endif 	/* __IFrequencyMap_FWD_DEFINED__ */


/* header files for imported files */
#include "unknwn.h"
#include "strmif.h"
#include "BdaTypes.h"

#ifdef __cplusplus
extern "C"{
#endif 

void __RPC_FAR * __RPC_USER MIDL_user_allocate(size_t);
void __RPC_USER MIDL_user_free( void __RPC_FAR * ); 

/* interface __MIDL_itf_bdaiface_0000 */
/* [local] */ 

typedef /* [public][public] */ struct __MIDL___MIDL_itf_bdaiface_0000_0001
    {
    CLSID clsMedium;
    DWORD dw1;
    DWORD dw2;
    }	REGPINMEDIUM;



extern RPC_IF_HANDLE __MIDL_itf_bdaiface_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_bdaiface_0000_v0_0_s_ifspec;

#ifndef __IBDA_NetworkProvider_INTERFACE_DEFINED__
#define __IBDA_NetworkProvider_INTERFACE_DEFINED__

/* interface IBDA_NetworkProvider */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IBDA_NetworkProvider;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("fd501041-8ebe-11ce-8183-00aa00577da2")
    IBDA_NetworkProvider : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE PutSignalSource( 
            /* [in] */ ULONG ulSignalSource) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetSignalSource( 
            /* [out][in] */ ULONG __RPC_FAR *pulSignalSource) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetNetworkType( 
            /* [out][in] */ GUID __RPC_FAR *pguidNetworkType) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE PutTuningSpace( 
            /* [in] */ REFGUID guidTuningSpace) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetTuningSpace( 
            /* [out][in] */ GUID __RPC_FAR *pguidTuingSpace) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE RegisterDeviceFilter( 
            /* [in] */ IUnknown __RPC_FAR *pUnkFilterControl,
            /* [out][in] */ ULONG __RPC_FAR *ppvRegisitrationContext) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE UnRegisterDeviceFilter( 
            /* [in] */ ULONG pvRegistrationContext) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IBDA_NetworkProviderVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IBDA_NetworkProvider __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IBDA_NetworkProvider __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IBDA_NetworkProvider __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *PutSignalSource )( 
            IBDA_NetworkProvider __RPC_FAR * This,
            /* [in] */ ULONG ulSignalSource);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetSignalSource )( 
            IBDA_NetworkProvider __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *pulSignalSource);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetNetworkType )( 
            IBDA_NetworkProvider __RPC_FAR * This,
            /* [out][in] */ GUID __RPC_FAR *pguidNetworkType);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *PutTuningSpace )( 
            IBDA_NetworkProvider __RPC_FAR * This,
            /* [in] */ REFGUID guidTuningSpace);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetTuningSpace )( 
            IBDA_NetworkProvider __RPC_FAR * This,
            /* [out][in] */ GUID __RPC_FAR *pguidTuingSpace);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *RegisterDeviceFilter )( 
            IBDA_NetworkProvider __RPC_FAR * This,
            /* [in] */ IUnknown __RPC_FAR *pUnkFilterControl,
            /* [out][in] */ ULONG __RPC_FAR *ppvRegisitrationContext);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *UnRegisterDeviceFilter )( 
            IBDA_NetworkProvider __RPC_FAR * This,
            /* [in] */ ULONG pvRegistrationContext);
        
        END_INTERFACE
    } IBDA_NetworkProviderVtbl;

    interface IBDA_NetworkProvider
    {
        CONST_VTBL struct IBDA_NetworkProviderVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IBDA_NetworkProvider_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IBDA_NetworkProvider_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IBDA_NetworkProvider_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IBDA_NetworkProvider_PutSignalSource(This,ulSignalSource)	\
    (This)->lpVtbl -> PutSignalSource(This,ulSignalSource)

#define IBDA_NetworkProvider_GetSignalSource(This,pulSignalSource)	\
    (This)->lpVtbl -> GetSignalSource(This,pulSignalSource)

#define IBDA_NetworkProvider_GetNetworkType(This,pguidNetworkType)	\
    (This)->lpVtbl -> GetNetworkType(This,pguidNetworkType)

#define IBDA_NetworkProvider_PutTuningSpace(This,guidTuningSpace)	\
    (This)->lpVtbl -> PutTuningSpace(This,guidTuningSpace)

#define IBDA_NetworkProvider_GetTuningSpace(This,pguidTuingSpace)	\
    (This)->lpVtbl -> GetTuningSpace(This,pguidTuingSpace)

#define IBDA_NetworkProvider_RegisterDeviceFilter(This,pUnkFilterControl,ppvRegisitrationContext)	\
    (This)->lpVtbl -> RegisterDeviceFilter(This,pUnkFilterControl,ppvRegisitrationContext)

#define IBDA_NetworkProvider_UnRegisterDeviceFilter(This,pvRegistrationContext)	\
    (This)->lpVtbl -> UnRegisterDeviceFilter(This,pvRegistrationContext)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IBDA_NetworkProvider_PutSignalSource_Proxy( 
    IBDA_NetworkProvider __RPC_FAR * This,
    /* [in] */ ULONG ulSignalSource);


void __RPC_STUB IBDA_NetworkProvider_PutSignalSource_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_NetworkProvider_GetSignalSource_Proxy( 
    IBDA_NetworkProvider __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *pulSignalSource);


void __RPC_STUB IBDA_NetworkProvider_GetSignalSource_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_NetworkProvider_GetNetworkType_Proxy( 
    IBDA_NetworkProvider __RPC_FAR * This,
    /* [out][in] */ GUID __RPC_FAR *pguidNetworkType);


void __RPC_STUB IBDA_NetworkProvider_GetNetworkType_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_NetworkProvider_PutTuningSpace_Proxy( 
    IBDA_NetworkProvider __RPC_FAR * This,
    /* [in] */ REFGUID guidTuningSpace);


void __RPC_STUB IBDA_NetworkProvider_PutTuningSpace_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_NetworkProvider_GetTuningSpace_Proxy( 
    IBDA_NetworkProvider __RPC_FAR * This,
    /* [out][in] */ GUID __RPC_FAR *pguidTuingSpace);


void __RPC_STUB IBDA_NetworkProvider_GetTuningSpace_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_NetworkProvider_RegisterDeviceFilter_Proxy( 
    IBDA_NetworkProvider __RPC_FAR * This,
    /* [in] */ IUnknown __RPC_FAR *pUnkFilterControl,
    /* [out][in] */ ULONG __RPC_FAR *ppvRegisitrationContext);


void __RPC_STUB IBDA_NetworkProvider_RegisterDeviceFilter_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_NetworkProvider_UnRegisterDeviceFilter_Proxy( 
    IBDA_NetworkProvider __RPC_FAR * This,
    /* [in] */ ULONG pvRegistrationContext);


void __RPC_STUB IBDA_NetworkProvider_UnRegisterDeviceFilter_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IBDA_NetworkProvider_INTERFACE_DEFINED__ */


#ifndef __IBDA_EthernetFilter_INTERFACE_DEFINED__
#define __IBDA_EthernetFilter_INTERFACE_DEFINED__

/* interface IBDA_EthernetFilter */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IBDA_EthernetFilter;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("71985F43-1CA1-11d3-9CC8-00C04F7971E0")
    IBDA_EthernetFilter : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetMulticastListSize( 
            /* [out][in] */ ULONG __RPC_FAR *pulcbAddresses) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE PutMulticastList( 
            /* [in] */ ULONG ulcbAddresses,
            /* [size_is][in] */ BYTE __RPC_FAR pAddressList[  ]) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetMulticastList( 
            /* [out][in] */ ULONG __RPC_FAR *pulcbAddresses,
            /* [size_is][out] */ BYTE __RPC_FAR pAddressList[  ]) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE PutMulticastMode( 
            /* [in] */ ULONG ulModeMask) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetMulticastMode( 
            /* [out] */ ULONG __RPC_FAR *pulModeMask) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IBDA_EthernetFilterVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IBDA_EthernetFilter __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IBDA_EthernetFilter __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IBDA_EthernetFilter __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetMulticastListSize )( 
            IBDA_EthernetFilter __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *pulcbAddresses);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *PutMulticastList )( 
            IBDA_EthernetFilter __RPC_FAR * This,
            /* [in] */ ULONG ulcbAddresses,
            /* [size_is][in] */ BYTE __RPC_FAR pAddressList[  ]);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetMulticastList )( 
            IBDA_EthernetFilter __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *pulcbAddresses,
            /* [size_is][out] */ BYTE __RPC_FAR pAddressList[  ]);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *PutMulticastMode )( 
            IBDA_EthernetFilter __RPC_FAR * This,
            /* [in] */ ULONG ulModeMask);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetMulticastMode )( 
            IBDA_EthernetFilter __RPC_FAR * This,
            /* [out] */ ULONG __RPC_FAR *pulModeMask);
        
        END_INTERFACE
    } IBDA_EthernetFilterVtbl;

    interface IBDA_EthernetFilter
    {
        CONST_VTBL struct IBDA_EthernetFilterVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IBDA_EthernetFilter_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IBDA_EthernetFilter_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IBDA_EthernetFilter_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IBDA_EthernetFilter_GetMulticastListSize(This,pulcbAddresses)	\
    (This)->lpVtbl -> GetMulticastListSize(This,pulcbAddresses)

#define IBDA_EthernetFilter_PutMulticastList(This,ulcbAddresses,pAddressList)	\
    (This)->lpVtbl -> PutMulticastList(This,ulcbAddresses,pAddressList)

#define IBDA_EthernetFilter_GetMulticastList(This,pulcbAddresses,pAddressList)	\
    (This)->lpVtbl -> GetMulticastList(This,pulcbAddresses,pAddressList)

#define IBDA_EthernetFilter_PutMulticastMode(This,ulModeMask)	\
    (This)->lpVtbl -> PutMulticastMode(This,ulModeMask)

#define IBDA_EthernetFilter_GetMulticastMode(This,pulModeMask)	\
    (This)->lpVtbl -> GetMulticastMode(This,pulModeMask)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IBDA_EthernetFilter_GetMulticastListSize_Proxy( 
    IBDA_EthernetFilter __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *pulcbAddresses);


void __RPC_STUB IBDA_EthernetFilter_GetMulticastListSize_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_EthernetFilter_PutMulticastList_Proxy( 
    IBDA_EthernetFilter __RPC_FAR * This,
    /* [in] */ ULONG ulcbAddresses,
    /* [size_is][in] */ BYTE __RPC_FAR pAddressList[  ]);


void __RPC_STUB IBDA_EthernetFilter_PutMulticastList_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_EthernetFilter_GetMulticastList_Proxy( 
    IBDA_EthernetFilter __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *pulcbAddresses,
    /* [size_is][out] */ BYTE __RPC_FAR pAddressList[  ]);


void __RPC_STUB IBDA_EthernetFilter_GetMulticastList_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_EthernetFilter_PutMulticastMode_Proxy( 
    IBDA_EthernetFilter __RPC_FAR * This,
    /* [in] */ ULONG ulModeMask);


void __RPC_STUB IBDA_EthernetFilter_PutMulticastMode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_EthernetFilter_GetMulticastMode_Proxy( 
    IBDA_EthernetFilter __RPC_FAR * This,
    /* [out] */ ULONG __RPC_FAR *pulModeMask);


void __RPC_STUB IBDA_EthernetFilter_GetMulticastMode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IBDA_EthernetFilter_INTERFACE_DEFINED__ */


#ifndef __IBDA_IPV4Filter_INTERFACE_DEFINED__
#define __IBDA_IPV4Filter_INTERFACE_DEFINED__

/* interface IBDA_IPV4Filter */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IBDA_IPV4Filter;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("71985F44-1CA1-11d3-9CC8-00C04F7971E0")
    IBDA_IPV4Filter : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetMulticastListSize( 
            /* [out][in] */ ULONG __RPC_FAR *pulcbAddresses) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE PutMulticastList( 
            /* [in] */ ULONG ulcbAddresses,
            /* [size_is][in] */ BYTE __RPC_FAR pAddressList[  ]) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetMulticastList( 
            /* [out][in] */ ULONG __RPC_FAR *pulcbAddresses,
            /* [size_is][out] */ BYTE __RPC_FAR pAddressList[  ]) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE PutMulticastMode( 
            /* [in] */ ULONG ulModeMask) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetMulticastMode( 
            /* [out] */ ULONG __RPC_FAR *pulModeMask) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IBDA_IPV4FilterVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IBDA_IPV4Filter __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IBDA_IPV4Filter __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IBDA_IPV4Filter __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetMulticastListSize )( 
            IBDA_IPV4Filter __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *pulcbAddresses);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *PutMulticastList )( 
            IBDA_IPV4Filter __RPC_FAR * This,
            /* [in] */ ULONG ulcbAddresses,
            /* [size_is][in] */ BYTE __RPC_FAR pAddressList[  ]);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetMulticastList )( 
            IBDA_IPV4Filter __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *pulcbAddresses,
            /* [size_is][out] */ BYTE __RPC_FAR pAddressList[  ]);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *PutMulticastMode )( 
            IBDA_IPV4Filter __RPC_FAR * This,
            /* [in] */ ULONG ulModeMask);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetMulticastMode )( 
            IBDA_IPV4Filter __RPC_FAR * This,
            /* [out] */ ULONG __RPC_FAR *pulModeMask);
        
        END_INTERFACE
    } IBDA_IPV4FilterVtbl;

    interface IBDA_IPV4Filter
    {
        CONST_VTBL struct IBDA_IPV4FilterVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IBDA_IPV4Filter_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IBDA_IPV4Filter_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IBDA_IPV4Filter_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IBDA_IPV4Filter_GetMulticastListSize(This,pulcbAddresses)	\
    (This)->lpVtbl -> GetMulticastListSize(This,pulcbAddresses)

#define IBDA_IPV4Filter_PutMulticastList(This,ulcbAddresses,pAddressList)	\
    (This)->lpVtbl -> PutMulticastList(This,ulcbAddresses,pAddressList)

#define IBDA_IPV4Filter_GetMulticastList(This,pulcbAddresses,pAddressList)	\
    (This)->lpVtbl -> GetMulticastList(This,pulcbAddresses,pAddressList)

#define IBDA_IPV4Filter_PutMulticastMode(This,ulModeMask)	\
    (This)->lpVtbl -> PutMulticastMode(This,ulModeMask)

#define IBDA_IPV4Filter_GetMulticastMode(This,pulModeMask)	\
    (This)->lpVtbl -> GetMulticastMode(This,pulModeMask)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IBDA_IPV4Filter_GetMulticastListSize_Proxy( 
    IBDA_IPV4Filter __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *pulcbAddresses);


void __RPC_STUB IBDA_IPV4Filter_GetMulticastListSize_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_IPV4Filter_PutMulticastList_Proxy( 
    IBDA_IPV4Filter __RPC_FAR * This,
    /* [in] */ ULONG ulcbAddresses,
    /* [size_is][in] */ BYTE __RPC_FAR pAddressList[  ]);


void __RPC_STUB IBDA_IPV4Filter_PutMulticastList_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_IPV4Filter_GetMulticastList_Proxy( 
    IBDA_IPV4Filter __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *pulcbAddresses,
    /* [size_is][out] */ BYTE __RPC_FAR pAddressList[  ]);


void __RPC_STUB IBDA_IPV4Filter_GetMulticastList_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_IPV4Filter_PutMulticastMode_Proxy( 
    IBDA_IPV4Filter __RPC_FAR * This,
    /* [in] */ ULONG ulModeMask);


void __RPC_STUB IBDA_IPV4Filter_PutMulticastMode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_IPV4Filter_GetMulticastMode_Proxy( 
    IBDA_IPV4Filter __RPC_FAR * This,
    /* [out] */ ULONG __RPC_FAR *pulModeMask);


void __RPC_STUB IBDA_IPV4Filter_GetMulticastMode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IBDA_IPV4Filter_INTERFACE_DEFINED__ */


#ifndef __IBDA_IPV6Filter_INTERFACE_DEFINED__
#define __IBDA_IPV6Filter_INTERFACE_DEFINED__

/* interface IBDA_IPV6Filter */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IBDA_IPV6Filter;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("E1785A74-2A23-4fb3-9245-A8F88017EF33")
    IBDA_IPV6Filter : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetMulticastListSize( 
            /* [out][in] */ ULONG __RPC_FAR *pulcbAddresses) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE PutMulticastList( 
            /* [in] */ ULONG ulcbAddresses,
            /* [size_is][in] */ BYTE __RPC_FAR pAddressList[  ]) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetMulticastList( 
            /* [out][in] */ ULONG __RPC_FAR *pulcbAddresses,
            /* [size_is][out] */ BYTE __RPC_FAR pAddressList[  ]) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE PutMulticastMode( 
            /* [in] */ ULONG ulModeMask) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetMulticastMode( 
            /* [out] */ ULONG __RPC_FAR *pulModeMask) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IBDA_IPV6FilterVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IBDA_IPV6Filter __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IBDA_IPV6Filter __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IBDA_IPV6Filter __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetMulticastListSize )( 
            IBDA_IPV6Filter __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *pulcbAddresses);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *PutMulticastList )( 
            IBDA_IPV6Filter __RPC_FAR * This,
            /* [in] */ ULONG ulcbAddresses,
            /* [size_is][in] */ BYTE __RPC_FAR pAddressList[  ]);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetMulticastList )( 
            IBDA_IPV6Filter __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *pulcbAddresses,
            /* [size_is][out] */ BYTE __RPC_FAR pAddressList[  ]);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *PutMulticastMode )( 
            IBDA_IPV6Filter __RPC_FAR * This,
            /* [in] */ ULONG ulModeMask);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetMulticastMode )( 
            IBDA_IPV6Filter __RPC_FAR * This,
            /* [out] */ ULONG __RPC_FAR *pulModeMask);
        
        END_INTERFACE
    } IBDA_IPV6FilterVtbl;

    interface IBDA_IPV6Filter
    {
        CONST_VTBL struct IBDA_IPV6FilterVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IBDA_IPV6Filter_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IBDA_IPV6Filter_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IBDA_IPV6Filter_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IBDA_IPV6Filter_GetMulticastListSize(This,pulcbAddresses)	\
    (This)->lpVtbl -> GetMulticastListSize(This,pulcbAddresses)

#define IBDA_IPV6Filter_PutMulticastList(This,ulcbAddresses,pAddressList)	\
    (This)->lpVtbl -> PutMulticastList(This,ulcbAddresses,pAddressList)

#define IBDA_IPV6Filter_GetMulticastList(This,pulcbAddresses,pAddressList)	\
    (This)->lpVtbl -> GetMulticastList(This,pulcbAddresses,pAddressList)

#define IBDA_IPV6Filter_PutMulticastMode(This,ulModeMask)	\
    (This)->lpVtbl -> PutMulticastMode(This,ulModeMask)

#define IBDA_IPV6Filter_GetMulticastMode(This,pulModeMask)	\
    (This)->lpVtbl -> GetMulticastMode(This,pulModeMask)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IBDA_IPV6Filter_GetMulticastListSize_Proxy( 
    IBDA_IPV6Filter __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *pulcbAddresses);


void __RPC_STUB IBDA_IPV6Filter_GetMulticastListSize_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_IPV6Filter_PutMulticastList_Proxy( 
    IBDA_IPV6Filter __RPC_FAR * This,
    /* [in] */ ULONG ulcbAddresses,
    /* [size_is][in] */ BYTE __RPC_FAR pAddressList[  ]);


void __RPC_STUB IBDA_IPV6Filter_PutMulticastList_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_IPV6Filter_GetMulticastList_Proxy( 
    IBDA_IPV6Filter __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *pulcbAddresses,
    /* [size_is][out] */ BYTE __RPC_FAR pAddressList[  ]);


void __RPC_STUB IBDA_IPV6Filter_GetMulticastList_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_IPV6Filter_PutMulticastMode_Proxy( 
    IBDA_IPV6Filter __RPC_FAR * This,
    /* [in] */ ULONG ulModeMask);


void __RPC_STUB IBDA_IPV6Filter_PutMulticastMode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_IPV6Filter_GetMulticastMode_Proxy( 
    IBDA_IPV6Filter __RPC_FAR * This,
    /* [out] */ ULONG __RPC_FAR *pulModeMask);


void __RPC_STUB IBDA_IPV6Filter_GetMulticastMode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IBDA_IPV6Filter_INTERFACE_DEFINED__ */


#ifndef __IBDA_DeviceControl_INTERFACE_DEFINED__
#define __IBDA_DeviceControl_INTERFACE_DEFINED__

/* interface IBDA_DeviceControl */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IBDA_DeviceControl;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("FD0A5AF3-B41D-11d2-9C95-00C04F7971E0")
    IBDA_DeviceControl : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE StartChanges( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE CheckChanges( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE CommitChanges( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetChangeState( 
            /* [out][in] */ ULONG __RPC_FAR *pState) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IBDA_DeviceControlVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IBDA_DeviceControl __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IBDA_DeviceControl __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IBDA_DeviceControl __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *StartChanges )( 
            IBDA_DeviceControl __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *CheckChanges )( 
            IBDA_DeviceControl __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *CommitChanges )( 
            IBDA_DeviceControl __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetChangeState )( 
            IBDA_DeviceControl __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *pState);
        
        END_INTERFACE
    } IBDA_DeviceControlVtbl;

    interface IBDA_DeviceControl
    {
        CONST_VTBL struct IBDA_DeviceControlVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IBDA_DeviceControl_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IBDA_DeviceControl_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IBDA_DeviceControl_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IBDA_DeviceControl_StartChanges(This)	\
    (This)->lpVtbl -> StartChanges(This)

#define IBDA_DeviceControl_CheckChanges(This)	\
    (This)->lpVtbl -> CheckChanges(This)

#define IBDA_DeviceControl_CommitChanges(This)	\
    (This)->lpVtbl -> CommitChanges(This)

#define IBDA_DeviceControl_GetChangeState(This,pState)	\
    (This)->lpVtbl -> GetChangeState(This,pState)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IBDA_DeviceControl_StartChanges_Proxy( 
    IBDA_DeviceControl __RPC_FAR * This);


void __RPC_STUB IBDA_DeviceControl_StartChanges_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_DeviceControl_CheckChanges_Proxy( 
    IBDA_DeviceControl __RPC_FAR * This);


void __RPC_STUB IBDA_DeviceControl_CheckChanges_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_DeviceControl_CommitChanges_Proxy( 
    IBDA_DeviceControl __RPC_FAR * This);


void __RPC_STUB IBDA_DeviceControl_CommitChanges_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_DeviceControl_GetChangeState_Proxy( 
    IBDA_DeviceControl __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *pState);


void __RPC_STUB IBDA_DeviceControl_GetChangeState_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IBDA_DeviceControl_INTERFACE_DEFINED__ */


#ifndef __IBDA_PinControl_INTERFACE_DEFINED__
#define __IBDA_PinControl_INTERFACE_DEFINED__

/* interface IBDA_PinControl */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IBDA_PinControl;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("0DED49D5-A8B7-4d5d-97A1-12B0C195874D")
    IBDA_PinControl : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetPinID( 
            /* [out][in] */ ULONG __RPC_FAR *pulPinID) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetPinType( 
            /* [out][in] */ ULONG __RPC_FAR *pulPinType) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE RegistrationContext( 
            /* [out][in] */ ULONG __RPC_FAR *pulRegistrationCtx) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IBDA_PinControlVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IBDA_PinControl __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IBDA_PinControl __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IBDA_PinControl __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetPinID )( 
            IBDA_PinControl __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *pulPinID);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetPinType )( 
            IBDA_PinControl __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *pulPinType);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *RegistrationContext )( 
            IBDA_PinControl __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *pulRegistrationCtx);
        
        END_INTERFACE
    } IBDA_PinControlVtbl;

    interface IBDA_PinControl
    {
        CONST_VTBL struct IBDA_PinControlVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IBDA_PinControl_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IBDA_PinControl_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IBDA_PinControl_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IBDA_PinControl_GetPinID(This,pulPinID)	\
    (This)->lpVtbl -> GetPinID(This,pulPinID)

#define IBDA_PinControl_GetPinType(This,pulPinType)	\
    (This)->lpVtbl -> GetPinType(This,pulPinType)

#define IBDA_PinControl_RegistrationContext(This,pulRegistrationCtx)	\
    (This)->lpVtbl -> RegistrationContext(This,pulRegistrationCtx)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IBDA_PinControl_GetPinID_Proxy( 
    IBDA_PinControl __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *pulPinID);


void __RPC_STUB IBDA_PinControl_GetPinID_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_PinControl_GetPinType_Proxy( 
    IBDA_PinControl __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *pulPinType);


void __RPC_STUB IBDA_PinControl_GetPinType_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_PinControl_RegistrationContext_Proxy( 
    IBDA_PinControl __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *pulRegistrationCtx);


void __RPC_STUB IBDA_PinControl_RegistrationContext_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IBDA_PinControl_INTERFACE_DEFINED__ */


#ifndef __IBDA_SignalProperties_INTERFACE_DEFINED__
#define __IBDA_SignalProperties_INTERFACE_DEFINED__

/* interface IBDA_SignalProperties */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IBDA_SignalProperties;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("D2F1644B-B409-11d2-BC69-00A0C9EE9E16")
    IBDA_SignalProperties : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE PutNetworkType( 
            /* [in] */ REFGUID guidNetworkType) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetNetworkType( 
            /* [out][in] */ GUID __RPC_FAR *pguidNetworkType) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE PutSignalSource( 
            /* [in] */ ULONG ulSignalSource) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetSignalSource( 
            /* [out][in] */ ULONG __RPC_FAR *pulSignalSource) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE PutTuningSpace( 
            /* [in] */ REFGUID guidTuningSpace) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetTuningSpace( 
            /* [out][in] */ GUID __RPC_FAR *pguidTuingSpace) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IBDA_SignalPropertiesVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IBDA_SignalProperties __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IBDA_SignalProperties __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IBDA_SignalProperties __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *PutNetworkType )( 
            IBDA_SignalProperties __RPC_FAR * This,
            /* [in] */ REFGUID guidNetworkType);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetNetworkType )( 
            IBDA_SignalProperties __RPC_FAR * This,
            /* [out][in] */ GUID __RPC_FAR *pguidNetworkType);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *PutSignalSource )( 
            IBDA_SignalProperties __RPC_FAR * This,
            /* [in] */ ULONG ulSignalSource);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetSignalSource )( 
            IBDA_SignalProperties __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *pulSignalSource);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *PutTuningSpace )( 
            IBDA_SignalProperties __RPC_FAR * This,
            /* [in] */ REFGUID guidTuningSpace);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetTuningSpace )( 
            IBDA_SignalProperties __RPC_FAR * This,
            /* [out][in] */ GUID __RPC_FAR *pguidTuingSpace);
        
        END_INTERFACE
    } IBDA_SignalPropertiesVtbl;

    interface IBDA_SignalProperties
    {
        CONST_VTBL struct IBDA_SignalPropertiesVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IBDA_SignalProperties_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IBDA_SignalProperties_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IBDA_SignalProperties_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IBDA_SignalProperties_PutNetworkType(This,guidNetworkType)	\
    (This)->lpVtbl -> PutNetworkType(This,guidNetworkType)

#define IBDA_SignalProperties_GetNetworkType(This,pguidNetworkType)	\
    (This)->lpVtbl -> GetNetworkType(This,pguidNetworkType)

#define IBDA_SignalProperties_PutSignalSource(This,ulSignalSource)	\
    (This)->lpVtbl -> PutSignalSource(This,ulSignalSource)

#define IBDA_SignalProperties_GetSignalSource(This,pulSignalSource)	\
    (This)->lpVtbl -> GetSignalSource(This,pulSignalSource)

#define IBDA_SignalProperties_PutTuningSpace(This,guidTuningSpace)	\
    (This)->lpVtbl -> PutTuningSpace(This,guidTuningSpace)

#define IBDA_SignalProperties_GetTuningSpace(This,pguidTuingSpace)	\
    (This)->lpVtbl -> GetTuningSpace(This,pguidTuingSpace)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IBDA_SignalProperties_PutNetworkType_Proxy( 
    IBDA_SignalProperties __RPC_FAR * This,
    /* [in] */ REFGUID guidNetworkType);


void __RPC_STUB IBDA_SignalProperties_PutNetworkType_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_SignalProperties_GetNetworkType_Proxy( 
    IBDA_SignalProperties __RPC_FAR * This,
    /* [out][in] */ GUID __RPC_FAR *pguidNetworkType);


void __RPC_STUB IBDA_SignalProperties_GetNetworkType_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_SignalProperties_PutSignalSource_Proxy( 
    IBDA_SignalProperties __RPC_FAR * This,
    /* [in] */ ULONG ulSignalSource);


void __RPC_STUB IBDA_SignalProperties_PutSignalSource_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_SignalProperties_GetSignalSource_Proxy( 
    IBDA_SignalProperties __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *pulSignalSource);


void __RPC_STUB IBDA_SignalProperties_GetSignalSource_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_SignalProperties_PutTuningSpace_Proxy( 
    IBDA_SignalProperties __RPC_FAR * This,
    /* [in] */ REFGUID guidTuningSpace);


void __RPC_STUB IBDA_SignalProperties_PutTuningSpace_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_SignalProperties_GetTuningSpace_Proxy( 
    IBDA_SignalProperties __RPC_FAR * This,
    /* [out][in] */ GUID __RPC_FAR *pguidTuingSpace);


void __RPC_STUB IBDA_SignalProperties_GetTuningSpace_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IBDA_SignalProperties_INTERFACE_DEFINED__ */


#ifndef __IBDA_SignalStatistics_INTERFACE_DEFINED__
#define __IBDA_SignalStatistics_INTERFACE_DEFINED__

/* interface IBDA_SignalStatistics */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IBDA_SignalStatistics;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("1347D106-CF3A-428a-A5CB-AC0D9A2A4338")
    IBDA_SignalStatistics : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE put_SignalStrength( 
            /* [in] */ LONG lDbStrength) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE get_SignalStrength( 
            /* [out][in] */ LONG __RPC_FAR *plDbStrength) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE put_SignalQuality( 
            /* [in] */ LONG lPercentQuality) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE get_SignalQuality( 
            /* [out][in] */ LONG __RPC_FAR *plPercentQuality) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE put_SignalPresent( 
            /* [in] */ BOOLEAN fPresent) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE get_SignalPresent( 
            /* [out][in] */ BOOLEAN __RPC_FAR *pfPresent) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE put_SignalLocked( 
            /* [in] */ BOOLEAN fLocked) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE get_SignalLocked( 
            /* [out][in] */ BOOLEAN __RPC_FAR *pfLocked) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE put_SampleTime( 
            /* [in] */ LONG lmsSampleTime) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE get_SampleTime( 
            /* [out][in] */ LONG __RPC_FAR *plmsSampleTime) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IBDA_SignalStatisticsVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IBDA_SignalStatistics __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IBDA_SignalStatistics __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IBDA_SignalStatistics __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_SignalStrength )( 
            IBDA_SignalStatistics __RPC_FAR * This,
            /* [in] */ LONG lDbStrength);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_SignalStrength )( 
            IBDA_SignalStatistics __RPC_FAR * This,
            /* [out][in] */ LONG __RPC_FAR *plDbStrength);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_SignalQuality )( 
            IBDA_SignalStatistics __RPC_FAR * This,
            /* [in] */ LONG lPercentQuality);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_SignalQuality )( 
            IBDA_SignalStatistics __RPC_FAR * This,
            /* [out][in] */ LONG __RPC_FAR *plPercentQuality);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_SignalPresent )( 
            IBDA_SignalStatistics __RPC_FAR * This,
            /* [in] */ BOOLEAN fPresent);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_SignalPresent )( 
            IBDA_SignalStatistics __RPC_FAR * This,
            /* [out][in] */ BOOLEAN __RPC_FAR *pfPresent);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_SignalLocked )( 
            IBDA_SignalStatistics __RPC_FAR * This,
            /* [in] */ BOOLEAN fLocked);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_SignalLocked )( 
            IBDA_SignalStatistics __RPC_FAR * This,
            /* [out][in] */ BOOLEAN __RPC_FAR *pfLocked);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_SampleTime )( 
            IBDA_SignalStatistics __RPC_FAR * This,
            /* [in] */ LONG lmsSampleTime);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_SampleTime )( 
            IBDA_SignalStatistics __RPC_FAR * This,
            /* [out][in] */ LONG __RPC_FAR *plmsSampleTime);
        
        END_INTERFACE
    } IBDA_SignalStatisticsVtbl;

    interface IBDA_SignalStatistics
    {
        CONST_VTBL struct IBDA_SignalStatisticsVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IBDA_SignalStatistics_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IBDA_SignalStatistics_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IBDA_SignalStatistics_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IBDA_SignalStatistics_put_SignalStrength(This,lDbStrength)	\
    (This)->lpVtbl -> put_SignalStrength(This,lDbStrength)

#define IBDA_SignalStatistics_get_SignalStrength(This,plDbStrength)	\
    (This)->lpVtbl -> get_SignalStrength(This,plDbStrength)

#define IBDA_SignalStatistics_put_SignalQuality(This,lPercentQuality)	\
    (This)->lpVtbl -> put_SignalQuality(This,lPercentQuality)

#define IBDA_SignalStatistics_get_SignalQuality(This,plPercentQuality)	\
    (This)->lpVtbl -> get_SignalQuality(This,plPercentQuality)

#define IBDA_SignalStatistics_put_SignalPresent(This,fPresent)	\
    (This)->lpVtbl -> put_SignalPresent(This,fPresent)

#define IBDA_SignalStatistics_get_SignalPresent(This,pfPresent)	\
    (This)->lpVtbl -> get_SignalPresent(This,pfPresent)

#define IBDA_SignalStatistics_put_SignalLocked(This,fLocked)	\
    (This)->lpVtbl -> put_SignalLocked(This,fLocked)

#define IBDA_SignalStatistics_get_SignalLocked(This,pfLocked)	\
    (This)->lpVtbl -> get_SignalLocked(This,pfLocked)

#define IBDA_SignalStatistics_put_SampleTime(This,lmsSampleTime)	\
    (This)->lpVtbl -> put_SampleTime(This,lmsSampleTime)

#define IBDA_SignalStatistics_get_SampleTime(This,plmsSampleTime)	\
    (This)->lpVtbl -> get_SampleTime(This,plmsSampleTime)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IBDA_SignalStatistics_put_SignalStrength_Proxy( 
    IBDA_SignalStatistics __RPC_FAR * This,
    /* [in] */ LONG lDbStrength);


void __RPC_STUB IBDA_SignalStatistics_put_SignalStrength_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_SignalStatistics_get_SignalStrength_Proxy( 
    IBDA_SignalStatistics __RPC_FAR * This,
    /* [out][in] */ LONG __RPC_FAR *plDbStrength);


void __RPC_STUB IBDA_SignalStatistics_get_SignalStrength_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_SignalStatistics_put_SignalQuality_Proxy( 
    IBDA_SignalStatistics __RPC_FAR * This,
    /* [in] */ LONG lPercentQuality);


void __RPC_STUB IBDA_SignalStatistics_put_SignalQuality_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_SignalStatistics_get_SignalQuality_Proxy( 
    IBDA_SignalStatistics __RPC_FAR * This,
    /* [out][in] */ LONG __RPC_FAR *plPercentQuality);


void __RPC_STUB IBDA_SignalStatistics_get_SignalQuality_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_SignalStatistics_put_SignalPresent_Proxy( 
    IBDA_SignalStatistics __RPC_FAR * This,
    /* [in] */ BOOLEAN fPresent);


void __RPC_STUB IBDA_SignalStatistics_put_SignalPresent_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_SignalStatistics_get_SignalPresent_Proxy( 
    IBDA_SignalStatistics __RPC_FAR * This,
    /* [out][in] */ BOOLEAN __RPC_FAR *pfPresent);


void __RPC_STUB IBDA_SignalStatistics_get_SignalPresent_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_SignalStatistics_put_SignalLocked_Proxy( 
    IBDA_SignalStatistics __RPC_FAR * This,
    /* [in] */ BOOLEAN fLocked);


void __RPC_STUB IBDA_SignalStatistics_put_SignalLocked_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_SignalStatistics_get_SignalLocked_Proxy( 
    IBDA_SignalStatistics __RPC_FAR * This,
    /* [out][in] */ BOOLEAN __RPC_FAR *pfLocked);


void __RPC_STUB IBDA_SignalStatistics_get_SignalLocked_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_SignalStatistics_put_SampleTime_Proxy( 
    IBDA_SignalStatistics __RPC_FAR * This,
    /* [in] */ LONG lmsSampleTime);


void __RPC_STUB IBDA_SignalStatistics_put_SampleTime_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_SignalStatistics_get_SampleTime_Proxy( 
    IBDA_SignalStatistics __RPC_FAR * This,
    /* [out][in] */ LONG __RPC_FAR *plmsSampleTime);


void __RPC_STUB IBDA_SignalStatistics_get_SampleTime_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IBDA_SignalStatistics_INTERFACE_DEFINED__ */


#ifndef __IBDA_Topology_INTERFACE_DEFINED__
#define __IBDA_Topology_INTERFACE_DEFINED__

/* interface IBDA_Topology */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IBDA_Topology;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("79B56888-7FEA-4690-B45D-38FD3C7849BE")
    IBDA_Topology : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetNodeTypes( 
            /* [out][in] */ ULONG __RPC_FAR *pulcNodeTypes,
            /* [in] */ ULONG ulcNodeTypesMax,
            /* [size_is][out][in] */ ULONG __RPC_FAR rgulNodeTypes[  ]) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetNodeDescriptors( 
            /* [out][in] */ ULONG __RPC_FAR *ulcNodeDescriptors,
            /* [in] */ ULONG ulcNodeDescriptorsMax,
            /* [size_is][out][in] */ BDANODE_DESCRIPTOR __RPC_FAR rgNodeDescriptors[  ]) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetNodeInterfaces( 
            /* [in] */ ULONG ulNodeType,
            /* [out][in] */ ULONG __RPC_FAR *pulcInterfaces,
            /* [in] */ ULONG ulcInterfacesMax,
            /* [size_is][out][in] */ GUID __RPC_FAR rgguidInterfaces[  ]) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetPinTypes( 
            /* [out][in] */ ULONG __RPC_FAR *pulcPinTypes,
            /* [in] */ ULONG ulcPinTypesMax,
            /* [size_is][out][in] */ ULONG __RPC_FAR rgulPinTypes[  ]) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetTemplateConnections( 
            /* [out][in] */ ULONG __RPC_FAR *pulcConnections,
            /* [in] */ ULONG ulcConnectionsMax,
            /* [size_is][out][in] */ BDA_TEMPLATE_CONNECTION __RPC_FAR rgConnections[  ]) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE CreatePin( 
            /* [in] */ ULONG ulPinType,
            /* [out][in] */ ULONG __RPC_FAR *pulPinId) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE DeletePin( 
            /* [in] */ ULONG ulPinId) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetMediaType( 
            /* [in] */ ULONG ulPinId,
            /* [in] */ AM_MEDIA_TYPE __RPC_FAR *pMediaType) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetMedium( 
            /* [in] */ ULONG ulPinId,
            /* [in] */ REGPINMEDIUM __RPC_FAR *pMedium) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE CreateTopology( 
            /* [in] */ ULONG ulInputPinId,
            /* [in] */ ULONG ulOutputPinId) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetControlNode( 
            /* [in] */ ULONG ulInputPinId,
            /* [in] */ ULONG ulOutputPinId,
            /* [in] */ ULONG ulNodeType,
            /* [out][in] */ IUnknown __RPC_FAR *__RPC_FAR *ppControlNode) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IBDA_TopologyVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IBDA_Topology __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IBDA_Topology __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IBDA_Topology __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetNodeTypes )( 
            IBDA_Topology __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *pulcNodeTypes,
            /* [in] */ ULONG ulcNodeTypesMax,
            /* [size_is][out][in] */ ULONG __RPC_FAR rgulNodeTypes[  ]);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetNodeDescriptors )( 
            IBDA_Topology __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *ulcNodeDescriptors,
            /* [in] */ ULONG ulcNodeDescriptorsMax,
            /* [size_is][out][in] */ BDANODE_DESCRIPTOR __RPC_FAR rgNodeDescriptors[  ]);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetNodeInterfaces )( 
            IBDA_Topology __RPC_FAR * This,
            /* [in] */ ULONG ulNodeType,
            /* [out][in] */ ULONG __RPC_FAR *pulcInterfaces,
            /* [in] */ ULONG ulcInterfacesMax,
            /* [size_is][out][in] */ GUID __RPC_FAR rgguidInterfaces[  ]);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetPinTypes )( 
            IBDA_Topology __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *pulcPinTypes,
            /* [in] */ ULONG ulcPinTypesMax,
            /* [size_is][out][in] */ ULONG __RPC_FAR rgulPinTypes[  ]);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetTemplateConnections )( 
            IBDA_Topology __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *pulcConnections,
            /* [in] */ ULONG ulcConnectionsMax,
            /* [size_is][out][in] */ BDA_TEMPLATE_CONNECTION __RPC_FAR rgConnections[  ]);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *CreatePin )( 
            IBDA_Topology __RPC_FAR * This,
            /* [in] */ ULONG ulPinType,
            /* [out][in] */ ULONG __RPC_FAR *pulPinId);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *DeletePin )( 
            IBDA_Topology __RPC_FAR * This,
            /* [in] */ ULONG ulPinId);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *SetMediaType )( 
            IBDA_Topology __RPC_FAR * This,
            /* [in] */ ULONG ulPinId,
            /* [in] */ AM_MEDIA_TYPE __RPC_FAR *pMediaType);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *SetMedium )( 
            IBDA_Topology __RPC_FAR * This,
            /* [in] */ ULONG ulPinId,
            /* [in] */ REGPINMEDIUM __RPC_FAR *pMedium);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *CreateTopology )( 
            IBDA_Topology __RPC_FAR * This,
            /* [in] */ ULONG ulInputPinId,
            /* [in] */ ULONG ulOutputPinId);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetControlNode )( 
            IBDA_Topology __RPC_FAR * This,
            /* [in] */ ULONG ulInputPinId,
            /* [in] */ ULONG ulOutputPinId,
            /* [in] */ ULONG ulNodeType,
            /* [out][in] */ IUnknown __RPC_FAR *__RPC_FAR *ppControlNode);
        
        END_INTERFACE
    } IBDA_TopologyVtbl;

    interface IBDA_Topology
    {
        CONST_VTBL struct IBDA_TopologyVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IBDA_Topology_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IBDA_Topology_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IBDA_Topology_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IBDA_Topology_GetNodeTypes(This,pulcNodeTypes,ulcNodeTypesMax,rgulNodeTypes)	\
    (This)->lpVtbl -> GetNodeTypes(This,pulcNodeTypes,ulcNodeTypesMax,rgulNodeTypes)

#define IBDA_Topology_GetNodeDescriptors(This,ulcNodeDescriptors,ulcNodeDescriptorsMax,rgNodeDescriptors)	\
    (This)->lpVtbl -> GetNodeDescriptors(This,ulcNodeDescriptors,ulcNodeDescriptorsMax,rgNodeDescriptors)

#define IBDA_Topology_GetNodeInterfaces(This,ulNodeType,pulcInterfaces,ulcInterfacesMax,rgguidInterfaces)	\
    (This)->lpVtbl -> GetNodeInterfaces(This,ulNodeType,pulcInterfaces,ulcInterfacesMax,rgguidInterfaces)

#define IBDA_Topology_GetPinTypes(This,pulcPinTypes,ulcPinTypesMax,rgulPinTypes)	\
    (This)->lpVtbl -> GetPinTypes(This,pulcPinTypes,ulcPinTypesMax,rgulPinTypes)

#define IBDA_Topology_GetTemplateConnections(This,pulcConnections,ulcConnectionsMax,rgConnections)	\
    (This)->lpVtbl -> GetTemplateConnections(This,pulcConnections,ulcConnectionsMax,rgConnections)

#define IBDA_Topology_CreatePin(This,ulPinType,pulPinId)	\
    (This)->lpVtbl -> CreatePin(This,ulPinType,pulPinId)

#define IBDA_Topology_DeletePin(This,ulPinId)	\
    (This)->lpVtbl -> DeletePin(This,ulPinId)

#define IBDA_Topology_SetMediaType(This,ulPinId,pMediaType)	\
    (This)->lpVtbl -> SetMediaType(This,ulPinId,pMediaType)

#define IBDA_Topology_SetMedium(This,ulPinId,pMedium)	\
    (This)->lpVtbl -> SetMedium(This,ulPinId,pMedium)

#define IBDA_Topology_CreateTopology(This,ulInputPinId,ulOutputPinId)	\
    (This)->lpVtbl -> CreateTopology(This,ulInputPinId,ulOutputPinId)

#define IBDA_Topology_GetControlNode(This,ulInputPinId,ulOutputPinId,ulNodeType,ppControlNode)	\
    (This)->lpVtbl -> GetControlNode(This,ulInputPinId,ulOutputPinId,ulNodeType,ppControlNode)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IBDA_Topology_GetNodeTypes_Proxy( 
    IBDA_Topology __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *pulcNodeTypes,
    /* [in] */ ULONG ulcNodeTypesMax,
    /* [size_is][out][in] */ ULONG __RPC_FAR rgulNodeTypes[  ]);


void __RPC_STUB IBDA_Topology_GetNodeTypes_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_Topology_GetNodeDescriptors_Proxy( 
    IBDA_Topology __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *ulcNodeDescriptors,
    /* [in] */ ULONG ulcNodeDescriptorsMax,
    /* [size_is][out][in] */ BDANODE_DESCRIPTOR __RPC_FAR rgNodeDescriptors[  ]);


void __RPC_STUB IBDA_Topology_GetNodeDescriptors_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_Topology_GetNodeInterfaces_Proxy( 
    IBDA_Topology __RPC_FAR * This,
    /* [in] */ ULONG ulNodeType,
    /* [out][in] */ ULONG __RPC_FAR *pulcInterfaces,
    /* [in] */ ULONG ulcInterfacesMax,
    /* [size_is][out][in] */ GUID __RPC_FAR rgguidInterfaces[  ]);


void __RPC_STUB IBDA_Topology_GetNodeInterfaces_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_Topology_GetPinTypes_Proxy( 
    IBDA_Topology __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *pulcPinTypes,
    /* [in] */ ULONG ulcPinTypesMax,
    /* [size_is][out][in] */ ULONG __RPC_FAR rgulPinTypes[  ]);


void __RPC_STUB IBDA_Topology_GetPinTypes_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_Topology_GetTemplateConnections_Proxy( 
    IBDA_Topology __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *pulcConnections,
    /* [in] */ ULONG ulcConnectionsMax,
    /* [size_is][out][in] */ BDA_TEMPLATE_CONNECTION __RPC_FAR rgConnections[  ]);


void __RPC_STUB IBDA_Topology_GetTemplateConnections_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_Topology_CreatePin_Proxy( 
    IBDA_Topology __RPC_FAR * This,
    /* [in] */ ULONG ulPinType,
    /* [out][in] */ ULONG __RPC_FAR *pulPinId);


void __RPC_STUB IBDA_Topology_CreatePin_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_Topology_DeletePin_Proxy( 
    IBDA_Topology __RPC_FAR * This,
    /* [in] */ ULONG ulPinId);


void __RPC_STUB IBDA_Topology_DeletePin_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_Topology_SetMediaType_Proxy( 
    IBDA_Topology __RPC_FAR * This,
    /* [in] */ ULONG ulPinId,
    /* [in] */ AM_MEDIA_TYPE __RPC_FAR *pMediaType);


void __RPC_STUB IBDA_Topology_SetMediaType_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_Topology_SetMedium_Proxy( 
    IBDA_Topology __RPC_FAR * This,
    /* [in] */ ULONG ulPinId,
    /* [in] */ REGPINMEDIUM __RPC_FAR *pMedium);


void __RPC_STUB IBDA_Topology_SetMedium_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_Topology_CreateTopology_Proxy( 
    IBDA_Topology __RPC_FAR * This,
    /* [in] */ ULONG ulInputPinId,
    /* [in] */ ULONG ulOutputPinId);


void __RPC_STUB IBDA_Topology_CreateTopology_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_Topology_GetControlNode_Proxy( 
    IBDA_Topology __RPC_FAR * This,
    /* [in] */ ULONG ulInputPinId,
    /* [in] */ ULONG ulOutputPinId,
    /* [in] */ ULONG ulNodeType,
    /* [out][in] */ IUnknown __RPC_FAR *__RPC_FAR *ppControlNode);


void __RPC_STUB IBDA_Topology_GetControlNode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IBDA_Topology_INTERFACE_DEFINED__ */


#ifndef __IBDA_VoidTransform_INTERFACE_DEFINED__
#define __IBDA_VoidTransform_INTERFACE_DEFINED__

/* interface IBDA_VoidTransform */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IBDA_VoidTransform;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("71985F46-1CA1-11d3-9CC8-00C04F7971E0")
    IBDA_VoidTransform : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Start( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Stop( void) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IBDA_VoidTransformVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IBDA_VoidTransform __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IBDA_VoidTransform __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IBDA_VoidTransform __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Start )( 
            IBDA_VoidTransform __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Stop )( 
            IBDA_VoidTransform __RPC_FAR * This);
        
        END_INTERFACE
    } IBDA_VoidTransformVtbl;

    interface IBDA_VoidTransform
    {
        CONST_VTBL struct IBDA_VoidTransformVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IBDA_VoidTransform_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IBDA_VoidTransform_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IBDA_VoidTransform_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IBDA_VoidTransform_Start(This)	\
    (This)->lpVtbl -> Start(This)

#define IBDA_VoidTransform_Stop(This)	\
    (This)->lpVtbl -> Stop(This)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IBDA_VoidTransform_Start_Proxy( 
    IBDA_VoidTransform __RPC_FAR * This);


void __RPC_STUB IBDA_VoidTransform_Start_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_VoidTransform_Stop_Proxy( 
    IBDA_VoidTransform __RPC_FAR * This);


void __RPC_STUB IBDA_VoidTransform_Stop_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IBDA_VoidTransform_INTERFACE_DEFINED__ */


#ifndef __IBDA_NullTransform_INTERFACE_DEFINED__
#define __IBDA_NullTransform_INTERFACE_DEFINED__

/* interface IBDA_NullTransform */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IBDA_NullTransform;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("DDF15B0D-BD25-11d2-9CA0-00C04F7971E0")
    IBDA_NullTransform : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Start( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Stop( void) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IBDA_NullTransformVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IBDA_NullTransform __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IBDA_NullTransform __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IBDA_NullTransform __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Start )( 
            IBDA_NullTransform __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Stop )( 
            IBDA_NullTransform __RPC_FAR * This);
        
        END_INTERFACE
    } IBDA_NullTransformVtbl;

    interface IBDA_NullTransform
    {
        CONST_VTBL struct IBDA_NullTransformVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IBDA_NullTransform_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IBDA_NullTransform_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IBDA_NullTransform_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IBDA_NullTransform_Start(This)	\
    (This)->lpVtbl -> Start(This)

#define IBDA_NullTransform_Stop(This)	\
    (This)->lpVtbl -> Stop(This)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IBDA_NullTransform_Start_Proxy( 
    IBDA_NullTransform __RPC_FAR * This);


void __RPC_STUB IBDA_NullTransform_Start_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_NullTransform_Stop_Proxy( 
    IBDA_NullTransform __RPC_FAR * This);


void __RPC_STUB IBDA_NullTransform_Stop_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IBDA_NullTransform_INTERFACE_DEFINED__ */


#ifndef __IBDA_FrequencyFilter_INTERFACE_DEFINED__
#define __IBDA_FrequencyFilter_INTERFACE_DEFINED__

/* interface IBDA_FrequencyFilter */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IBDA_FrequencyFilter;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("71985F47-1CA1-11d3-9CC8-00C04F7971E0")
    IBDA_FrequencyFilter : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE put_Autotune( 
            /* [in] */ ULONG ulTransponder) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE get_Autotune( 
            /* [out][in] */ ULONG __RPC_FAR *pulTransponder) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE put_Frequency( 
            /* [in] */ ULONG ulFrequency) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE get_Frequency( 
            /* [out][in] */ ULONG __RPC_FAR *pulFrequency) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE put_Polarity( 
            /* [in] */ Polarisation Polarity) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE get_Polarity( 
            /* [out][in] */ Polarisation __RPC_FAR *pPolarity) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE put_Range( 
            /* [in] */ ULONG ulRange) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE get_Range( 
            /* [out][in] */ ULONG __RPC_FAR *pulRange) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE put_Bandwidth( 
            /* [in] */ ULONG ulBandwidth) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE get_Bandwidth( 
            /* [out][in] */ ULONG __RPC_FAR *pulBandwidth) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE put_FrequencyMultiplier( 
            /* [in] */ ULONG ulMultiplier) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE get_FrequencyMultiplier( 
            /* [out][in] */ ULONG __RPC_FAR *pulMultiplier) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IBDA_FrequencyFilterVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IBDA_FrequencyFilter __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IBDA_FrequencyFilter __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IBDA_FrequencyFilter __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_Autotune )( 
            IBDA_FrequencyFilter __RPC_FAR * This,
            /* [in] */ ULONG ulTransponder);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_Autotune )( 
            IBDA_FrequencyFilter __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *pulTransponder);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_Frequency )( 
            IBDA_FrequencyFilter __RPC_FAR * This,
            /* [in] */ ULONG ulFrequency);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_Frequency )( 
            IBDA_FrequencyFilter __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *pulFrequency);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_Polarity )( 
            IBDA_FrequencyFilter __RPC_FAR * This,
            /* [in] */ Polarisation Polarity);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_Polarity )( 
            IBDA_FrequencyFilter __RPC_FAR * This,
            /* [out][in] */ Polarisation __RPC_FAR *pPolarity);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_Range )( 
            IBDA_FrequencyFilter __RPC_FAR * This,
            /* [in] */ ULONG ulRange);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_Range )( 
            IBDA_FrequencyFilter __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *pulRange);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_Bandwidth )( 
            IBDA_FrequencyFilter __RPC_FAR * This,
            /* [in] */ ULONG ulBandwidth);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_Bandwidth )( 
            IBDA_FrequencyFilter __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *pulBandwidth);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_FrequencyMultiplier )( 
            IBDA_FrequencyFilter __RPC_FAR * This,
            /* [in] */ ULONG ulMultiplier);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_FrequencyMultiplier )( 
            IBDA_FrequencyFilter __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *pulMultiplier);
        
        END_INTERFACE
    } IBDA_FrequencyFilterVtbl;

    interface IBDA_FrequencyFilter
    {
        CONST_VTBL struct IBDA_FrequencyFilterVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IBDA_FrequencyFilter_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IBDA_FrequencyFilter_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IBDA_FrequencyFilter_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IBDA_FrequencyFilter_put_Autotune(This,ulTransponder)	\
    (This)->lpVtbl -> put_Autotune(This,ulTransponder)

#define IBDA_FrequencyFilter_get_Autotune(This,pulTransponder)	\
    (This)->lpVtbl -> get_Autotune(This,pulTransponder)

#define IBDA_FrequencyFilter_put_Frequency(This,ulFrequency)	\
    (This)->lpVtbl -> put_Frequency(This,ulFrequency)

#define IBDA_FrequencyFilter_get_Frequency(This,pulFrequency)	\
    (This)->lpVtbl -> get_Frequency(This,pulFrequency)

#define IBDA_FrequencyFilter_put_Polarity(This,Polarity)	\
    (This)->lpVtbl -> put_Polarity(This,Polarity)

#define IBDA_FrequencyFilter_get_Polarity(This,pPolarity)	\
    (This)->lpVtbl -> get_Polarity(This,pPolarity)

#define IBDA_FrequencyFilter_put_Range(This,ulRange)	\
    (This)->lpVtbl -> put_Range(This,ulRange)

#define IBDA_FrequencyFilter_get_Range(This,pulRange)	\
    (This)->lpVtbl -> get_Range(This,pulRange)

#define IBDA_FrequencyFilter_put_Bandwidth(This,ulBandwidth)	\
    (This)->lpVtbl -> put_Bandwidth(This,ulBandwidth)

#define IBDA_FrequencyFilter_get_Bandwidth(This,pulBandwidth)	\
    (This)->lpVtbl -> get_Bandwidth(This,pulBandwidth)

#define IBDA_FrequencyFilter_put_FrequencyMultiplier(This,ulMultiplier)	\
    (This)->lpVtbl -> put_FrequencyMultiplier(This,ulMultiplier)

#define IBDA_FrequencyFilter_get_FrequencyMultiplier(This,pulMultiplier)	\
    (This)->lpVtbl -> get_FrequencyMultiplier(This,pulMultiplier)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IBDA_FrequencyFilter_put_Autotune_Proxy( 
    IBDA_FrequencyFilter __RPC_FAR * This,
    /* [in] */ ULONG ulTransponder);


void __RPC_STUB IBDA_FrequencyFilter_put_Autotune_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_FrequencyFilter_get_Autotune_Proxy( 
    IBDA_FrequencyFilter __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *pulTransponder);


void __RPC_STUB IBDA_FrequencyFilter_get_Autotune_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_FrequencyFilter_put_Frequency_Proxy( 
    IBDA_FrequencyFilter __RPC_FAR * This,
    /* [in] */ ULONG ulFrequency);


void __RPC_STUB IBDA_FrequencyFilter_put_Frequency_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_FrequencyFilter_get_Frequency_Proxy( 
    IBDA_FrequencyFilter __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *pulFrequency);


void __RPC_STUB IBDA_FrequencyFilter_get_Frequency_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_FrequencyFilter_put_Polarity_Proxy( 
    IBDA_FrequencyFilter __RPC_FAR * This,
    /* [in] */ Polarisation Polarity);


void __RPC_STUB IBDA_FrequencyFilter_put_Polarity_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_FrequencyFilter_get_Polarity_Proxy( 
    IBDA_FrequencyFilter __RPC_FAR * This,
    /* [out][in] */ Polarisation __RPC_FAR *pPolarity);


void __RPC_STUB IBDA_FrequencyFilter_get_Polarity_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_FrequencyFilter_put_Range_Proxy( 
    IBDA_FrequencyFilter __RPC_FAR * This,
    /* [in] */ ULONG ulRange);


void __RPC_STUB IBDA_FrequencyFilter_put_Range_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_FrequencyFilter_get_Range_Proxy( 
    IBDA_FrequencyFilter __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *pulRange);


void __RPC_STUB IBDA_FrequencyFilter_get_Range_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_FrequencyFilter_put_Bandwidth_Proxy( 
    IBDA_FrequencyFilter __RPC_FAR * This,
    /* [in] */ ULONG ulBandwidth);


void __RPC_STUB IBDA_FrequencyFilter_put_Bandwidth_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_FrequencyFilter_get_Bandwidth_Proxy( 
    IBDA_FrequencyFilter __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *pulBandwidth);


void __RPC_STUB IBDA_FrequencyFilter_get_Bandwidth_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_FrequencyFilter_put_FrequencyMultiplier_Proxy( 
    IBDA_FrequencyFilter __RPC_FAR * This,
    /* [in] */ ULONG ulMultiplier);


void __RPC_STUB IBDA_FrequencyFilter_put_FrequencyMultiplier_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_FrequencyFilter_get_FrequencyMultiplier_Proxy( 
    IBDA_FrequencyFilter __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *pulMultiplier);


void __RPC_STUB IBDA_FrequencyFilter_get_FrequencyMultiplier_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IBDA_FrequencyFilter_INTERFACE_DEFINED__ */


#ifndef __IBDA_LNBInfo_INTERFACE_DEFINED__
#define __IBDA_LNBInfo_INTERFACE_DEFINED__

/* interface IBDA_LNBInfo */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IBDA_LNBInfo;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("992CF102-49F9-4719-A664-C4F23E2408F4")
    IBDA_LNBInfo : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE put_LocalOscilatorFrequencyLowBand( 
            /* [in] */ ULONG ulLOFLow) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE get_LocalOscilatorFrequencyLowBand( 
            /* [out][in] */ ULONG __RPC_FAR *pulLOFLow) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE put_LocalOscilatorFrequencyHighBand( 
            /* [in] */ ULONG ulLOFHigh) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE get_LocalOscilatorFrequencyHighBand( 
            /* [out][in] */ ULONG __RPC_FAR *pulLOFHigh) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE put_HighLowSwitchFrequency( 
            /* [in] */ ULONG ulSwitchFrequency) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE get_HighLowSwitchFrequency( 
            /* [out][in] */ ULONG __RPC_FAR *pulSwitchFrequency) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IBDA_LNBInfoVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IBDA_LNBInfo __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IBDA_LNBInfo __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IBDA_LNBInfo __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_LocalOscilatorFrequencyLowBand )( 
            IBDA_LNBInfo __RPC_FAR * This,
            /* [in] */ ULONG ulLOFLow);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_LocalOscilatorFrequencyLowBand )( 
            IBDA_LNBInfo __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *pulLOFLow);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_LocalOscilatorFrequencyHighBand )( 
            IBDA_LNBInfo __RPC_FAR * This,
            /* [in] */ ULONG ulLOFHigh);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_LocalOscilatorFrequencyHighBand )( 
            IBDA_LNBInfo __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *pulLOFHigh);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_HighLowSwitchFrequency )( 
            IBDA_LNBInfo __RPC_FAR * This,
            /* [in] */ ULONG ulSwitchFrequency);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_HighLowSwitchFrequency )( 
            IBDA_LNBInfo __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *pulSwitchFrequency);
        
        END_INTERFACE
    } IBDA_LNBInfoVtbl;

    interface IBDA_LNBInfo
    {
        CONST_VTBL struct IBDA_LNBInfoVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IBDA_LNBInfo_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IBDA_LNBInfo_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IBDA_LNBInfo_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IBDA_LNBInfo_put_LocalOscilatorFrequencyLowBand(This,ulLOFLow)	\
    (This)->lpVtbl -> put_LocalOscilatorFrequencyLowBand(This,ulLOFLow)

#define IBDA_LNBInfo_get_LocalOscilatorFrequencyLowBand(This,pulLOFLow)	\
    (This)->lpVtbl -> get_LocalOscilatorFrequencyLowBand(This,pulLOFLow)

#define IBDA_LNBInfo_put_LocalOscilatorFrequencyHighBand(This,ulLOFHigh)	\
    (This)->lpVtbl -> put_LocalOscilatorFrequencyHighBand(This,ulLOFHigh)

#define IBDA_LNBInfo_get_LocalOscilatorFrequencyHighBand(This,pulLOFHigh)	\
    (This)->lpVtbl -> get_LocalOscilatorFrequencyHighBand(This,pulLOFHigh)

#define IBDA_LNBInfo_put_HighLowSwitchFrequency(This,ulSwitchFrequency)	\
    (This)->lpVtbl -> put_HighLowSwitchFrequency(This,ulSwitchFrequency)

#define IBDA_LNBInfo_get_HighLowSwitchFrequency(This,pulSwitchFrequency)	\
    (This)->lpVtbl -> get_HighLowSwitchFrequency(This,pulSwitchFrequency)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IBDA_LNBInfo_put_LocalOscilatorFrequencyLowBand_Proxy( 
    IBDA_LNBInfo __RPC_FAR * This,
    /* [in] */ ULONG ulLOFLow);


void __RPC_STUB IBDA_LNBInfo_put_LocalOscilatorFrequencyLowBand_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_LNBInfo_get_LocalOscilatorFrequencyLowBand_Proxy( 
    IBDA_LNBInfo __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *pulLOFLow);


void __RPC_STUB IBDA_LNBInfo_get_LocalOscilatorFrequencyLowBand_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_LNBInfo_put_LocalOscilatorFrequencyHighBand_Proxy( 
    IBDA_LNBInfo __RPC_FAR * This,
    /* [in] */ ULONG ulLOFHigh);


void __RPC_STUB IBDA_LNBInfo_put_LocalOscilatorFrequencyHighBand_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_LNBInfo_get_LocalOscilatorFrequencyHighBand_Proxy( 
    IBDA_LNBInfo __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *pulLOFHigh);


void __RPC_STUB IBDA_LNBInfo_get_LocalOscilatorFrequencyHighBand_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_LNBInfo_put_HighLowSwitchFrequency_Proxy( 
    IBDA_LNBInfo __RPC_FAR * This,
    /* [in] */ ULONG ulSwitchFrequency);


void __RPC_STUB IBDA_LNBInfo_put_HighLowSwitchFrequency_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_LNBInfo_get_HighLowSwitchFrequency_Proxy( 
    IBDA_LNBInfo __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *pulSwitchFrequency);


void __RPC_STUB IBDA_LNBInfo_get_HighLowSwitchFrequency_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IBDA_LNBInfo_INTERFACE_DEFINED__ */


#ifndef __IBDA_AutoDemodulate_INTERFACE_DEFINED__
#define __IBDA_AutoDemodulate_INTERFACE_DEFINED__

/* interface IBDA_AutoDemodulate */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IBDA_AutoDemodulate;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("DDF15B12-BD25-11d2-9CA0-00C04F7971E0")
    IBDA_AutoDemodulate : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE put_AutoDemodulate( void) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IBDA_AutoDemodulateVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IBDA_AutoDemodulate __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IBDA_AutoDemodulate __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IBDA_AutoDemodulate __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_AutoDemodulate )( 
            IBDA_AutoDemodulate __RPC_FAR * This);
        
        END_INTERFACE
    } IBDA_AutoDemodulateVtbl;

    interface IBDA_AutoDemodulate
    {
        CONST_VTBL struct IBDA_AutoDemodulateVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IBDA_AutoDemodulate_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IBDA_AutoDemodulate_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IBDA_AutoDemodulate_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IBDA_AutoDemodulate_put_AutoDemodulate(This)	\
    (This)->lpVtbl -> put_AutoDemodulate(This)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IBDA_AutoDemodulate_put_AutoDemodulate_Proxy( 
    IBDA_AutoDemodulate __RPC_FAR * This);


void __RPC_STUB IBDA_AutoDemodulate_put_AutoDemodulate_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IBDA_AutoDemodulate_INTERFACE_DEFINED__ */


#ifndef __IBDA_DigitalDemodulator_INTERFACE_DEFINED__
#define __IBDA_DigitalDemodulator_INTERFACE_DEFINED__

/* interface IBDA_DigitalDemodulator */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IBDA_DigitalDemodulator;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("EF30F379-985B-4d10-B640-A79D5E04E1E0")
    IBDA_DigitalDemodulator : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE put_ModulationType( 
            /* [in] */ ModulationType __RPC_FAR *pModulationType) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE get_ModulationType( 
            /* [out][in] */ ModulationType __RPC_FAR *pModulationType) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE put_InnerFECMethod( 
            /* [in] */ FECMethod __RPC_FAR *pFECMethod) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE get_InnerFECMethod( 
            /* [out][in] */ FECMethod __RPC_FAR *pFECMethod) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE put_InnerFECRate( 
            /* [in] */ BinaryConvolutionCodeRate __RPC_FAR *pFECRate) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE get_InnerFECRate( 
            /* [out][in] */ BinaryConvolutionCodeRate __RPC_FAR *pFECRate) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE put_OuterFECMethod( 
            /* [in] */ FECMethod __RPC_FAR *pFECMethod) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE get_OuterFECMethod( 
            /* [out][in] */ FECMethod __RPC_FAR *pFECMethod) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE put_OuterFECRate( 
            /* [in] */ BinaryConvolutionCodeRate __RPC_FAR *pFECRate) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE get_OuterFECRate( 
            /* [out][in] */ BinaryConvolutionCodeRate __RPC_FAR *pFECRate) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE put_SymbolRate( 
            /* [in] */ ULONG __RPC_FAR *pSymbolRate) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE get_SymbolRate( 
            /* [out][in] */ ULONG __RPC_FAR *pSymbolRate) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE put_SpectralInversion( 
            /* [in] */ SpectralInversion __RPC_FAR *pSpectralInversion) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE get_SpectralInversion( 
            /* [out][in] */ SpectralInversion __RPC_FAR *pSpectralInversion) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IBDA_DigitalDemodulatorVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IBDA_DigitalDemodulator __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IBDA_DigitalDemodulator __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IBDA_DigitalDemodulator __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_ModulationType )( 
            IBDA_DigitalDemodulator __RPC_FAR * This,
            /* [in] */ ModulationType __RPC_FAR *pModulationType);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_ModulationType )( 
            IBDA_DigitalDemodulator __RPC_FAR * This,
            /* [out][in] */ ModulationType __RPC_FAR *pModulationType);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_InnerFECMethod )( 
            IBDA_DigitalDemodulator __RPC_FAR * This,
            /* [in] */ FECMethod __RPC_FAR *pFECMethod);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_InnerFECMethod )( 
            IBDA_DigitalDemodulator __RPC_FAR * This,
            /* [out][in] */ FECMethod __RPC_FAR *pFECMethod);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_InnerFECRate )( 
            IBDA_DigitalDemodulator __RPC_FAR * This,
            /* [in] */ BinaryConvolutionCodeRate __RPC_FAR *pFECRate);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_InnerFECRate )( 
            IBDA_DigitalDemodulator __RPC_FAR * This,
            /* [out][in] */ BinaryConvolutionCodeRate __RPC_FAR *pFECRate);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_OuterFECMethod )( 
            IBDA_DigitalDemodulator __RPC_FAR * This,
            /* [in] */ FECMethod __RPC_FAR *pFECMethod);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_OuterFECMethod )( 
            IBDA_DigitalDemodulator __RPC_FAR * This,
            /* [out][in] */ FECMethod __RPC_FAR *pFECMethod);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_OuterFECRate )( 
            IBDA_DigitalDemodulator __RPC_FAR * This,
            /* [in] */ BinaryConvolutionCodeRate __RPC_FAR *pFECRate);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_OuterFECRate )( 
            IBDA_DigitalDemodulator __RPC_FAR * This,
            /* [out][in] */ BinaryConvolutionCodeRate __RPC_FAR *pFECRate);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_SymbolRate )( 
            IBDA_DigitalDemodulator __RPC_FAR * This,
            /* [in] */ ULONG __RPC_FAR *pSymbolRate);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_SymbolRate )( 
            IBDA_DigitalDemodulator __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *pSymbolRate);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_SpectralInversion )( 
            IBDA_DigitalDemodulator __RPC_FAR * This,
            /* [in] */ SpectralInversion __RPC_FAR *pSpectralInversion);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_SpectralInversion )( 
            IBDA_DigitalDemodulator __RPC_FAR * This,
            /* [out][in] */ SpectralInversion __RPC_FAR *pSpectralInversion);
        
        END_INTERFACE
    } IBDA_DigitalDemodulatorVtbl;

    interface IBDA_DigitalDemodulator
    {
        CONST_VTBL struct IBDA_DigitalDemodulatorVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IBDA_DigitalDemodulator_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IBDA_DigitalDemodulator_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IBDA_DigitalDemodulator_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IBDA_DigitalDemodulator_put_ModulationType(This,pModulationType)	\
    (This)->lpVtbl -> put_ModulationType(This,pModulationType)

#define IBDA_DigitalDemodulator_get_ModulationType(This,pModulationType)	\
    (This)->lpVtbl -> get_ModulationType(This,pModulationType)

#define IBDA_DigitalDemodulator_put_InnerFECMethod(This,pFECMethod)	\
    (This)->lpVtbl -> put_InnerFECMethod(This,pFECMethod)

#define IBDA_DigitalDemodulator_get_InnerFECMethod(This,pFECMethod)	\
    (This)->lpVtbl -> get_InnerFECMethod(This,pFECMethod)

#define IBDA_DigitalDemodulator_put_InnerFECRate(This,pFECRate)	\
    (This)->lpVtbl -> put_InnerFECRate(This,pFECRate)

#define IBDA_DigitalDemodulator_get_InnerFECRate(This,pFECRate)	\
    (This)->lpVtbl -> get_InnerFECRate(This,pFECRate)

#define IBDA_DigitalDemodulator_put_OuterFECMethod(This,pFECMethod)	\
    (This)->lpVtbl -> put_OuterFECMethod(This,pFECMethod)

#define IBDA_DigitalDemodulator_get_OuterFECMethod(This,pFECMethod)	\
    (This)->lpVtbl -> get_OuterFECMethod(This,pFECMethod)

#define IBDA_DigitalDemodulator_put_OuterFECRate(This,pFECRate)	\
    (This)->lpVtbl -> put_OuterFECRate(This,pFECRate)

#define IBDA_DigitalDemodulator_get_OuterFECRate(This,pFECRate)	\
    (This)->lpVtbl -> get_OuterFECRate(This,pFECRate)

#define IBDA_DigitalDemodulator_put_SymbolRate(This,pSymbolRate)	\
    (This)->lpVtbl -> put_SymbolRate(This,pSymbolRate)

#define IBDA_DigitalDemodulator_get_SymbolRate(This,pSymbolRate)	\
    (This)->lpVtbl -> get_SymbolRate(This,pSymbolRate)

#define IBDA_DigitalDemodulator_put_SpectralInversion(This,pSpectralInversion)	\
    (This)->lpVtbl -> put_SpectralInversion(This,pSpectralInversion)

#define IBDA_DigitalDemodulator_get_SpectralInversion(This,pSpectralInversion)	\
    (This)->lpVtbl -> get_SpectralInversion(This,pSpectralInversion)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IBDA_DigitalDemodulator_put_ModulationType_Proxy( 
    IBDA_DigitalDemodulator __RPC_FAR * This,
    /* [in] */ ModulationType __RPC_FAR *pModulationType);


void __RPC_STUB IBDA_DigitalDemodulator_put_ModulationType_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_DigitalDemodulator_get_ModulationType_Proxy( 
    IBDA_DigitalDemodulator __RPC_FAR * This,
    /* [out][in] */ ModulationType __RPC_FAR *pModulationType);


void __RPC_STUB IBDA_DigitalDemodulator_get_ModulationType_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_DigitalDemodulator_put_InnerFECMethod_Proxy( 
    IBDA_DigitalDemodulator __RPC_FAR * This,
    /* [in] */ FECMethod __RPC_FAR *pFECMethod);


void __RPC_STUB IBDA_DigitalDemodulator_put_InnerFECMethod_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_DigitalDemodulator_get_InnerFECMethod_Proxy( 
    IBDA_DigitalDemodulator __RPC_FAR * This,
    /* [out][in] */ FECMethod __RPC_FAR *pFECMethod);


void __RPC_STUB IBDA_DigitalDemodulator_get_InnerFECMethod_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_DigitalDemodulator_put_InnerFECRate_Proxy( 
    IBDA_DigitalDemodulator __RPC_FAR * This,
    /* [in] */ BinaryConvolutionCodeRate __RPC_FAR *pFECRate);


void __RPC_STUB IBDA_DigitalDemodulator_put_InnerFECRate_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_DigitalDemodulator_get_InnerFECRate_Proxy( 
    IBDA_DigitalDemodulator __RPC_FAR * This,
    /* [out][in] */ BinaryConvolutionCodeRate __RPC_FAR *pFECRate);


void __RPC_STUB IBDA_DigitalDemodulator_get_InnerFECRate_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_DigitalDemodulator_put_OuterFECMethod_Proxy( 
    IBDA_DigitalDemodulator __RPC_FAR * This,
    /* [in] */ FECMethod __RPC_FAR *pFECMethod);


void __RPC_STUB IBDA_DigitalDemodulator_put_OuterFECMethod_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_DigitalDemodulator_get_OuterFECMethod_Proxy( 
    IBDA_DigitalDemodulator __RPC_FAR * This,
    /* [out][in] */ FECMethod __RPC_FAR *pFECMethod);


void __RPC_STUB IBDA_DigitalDemodulator_get_OuterFECMethod_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_DigitalDemodulator_put_OuterFECRate_Proxy( 
    IBDA_DigitalDemodulator __RPC_FAR * This,
    /* [in] */ BinaryConvolutionCodeRate __RPC_FAR *pFECRate);


void __RPC_STUB IBDA_DigitalDemodulator_put_OuterFECRate_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_DigitalDemodulator_get_OuterFECRate_Proxy( 
    IBDA_DigitalDemodulator __RPC_FAR * This,
    /* [out][in] */ BinaryConvolutionCodeRate __RPC_FAR *pFECRate);


void __RPC_STUB IBDA_DigitalDemodulator_get_OuterFECRate_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_DigitalDemodulator_put_SymbolRate_Proxy( 
    IBDA_DigitalDemodulator __RPC_FAR * This,
    /* [in] */ ULONG __RPC_FAR *pSymbolRate);


void __RPC_STUB IBDA_DigitalDemodulator_put_SymbolRate_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_DigitalDemodulator_get_SymbolRate_Proxy( 
    IBDA_DigitalDemodulator __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *pSymbolRate);


void __RPC_STUB IBDA_DigitalDemodulator_get_SymbolRate_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_DigitalDemodulator_put_SpectralInversion_Proxy( 
    IBDA_DigitalDemodulator __RPC_FAR * This,
    /* [in] */ SpectralInversion __RPC_FAR *pSpectralInversion);


void __RPC_STUB IBDA_DigitalDemodulator_put_SpectralInversion_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_DigitalDemodulator_get_SpectralInversion_Proxy( 
    IBDA_DigitalDemodulator __RPC_FAR * This,
    /* [out][in] */ SpectralInversion __RPC_FAR *pSpectralInversion);


void __RPC_STUB IBDA_DigitalDemodulator_get_SpectralInversion_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IBDA_DigitalDemodulator_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_bdaiface_0265 */
/* [local] */ 

typedef /* [public] */ 
enum __MIDL___MIDL_itf_bdaiface_0265_0001
    {	KSPROPERTY_IPSINK_MULTICASTLIST	= 0,
	KSPROPERTY_IPSINK_ADAPTER_DESCRIPTION	= KSPROPERTY_IPSINK_MULTICASTLIST + 1,
	KSPROPERTY_IPSINK_ADAPTER_ADDRESS	= KSPROPERTY_IPSINK_ADAPTER_DESCRIPTION + 1
    }	KSPROPERTY_IPSINK;



extern RPC_IF_HANDLE __MIDL_itf_bdaiface_0265_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_bdaiface_0265_v0_0_s_ifspec;

#ifndef __IBDA_IPSinkControl_INTERFACE_DEFINED__
#define __IBDA_IPSinkControl_INTERFACE_DEFINED__

/* interface IBDA_IPSinkControl */
/* [helpstring][unique][uuid][object] */ 


EXTERN_C const IID IID_IBDA_IPSinkControl;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("3F4DC8E2-4050-11d3-8F4B-00C04F7971E2")
    IBDA_IPSinkControl : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetMulticastList( 
            /* [out][in] */ unsigned long __RPC_FAR *pulcbSize,
            /* [out][in] */ BYTE __RPC_FAR *__RPC_FAR *pbBuffer) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetAdapterIPAddress( 
            /* [out][in] */ unsigned long __RPC_FAR *pulcbSize,
            /* [out][in] */ BYTE __RPC_FAR *__RPC_FAR *pbBuffer) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IBDA_IPSinkControlVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IBDA_IPSinkControl __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IBDA_IPSinkControl __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IBDA_IPSinkControl __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetMulticastList )( 
            IBDA_IPSinkControl __RPC_FAR * This,
            /* [out][in] */ unsigned long __RPC_FAR *pulcbSize,
            /* [out][in] */ BYTE __RPC_FAR *__RPC_FAR *pbBuffer);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetAdapterIPAddress )( 
            IBDA_IPSinkControl __RPC_FAR * This,
            /* [out][in] */ unsigned long __RPC_FAR *pulcbSize,
            /* [out][in] */ BYTE __RPC_FAR *__RPC_FAR *pbBuffer);
        
        END_INTERFACE
    } IBDA_IPSinkControlVtbl;

    interface IBDA_IPSinkControl
    {
        CONST_VTBL struct IBDA_IPSinkControlVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IBDA_IPSinkControl_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IBDA_IPSinkControl_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IBDA_IPSinkControl_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IBDA_IPSinkControl_GetMulticastList(This,pulcbSize,pbBuffer)	\
    (This)->lpVtbl -> GetMulticastList(This,pulcbSize,pbBuffer)

#define IBDA_IPSinkControl_GetAdapterIPAddress(This,pulcbSize,pbBuffer)	\
    (This)->lpVtbl -> GetAdapterIPAddress(This,pulcbSize,pbBuffer)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IBDA_IPSinkControl_GetMulticastList_Proxy( 
    IBDA_IPSinkControl __RPC_FAR * This,
    /* [out][in] */ unsigned long __RPC_FAR *pulcbSize,
    /* [out][in] */ BYTE __RPC_FAR *__RPC_FAR *pbBuffer);


void __RPC_STUB IBDA_IPSinkControl_GetMulticastList_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_IPSinkControl_GetAdapterIPAddress_Proxy( 
    IBDA_IPSinkControl __RPC_FAR * This,
    /* [out][in] */ unsigned long __RPC_FAR *pulcbSize,
    /* [out][in] */ BYTE __RPC_FAR *__RPC_FAR *pbBuffer);


void __RPC_STUB IBDA_IPSinkControl_GetAdapterIPAddress_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IBDA_IPSinkControl_INTERFACE_DEFINED__ */


#ifndef __IBDA_IPSinkInfo_INTERFACE_DEFINED__
#define __IBDA_IPSinkInfo_INTERFACE_DEFINED__

/* interface IBDA_IPSinkInfo */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IBDA_IPSinkInfo;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("A750108F-492E-4d51-95F7-649B23FF7AD7")
    IBDA_IPSinkInfo : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE get_MulticastList( 
            /* [out][in] */ ULONG __RPC_FAR *pulcbAddresses,
            /* [size_is][out] */ BYTE __RPC_FAR *__RPC_FAR *ppbAddressList) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE get_AdapterIPAddress( 
            /* [out] */ BSTR __RPC_FAR *pbstrBuffer) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE get_AdapterDescription( 
            /* [out] */ BSTR __RPC_FAR *pbstrBuffer) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IBDA_IPSinkInfoVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IBDA_IPSinkInfo __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IBDA_IPSinkInfo __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IBDA_IPSinkInfo __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_MulticastList )( 
            IBDA_IPSinkInfo __RPC_FAR * This,
            /* [out][in] */ ULONG __RPC_FAR *pulcbAddresses,
            /* [size_is][out] */ BYTE __RPC_FAR *__RPC_FAR *ppbAddressList);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_AdapterIPAddress )( 
            IBDA_IPSinkInfo __RPC_FAR * This,
            /* [out] */ BSTR __RPC_FAR *pbstrBuffer);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_AdapterDescription )( 
            IBDA_IPSinkInfo __RPC_FAR * This,
            /* [out] */ BSTR __RPC_FAR *pbstrBuffer);
        
        END_INTERFACE
    } IBDA_IPSinkInfoVtbl;

    interface IBDA_IPSinkInfo
    {
        CONST_VTBL struct IBDA_IPSinkInfoVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IBDA_IPSinkInfo_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IBDA_IPSinkInfo_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IBDA_IPSinkInfo_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IBDA_IPSinkInfo_get_MulticastList(This,pulcbAddresses,ppbAddressList)	\
    (This)->lpVtbl -> get_MulticastList(This,pulcbAddresses,ppbAddressList)

#define IBDA_IPSinkInfo_get_AdapterIPAddress(This,pbstrBuffer)	\
    (This)->lpVtbl -> get_AdapterIPAddress(This,pbstrBuffer)

#define IBDA_IPSinkInfo_get_AdapterDescription(This,pbstrBuffer)	\
    (This)->lpVtbl -> get_AdapterDescription(This,pbstrBuffer)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IBDA_IPSinkInfo_get_MulticastList_Proxy( 
    IBDA_IPSinkInfo __RPC_FAR * This,
    /* [out][in] */ ULONG __RPC_FAR *pulcbAddresses,
    /* [size_is][out] */ BYTE __RPC_FAR *__RPC_FAR *ppbAddressList);


void __RPC_STUB IBDA_IPSinkInfo_get_MulticastList_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_IPSinkInfo_get_AdapterIPAddress_Proxy( 
    IBDA_IPSinkInfo __RPC_FAR * This,
    /* [out] */ BSTR __RPC_FAR *pbstrBuffer);


void __RPC_STUB IBDA_IPSinkInfo_get_AdapterIPAddress_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IBDA_IPSinkInfo_get_AdapterDescription_Proxy( 
    IBDA_IPSinkInfo __RPC_FAR * This,
    /* [out] */ BSTR __RPC_FAR *pbstrBuffer);


void __RPC_STUB IBDA_IPSinkInfo_get_AdapterDescription_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IBDA_IPSinkInfo_INTERFACE_DEFINED__ */


#ifndef __IEnumPIDMap_INTERFACE_DEFINED__
#define __IEnumPIDMap_INTERFACE_DEFINED__

/* interface IEnumPIDMap */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IEnumPIDMap;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("afb6c2a2-2c41-11d3-8a60-0000f81e0e4a")
    IEnumPIDMap : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Next( 
            /* [in] */ ULONG cRequest,
            /* [size_is][out][in] */ PID_MAP __RPC_FAR *pPIDMap,
            /* [out] */ ULONG __RPC_FAR *pcReceived) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Skip( 
            /* [in] */ ULONG cRecords) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Reset( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Clone( 
            /* [out] */ IEnumPIDMap __RPC_FAR *__RPC_FAR *ppIEnumPIDMap) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IEnumPIDMapVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IEnumPIDMap __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IEnumPIDMap __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IEnumPIDMap __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Next )( 
            IEnumPIDMap __RPC_FAR * This,
            /* [in] */ ULONG cRequest,
            /* [size_is][out][in] */ PID_MAP __RPC_FAR *pPIDMap,
            /* [out] */ ULONG __RPC_FAR *pcReceived);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Skip )( 
            IEnumPIDMap __RPC_FAR * This,
            /* [in] */ ULONG cRecords);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Reset )( 
            IEnumPIDMap __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Clone )( 
            IEnumPIDMap __RPC_FAR * This,
            /* [out] */ IEnumPIDMap __RPC_FAR *__RPC_FAR *ppIEnumPIDMap);
        
        END_INTERFACE
    } IEnumPIDMapVtbl;

    interface IEnumPIDMap
    {
        CONST_VTBL struct IEnumPIDMapVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IEnumPIDMap_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IEnumPIDMap_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IEnumPIDMap_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IEnumPIDMap_Next(This,cRequest,pPIDMap,pcReceived)	\
    (This)->lpVtbl -> Next(This,cRequest,pPIDMap,pcReceived)

#define IEnumPIDMap_Skip(This,cRecords)	\
    (This)->lpVtbl -> Skip(This,cRecords)

#define IEnumPIDMap_Reset(This)	\
    (This)->lpVtbl -> Reset(This)

#define IEnumPIDMap_Clone(This,ppIEnumPIDMap)	\
    (This)->lpVtbl -> Clone(This,ppIEnumPIDMap)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IEnumPIDMap_Next_Proxy( 
    IEnumPIDMap __RPC_FAR * This,
    /* [in] */ ULONG cRequest,
    /* [size_is][out][in] */ PID_MAP __RPC_FAR *pPIDMap,
    /* [out] */ ULONG __RPC_FAR *pcReceived);


void __RPC_STUB IEnumPIDMap_Next_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IEnumPIDMap_Skip_Proxy( 
    IEnumPIDMap __RPC_FAR * This,
    /* [in] */ ULONG cRecords);


void __RPC_STUB IEnumPIDMap_Skip_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IEnumPIDMap_Reset_Proxy( 
    IEnumPIDMap __RPC_FAR * This);


void __RPC_STUB IEnumPIDMap_Reset_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IEnumPIDMap_Clone_Proxy( 
    IEnumPIDMap __RPC_FAR * This,
    /* [out] */ IEnumPIDMap __RPC_FAR *__RPC_FAR *ppIEnumPIDMap);


void __RPC_STUB IEnumPIDMap_Clone_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IEnumPIDMap_INTERFACE_DEFINED__ */


#ifndef __IMPEG2PIDMap_INTERFACE_DEFINED__
#define __IMPEG2PIDMap_INTERFACE_DEFINED__

/* interface IMPEG2PIDMap */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_IMPEG2PIDMap;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("afb6c2a1-2c41-11d3-8a60-0000f81e0e4a")
    IMPEG2PIDMap : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE MapPID( 
            /* [in] */ ULONG culPID,
            /* [in] */ ULONG __RPC_FAR *pulPID,
            /* [in] */ MEDIA_SAMPLE_CONTENT MediaSampleContent) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE UnmapPID( 
            /* [in] */ ULONG culPID,
            /* [in] */ ULONG __RPC_FAR *pulPID) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE EnumPIDMap( 
            /* [out] */ IEnumPIDMap __RPC_FAR *__RPC_FAR *pIEnumPIDMap) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IMPEG2PIDMapVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IMPEG2PIDMap __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IMPEG2PIDMap __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IMPEG2PIDMap __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *MapPID )( 
            IMPEG2PIDMap __RPC_FAR * This,
            /* [in] */ ULONG culPID,
            /* [in] */ ULONG __RPC_FAR *pulPID,
            /* [in] */ MEDIA_SAMPLE_CONTENT MediaSampleContent);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *UnmapPID )( 
            IMPEG2PIDMap __RPC_FAR * This,
            /* [in] */ ULONG culPID,
            /* [in] */ ULONG __RPC_FAR *pulPID);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *EnumPIDMap )( 
            IMPEG2PIDMap __RPC_FAR * This,
            /* [out] */ IEnumPIDMap __RPC_FAR *__RPC_FAR *pIEnumPIDMap);
        
        END_INTERFACE
    } IMPEG2PIDMapVtbl;

    interface IMPEG2PIDMap
    {
        CONST_VTBL struct IMPEG2PIDMapVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMPEG2PIDMap_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IMPEG2PIDMap_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IMPEG2PIDMap_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IMPEG2PIDMap_MapPID(This,culPID,pulPID,MediaSampleContent)	\
    (This)->lpVtbl -> MapPID(This,culPID,pulPID,MediaSampleContent)

#define IMPEG2PIDMap_UnmapPID(This,culPID,pulPID)	\
    (This)->lpVtbl -> UnmapPID(This,culPID,pulPID)

#define IMPEG2PIDMap_EnumPIDMap(This,pIEnumPIDMap)	\
    (This)->lpVtbl -> EnumPIDMap(This,pIEnumPIDMap)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IMPEG2PIDMap_MapPID_Proxy( 
    IMPEG2PIDMap __RPC_FAR * This,
    /* [in] */ ULONG culPID,
    /* [in] */ ULONG __RPC_FAR *pulPID,
    /* [in] */ MEDIA_SAMPLE_CONTENT MediaSampleContent);


void __RPC_STUB IMPEG2PIDMap_MapPID_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IMPEG2PIDMap_UnmapPID_Proxy( 
    IMPEG2PIDMap __RPC_FAR * This,
    /* [in] */ ULONG culPID,
    /* [in] */ ULONG __RPC_FAR *pulPID);


void __RPC_STUB IMPEG2PIDMap_UnmapPID_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IMPEG2PIDMap_EnumPIDMap_Proxy( 
    IMPEG2PIDMap __RPC_FAR * This,
    /* [out] */ IEnumPIDMap __RPC_FAR *__RPC_FAR *pIEnumPIDMap);


void __RPC_STUB IMPEG2PIDMap_EnumPIDMap_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IMPEG2PIDMap_INTERFACE_DEFINED__ */


#ifndef __IFrequencyMap_INTERFACE_DEFINED__
#define __IFrequencyMap_INTERFACE_DEFINED__

/* interface IFrequencyMap */
/* [restricted][hidden][unique][helpstring][uuid][object] */ 


EXTERN_C const IID IID_IFrequencyMap;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("06FB45C1-693C-4ea7-B79F-7A6A54D8DEF2")
    IFrequencyMap : public IUnknown
    {
    public:
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE get_FrequencyMapping( 
            /* [out] */ ULONG __RPC_FAR *ulCount,
            /* [size_is][size_is][out] */ ULONG __RPC_FAR *__RPC_FAR *ppulList) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE put_FrequencyMapping( 
            /* [in] */ ULONG ulCount,
            /* [size_is][in] */ ULONG __RPC_FAR pList[  ]) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE get_CountryCode( 
            /* [out] */ ULONG __RPC_FAR *pulCountryCode) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE put_CountryCode( 
            /* [in] */ ULONG ulCountryCode) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE get_DefaultFrequencyMapping( 
            /* [in] */ ULONG ulCountryCode,
            /* [out] */ ULONG __RPC_FAR *pulCount,
            /* [size_is][size_is][out] */ ULONG __RPC_FAR *__RPC_FAR *ppulList) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE get_CountryCodeList( 
            /* [out] */ ULONG __RPC_FAR *pulCount,
            /* [size_is][size_is][out] */ ULONG __RPC_FAR *__RPC_FAR *ppulList) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IFrequencyMapVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IFrequencyMap __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IFrequencyMap __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IFrequencyMap __RPC_FAR * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_FrequencyMapping )( 
            IFrequencyMap __RPC_FAR * This,
            /* [out] */ ULONG __RPC_FAR *ulCount,
            /* [size_is][size_is][out] */ ULONG __RPC_FAR *__RPC_FAR *ppulList);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_FrequencyMapping )( 
            IFrequencyMap __RPC_FAR * This,
            /* [in] */ ULONG ulCount,
            /* [size_is][in] */ ULONG __RPC_FAR pList[  ]);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_CountryCode )( 
            IFrequencyMap __RPC_FAR * This,
            /* [out] */ ULONG __RPC_FAR *pulCountryCode);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_CountryCode )( 
            IFrequencyMap __RPC_FAR * This,
            /* [in] */ ULONG ulCountryCode);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_DefaultFrequencyMapping )( 
            IFrequencyMap __RPC_FAR * This,
            /* [in] */ ULONG ulCountryCode,
            /* [out] */ ULONG __RPC_FAR *pulCount,
            /* [size_is][size_is][out] */ ULONG __RPC_FAR *__RPC_FAR *ppulList);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_CountryCodeList )( 
            IFrequencyMap __RPC_FAR * This,
            /* [out] */ ULONG __RPC_FAR *pulCount,
            /* [size_is][size_is][out] */ ULONG __RPC_FAR *__RPC_FAR *ppulList);
        
        END_INTERFACE
    } IFrequencyMapVtbl;

    interface IFrequencyMap
    {
        CONST_VTBL struct IFrequencyMapVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IFrequencyMap_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IFrequencyMap_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IFrequencyMap_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IFrequencyMap_get_FrequencyMapping(This,ulCount,ppulList)	\
    (This)->lpVtbl -> get_FrequencyMapping(This,ulCount,ppulList)

#define IFrequencyMap_put_FrequencyMapping(This,ulCount,pList)	\
    (This)->lpVtbl -> put_FrequencyMapping(This,ulCount,pList)

#define IFrequencyMap_get_CountryCode(This,pulCountryCode)	\
    (This)->lpVtbl -> get_CountryCode(This,pulCountryCode)

#define IFrequencyMap_put_CountryCode(This,ulCountryCode)	\
    (This)->lpVtbl -> put_CountryCode(This,ulCountryCode)

#define IFrequencyMap_get_DefaultFrequencyMapping(This,ulCountryCode,pulCount,ppulList)	\
    (This)->lpVtbl -> get_DefaultFrequencyMapping(This,ulCountryCode,pulCount,ppulList)

#define IFrequencyMap_get_CountryCodeList(This,pulCount,ppulList)	\
    (This)->lpVtbl -> get_CountryCodeList(This,pulCount,ppulList)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring] */ HRESULT STDMETHODCALLTYPE IFrequencyMap_get_FrequencyMapping_Proxy( 
    IFrequencyMap __RPC_FAR * This,
    /* [out] */ ULONG __RPC_FAR *ulCount,
    /* [size_is][size_is][out] */ ULONG __RPC_FAR *__RPC_FAR *ppulList);


void __RPC_STUB IFrequencyMap_get_FrequencyMapping_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IFrequencyMap_put_FrequencyMapping_Proxy( 
    IFrequencyMap __RPC_FAR * This,
    /* [in] */ ULONG ulCount,
    /* [size_is][in] */ ULONG __RPC_FAR pList[  ]);


void __RPC_STUB IFrequencyMap_put_FrequencyMapping_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IFrequencyMap_get_CountryCode_Proxy( 
    IFrequencyMap __RPC_FAR * This,
    /* [out] */ ULONG __RPC_FAR *pulCountryCode);


void __RPC_STUB IFrequencyMap_get_CountryCode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IFrequencyMap_put_CountryCode_Proxy( 
    IFrequencyMap __RPC_FAR * This,
    /* [in] */ ULONG ulCountryCode);


void __RPC_STUB IFrequencyMap_put_CountryCode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IFrequencyMap_get_DefaultFrequencyMapping_Proxy( 
    IFrequencyMap __RPC_FAR * This,
    /* [in] */ ULONG ulCountryCode,
    /* [out] */ ULONG __RPC_FAR *pulCount,
    /* [size_is][size_is][out] */ ULONG __RPC_FAR *__RPC_FAR *ppulList);


void __RPC_STUB IFrequencyMap_get_DefaultFrequencyMapping_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IFrequencyMap_get_CountryCodeList_Proxy( 
    IFrequencyMap __RPC_FAR * This,
    /* [out] */ ULONG __RPC_FAR *pulCount,
    /* [size_is][size_is][out] */ ULONG __RPC_FAR *__RPC_FAR *ppulList);


void __RPC_STUB IFrequencyMap_get_CountryCodeList_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IFrequencyMap_INTERFACE_DEFINED__ */


/* Additional Prototypes for ALL interfaces */

unsigned long             __RPC_USER  BSTR_UserSize(     unsigned long __RPC_FAR *, unsigned long            , BSTR __RPC_FAR * ); 
unsigned char __RPC_FAR * __RPC_USER  BSTR_UserMarshal(  unsigned long __RPC_FAR *, unsigned char __RPC_FAR *, BSTR __RPC_FAR * ); 
unsigned char __RPC_FAR * __RPC_USER  BSTR_UserUnmarshal(unsigned long __RPC_FAR *, unsigned char __RPC_FAR *, BSTR __RPC_FAR * ); 
void                      __RPC_USER  BSTR_UserFree(     unsigned long __RPC_FAR *, BSTR __RPC_FAR * ); 

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


