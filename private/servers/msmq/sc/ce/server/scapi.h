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
/*++


Module Name:

    scapi.h

Abstract:

    Small client - Windows CE nonport part


--*/
#if ! defined (__scapi_H__)
#define __scapi_H__	1

#include <objbase.h>
#include <objidl.h>
#include <wtypes.h>

typedef struct  tagSCPROPVAR
    {
    DWORD cProp;
    PROPID __RPC_FAR *aPropID;
    PROPVARIANT __RPC_FAR *aPropVar;
    HRESULT __RPC_FAR *aStatus;
    }	SCPROPVAR;

typedef unsigned long SCHANDLE;

#if defined (__cplusplus)
extern "C" {
#endif

void scce_Listen (void);
HRESULT scce_Shutdown (void);

int  scce_RegisterDLL (void);
int  scce_UnregisterDLL (void);
int  scce_RegisterNET (void *ptr);
void scce_UnregisterNET (void);

#if defined (__cplusplus)
};
#endif

#include "expapis.hxx"
#include <psl_marshaler.hxx>
#include <svsutil.hxx>


// 
//  PSL marshalling helpers and function wrappers
//

// Called when unmarshalling a pointer fails.
inline void MSMQMarshalPtrError() {
    SVSUTIL_ASSERT(0);
}

// CMarshalPropVariantIn marshalls a single PROPVARIANT structure with
// copy_in semantics.
class CMarshalPropVariantIn {
private:
    // Additional data in the case we did a deep copy.
    MarshalledBuffer_t m_MappedClientPtr;
    // The actual PROPVARIANT data itself.
    PROPVARIANT        m_PropVar;
    // When type is of (VT_VECTOR | VT_VARIANT), stores variants.
    CMarshalPropVariantIn *m_variantArray;

public:
    // The pProvVar itself has been mapped by the caller.  This
    // wrapper however provides the deep copy functionality.
    CMarshalPropVariantIn() {
        m_variantArray = NULL;
    }

    void MarshallPropVar(PROPVARIANT *pPropVar, MSGPROPID propID);
    ~CMarshalPropVariantIn();

    PROPVARIANT *GetPropVar(void) { return &m_PropVar; }
};

// Wrapper for WriteProcessToMemory
class CMarshallDataToProcess {
private:
	// Process to write memory to
	HANDLE m_hOwnerProcess;

public:
	CMarshallDataToProcess(HANDLE hOwnerProcess) {
		m_hOwnerProcess = hOwnerProcess;
	}

	// Writes pbBuffer into pbBaseAddress, cbBuffer bytes worth
	BOOL WriteBufToProc(UCHAR *pbBaseAddress, const UCHAR *pbBuffer, DWORD cbBuffer, HRESULT *pHR) {
		BOOL fSuccess = TRUE;

		SVSUTIL_ASSERT(pbBaseAddress && pbBuffer && (cbBuffer != 0));
		
		__try {
			fSuccess = WriteProcessMemory(m_hOwnerProcess,pbBaseAddress,(LPVOID)pbBuffer,cbBuffer,NULL);
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			fSuccess = FALSE;
		}

		if (pHR)
			*pHR = fSuccess ? MQ_OK : MQ_ERROR_INVALID_PARAMETER;

		return fSuccess;
	}

	// Since we copy a lot of strings, utility function for that.
	BOOL WriteStrToProc(WCHAR *szBaseAddress, const WCHAR *szBuffer, DWORD numChars, HRESULT *pHR) {
		// numChars must include the terminating NULL charater
		SVSUTIL_ASSERT(szBuffer[numChars-1] == L'\0');
		return WriteBufToProc((UCHAR*)szBaseAddress,(UCHAR*)szBuffer,numChars*sizeof(WCHAR),pHR);
	}

	BOOL WriteGUIDToProc(GUID *pBaseAddress, const GUID *pGuid, HRESULT *pHR) {
		return WriteBufToProc((UCHAR *)pBaseAddress, (UCHAR *)pGuid, sizeof(GUID), pHR);
	}
};


