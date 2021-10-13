//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//=--------------------------------------------------------------------------=
// MSMQQueueInfoObj.Cpp
//=--------------------------------------------------------------------------=
//
// the MSMQQueueInfo object
//
//
#include "IPServer.H"

#include "LocalObj.H"

#include "oautil.h"
#include "q.h"
#include "qinfo.H"
#include "limits.h"
#include "time.h"

#if DEBUG
extern VOID RemBstrNode(void *pv);
#endif // DEBUG

// Falcon includes
// #include "rt.h"

// for ASSERT and FAIL
//
SZTHISFILE

// debug...
#define new DEBUG_NEW
#if DEBUG
#define SysAllocString DebSysAllocString
#define SysReAllocString DebSysReAllocString
#define SysFreeString DebSysFreeString
#endif // DEBUG


//=--------------------------------------------------------------------------=
// HELPER::InitQueueProps
//=--------------------------------------------------------------------------=
// Inits MQQUEUEPROPS struct.  
//
// Parameters:
//
// Output:
//
// Notes:
//
void InitQueueProps(MQQUEUEPROPS *pqueueprops)
{
    pqueueprops->aPropID = NULL;
    pqueueprops->aPropVar = NULL;
    pqueueprops->aStatus = NULL;
}


//=--------------------------------------------------------------------------=
// HELPER: GetFormatNameOfPathName
//=--------------------------------------------------------------------------=
// Gets formatname member if necessary from pathname
//
// Parameters:
//    pbstrFormatName [out] callee-allocated, caller-freed
//
// Output:
//    HRESULT       - S_OK, E_OUTOFMEMORY
//
// Notes:
//
HRESULT GetFormatNameOfPathName(
    BSTR bstrPathName,
    BSTR *pbstrFormatName)
{
    ULONG uiFormatNameLen = FORMAT_NAME_INIT_BUFFER;
#if _DEBUG
    ULONG uiFormatNameLenSave = uiFormatNameLen;
#endif // _DEBUG
    BSTR bstrFormatName = NULL;
    HRESULT hresult = NOERROR;

    // error if no pathname
    if (bstrPathName == NULL) {
      return E_INVALIDARG;
    }
    //
    // synthesize from pathname
    //
    IfNullRet(
      bstrFormatName = SysAllocStringLen(NULL, uiFormatNameLen));
    IfFailGo(
      MQPathNameToFormatName(bstrPathName,
                             bstrFormatName,
                             &uiFormatNameLen));

    // FormatName mucking...
#if _DEBUG
    ASSERT(uiFormatNameLen <= uiFormatNameLenSave,
           L"insufficient buffer.");
#endif

    IfNullGo(SysReAllocString(pbstrFormatName, bstrFormatName));
    // fall through...

Error:
    SysFreeString(bstrFormatName);

    return hresult;
}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::Create
//=--------------------------------------------------------------------------=
// creates a new MSMQQueueInfo object.
//
// Parameters:
//    IUnknown *        - [in] controlling unkonwn
//
// Output:
//    IUnknown *        - new object.
//
// Notes:
//
IUnknown *CMSMQQueueInfo::Create
(
    IUnknown *pUnkOuter
)
{
    // make sure we return the private unknown so that we support aggegation
    // correctly!
    //
    CMSMQQueueInfo *pNew = new CMSMQQueueInfo(pUnkOuter);
    return pNew ? pNew->PrivateUnknown() : NULL;
}

//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::CMSMQQueueInfo
//=--------------------------------------------------------------------------=
// create the object and initialize the refcount
//
// Parameters:
//    IUnknown *    - [in] controlling unknown
//
// Notes:
//
#pragma warning(disable:4355)  // using 'this' in constructor
CMSMQQueueInfo::CMSMQQueueInfo
(
    IUnknown *pUnkOuter
)
: CAutomationObject(pUnkOuter, OBJECT_TYPE_OBJMQQUEUEINFO, (void *)this)
{

    // TODO: initialize anything here
    // UNDONE: use DEFAULT_ values
    //
    m_pguidQueue = new GUID(GUID_NULL);
    m_pguidServiceType = new GUID(GUID_NULL);
    m_bstrLabel = SysAllocString(L"");
    m_bstrFormatName = NULL;
    m_isValidFormatName = FALSE;  // 2026
    m_bstrPathName = NULL;
    m_isTransactional = FALSE;
    m_lPrivLevel = (long)DEFAULT_Q_PRIV_LEVEL;
    m_lJournal = DEFAULT_Q_JOURNAL;                 
    m_lQuota = (long)DEFAULT_Q_QUOTA;                                          
    m_lBasePriority = DEFAULT_Q_BASEPRIORITY;                                   
    m_lCreateTime = 0;
    m_lModifyTime = 0;
    m_lAuthenticate = (long)DEFAULT_Q_AUTHENTICATE;  
    m_lJournalQuota = (long)DEFAULT_Q_JOURNAL_QUOTA ;
    m_isRefreshed = FALSE;    // 2536
}
#pragma warning(default:4355)  // using 'this' in constructor

//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::CMSMQQueueInfo
//=--------------------------------------------------------------------------=
// "We all labour against our own cure, for death is the cure of all diseases"
//    - Sir Thomas Browne (1605 - 82)
//
// Notes:
//
CMSMQQueueInfo::~CMSMQQueueInfo ()
{
    // TODO: clean up anything here.
    SysFreeString(m_bstrFormatName);
    SysFreeString(m_bstrPathName);
    SysFreeString(m_bstrLabel);
    delete m_pguidQueue;
    delete m_pguidServiceType;
}

//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::InternalQueryInterface
//=--------------------------------------------------------------------------=
// the controlling unknown will call this for us in the case where they're
// looking for a specific interface.
//
// Parameters:
//    REFIID        - [in]  interface they want
//    void **       - [out] where they want to put the resulting object ptr.
//
// Output:
//    HRESULT       - S_OK, E_NOINTERFACE
//
// Notes:
//
HRESULT CMSMQQueueInfo::InternalQueryInterface
(
    REFIID riid,
    void **ppvObjOut
)
{
    CHECK_POINTER(ppvObjOut);

    // we support IMSMQQueueInfo and ISupportErrorInfo
    //
    if (DO_GUIDS_MATCH(riid, IID_IMSMQQueueInfo)) {
        *ppvObjOut = (void *)(IMSMQQueueInfo *)this;
        AddRef();
        return S_OK;
    } else if (DO_GUIDS_MATCH(riid, IID_ISupportErrorInfo)) {
        *ppvObjOut = (void *)(ISupportErrorInfo *)this;
        AddRef();
        return S_OK;
    }

    // call the super-class version and see if it can oblige.
    //
    return CAutomationObject::InternalQueryInterface(riid, ppvObjOut);
}



// TODO: implement your interface methods and property exchange functions
//       here.


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::SetProperty
//=--------------------------------------------------------------------------=
// Helper: to set a single Falcon property when ActiveX
//  property has been updated.
//
// Parameters:
//    queuepropid
//
// Output:
//    HRESULT       - S_OK, E_NOINTERFACE
//
// Notes:
//    NOP if no format name yet.
//
#if 0
HRESULT CMSMQQueueInfo::SetProperty(QUEUEPROPID queuepropid) 
{
    MQQUEUEPROPS queueprops;
    HRESULT hresult = NOERROR;

    // Set queue prop if we have a formatname,
    //  if not, means they haven't created or opened queue yet.
    //
    if (m_bstrFormatName) {
      IfFailRet(CreateQueueProps(
                 TRUE,
                 1, 
                 &queueprops, 
                 FALSE,                  // isTransactional
                 queuepropid));
      hresult = MQSetQueueProperties(m_bstrFormatName, &queueprops);
      //
      // we ignore MQ_ERROR_QUEUE_NOT_FOUND because it might never
      //  have been opened/created yet though the formatname has
      //  been set...
      // REVIEW: is this dangerous?
      //
      if (hresult == MQ_ERROR_QUEUE_NOT_FOUND) {
        hresult = NOERROR;
      }
      FreeQueueProps(&queueprops);
    };
    return hresult;
}
#endif // 0

