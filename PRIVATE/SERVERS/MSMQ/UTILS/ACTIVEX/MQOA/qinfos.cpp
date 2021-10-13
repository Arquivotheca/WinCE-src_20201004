//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//=--------------------------------------------------------------------------=
// MSMQQueueInfosObj.Cpp
//=--------------------------------------------------------------------------=
//
// the MSMQQueueInfos object
//
//

#include "IPServer.H"

#include "LocalObj.H"


#include "oautil.h"
#include "qinfo.h"
#include "qinfos.H"
#include "query.h"
#include "mq.h"

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
// CMSMQQueueInfos::Create
//=--------------------------------------------------------------------------=
// creates a new MSMQQueueInfos object.
//
// Parameters:
//    IUnknown *        - [in] controlling unkonwn
//
// Output:
//    IUnknown *        - new object.
//
// Notes:
//
IUnknown *CMSMQQueueInfos::Create
(
    IUnknown *pUnkOuter
)
{
    // make sure we return the private unknown so that we support aggegation
    // correctly!
    //
    CMSMQQueueInfos *pNew = new CMSMQQueueInfos(pUnkOuter);
    return pNew ? pNew->PrivateUnknown() : NULL;
}

//=--------------------------------------------------------------------------=
// CMSMQQueueInfos::CMSMQQueueInfos
//=--------------------------------------------------------------------------=
// create the object and initialize the refcount
//
// Parameters:
//    IUnknown *    - [in] controlling unknown
//
// Notes:
//
#pragma warning(disable:4355)  // using 'this' in constructor
CMSMQQueueInfos::CMSMQQueueInfos
(
    IUnknown *pUnkOuter
)
: CAutomationObject(pUnkOuter, OBJECT_TYPE_OBJMQQUEUEINFOS, (void *)this)
{

    // TODO: initialize anything here
    m_hEnum = INVALID_HANDLE_VALUE;
    m_bstrContext = NULL;
    m_pRestriction = NULL;
    m_pColumns = NULL;
    m_pSort = NULL;
}
#pragma warning(default:4355)  // using 'this' in constructor

//=--------------------------------------------------------------------------=
// CMSMQQueueInfos::CMSMQQueueInfos
//=--------------------------------------------------------------------------=
// "We all labour against our own cure, for death is the cure of all diseases"
//    - Sir Thomas Browne (1605 - 82)
//
// Notes:
//
CMSMQQueueInfos::~CMSMQQueueInfos ()
{

	// TODO: clean up anything here.
    SysFreeString(m_bstrContext);
    CMSMQQuery::FreeRestriction(m_pRestriction);
    delete m_pRestriction;
    CMSMQQuery::FreeColumnSet(m_pColumns);
    delete m_pColumns;
    delete m_pSort;
    if (m_hEnum != INVALID_HANDLE_VALUE) {
      
		if(MQLocateEnd(m_hEnum) != MQ_OK)
		{
			m_hEnum = INVALID_HANDLE_VALUE;  
		}
    }

}