// Marshals the OVERLAPPED structure.  Note that this is allocated by one
// thread (PSL) and (potentially) deleted by another (servicesd.exe worker).
class CMarshalOverlapped {
private:
	// Used for writing back the status paramater of the OVERLAPPED.
	// This is stored in Overlapped.Internal.
	MarshalledBuffer_t m_overlapped;
	// Pointer to the marshalled data
	LPOVERLAPPED       m_pOverlapped;
	// Duplicated handle of Overlapped.hEvent
	HANDLE             m_hEvent;
	// Owner process.  We have to store this since we may do writes
	// on a servicesd.exe spun up thread, not on original PSL caller.
	HANDLE             m_hSourceProcess;
	// Was initialization successful?
	BOOL               m_fInited;

public:
	CMarshalOverlapped(LPOVERLAPPED pOverlapped, HANDLE hCallerProc) {
		m_fInited = FALSE;
		m_hEvent  = NULL;
		m_hSourceProcess = hCallerProc;

		HRESULT hr = m_overlapped.Marshal(pOverlapped,sizeof(pOverlapped),ARG_IO_PTR,
			                              TRUE,TRUE);
		if (FAILED(hr))
			return;

		m_hSourceProcess  = (HANDLE)GetCallerVMProcessId();
		m_pOverlapped     = (LPOVERLAPPED)m_overlapped.ptr();

		if (pOverlapped->hEvent) {
			if (! DuplicateHandle(m_hSourceProcess,pOverlapped->hEvent,GetCurrentProcess(),
			                      &m_hEvent,0,FALSE,DUPLICATE_SAME_ACCESS))
			{
				return;
			}
		}
		m_fInited = TRUE;
	}

	~CMarshalOverlapped() {
		// Writing back status to user-mode process is handled 
		// automatically by MarshalledBuffer_t destructor.
		if (m_hEvent)
			CloseHandle(m_hEvent);
	}

	void SetOverlappedEvent(void) {
		if (m_hEvent)
			SetEvent(m_hEvent);
	}

	void SetStatus(DWORD dwStatus) {
		m_pOverlapped->Internal = dwStatus;
	}

	BOOL IsInited(void) {
		return m_fInited;
	}
};


// Marshall SCPROPVAR pointer arrays from user mode caller -> services.exe process space
template<>
class ce::copy_in<SCPROPVAR*>
    : public ce::ptr_copy_traits_base<SCPROPVAR>
{
public:
    typedef ce::ptr_copy_traits_base<SCPROPVAR> my_base;

    copy_in(HANDLE hClientProcess) :
        my_base(hClientProcess)
    {
        m_fCopyInPropVar = TRUE;
		m_fInited = FALSE;
		m_pPropVariants = NULL;
    }

    ~copy_in() {
        if (m_pPropVariants)
            delete [] m_pPropVariants;
    }

    void copy_arg_in(SCPROPVAR *pPropVar);

    void copy_arg_out()
    {

    }

	void EnableCopyOutPropVar(void) {
		m_fCopyInPropVar = FALSE;
	}

	BOOL IsInited(void) {
		return m_fInited;
	}
	
private:
    // Contains (copy_in) aPropID data
    MarshalledBuffer_t m_MarshalPropIds;
    // Contains (copy_out) aStatus data.  The status is always returned
    // by MSMQ per param, even on read-only params, which is why
    // we're copying out on an otherwise copy_in set of structures
    MarshalledBuffer_t m_MarshalStatus;
    // Contains (copy_in) data about aPropVar elements.  These may 
    // involve deep copies, hence extra layer of abstraction.
    MarshalledBuffer_t m_MarshalPropVar;
    CMarshalPropVariantIn *m_pPropVariants;
	// Should we marshal aPropVar In or In/Out?  The default is In only.
	// For the In case, we also do a deep marshal of pointers.  In In/Out,
	// only marshal aPropVar and leave deep marshalling to later.
	BOOL               m_fCopyInPropVar;
	// Was the class marshalled properly.  This does not indicate that all parameters
	// passed were what we expected; just indicates internal memory allocs
	// and required marshalling completeded successfully.
	BOOL               m_fInited;
};

HRESULT	scapi_MQCreateQueuePSL
(
ce::marshal_arg<ce::copy_in,SCPROPVAR*> pQueueProps,
ce::marshal_arg<ce::copy_out,ce::psl_buffer_wrapper<WCHAR*> > lpszFormatName,
ce::marshal_arg<ce::copy_in_out, LPDWORD> pdwNameLen
);

HRESULT	scapi_MQDeleteQueuePSL
(
ce::marshal_arg<ce::copy_in,LPCWSTR> lpszFormatName
);