//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::get_QueueGuid
//=--------------------------------------------------------------------------=
//
// Parameters:
//    pbstrGuidQueue  [out] this queue's guid
//
// Output:
//    HRESULT       - S_OK, E_NOINTERFACE
//
// Notes:
//
HRESULT CMSMQQueueInfo::get_QueueGuid(BSTR *pbstrGuidQueue) 
{ 
    int cbStr;
    
    InitProps();
    *pbstrGuidQueue = SysAllocStringLen(NULL, LENSTRCLSID);
    if (*pbstrGuidQueue) {
      cbStr = StringFromGUID2(*m_pguidQueue, *pbstrGuidQueue, LENSTRCLSID*2);
#if DEBUG
      RemBstrNode(*pbstrGuidQueue);
#endif // DEBUG
      return cbStr == 0 ? E_OUTOFMEMORY : NOERROR;
    }
    else {
      return E_OUTOFMEMORY;
    }
}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::get_ServiceTypeGuid
//=--------------------------------------------------------------------------=
//
// Parameters:
//    pbstrGuidQueue  [out] this queue's service type guid
//
// Output:
//    HRESULT       - S_OK
//
// Notes:
//
HRESULT CMSMQQueueInfo::get_ServiceTypeGuid(BSTR *pbstrGuidServiceType)
{ 
    int cbStr;

    InitProps();
    *pbstrGuidServiceType = SysAllocStringLen(NULL, LENSTRCLSID);
    if (*pbstrGuidServiceType) {
      cbStr = StringFromGUID2(*m_pguidServiceType, *pbstrGuidServiceType, LENSTRCLSID*2);
#if DEBUG
      RemBstrNode(*pbstrGuidServiceType);
#endif // DEBUG
      return cbStr == 0 ? E_OUTOFMEMORY : NOERROR;
    }
    else {
      return E_OUTOFMEMORY;
    }
}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::PutServiceType
//=--------------------------------------------------------------------------=
//
// Parameters:
//    bstrGuidServiceType  [in]  this queue's guid
//    pguidServiceType     [out] where to put it
//
// Output:
//    HRESULT       - S_OK, E_OUTOFMEMORY
//
// Notes:
//

#if 0 // no longer used
HRESULT CMSMQQueueInfo::PutServiceType(
    BSTR bstrGuidServiceType,
    GUID *pguidServiceType) 
{
    BSTR bstrTemp;
    HRESULT hresult; 

    IfNullRet(bstrTemp = SYSALLOCSTRING(bstrGuidServiceType));
    hresult = CLSIDFromString(bstrTemp, pguidServiceType);
    if (FAILED(hresult)) {
      // 1194: map OLE error to Falcon
      hresult = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
    }

    // fall through...
    SysFreeString(bstrTemp);
    return CreateErrorHelper(hresult, m_ObjectType);
}
#endif // 0

