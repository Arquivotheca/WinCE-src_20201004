//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//



#ifndef __ssdp_h__
#define __ssdp_h__

//#include <windef.h>
//#include <types.h>
//#include <winbase.h>
#include <windows.h>

#ifndef __RPC_FAR
#define __RPC_FAR
#define __RPC_USER
#endif

/* Forward Declarations */ 


#ifdef __cplusplus
extern "C"{
#endif 

void __RPC_FAR * __RPC_USER MIDL_user_allocate(size_t);
void __RPC_USER MIDL_user_free( void __RPC_FAR * ); 


//#define SSDP_SERVICE_PERSISTENT 0x00000001
#define NUM_OF_METHODS 4
typedef 
enum _NOTIFY_TYPE
    {
		NOTIFY_ALIVE	= 1,
        NOTIFY_BYEBYE   = 2,
		NOTIFY_PROP_CHANGE	= 4,
    }NOTIFY_TYPE;

#define MAX_MSG_SIZE 1024

struct event_msg_hdr
{
	int			nNumberOfBlocks;
	HANDLE		hSubscription;
	NOTIFY_TYPE	nt;
	union
    {
        DWORD   dwEventSEQ;
        DWORD   dwLifeTime;
    };
};


typedef 
enum _SSDP_METHOD
    {	SSDP_NOTIFY	= 0,
	SSDP_M_SEARCH	= 1,
	GENA_SUBSCRIBE	= 2,
	GENA_UNSUBSCRIBE	= 3,
	SSDP_INVALID	= 4
    }	SSDP_METHOD;

typedef enum _SSDP_METHOD __RPC_FAR *PSSDP_METHOD;

typedef 
enum _SSDP_HEADER
    {	SSDP_HOST	= 0,
	    SSDP_NT	= 1,
	    SSDP_NTS	= 2,
	    SSDP_ST	= 3,
	    SSDP_MAN	= 4,
	    SSDP_MX	= 5,
	    SSDP_LOCATION	= 6,
	    SSDP_AL	= 7,
	    SSDP_USN	= 8,
	    SSDP_CACHECONTROL	= 9,
	    GENA_CALLBACK	= 10,
	    GENA_TIMEOUT	= 11,
	    GENA_SCOPE	= 12,
	    GENA_SID	= 13,
	    GENA_SEQ	= 14,
	    CONTENT_LENGTH	= 15,
	    CONTENT_TYPE = 16,
	    SSDP_SERVER = 17,
	    SSDP_EXT = 18,
	    SSDP_OPT = 19,
	    SSDP_NLS = 20,
	    SSDP_LAST		// end marker
    }SSDP_HEADER;

#define NUM_OF_HEADERS SSDP_LAST

typedef enum _SSDP_HEADER __RPC_FAR *PSSDP_HEADER;


typedef struct _SSDP_REQUEST
    {
    SSDP_METHOD Method;
    LPSTR RequestUri;
    LPSTR Headers[ SSDP_LAST ];
    LPSTR ContentType;
    DWORD ContentLength;
    LPSTR Content;
    DWORD status;
    
    }	SSDP_REQUEST;

typedef struct _SSDP_REQUEST __RPC_FAR *PSSDP_REQUEST;

typedef struct _SSDP_MESSAGE
    {
    /* [string] */ LPSTR szType;
    /* [string] */ LPSTR szLocHeader;
    /* [string] */ LPSTR szAltHeaders;
    /* [string] */ LPSTR szNls;
    /* [string] */ LPSTR szUSN;
    /* [string] */ LPSTR szSid;
    DWORD iSeq;
    UINT iLifeTime;
    /* [string] */ LPSTR szContent;
    }	SSDP_MESSAGE;

typedef struct _SSDP_MESSAGE __RPC_FAR *PSSDP_MESSAGE;

typedef struct _SSDP_REGISTER_INFO
    {
    /* [string] */ LPSTR szSid;
    DWORD csecTimeout;
    }	SSDP_REGISTER_INFO;

typedef struct _MessageList
    {
    long size;
    /* [size_is] */ SSDP_REQUEST __RPC_FAR *list;
    }	MessageList;

typedef 
enum _UPNP_PROPERTY_FLAG
    {	UPF_NON_EVENTED	= 0x1
    }	UPNP_PROPERTY_FLAG;

typedef struct _UPNP_PROPERTY
    {
    /* [string] */ LPWSTR szName;
    DWORD dwFlags;
    /* [string] */ LPWSTR szValue;
    }	UPNP_PROPERTY;

typedef struct _SUBSCRIBER_INFO
    {
    /* [string] */ LPSTR szDestUrl;
    DWORD csecTimeout;
    DWORD iSeq;
    /* [string] */ LPSTR szSid;
    }	SUBSCRIBER_INFO;

typedef struct _EVTSRC_INFO
    {
    DWORD cSubs;
    /* [size_is] */ SUBSCRIBER_INFO __RPC_FAR *rgSubs;
    }	EVTSRC_INFO;

typedef /* [context_handle] */ void __RPC_FAR *PCONTEXT_HANDLE_TYPE;

typedef /* [context_handle] */ void __RPC_FAR *PSYNC_HANDLE_TYPE;

#ifdef __cplusplus
}
#endif

#endif