HRESULT	scapi_MQGetQueueProperties
(
WCHAR			*lpszFormatName,
SCPROPVAR		*pQueueProps
);

HRESULT	scapi_MQGetQueuePropertiesPSL
(
ce::marshal_arg<ce::copy_in,LPCWSTR> lpszFormatName,
SCPROPVAR		*pQueuePropsUnmarshalled
);

HRESULT scapi_MQSetQueuePropertiesPSL
(
    ce::marshal_arg<ce::copy_in,LPCWSTR> lpszFormatName,
    ce::marshal_arg<ce::copy_in,SCPROPVAR*> pMsgProps
);


HRESULT	scapi_MQOpenQueuePSL
(
ce::marshal_arg<ce::copy_in,LPCWSTR>, 
DWORD			dwAccess,
DWORD			dwShareMode,
ce::marshal_arg<ce::copy_out, LPDWORD> phQueue
);


HRESULT scapi_MQCloseQueuePSL
(
SCHANDLE		hQueue
);

HRESULT scapi_MQCreateCursorPSL
(
SCHANDLE		hQueue,
ce::marshal_arg<ce::copy_out, LPDWORD> phCursor
);

HRESULT scapi_MQCloseCursorPSL
(
SCHANDLE		hCursor
);

HRESULT scapi_MQHandleToFormatNamePSL
(
SCHANDLE hQueue,
ce::marshal_arg<ce::copy_out,ce::psl_buffer_wrapper<WCHAR*> > lpszFormatName,
ce::marshal_arg<ce::copy_in_out, LPDWORD> dwNameLen
);

HRESULT scapi_MQPathNameToFormatNamePSL
(
ce::marshal_arg<ce::copy_in,LPCWSTR> lpszPathName,
ce::marshal_arg<ce::copy_out,ce::psl_buffer_wrapper<WCHAR*> > lpszFormatName,
ce::marshal_arg<ce::copy_in_out, LPDWORD> pdwCount
);

HRESULT scapi_MQSendMessagePSL(
    SCHANDLE hQueue, 
    ce::marshal_arg<ce::copy_in,SCPROPVAR*> pMsgProps,
    int iTransaction
);

HRESULT scapi_MQReceiveMessage
(
SCHANDLE		hQueue,
DWORD			dwTimeout,
DWORD			dwAction,
SCPROPVAR		*pMsgProps,
OVERLAPPED		*lpOverlapped,
SCHANDLE		hCursor,
int				iNull3
);

HRESULT scapi_MQReceiveMessagePSL
(
SCHANDLE			hQueue,
DWORD				dwTimeout,
DWORD				dwAction,
SCPROPVAR			*pMsgPropsUnmarshalled,
OVERLAPPED			*lpOverlapped,
SCHANDLE			hCursor,
int					iNull3
);

HRESULT scapi_MQGetMachineProperties
(
const WCHAR     *lpszMachineName,
const GUID      *pguidMachineID,
SCPROPVAR       *pQMProps
);

HRESULT scapi_MQGetMachinePropertiesPSL
(
ce::marshal_arg<ce::copy_in,LPCWSTR> lpszMachineName,
ce::marshal_arg<ce::copy_in,GUID*> pguidMachineID,
SCPROPVAR		*pQMPropsUnmarshalled
);

typedef struct tagMQMGMTPROPS MQMGMTPROPS;

HRESULT scmgmt_MQMgmtGetInfo2PSL
(
	ce::marshal_arg<ce::copy_in,LPCWSTR> pMachineName,
	ce::marshal_arg<ce::copy_in,LPCWSTR> pObjectName,
	MQMGMTPROPS *pMgmtPropsUnmarshalled
);


HRESULT scmgmt_MQMgmtAction
(
LPCWSTR			pMachineName,
LPCWSTR			pObjectName,
LPCWSTR			pAction
);

HRESULT scmgmt_MQMgmtActionPSL
(
ce::marshal_arg<ce::copy_in,LPCWSTR> pMachineName,
ce::marshal_arg<ce::copy_in,LPCWSTR> pObjectName,
ce::marshal_arg<ce::copy_in,LPCWSTR> pAction
);

HRESULT scapi_MQFreeMemoryPSL(void *pvPtr);

void scapi_ProcExit (void *pvProcId);


#endif