//=--------------------------------------------------------------------------=
// CMSMQQueueInfos::InternalQueryInterface
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
HRESULT CMSMQQueueInfos::InternalQueryInterface
(
    REFIID riid,
    void **ppvObjOut
)
{
    CHECK_POINTER(ppvObjOut);

    // we support IMSMQQueueInfos and ISupportErrorInfo
    //
    if (DO_GUIDS_MATCH(riid, IID_IMSMQQueueInfos)) {
        *ppvObjOut = (void *)(IMSMQQueueInfos *)this;
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
// CMSMQQueueInfos::Reset
//=--------------------------------------------------------------------------=
// Resets collection to beginning.
//
// Parameters:
//
// Output:
//    HRESULT       - S_OK, E_NOINTERFACE
//
// Notes:
//
HRESULT CMSMQQueueInfos::Reset()
{

return E_NOTIMPL;
#if 0 // not implemented in CE

    HRESULT hresult = NOERROR;
    //
    // 2006: close current open enum if any
    //

    if (m_hEnum != INVALID_HANDLE_VALUE) {
      hresult = MQLocateEnd(m_hEnum);
      m_hEnum = INVALID_HANDLE_VALUE;
      IfFailGo(hresult);
    }
    hresult = MQLocateBegin(NULL,     // context
                            m_pRestriction,
                            m_pColumns, 
                            0,        // sort not used yet
                            &m_hEnum);
   
Error:

    return CreateErrorHelper(hresult, m_ObjectType);
#endif // 0 
}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfos::Init
//=--------------------------------------------------------------------------=
// Inits collection
//
// Parameters:
//    bstrContext     [in]
//    pRestriction    [in]
//    pColumns        [in]
//    pSort           [in]
//
// Output:
//    HRESULT       - S_OK, E_NOINTERFACE
//
// Notes:
//
HRESULT CMSMQQueueInfos::Init(
    BSTR bstrContext,
    MQRESTRICTION *pRestriction,
    MQCOLUMNSET *pColumns,
    MQSORTSET *pSort)
{
	
    m_bstrContext = bstrContext;
    m_pRestriction = pRestriction;
    m_pColumns = pColumns;
    m_pSort = pSort;
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQQueueInfos::Next
//=--------------------------------------------------------------------------=
// Returns next element in collection.
//
// Parameters:
//    ppqNext       - [out] where they want to put the resulting object ptr.
//                          NULL if end of list.
//
// Output:
//    HRESULT       - S_OK, E_NOINTERFACE
//
// Notes:
//
HRESULT CMSMQQueueInfos::Next(IMSMQQueueInfo **ppqinfoNext)
{

return E_NOTIMPL;
#if 0 // not implemented in CE

    ULONG cProps = 1;
    MQPROPVARIANT *rgPropVar;
    CMSMQQueueInfo *pqinfo = NULL;
    BSTR bstrPathName = NULL;
    BSTR bstrFormatName = NULL;
    BSTR bstrFormatName2 = NULL;
    ULONG uiFormatNameLen = FORMAT_NAME_INIT_BUFFER;
#if _DEBUG
    ULONG uiFormatNameLenSave = uiFormatNameLen;
#endif // _DEBUG
    HRESULT hresult = NOERROR;

    *ppqinfoNext = NULL;
    IfNullFail(rgPropVar = new MQPROPVARIANT[cProps]);
    if (m_hEnum == INVALID_HANDLE_VALUE) {
      IfFailGo(Reset());
    }  
    IfFailGo(MQLocateNext(m_hEnum, &cProps, rgPropVar));
    if (cProps == 0) {
      // EOL
      // 2006: do not close enum on EOL since this
      //  will cause the next Next to wraparound due
      //  to the rest of m_hEnum.
      //
      goto Error;
    }
    if (!(bstrPathName = SysAllocString(rgPropVar[0].pwszVal))) {
      IfFailGoTo(E_OUTOFMEMORY, Error2);
    }
    if (!(bstrFormatName = SysAllocStringLen(NULL, uiFormatNameLen))) {
      IfFailGoTo(E_OUTOFMEMORY, Error2);
    }
    IfFailGoTo(MQPathNameToFormatName(bstrPathName,
                                      bstrFormatName,
                                      &uiFormatNameLen),
               Error2);
#if _DEBUG
    ASSERT(uiFormatNameLen <= uiFormatNameLenSave,
           L"insufficient buffer.");
#endif

    if (!SysReAllocString(&bstrFormatName2, bstrFormatName)) {
      IfFailGoTo(E_OUTOFMEMORY, Error2);
    }
    if (!(pqinfo = new CMSMQQueueInfo(NULL))) {
      IfFailGoTo(E_OUTOFMEMORY, Error3);
    }

    // ownership transfers
    IfFailGoTo(pqinfo->Init(bstrFormatName2), Error4);
    IfFailGoTo(pqinfo->Refresh(), Error4);
    *ppqinfoNext = pqinfo;
    goto Error3;      // normal cleanup

Error4:
    RELEASE(pqinfo);
    //
    // fall through...
    //
Error3:
    SysFreeString(bstrFormatName2);
    //
    // fall through...
    //
Error2:
    // delete [] rgPropVar[0].pwszVal;
    MQFreeMemory(rgPropVar[0].pwszVal);

    //
    // fall through...
    //
Error:
    delete [] rgPropVar;
    SysFreeString(bstrFormatName);
    SysFreeString(bstrPathName);
    return CreateErrorHelper(hresult, m_ObjectType);
#endif

}