//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::put_ServiceTypeGuid
//=--------------------------------------------------------------------------=
//
// Parameters:
//    bstrGuidServiceType [in] this queue's guid
//
// Output:
//    HRESULT       - S_OK, E_OUTOFMEMORY
//
// Notes:
//
HRESULT CMSMQQueueInfo::put_ServiceTypeGuid(BSTR bstrGuidServiceType) 
{

	BSTR bstrTemp;
    HRESULT hresult; 

    IfNullRet(bstrTemp = SYSALLOCSTRING(bstrGuidServiceType));

    EnterCriticalSection(&m_CSlocal);
    hresult = CLSIDFromString(bstrTemp, m_pguidServiceType);
    LeaveCriticalSection(&m_CSlocal);
    if (FAILED(hresult)) {
      // 1194: map OLE error to Falcon
      hresult = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
    }

    // fall through...
    SysFreeString(bstrTemp);
    return CreateErrorHelper(hresult, m_ObjectType);

}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::get_Label
//=--------------------------------------------------------------------------=
//
// Parameters:
//    pbstrLabel  [in] this queue's label
//
// Output:
//    HRESULT       - S_OK
//
// Notes:
//
HRESULT CMSMQQueueInfo::get_Label(BSTR *pbstrLabel)
{ 
    InitProps();
    //
    // Copy our member variable and return ownership of
    //  the copy to the caller.
    //
    IfNullRet(*pbstrLabel = SYSALLOCSTRING(m_bstrLabel));
#if DEBUG
    RemBstrNode(*pbstrLabel);
#endif // DEBUG
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::PutLabel
//=--------------------------------------------------------------------------=
//
// Parameters:
//    bstrLabel  [in] this queue's label
//    pbstrLabel [in] where to put it
//
// Output:
//    HRESULT       - S_OK, E_OUTOFMEMORY
//
// Notes:
//

#if 0 // no longer used
HRESULT CMSMQQueueInfo::PutLabel(
    BSTR bstrLabel,
    BSTR *pbstrLabel) 
{
    SysFreeString(*pbstrLabel);
    IfNullRet(*pbstrLabel = SYSALLOCSTRING(bstrLabel));
    return NOERROR;
}
#endif // 0

//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::put_Label
//=--------------------------------------------------------------------------=
//
// Parameters:
//    bstrLabel   [in] this queue's label
//
// Output:
//    HRESULT       - S_OK, E_OUTOFMEMORY
//
// Notes:
//
HRESULT CMSMQQueueInfo::put_Label(BSTR bstrLabel)
{
	HRESULT hr = NOERROR;

	EnterCriticalSection(&m_CSlocal);
	SysFreeString(m_bstrLabel);
    m_bstrLabel = SYSALLOCSTRING(bstrLabel);
	LeaveCriticalSection(&m_CSlocal);

    if(m_bstrLabel == NULL)
    	hr = E_OUTOFMEMORY;
    
    return hr;

}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::get_PathName
//=--------------------------------------------------------------------------=
//
// Parameters:
//    pbstrPathName [in] this queue's pathname
//
// Output:
//    HRESULT       - S_OK
//
// Notes:
//
HRESULT CMSMQQueueInfo::get_PathName(BSTR *pbstrPathName)
{ 
    InitProps();
    IfNullRet(*pbstrPathName = SYSALLOCSTRING(m_bstrPathName));
#if DEBUG
    RemBstrNode(*pbstrPathName);
#endif // DEBUG
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::PutPathName
//=--------------------------------------------------------------------------=
//
// Parameters:
//    bstrPathName  [in] this queue's PathName
//    pbstrPathName [in] where to put it
//
// Output:
//    HRESULT       - S_OK, E_OUTOFMEMORY
//
// Notes:
//
#if 0 // no longer used

HRESULT CMSMQQueueInfo::PutPathName(
    BSTR bstrPathName,
    BSTR *pbstrPathName) 
{
    SysFreeString(*pbstrPathName);
    IfNullRet(*pbstrPathName = SYSALLOCSTRING(bstrPathName));
    return NOERROR;
}
#endif // 0

//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::put_PathName
//=--------------------------------------------------------------------------=
//
// Parameters:
//    bstrPathName  [in] this queue's pathname
//
// Output:
//    HRESULT       - S_OK, E_OUTOFMEMORY
//
// Notes:
//
HRESULT CMSMQQueueInfo::put_PathName(BSTR bstrPathName)
{

	HRESULT hresult = NOERROR;
    //
    // note: we don't update the Falcon property --
    //  the user must explicitly create/open to
    //  use this new pathname.
    //
    EnterCriticalSection(&m_CSlocal);
	SysFreeString(m_bstrPathName);
    m_bstrPathName = SYSALLOCSTRING(bstrPathName);
    if(m_bstrPathName == NULL)
    	hresult = E_OUTOFMEMORY;
    else
    	m_isValidFormatName = FALSE;
    //
    // formatname is now invalid
    //
   	LeaveCriticalSection(&m_CSlocal);
    return hresult;
    
}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::get_FormatName
//=--------------------------------------------------------------------------=
//
// Parameters:
//    pbstrFormatName [out] this queue's format name
//
// Output:
//    HRESULT       - S_OK
//
// Notes:
//
HRESULT CMSMQQueueInfo::get_FormatName(BSTR *pbstrFormatName)
{ 
    //
    // UNDONE: if the formatname is no longer valid, shouldn't
    //  we refresh it immediately via MQPathNameToFormatName
    //  since otherwise we're returning something out of date
    //  to the user?
    //
    IfNullRet(*pbstrFormatName = SYSALLOCSTRING(m_bstrFormatName));
#if DEBUG
    RemBstrNode(*pbstrFormatName);
#endif // DEBUG
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::PutFormatName
//=--------------------------------------------------------------------------=
//
// Parameters:
//    bstrFormatName  [in] this queue's FormatName
//    pbstrFormatName [in] where to put it
//
// Output:
//    HRESULT       - S_OK, E_OUTOFMEMORY
//
// Notes:
//
#if 0 // no longer used
HRESULT CMSMQQueueInfo::PutFormatName(
    BSTR bstrFormatName,
    BSTR *pbstrFormatName) 
{
    SysFreeString(*pbstrFormatName);
    IfNullRet(*pbstrFormatName = SYSALLOCSTRING(bstrFormatName));
    return NOERROR;
}
#endif

//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::put_FormatName
//=--------------------------------------------------------------------------=
//
// Parameters:
//    bstrFormatName  [in] this queue's formatname
//
// Output:
//    HRESULT       - S_OK, E_OUTOFMEMORY
//
// Notes:
//
HRESULT CMSMQQueueInfo::put_FormatName(BSTR bstrFormatName)
{
    HRESULT hresult = NOERROR;
    //
    // note: we don't update the Falcon property --
    //  the user must explicitly create/open to
    //  use this new pathname.
    //
    EnterCriticalSection(&m_CSlocal);
	SysFreeString(m_bstrFormatName);
    m_bstrFormatName = SYSALLOCSTRING(bstrFormatName);
    if(m_bstrFormatName == NULL)
    	hresult = E_OUTOFMEMORY;
    else
    	m_isValidFormatName = TRUE;
    //
    // formatname is now valid
    //
   	LeaveCriticalSection(&m_CSlocal);
    return hresult;
}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::get_IsTransactional
//=--------------------------------------------------------------------------=
//
// Parameters:
//    pisTransactional  [out]
//
// Output:
//
// Notes:
//
HRESULT CMSMQQueueInfo::get_IsTransactional(VARIANT_BOOL *pisTransactional) 
{ 
    InitProps();
    *pisTransactional = m_isTransactional;
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::PutPrivLevel
//=--------------------------------------------------------------------------=
//
// Parameters:
//    plPrivLevel    [out]
//    lPrivLevel     [in]
//
// Output:
//
// Notes:
//

#if 0 // no longer used
HRESULT CMSMQQueueInfo::PutPrivLevel(
    long lPrivLevel,
    long *plPrivLevel)
{
    switch (lPrivLevel) {
    case MQ_PRIV_LEVEL_NONE:
    case MQ_PRIV_LEVEL_OPTIONAL:
    case MQ_PRIV_LEVEL_BODY:
      *plPrivLevel = lPrivLevel;
      return NOERROR;
    default:
      return CreateErrorHelper(
               MQ_ERROR_ILLEGAL_PROPERTY_VALUE,
               m_ObjectType);
    }
}
#endif // 0

//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::get/put_PrivLevel
//=--------------------------------------------------------------------------=
//
// Parameters:
//    plPrivLevel    [out]
//    lPrivLevel     [in]
//
// Output:
//
// Notes:
//
HRESULT CMSMQQueueInfo::get_PrivLevel(long *plPrivLevel)
{
    InitProps();
    *plPrivLevel = m_lPrivLevel;
    return NOERROR;
}


HRESULT CMSMQQueueInfo::put_PrivLevel(long lPrivLevel)
{
	 switch (lPrivLevel) {
    case MQ_PRIV_LEVEL_NONE:
    case MQ_PRIV_LEVEL_OPTIONAL:
    case MQ_PRIV_LEVEL_BODY:
   	EnterCriticalSection(&m_CSlocal);
      m_lPrivLevel = lPrivLevel;
    LeaveCriticalSection(&m_CSlocal);
      return NOERROR;
    default:
      return CreateErrorHelper(
               MQ_ERROR_ILLEGAL_PROPERTY_VALUE,
               m_ObjectType);
    }

}

//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::PutJournal
//=--------------------------------------------------------------------------=
//
// Parameters:
//    plJournal [out]
//    lJournal  [in]
//
// Output:
//
// Notes:
//

#if 0 // no longer used
HRESULT CMSMQQueueInfo::PutJournal(
    long lJournal, 
    long *plJournal)
{
    switch (lJournal) {
    case MQ_JOURNAL_NONE:
    case MQ_JOURNAL:
      *plJournal = lJournal;
      return NOERROR;
    default:
      return CreateErrorHelper(
               MQ_ERROR_ILLEGAL_PROPERTY_VALUE,
               m_ObjectType);
    }
}
#endif

//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::get/put_Journal
//=--------------------------------------------------------------------------=
//
// Parameters:
//    plJournal      [out]
//    lJournal       [in]
//
// Output:
//
// Notes:
//
HRESULT CMSMQQueueInfo::get_Journal(long *plJournal)
{
    InitProps();
    *plJournal = m_lJournal;
    return NOERROR;
}


HRESULT CMSMQQueueInfo::put_Journal(long lJournal)
{

	switch (lJournal) {
    case MQ_JOURNAL_NONE:
    case MQ_JOURNAL:
    EnterCriticalSection(&m_CSlocal);
      m_lJournal = lJournal;
    LeaveCriticalSection(&m_CSlocal);
      return NOERROR;
    default:
      return CreateErrorHelper(
               MQ_ERROR_ILLEGAL_PROPERTY_VALUE,
               m_ObjectType);
    }
    
}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::PutQuota
//=--------------------------------------------------------------------------=
//
// Parameters:
//    plQuota [out]
//    lQuota  [in]
//
// Output:
//
// Notes:
//

#if 0 // no longer used
inline HRESULT CMSMQQueueInfo::PutQuota(long lQuota, long *plQuota)
{
    // UNDONE: validate...
    *plQuota = lQuota;
    return NOERROR;
}
#endif // 0

//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::get/put_quota
//=--------------------------------------------------------------------------=
//
// Parameters:
//    plQuota       [out]
//    lQuota        [in]
//
// Output:
//
// Notes:
//
HRESULT CMSMQQueueInfo::get_Quota(long *plQuota)
{
    InitProps();
    *plQuota = m_lQuota;
    return NOERROR;
}


HRESULT CMSMQQueueInfo::put_Quota(long lQuota)
{
	EnterCriticalSection(&m_CSlocal);
	m_lQuota = lQuota;
	LeaveCriticalSection(&m_CSlocal);

    return NOERROR;
    
}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::PutBasePriority
//=--------------------------------------------------------------------------=
//
// Parameters:
//    plBasePriority [out]
//    lBasePriority  [in]
//
// Output:
//
// Notes:
//
#if 0 // no longer used
inline HRESULT CMSMQQueueInfo::PutBasePriority(
    long lBasePriority, 
    long *plBasePriority)
{
    if ((lBasePriority >= (long)SHRT_MIN) &&
        (lBasePriority <= (long)SHRT_MAX)) {
      *plBasePriority = lBasePriority;
      return NOERROR;
    }
    else {
      return CreateErrorHelper(
               MQ_ERROR_ILLEGAL_PROPERTY_VALUE,
               m_ObjectType);
    }
}
#endif // 0

//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::get/put_BasePriority
//=--------------------------------------------------------------------------=
//
// Parameters:
//    plBasePriority       [out]
//    lBasePriority        [in]
//
// Output:
//
// Notes:
//
HRESULT CMSMQQueueInfo::get_BasePriority(long *plBasePriority)
{
    InitProps();
    *plBasePriority = m_lBasePriority;
    return NOERROR;
}


HRESULT CMSMQQueueInfo::put_BasePriority(long lBasePriority)
{

	if ((lBasePriority >= (long)SHRT_MIN) &&
        (lBasePriority <= (long)SHRT_MAX)) {
      EnterCriticalSection(&m_CSlocal);
      m_lBasePriority = lBasePriority;
      LeaveCriticalSection(&m_CSlocal);
      return NOERROR;
    }
    else {
      return CreateErrorHelper(
               MQ_ERROR_ILLEGAL_PROPERTY_VALUE,
               m_ObjectType);
    }

   
}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::get_CreateTime/dateModifyTime
//=--------------------------------------------------------------------------=
//
// Parameters:
//    pvarCreateTime          [out]
//    pvarModifyTime          [out]
//
// Output:
//
// Notes:
//
HRESULT CMSMQQueueInfo::get_CreateTime(VARIANT *pvarCreateTime)
{
    InitProps();
    return GetVariantTimeOfTime(m_lCreateTime, pvarCreateTime);
}


HRESULT CMSMQQueueInfo::get_ModifyTime(VARIANT *pvarModifyTime)
{
    InitProps();
    return GetVariantTimeOfTime(m_lModifyTime, pvarModifyTime);
}


#if 0
//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::get_strCreateTime/strModifyTime
//=--------------------------------------------------------------------------=
//
// Parameters:
//    pbstrCreateTime          [out]
//    pbstrModifyTime          [out]
//
// Output:
//
// Notes:
//
HRESULT CMSMQQueueInfo::get_strCreateTime(BSTR *pbstrCreateTime)
{
    //
    // 1271: uninit'ed time will return the empty string
    //
    IfNullRet(*pbstrCreateTime =
                m_lCreateTime ? 
                  BstrOfTime(m_lCreateTime) : 
                  SysAllocString(L""));
#if DEBUG
    RemBstrNode(*pbstrCreateTime);
#endif // DEBUG
    return NOERROR;
}

HRESULT CMSMQQueueInfo::get_strModifyTime(BSTR *pbstrModifyTime)
{
    IfNullRet(*pbstrModifyTime =
                m_lModifyTime ? 
                  BstrOfTime(m_lModifyTime) : 
                  SysAllocString(L""));
#if DEBUG
    RemBstrNode(*pbstrModifyTime);
#endif // DEBUG
    return NOERROR;
}
#endif // 0

//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::PutAuthenticate
//=--------------------------------------------------------------------------=
//
// Parameters:
//    plAuthenticate [out]
//    lAuthenticate  [in]
//
// Output:
//
// Notes:
//
#if 0 // no longer used
inline HRESULT CMSMQQueueInfo::PutAuthenticate(
    long lAuthenticate, 
    long *plAuthenticate)
{
    switch (lAuthenticate) {
    case MQ_AUTHENTICATE_NONE:
    case MQ_AUTHENTICATE:
      *plAuthenticate = lAuthenticate;
      return NOERROR;
    default:
      return CreateErrorHelper(
               MQ_ERROR_ILLEGAL_PROPERTY_VALUE,
               m_ObjectType);
    }
}
#endif // 0

//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::get/put_Authenticate
//=--------------------------------------------------------------------------=
//
// Parameters:
//    plAuthenticate         [out]
//    lAuthenticate          [in]
//
// Output:
//
// Notes:
//
HRESULT CMSMQQueueInfo::get_Authenticate(long *plAuthenticate)
{
    InitProps();
    *plAuthenticate = m_lAuthenticate;
    return NOERROR;
}


HRESULT CMSMQQueueInfo::put_Authenticate(long lAuthenticate)
{

	switch (lAuthenticate) {
    case MQ_AUTHENTICATE_NONE:
    case MQ_AUTHENTICATE:
    EnterCriticalSection(&m_CSlocal);
      m_lAuthenticate = lAuthenticate;
    LeaveCriticalSection(&m_CSlocal);
      return NOERROR;
    default:
      return CreateErrorHelper(
               MQ_ERROR_ILLEGAL_PROPERTY_VALUE,
               m_ObjectType);
    }
    
}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::get/put_JournalQuota
//=--------------------------------------------------------------------------=
//
// Parameters:
//    plJournalQuota         [out]
//    lJournalQuota          [in]
//
// Output:
//
// Notes:
//
HRESULT CMSMQQueueInfo::get_JournalQuota(long *plJournalQuota)
{
    InitProps();
    *plJournalQuota = m_lJournalQuota;
    return NOERROR;
}


HRESULT CMSMQQueueInfo::put_JournalQuota(long lJournalQuota)
{
	EnterCriticalSection(&m_CSlocal);
    m_lJournalQuota = lJournalQuota;
    LeaveCriticalSection(&m_CSlocal);
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::CreateQueueProps
//=--------------------------------------------------------------------------=
// Creates and updates an MQQUEUEPROPS struct.  Fills in struct with
//  this queue's properties.  Specifically: label, pathname, guid, servicetype.
//
// Parameters:
//  fUpdate           TRUE if they want to update struct with
//                     current datamembers
//  cProp
//  pqueueprops
//  isTransactional   TRUE if they want a transacted queue.
//  varlist of PROPIDs.
//
// Output:
//    HRESULT       - S_OK, E_OUTOFMEMORY
//
// Notes:
// uses variable length list of PROPID params
//
HRESULT CMSMQQueueInfo::CreateQueueProps(
    BOOL fUpdate,
    UINT cProp, 
    MQQUEUEPROPS *pqueueprops, 
    BOOL isTransactional,
    ...)
{
    UINT i;
    va_list marker;
    QUEUEPROPID queuepropid;
    HRESULT hresult = NOERROR;

    IfNullRet(pqueueprops->aPropID = new QUEUEPROPID[cProp]);
    IfNullFail(pqueueprops->aStatus = new HRESULT[cProp]);
    pqueueprops->aPropVar = new MQPROPVARIANT[cProp];
    if (pqueueprops->aPropVar == NULL) {
      IfFailGoTo(hresult = E_OUTOFMEMORY, Error2);
    }
    pqueueprops->cProp = cProp;

    // process variable list of PROPID params
    va_start(marker, isTransactional);
    for (i = 0; i < cProp; i++) {
      queuepropid = va_arg(marker, QUEUEPROPID);    
      pqueueprops->aPropID[i] = queuepropid;
      switch (queuepropid) {
      case PROPID_Q_INSTANCE:
        // This is an OUT param so we just use the current member 
        //  as a buffer.
        //
        ASSERT(m_pguidQueue, L"should always be non-null");
        if (fUpdate) {
          pqueueprops->aPropVar[i].vt = VT_CLSID;
          pqueueprops->aPropVar[i].puuid = m_pguidQueue;
        }
        else {
          pqueueprops->aPropVar[i].puuid = NULL;  
          pqueueprops->aPropVar[i].vt = VT_NULL;  
        }
        break;
      case PROPID_Q_TYPE:
        ASSERT(m_pguidServiceType, L"should always be non-null");
        if (fUpdate) {
          pqueueprops->aPropVar[i].vt = VT_CLSID;
          pqueueprops->aPropVar[i].puuid = m_pguidServiceType;
        }
        else {
          pqueueprops->aPropVar[i].puuid = NULL;  
          pqueueprops->aPropVar[i].vt = VT_NULL;
        }
        break;
      case PROPID_Q_LABEL:
        ASSERT(m_bstrLabel, L"should always be non-null");
        if (fUpdate) {
          pqueueprops->aPropVar[i].vt = VT_LPWSTR;
          pqueueprops->aPropVar[i].pwszVal = m_bstrLabel;
        }
        else {
          pqueueprops->aPropVar[i].pwszVal = NULL;  
          pqueueprops->aPropVar[i].vt = VT_NULL;
        }
        break;
      case PROPID_Q_PATHNAME:
        pqueueprops->aPropVar[i].vt = VT_LPWSTR;
        if (fUpdate) {
          if (m_bstrPathName) {
            pqueueprops->aPropVar[i].pwszVal = m_bstrPathName;
          }
          else {
            // no default: param is mandatory
            hresult = ERROR_INVALID_PARAMETER;
          }
        }
        else {
          pqueueprops->aPropVar[i].pwszVal = NULL;  
          pqueueprops->aPropVar[i].vt = VT_NULL;
        }
        break;
      case PROPID_Q_JOURNAL:
        pqueueprops->aPropVar[i].vt = VT_UI1;
        if (fUpdate) pqueueprops->aPropVar[i].bVal = (UCHAR)m_lJournal;
        break;
      case PROPID_Q_QUOTA:
        pqueueprops->aPropVar[i].vt = VT_UI4;
        if (fUpdate) pqueueprops->aPropVar[i].lVal = (ULONG)m_lQuota;
        break;
      case PROPID_Q_BASEPRIORITY:
        pqueueprops->aPropVar[i].vt = VT_I2;
        if (fUpdate) pqueueprops->aPropVar[i].iVal = (SHORT)m_lBasePriority;
        break;
      case PROPID_Q_PRIV_LEVEL:
        pqueueprops->aPropVar[i].vt = VT_UI4;
        if (fUpdate) pqueueprops->aPropVar[i].lVal = m_lPrivLevel;
        break;
      case PROPID_Q_AUTHENTICATE:
        pqueueprops->aPropVar[i].vt = VT_UI1;
        if (fUpdate) pqueueprops->aPropVar[i].bVal = (UCHAR)m_lAuthenticate;
        break;
      case PROPID_Q_TRANSACTION:
        pqueueprops->aPropVar[i].vt = VT_UI1;
        if (fUpdate) {
          pqueueprops->aPropVar[i].bVal = 
            (UCHAR)isTransactional ? MQ_TRANSACTIONAL : MQ_TRANSACTIONAL_NONE;
        }
        break;
      case PROPID_Q_CREATE_TIME:
        pqueueprops->aPropVar[i].vt = VT_I4;
        if (fUpdate) {
          // R/O
          // hresult = ERROR_INVALID_PARAMETER;
        }
        break;
      case PROPID_Q_MODIFY_TIME:
        pqueueprops->aPropVar[i].vt = VT_I4;
        if (fUpdate) {
          // R/O
          // hresult = ERROR_INVALID_PARAMETER;
        }
        break;
      case PROPID_Q_JOURNAL_QUOTA:
        pqueueprops->aPropVar[i].vt = VT_UI4;
        if (fUpdate) pqueueprops->aPropVar[i].lVal = m_lJournalQuota;
        break;
      default:
        ASSERT(0, L"unhandled queuepropid");
      } // switch
    } // for
    return hresult;

Error2:
    delete [] pqueueprops->aStatus;
    pqueueprops->aStatus = NULL;

Error:
    delete [] pqueueprops->aPropID;
    pqueueprops->aPropID = NULL;
    pqueueprops->cProp = 0;

    return hresult;
}


//=--------------------------------------------------------------------------=
// Helper: FreeFalconBuffers
//=--------------------------------------------------------------------------=
// Frees dynamic memory allocated by Falcon on behalf of an 
//  MQQUEUEPROPS struct.  
//
// Parameters:
//  pqueueprops
//
// Output:
//
// Notes:
//
void FreeFalconBuffers(MQQUEUEPROPS *pqueueprops)
{
    UINT cProp, i;
    QUEUEPROPID queuepropid;

    cProp = pqueueprops->cProp ;
    for (i = 0; i < cProp; i++) {
      queuepropid = pqueueprops->aPropID[i];
      switch (queuepropid) {
      case PROPID_Q_INSTANCE:
        MQFreeMemory(pqueueprops->aPropVar[i].puuid);
        break;
      case PROPID_Q_TYPE:
        MQFreeMemory(pqueueprops->aPropVar[i].puuid);
        break;
      case PROPID_Q_LABEL:
        MQFreeMemory(pqueueprops->aPropVar[i].pwszVal);
        break;
      case PROPID_Q_PATHNAME:
        MQFreeMemory(pqueueprops->aPropVar[i].pwszVal);
        break;
      } // switch
    } // for

}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::FreeQueueProps
//=--------------------------------------------------------------------------=
// Frees dynamic memory allocated on behalf of an 
//  MQQUEUEPROPS struct.  
//
// Parameters:
//  pRestriction
//
// Output:
//
// Notes:
//
void CMSMQQueueInfo::FreeQueueProps(MQQUEUEPROPS *pqueueprops)
{
    // Note: all properties are owned by the MSMQQueueInfo.
    delete [] pqueueprops->aPropID;
    delete [] pqueueprops->aPropVar;
    delete [] pqueueprops->aStatus;
    return;
}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::SetQueueProps
//=--------------------------------------------------------------------------=
// Sets queue's props from an MQQUEUEPROPS struct.  
//
// Parameters:
//  pqueueprops
//
// Output:
//    HRESULT       - S_OK, E_OUTOFMEMORY
//
// Notes:
//
HRESULT CMSMQQueueInfo::SetQueueProps(MQQUEUEPROPS *pqueueprops)
{
    UINT i;
    UINT cProp;
    QUEUEPROPID queuepropid;
    HRESULT hresult = NOERROR;


	EnterCriticalSection(&m_CSlocal);
    cProp = pqueueprops->cProp ;
    for (i = 0; i < cProp; i++) {
      queuepropid = pqueueprops->aPropID[i];
      
      // outcoming VT_NULL indicates that property was ignored
      //  so skip setting it...
      //
      if (pqueueprops->aPropVar[i].vt == VT_NULL) {
        continue;
      }
      switch (queuepropid) {
      case PROPID_Q_INSTANCE:
        *m_pguidQueue = *pqueueprops->aPropVar[i].puuid;
        break;
      case PROPID_Q_TYPE:
        *m_pguidServiceType = *pqueueprops->aPropVar[i].puuid;
        break;
      case PROPID_Q_LABEL:
        IfNullFail(SysReAllocString(&m_bstrLabel, 
                                    pqueueprops->aPropVar[i].pwszVal));
        break;
      case PROPID_Q_PATHNAME:
        IfNullFail(SysReAllocString(&m_bstrPathName, 
                                    pqueueprops->aPropVar[i].pwszVal));
        break;
      case PROPID_Q_JOURNAL:
        m_lJournal = (long)pqueueprops->aPropVar[i].bVal;
        break;
      case PROPID_Q_QUOTA:
        m_lQuota = pqueueprops->aPropVar[i].lVal;
        break;
      case PROPID_Q_BASEPRIORITY:
        m_lBasePriority = pqueueprops->aPropVar[i].iVal;
        break;
      case PROPID_Q_PRIV_LEVEL:
        m_lPrivLevel = (long)pqueueprops->aPropVar[i].lVal;
        break;
      case PROPID_Q_AUTHENTICATE:
        m_lAuthenticate = (long)pqueueprops->aPropVar[i].bVal;
        break;
      case PROPID_Q_CREATE_TIME:
        m_lCreateTime = pqueueprops->aPropVar[i].lVal;
        break;
      case PROPID_Q_MODIFY_TIME:
        m_lModifyTime = pqueueprops->aPropVar[i].lVal;
        break;
      case PROPID_Q_TRANSACTION:
        m_isTransactional = (BOOL)pqueueprops->aPropVar[i].bVal;
        break;
      case PROPID_Q_JOURNAL_QUOTA:
        m_lJournalQuota = pqueueprops->aPropVar[i].lVal;
        break;
      default:
        ASSERT(0, L"unhandled queuepropid");
      } // switch
    } // for
    // fall through...

Error:
	LeaveCriticalSection(&m_CSlocal);
    return hresult;
}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::Init
//=--------------------------------------------------------------------------=
// Inits a new queue based on instance params (guidQueue etc.)
//
// Parameters:
//  bstrFormatName
//
// Output:
//    HRESULT       - S_OK, E_OUTOFMEMORY
//
// Notes:
//
HRESULT CMSMQQueueInfo::Init(
    BSTR bstrFormatName)
{
#if DEBUG
    if (m_bstrFormatName || bstrFormatName) {
      ASSERT(m_bstrFormatName != bstrFormatName, L"bad strings.");
    }
#endif // DEBUG
    //
    // 2026: put_FormatName validates formatname
    //
    return put_FormatName(bstrFormatName);
}

//
// HELPER: GetCurrentUserSid
//
#if 0 // No Security
BOOL GetCurrentUserSid(PSID psid)
{
    BYTE rgbTokenUserInfo[128];
    DWORD cbBuf;
    HANDLE hToken = NULL;

    if (!OpenThreadToken(
          GetCurrentThread(),
          TOKEN_QUERY,
          TRUE,   // OpenAsSelf
          &hToken)) {
      if (GetLastError() != ERROR_NO_TOKEN) {
        return FALSE;
      }
      else {
        if (!OpenProcessToken(
              GetCurrentProcess(),
              TOKEN_QUERY,
              &hToken)) {
          return FALSE;
        }
      }
    }
    ASSERT(hToken, L"no current token!");
    if (!GetTokenInformation(
          hToken,
          TokenUser,
          (VOID *)rgbTokenUserInfo,
          sizeof(rgbTokenUserInfo),
          &cbBuf)) {
      //
      // UNDONE: realloc token buffer if not big enough...
      //
      return FALSE;
    }
    TOKEN_USER *ptokenuser = (TOKEN_USER *)rgbTokenUserInfo;
    PSID psidUser = ptokenuser->User.Sid;

    if (!CopySid(GetLengthSid(psidUser), psid, psidUser)) {
      return FALSE;
    }
    return TRUE;
}


//
// HELPER: GetWorldReadableSecurityDescriptor
//
BOOL GetWorldReadableSecurityDescriptor(
    SECURITY_DESCRIPTOR *psd,
    BYTE **ppbBufDacl)
{
    BYTE rgbBufSidUser[128];
    PSID psidWorld = NULL, psidUser = (PSID)rgbBufSidUser;
    PACL pDacl = NULL;
    SID_IDENTIFIER_AUTHORITY WorldAuth = SECURITY_WORLD_SID_AUTHORITY;
    DWORD dwAclSize;
    BOOL fRet = TRUE;   // optimism!

    ASSERT(ppbBufDacl, L"bad param!");
    *ppbBufDacl = NULL;
    IfNullGo(AllocateAndInitializeSid(
                    &WorldAuth,
                    1,
                    SECURITY_WORLD_RID,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    &psidWorld));
    IfNullGo(GetCurrentUserSid(psidUser));
    //
    // Calculate the required size for the new DACL and allocate it.
    //
    dwAclSize = sizeof(ACL) + 
                  2 * (sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD)) +
                  GetLengthSid(psidUser) + 
                  GetLengthSid(psidWorld);
    IfNullGo(pDacl = (PACL)new BYTE[dwAclSize]);
    //
    // Initialize the ACL.
    //
    IfNullGo(InitializeAcl(pDacl, dwAclSize, ACL_REVISION));
    //
    // Add the ACEs to the DACL.
    //
    IfNullGo(AddAccessAllowedAce(pDacl, ACL_REVISION, MQSEC_QUEUE_GENERIC_ALL, psidUser));
    IfNullGo(AddAccessAllowedAce(
          pDacl, 
          ACL_REVISION, 
          MQSEC_QUEUE_GENERIC_WRITE | MQSEC_QUEUE_GENERIC_READ, 
          psidWorld));
    //
    // Initialize the security descriptor.
    //
    IfNullGo(InitializeSecurityDescriptor(psd, SECURITY_DESCRIPTOR_REVISION));
    //
    // Set the security descriptor's DACL.
    //
    IfNullGo(SetSecurityDescriptorDacl(psd, TRUE, pDacl, FALSE));
    //
    // setup out DACL
    //
    *ppbBufDacl = (BYTE *)pDacl;
    goto Done;

Error:
    fRet = FALSE;
    delete [] (BYTE *)pDacl;
    //
    // All done...
    //
Done:
    if (psidWorld) {
      FreeSid(psidWorld);
    }

    return fRet;
}
#endif

//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::Create
//=--------------------------------------------------------------------------=
// Creates a new queue based on instance params (guidQueue etc.)
//
// Parameters:
//    pvarTransactional   [in, optional]
//    isWorldReadable     [in, optional]
//
// Output:
//    HRESULT       - S_OK, E_OUTOFMEMORY
//
// Notes:
//
HRESULT CMSMQQueueInfo::Create(
    VARIANT *pvarTransactional,
    VARIANT *pvarWorldReadable)
{
    MQQUEUEPROPS queueprops;
    ULONG uiFormatNameLen = FORMAT_NAME_INIT_BUFFER;
#if _DEBUG
    ULONG uiFormatNameLenSave = uiFormatNameLen;
#endif // _DEBUG
    BSTR bstrFormatName = NULL;
    BOOL isTransactional, isWorldReadable;
#if 0 // no security
  	SECURITY_DESCRIPTOR sd;
    SECURITY_DESCRIPTOR *psd;
#endif // 0
    BYTE *pbBufDacl = NULL;
    HRESULT hresult;


    isTransactional = GetBool(pvarTransactional);
    isWorldReadable = GetBool(pvarWorldReadable);
    InitQueueProps(&queueprops);  
    IfFailGo(CreateQueueProps(TRUE,
                               8,   // number of properties
                               &queueprops,
                               isTransactional,
                               PROPID_Q_TYPE, 
                               PROPID_Q_LABEL, 
                               PROPID_Q_PATHNAME,
                               PROPID_Q_JOURNAL,
                               PROPID_Q_QUOTA,
                               PROPID_Q_BASEPRIORITY,     
                               PROPID_Q_AUTHENTICATE,
                               PROPID_Q_JOURNAL_QUOTA));
    IfNullGo(bstrFormatName = 
      SysAllocStringLen(NULL, uiFormatNameLen));

#if 0  // no security
    if (isWorldReadable) {
      //
      // construct a security descriptor for this queue
      //  that is world generic readable.
      //
      if (!GetWorldReadableSecurityDescriptor(&sd, &pbBufDacl)) {
        IfFailGo(hresult = MQ_ERROR_ILLEGAL_SECURITY_DESCRIPTOR);
      }
      psd = &sd;
    }
    else {
      //
      // default Falcon security
      //
      psd = NULL;
    }
#endif // 0 

    IfFailGo(MQCreateQueue(
               NULL,
               &queueprops,
               bstrFormatName,
               &uiFormatNameLen));

    // FormatName mucking...
    ASSERT(hresult != MQ_INFORMATION_FORMATNAME_BUFFER_TOO_SMALL,
           L"Warning - insufficient format name buffer.");

#if _DEBUG
    ASSERT(uiFormatNameLen <= uiFormatNameLenSave,
           L"insufficient buffer.");
#endif

    IfNullFail(SysReAllocString(&m_bstrFormatName, bstrFormatName));

    //
    // Note: we don't SetQueueProps or Refresh since for now
    //  MQCreateQueue is IN-only.
    // need to update transactional field
    //
    EnterCriticalSection(&m_CSlocal);
    	m_isTransactional = isTransactional;
	LeaveCriticalSection(&m_CSlocal);
	
Error:
    delete [] pbBufDacl;
    FreeQueueProps(&queueprops);
    SysFreeString(bstrFormatName);
    return CreateErrorHelper(hresult, m_ObjectType);
}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::Delete
//=--------------------------------------------------------------------------=
// Deletes this queue 
//
// Parameters:
//
// Output:
//    HRESULT       - S_OK, E_OUTOFMEMORY
//
// Notes:
//
HRESULT CMSMQQueueInfo::Delete() 
{
    HRESULT hresult = NOERROR;
    //
    // 2026: ensure we have a format name...
    //

    hresult = UpdateFormatName();
    if (SUCCEEDED(hresult)) {
      hresult = MQDeleteQueue(m_bstrFormatName);
    }


    return CreateErrorHelper(hresult, m_ObjectType);
}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::Open
//=--------------------------------------------------------------------------=
// Opens this queue 
//
// Parameters:
//  lAccess       IN
//  lShareMode    IN
//  ppq           OUT
//
// Output:
//    HRESULT       - S_OK, E_OUTOFMEMORY
//
// Notes:
//
HRESULT CMSMQQueueInfo::Open(long lAccess, long lShareMode, IMSMQQueue **ppq)
{
    QUEUEHANDLE lHandle;
    CMSMQQueue *pq = NULL;
    HRESULT hresult;

    // pessimism
    *ppq = NULL;

    if (lAccess != MQ_SEND_ACCESS && 
        lAccess != MQ_RECEIVE_ACCESS && 
        lAccess != MQ_PEEK_ACCESS) {
      return CreateErrorHelper(E_INVALIDARG, m_ObjectType);
    }

    if (lShareMode != MQ_DENY_RECEIVE_SHARE &&
        lShareMode != 0 /* MQ_DENY_NONE */) {
      return CreateErrorHelper(E_INVALIDARG, m_ObjectType);
    }

    // ensure we have a format name...
    IfFailGo(UpdateFormatName());
    IfFailGo(MQOpenQueue(m_bstrFormatName,
                         lAccess, 
                         lShareMode,
                         (QUEUEHANDLE *)&lHandle));
    //
    // 2536: only attempt to use the DS when
    //  the first prop is accessed... or Refresh
    //  is called explicitly.
    //
#if 0
    // We ignore errors since perhaps no DS...
    hresult = Refresh();
#endif // 0

    // Create MSMQQueue object and init with handle
    IfNullFail((pq = new CMSMQQueue(NULL)));
    IfFailGo(pq->Init(this, lHandle, lAccess, lShareMode));
    *ppq = pq;
    return NOERROR;

Error:
    delete pq;

    return CreateErrorHelper(hresult, m_ObjectType);
}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::UpdateFormatName
//=--------------------------------------------------------------------------=
// Updates formatname member if necessary from pathname
//
// Parameters:
//
// Output:
//    HRESULT       - S_OK, E_OUTOFMEMORY
//
// Notes:
//    sets m_bstrFormatName
//
HRESULT CMSMQQueueInfo::UpdateFormatName()
{
    HRESULT hresult = NOERROR;

    // error if no pathname nor formatname yet..
    if ((m_bstrPathName == NULL) && (m_bstrFormatName == NULL)) {
      return E_INVALIDARG;
    }
    //
    // if no format name yet, synthesize from pathname
    // 2026: check for formatname validity.
    //
    EnterCriticalSection(&m_CSlocal);
    if (!m_isValidFormatName ||
        (m_bstrFormatName == NULL) || 
        SysStringLen(m_bstrFormatName) == 0) {
      IfFailRet(GetFormatNameOfPathName(
                  m_bstrPathName, 
                  &m_bstrFormatName));
      m_isValidFormatName = TRUE;
    };
    LeaveCriticalSection(&m_CSlocal);
    
    return hresult;
}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::Refresh
//=--------------------------------------------------------------------------=
// Refreshes all queue properties from DS.
//
// Parameters:
//
// Output:
//    HRESULT       - S_OK, E_OUTOFMEMORY
//
// Notes:
//
HRESULT CMSMQQueueInfo::Refresh()
{
    MQQUEUEPROPS queueprops;
    HRESULT hresult = NOERROR;

    InitQueueProps(&queueprops);  
    IfFailGo(UpdateFormatName());
    IfFailGo(CreateQueueProps(
                FALSE,
                11,       
                &queueprops,
                FALSE,                   // ignored
                PROPID_Q_INSTANCE,
                PROPID_Q_TYPE,
                PROPID_Q_LABEL,
                PROPID_Q_PATHNAME,
                PROPID_Q_JOURNAL,
                PROPID_Q_QUOTA,
                PROPID_Q_BASEPRIORITY,
             // PROPID_Q_PRIV_LEVEL,
                PROPID_Q_AUTHENTICATE,
             // PROPID_Q_TRANSACTION,
                PROPID_Q_CREATE_TIME,
                PROPID_Q_MODIFY_TIME,
                PROPID_Q_JOURNAL_QUOTA
                ));
    if (!IsDirectQueueOfFormatName(m_bstrFormatName)) {
      IfFailGo(MQGetQueueProperties(m_bstrFormatName, &queueprops));
      IfFailGoTo(SetQueueProps(&queueprops), Error2);
    }else
    {
		
		// fall "around"...since falcon didn't touch this memory...
    	goto Error;
   	}

Error2:
    FreeFalconBuffers(&queueprops);
    // fall through...

Error:
    FreeQueueProps(&queueprops);

    return CreateErrorHelper(hresult, m_ObjectType);
}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::Update
//=--------------------------------------------------------------------------=
// Updates queue properties in DS from ActiveX object.
//
// Parameters:
//
// Output:
//    HRESULT       - S_OK, E_OUTOFMEMORY
//
// Notes:
//
HRESULT CMSMQQueueInfo::Update()
{
    MQQUEUEPROPS queueprops;
    HRESULT hresult;
    // 
    // CONSIDER: NOP if no props have changed since last Refresh.  
    //  Better yet do this on a per-prop basis.
    //

    InitQueueProps(&queueprops);  
    IfFailGo(UpdateFormatName());
    if (IsPrivateQueueOfFormatName(m_bstrFormatName)) {
      IfFailGo(CreateQueueProps(
                  TRUE,
                  5, 
                  &queueprops,
                  m_isTransactional,
                  PROPID_Q_TYPE,
                  PROPID_Q_LABEL,
               // PROPID_Q_JOURNAL,
                  PROPID_Q_QUOTA,
               // PROPID_Q_BASEPRIORITY,
                  PROPID_Q_PRIV_LEVEL,
               // PROPID_Q_JOURNAL_QUOTA,
                  PROPID_Q_AUTHENTICATE
                  ));
    }
    else {
      IfFailGo(CreateQueueProps(
                  TRUE,
                  7, 
                  &queueprops,
                  m_isTransactional,
                  PROPID_Q_TYPE,
                  PROPID_Q_LABEL,
                  PROPID_Q_JOURNAL,
                  PROPID_Q_QUOTA,
                  PROPID_Q_BASEPRIORITY,
               // PROPID_Q_PRIV_LEVEL,
                  PROPID_Q_AUTHENTICATE,
                  PROPID_Q_JOURNAL_QUOTA
                  ));
    }
    IfFailGo(UpdateFormatName());
    
    // 1042: DIRECT queues aren't ds'able
    if (!IsDirectQueueOfFormatName(m_bstrFormatName)) {
      IfFailGo(MQSetQueueProperties(m_bstrFormatName, &queueprops));
    }

    // fall through...

Error:
    FreeQueueProps(&queueprops);

    return CreateErrorHelper(hresult, m_ObjectType);
}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfo::get_IsWorldReadable
//=--------------------------------------------------------------------------=
//
// Parameters:
//    pisWorldReadable [out]
//
// Output:
//
// Notes:
//    Can't world readable state since other users can
//     change queue's state dynamically.
//
HRESULT CMSMQQueueInfo::get_IsWorldReadable(
    VARIANT_BOOL *pisWorldReadable)
{
return E_NOTIMPL;
#if 0 // no security support
    PSECURITY_DESCRIPTOR psd = NULL;
    BYTE rgbBufSecurityDescriptor[256];
    BYTE *rgbBufSecurityDescriptor2 = NULL;
    DWORD cbBuf, cbBuf2;
    BOOL bDaclExists;
    PACL pDacl;
    BOOL bDefDacl;
    BOOL isWorldReadable = FALSE;
    PSID psidWorld = NULL;
    DWORD dwMaskGenericRead = MQSEC_QUEUE_GENERIC_READ;
    HRESULT hresult;

    // UNDONE: null format name? for now UpdateFormatName
    IfFailGo(UpdateFormatName());
    psd = (PSECURITY_DESCRIPTOR)rgbBufSecurityDescriptor;
    hresult = MQGetQueueSecurity(
                m_bstrFormatName,
                DACL_SECURITY_INFORMATION,
                psd,
                sizeof(rgbBufSecurityDescriptor),
                &cbBuf);
    if (FAILED(hresult)) {
      if (hresult != MQ_ERROR_SECURITY_DESCRIPTOR_TOO_SMALL) {
        return hresult;
      }
      IfNullRet(rgbBufSecurityDescriptor2 = new BYTE[cbBuf]);
      //
      // retry with large enough buffer
      //
      psd = (PSECURITY_DESCRIPTOR)rgbBufSecurityDescriptor2;
      IfFailGo(MQGetQueueSecurity(
                  m_bstrFormatName,
                  DACL_SECURITY_INFORMATION,
                  psd,
                  cbBuf,
                  &cbBuf2));
      ASSERT(cbBuf >= cbBuf2, L"bad buffer sizes!");
    }
    ASSERT(psd, L"should have security descriptor!");
    IfNullGo(GetSecurityDescriptorDacl(
              psd, 
              &bDaclExists, 
              &pDacl, 
              &bDefDacl));
    if (!bDaclExists || !pDacl) {
      isWorldReadable = TRUE;
    }
    else {
      //
      // Get the ACL's size information.
      //
      ACL_SIZE_INFORMATION DaclSizeInfo;
      IfNullGo(GetAclInformation(
                pDacl, 
                &DaclSizeInfo, 
                sizeof(ACL_SIZE_INFORMATION),
                AclSizeInformation));
      //
      // Traverse the ACEs looking for world ACEs
      //  
      SID_IDENTIFIER_AUTHORITY WorldAuth = SECURITY_WORLD_SID_AUTHORITY;
      IfNullGo(AllocateAndInitializeSid(
                      &WorldAuth,
                      1,
                      SECURITY_WORLD_RID,
                      0,
                      0,
                      0,
                      0,
                      0,
                      0,
                      0,
                      &psidWorld));
      DWORD i;
      BOOL fDone;
      for (i = 0, fDone = FALSE; 
           i < DaclSizeInfo.AceCount && !fDone;
           i++) {
        LPVOID pAce;
        //
        // Retrieve the ACE
        //
        IfNullGo(GetAce(pDacl, i, &pAce));
        ACCESS_ALLOWED_ACE *pAceStruct = (ACCESS_ALLOWED_ACE *)pAce;
        if (!EqualSid((PSID)&pAceStruct->SidStart, psidWorld)) {
          continue;
        }
        //
        // we've found another world
        //
        switch (pAceStruct->Header.AceType) {
        case ACCESS_ALLOWED_ACE_TYPE:
          dwMaskGenericRead &= ~pAceStruct->Mask;
          if (!dwMaskGenericRead) {
            isWorldReadable = TRUE;
            fDone = TRUE;
          }
          break;
        case ACCESS_DENIED_ACE_TYPE:
          if (pAceStruct->Mask | MQSEC_QUEUE_GENERIC_READ) {
            isWorldReadable = FALSE;
            fDone = TRUE;
          }
          break;
        default:
          continue;
        } // switch
      } // for
    }
    //
    // setup return
    //
    *pisWorldReadable = isWorldReadable;
    //
    // fall through...
    //
Error:
    delete [] rgbBufSecurityDescriptor2;
    if (psidWorld) {
      FreeSid(psidWorld);
    }

    return CreateErrorHelper(hresult, m_ObjectType);
#endif  // 0 no security support

}


//=--------------------------------------------------------------------------=
// InitProps
//=--------------------------------------------------------------------------=
// Init DS props if not already refreshed...
//
HRESULT CMSMQQueueInfo::InitProps()
{
    HRESULT hresult = NOERROR;

    EnterCriticalSection(&m_CSlocal);
    if (!m_isRefreshed) {
      hresult = Refresh();    // ignore DS errors...
      if (SUCCEEDED(hresult)) {
        m_isRefreshed = TRUE;
      }
    }
    LeaveCriticalSection(&m_CSlocal);
    return hresult;
}


