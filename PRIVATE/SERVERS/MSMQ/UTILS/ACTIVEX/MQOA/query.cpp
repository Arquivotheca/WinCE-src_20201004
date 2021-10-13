//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//=--------------------------------------------------------------------------=
// MSMQQueryObj.Cpp
//=--------------------------------------------------------------------------=
//
// the MSMQQuery object
//
//
#include "IPServer.H"

#include "LocalObj.H"
#include "Query.H"

#include "limits.h"   // for UINT_MAX
//#include "mq.h"
//#include "lookupx.h"  // from mqadminx

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


// Helper: PrOfRel:
//  Map RELOPS enumerator to PR enumerator.
//
UINT PrOfRel(RELOPS rel)
{
    UINT uPr = UINT_MAX;

    // UNDONE: must as well be a hard-wired array...

    switch (rel) {
    case REL_NOP:
      // maps to default
      break;
    case REL_EQ:
      uPr = PREQ;
      break;
    case REL_NEQ:
      uPr = PRNE;
      break;
    case REL_LT:
      uPr = PRLT;
      break;
    case REL_GT:
      uPr = PRGT;
      break;
    case REL_LE:
      uPr = PRLE;
      break;
    case REL_GE:
      uPr = PRGE;
      break;
    default:
      ASSERT(0, L"bad enumerator.");
      break;
    } // switch
    return uPr;
}


// Helper: PrOfVariant
//  Maps VARIANT to PR enumerator.
//  Returns UINT_MAX if out of bounds or illegal.
//
UINT PrOfVar(VARIANT *pvarRel)
{
    UINT uPr = UINT_MAX;
    HRESULT hresult;

    // ensure we can coerce rel to UINT.
    hresult = VariantChangeType(pvarRel, 
                                pvarRel, 
                                0, 
                                VT_I4);
    if (SUCCEEDED(hresult)) {
      uPr = PrOfRel((RELOPS)pvarRel->lVal);
    }
    return uPr; // == UINT_MAX ? PREQ : uPr;
}


//=--------------------------------------------------------------------------=
// CMSMQQuery::Create
//=--------------------------------------------------------------------------=
// creates a new MSMQQuery object.
//
// Parameters:
//    IUnknown *        - [in] controlling unkonwn
//
// Output:
//    IUnknown *        - new object.
//
// Notes:
//
IUnknown *CMSMQQuery::Create
(
    IUnknown *pUnkOuter
)
{
    // make sure we return the private unknown so that we support aggegation
    // correctly!
    //
    CMSMQQuery *pNew = new CMSMQQuery(pUnkOuter);
    return pNew ? pNew->PrivateUnknown() : NULL;
}

//=--------------------------------------------------------------------------=
// CMSMQQuery::CMSMQQuery
//=--------------------------------------------------------------------------=
// create the object and initialize the refcount
//
// Parameters:
//    IUnknown *    - [in] controlling unknown
//
// Notes:
//
#pragma warning(disable:4355)  // using 'this' in constructor
CMSMQQuery::CMSMQQuery
(
    IUnknown *pUnkOuter
)
: CAutomationObject(pUnkOuter, OBJECT_TYPE_OBJMQQUERY, (void *)this)
{

    // TODO: initialize anything here
    // m_pguidServiceType = new GUID(GUID_NULL);
    // m_pguidQueue = new GUID(GUID_NULL);
    // m_bstrLabel = NULL;
}
#pragma warning(default:4355)  // using 'this' in constructor

//=--------------------------------------------------------------------------=
// CMSMQQuery::CMSMQQuery
//=--------------------------------------------------------------------------=
// "We all labour against our own cure, for death is the cure of all diseases"
//    - Sir Thomas Browne (1605 - 82)
//
// Notes:
//
CMSMQQuery::~CMSMQQuery ()
{
    // TODO: clean up anything here.
    // delete m_pguidServiceType;
    // delete m_pguidQueue;
    // SysFreeString(m_bstrLabel);
} 

//=--------------------------------------------------------------------------=
// CMSMQQuery::InternalQueryInterface
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
HRESULT CMSMQQuery::InternalQueryInterface
(
    REFIID riid,
    void **ppvObjOut
)
{
    CHECK_POINTER(ppvObjOut);

    // we support IMSMQQuery and ISupportErrorInfo
    //
    if (DO_GUIDS_MATCH(riid, IID_IMSMQQuery)) {
        *ppvObjOut = (void *)(IMSMQQuery *)this;
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


// helper: If this a valid non-NOP implicit or explicit REL?
BOOL IsValidRel(VARIANT *prel)
{
    // we return TRUE only if:
    //  a REL is supplied and it's not REL_NOP
    //   or a REL isn't supplied at all
    //  
    return ((prel->vt != VT_ERROR) && (PrOfVar(prel) != UINT_MAX)) ||
            (prel->vt == VT_ERROR);
}


//=--------------------------------------------------------------------------=
// CMSMQQuery::CreateRestriction
//=--------------------------------------------------------------------------=
//  Creates a restriction for MQLocateBegin.
//  NOTE: the hungarian lies here -- all params are formally
//   VARIANTs but we use their real underlying typetag.
//
// Parameters:
// [IN] varguidQueue 
// [IN] varGuidServiceType 
// [IN] strLabel 
// [IN] strPathName 
// [IN] relServiceType 
// [IN] relLabel 
// [OUT] prestriction
// [OUT] pcolumnset
//
// Output:
//    HRESULT       - S_OK, E_NOINTERFACE
//
// Notes:
//
HRESULT CMSMQQuery::CreateRestriction(
    VARIANT *pstrGuidQueue, 
    VARIANT *pstrGuidServiceType, 
    VARIANT *pstrLabel, 
    VARIANT *pdateCreateTime,
    VARIANT *pdateModifyTime,
    VARIANT *prelServiceType, 
    VARIANT *prelLabel, 
    VARIANT *prelCreateTime,
    VARIANT *prelModifyTime,
    MQRESTRICTION *prestriction,
    MQCOLUMNSET *pcolumnset)
{
return E_NOTIMPL;
#if 0 // not implemented in CE
    UINT cRestriction = 0, iProp, cCol, uPr;
    MQPROPERTYRESTRICTION *rgPropertyRestriction;
    BSTR bstrTemp = NULL;
    ULONG ulTime;
    CLSID *pguidQueue = NULL;
    CLSID *pguidServiceType = NULL;
    HRESULT hresult = NOERROR;

    IfNullRet(pguidQueue = new GUID(GUID_NULL));
    IfNullFail(pguidServiceType = new GUID(GUID_NULL));

    // Count optional params
    if (pstrGuidQueue->vt != VT_ERROR) {
      cRestriction++;
    }
    if (pstrGuidServiceType->vt != VT_ERROR) {

      // ignore if rel is NOP:
      if (IsValidRel(prelServiceType)) {
        cRestriction++;
      }
    }
    if (pstrLabel->vt != VT_ERROR) {

      // ignore if rel is NOP:
      if (IsValidRel(prelLabel)) {
        cRestriction++;
      }
    }
    if (pdateCreateTime->vt != VT_ERROR) {

      // ignore if rel is NOP:
      if (IsValidRel(prelCreateTime)) {
        cRestriction++;
      }
    }
    if (pdateModifyTime->vt != VT_ERROR) {

      // ignore if rel is NOP:
      if (IsValidRel(prelModifyTime)) {
        cRestriction++;
      }
    }
    rgPropertyRestriction = new MQPROPERTYRESTRICTION[cRestriction];

    // setup OUT param
    prestriction->cRes = cRestriction;
    prestriction->paPropRes = rgPropertyRestriction;

    // populate...
    iProp = 0;
    if (pstrGuidQueue->vt != VT_ERROR) {
      rgPropertyRestriction[iProp].prval.vt = VT_ERROR;
      if ((bstrTemp = GetBstr(pstrGuidQueue)) == NULL) {
        IfFailRet(hresult = E_INVALIDARG);
      }
      IfFailGo(CLSIDFromString(bstrTemp, pguidQueue));
      IfNullFail(rgPropertyRestriction[iProp].prval.puuid = 
        new CLSID(*pguidQueue));
      rgPropertyRestriction[iProp].rel = PREQ;
      rgPropertyRestriction[iProp].prop = PROPID_Q_INSTANCE;
      rgPropertyRestriction[iProp].prval.vt = VT_CLSID;
      iProp++;
    }
    if (pstrGuidServiceType->vt != VT_ERROR) {
      rgPropertyRestriction[iProp].prval.vt = VT_ERROR;
      if ((bstrTemp = GetBstr(pstrGuidServiceType)) == NULL) {
        IfFailGo(hresult = E_INVALIDARG);
      }
      if (IsValidRel(prelServiceType)) {
        IfFailGo(CLSIDFromString(bstrTemp, pguidServiceType));
        IfNullFail(rgPropertyRestriction[iProp].prval.puuid = 
          new CLSID(*pguidServiceType));
        uPr = PrOfVar(prelServiceType);
        rgPropertyRestriction[iProp].rel = 
          uPr == UINT_MAX ? PREQ : uPr;
        rgPropertyRestriction[iProp].prop = PROPID_Q_TYPE;
        rgPropertyRestriction[iProp].prval.vt = VT_CLSID;
        iProp++;
      }
    }
    if (pstrLabel->vt != VT_ERROR) {
      rgPropertyRestriction[iProp].prval.vt = VT_ERROR;
      if (IsValidRel(prelLabel)) {
        if ((bstrTemp = GetBstr(pstrLabel)) == NULL) {
          // NULL label interpreted as empty string
          //  so don't do anything here... we'll convert
          //  it to an explicit empty string below...
          //
        }
        UINT cch;
        // SysFreeString(m_bstrLabel);
        // IfNullFail(m_bstrLabel = SYSALLOCSTRING(bstrTemp));
        IfNullFail(rgPropertyRestriction[iProp].prval.pwszVal =
          new WCHAR[(cch = SysStringLen(bstrTemp)) + 1]);
        wcsncpy(rgPropertyRestriction[iProp].prval.pwszVal, 
                bstrTemp,
                cch);
        // null terminate
        rgPropertyRestriction[iProp].prval.pwszVal[cch] = 0;
        uPr = PrOfVar(prelLabel);
        rgPropertyRestriction[iProp].prop = PROPID_Q_LABEL;
        rgPropertyRestriction[iProp].prval.vt = VT_LPWSTR;
        rgPropertyRestriction[iProp].rel = 
          uPr == UINT_MAX ? PREQ : uPr;
        iProp++;
      }
    }
    if (pdateCreateTime->vt != VT_ERROR) {
      rgPropertyRestriction[iProp].prval.vt = VT_ERROR;
      if (IsValidRel(prelCreateTime)) {
        if (!VariantTimeToTime(pdateCreateTime, &ulTime)) {
          IfFailGo(hresult = E_INVALIDARG);
        }
        rgPropertyRestriction[iProp].prop = PROPID_Q_CREATE_TIME;
        rgPropertyRestriction[iProp].prval.vt = VT_UI4;
        rgPropertyRestriction[iProp].prval.lVal = (long)ulTime;
        uPr = PrOfVar(prelCreateTime);
        rgPropertyRestriction[iProp].rel = 
          uPr == UINT_MAX ? PREQ : uPr;
        iProp++;
      }
    }
    if (pdateModifyTime->vt != VT_ERROR) {
      rgPropertyRestriction[iProp].prval.vt = VT_ERROR;
      if (IsValidRel(prelModifyTime)) {
        if (!VariantTimeToTime(pdateModifyTime, &ulTime)) {
          IfFailGo(hresult = E_INVALIDARG);
        }
        rgPropertyRestriction[iProp].prop = PROPID_Q_MODIFY_TIME;
        rgPropertyRestriction[iProp].prval.vt = VT_UI4;
        rgPropertyRestriction[iProp].prval.lVal = (long)ulTime;
        uPr = PrOfVar(prelModifyTime);
        rgPropertyRestriction[iProp].rel = 
          uPr == UINT_MAX ? PREQ : uPr;
        iProp++;
      }
    }
    //    
    // Column set
    // For now we just return in the following order:
    //    pathname
    //	CONSIDER: optimization: return instance guid instead of pathname 
	//
    cCol = 1;
    pcolumnset->aCol = new PROPID[cCol];
    pcolumnset->cCol = cCol;
    pcolumnset->aCol[0] = PROPID_Q_PATHNAME;
    // fall through...

Error:
    delete [] pguidQueue;
    delete [] pguidServiceType;

    return hresult;
#endif // 0
}

//=--------------------------------------------------------------------------=
// CMSMQQuery::LookupQueue
//=--------------------------------------------------------------------------=
//
// Parameters:
// [IN] strGuidQueue 
// [IN] strGuidServiceType 
// [IN] strLabel 
// [IN] dateCreateTime
// [IN] dateModifyTime
// [IN] relServiceType 
// [IN] relLabel 
// [IN] relCreateTime
// [IN] relModifyTime
// [OUT] ppqinfos
//
// Output:
//    HRESULT       - S_OK, E_NOINTERFACE
//
// Notes:
//
HRESULT CMSMQQuery::LookupQueue(
    VARIANT *strGuidQueue, 
    VARIANT *strGuidServiceType, 
    VARIANT *strLabel, 
    VARIANT *dateCreateTime, 
    VARIANT *dateModifyTime, 
    VARIANT *relServiceType, 
    VARIANT *relLabel, 
    VARIANT *relCreateTime, 
    VARIANT *relModifyTime, 
    IMSMQQueueInfos **ppqinfos)
{
	return E_NOTIMPL;
	#if 0 // not implemented on CE

    MQRESTRICTION *prestriction;
    MQCOLUMNSET *pcolumnset;
    CMSMQQueueInfos *pqinfos = NULL;
    HRESULT hresult = NOERROR;

    *ppqinfos = NULL;
    IfNullRet(prestriction = new MQRESTRICTION);
    IfNullFail(pcolumnset = new MQCOLUMNSET);
    IfNullFail(pqinfos = new CMSMQQueueInfos(NULL));
    //
    // important for cleanup to work
    //
    pcolumnset->aCol = NULL;
    prestriction->paPropRes = NULL;
    prestriction->cRes = 0;
    IfFailGoTo(CreateRestriction(strGuidQueue, 
                                 strGuidServiceType, 
                                 strLabel, 
                                 dateCreateTime,
                                 dateModifyTime,
                                 relServiceType, 
                                 relLabel, 
                                 relCreateTime,
                                 relModifyTime,
                                 prestriction,
                                 pcolumnset),
      Error2);
    //
    // prestriction, pcolumnset ownership transfers
    //
    IfFailGoTo(pqinfos->Init(NULL,    // context
                             prestriction,
                             pcolumnset,
                             NULL),   // sort
      Error2);
    *ppqinfos = pqinfos;
    //
    // fall through...
    //
Error2:
    if (FAILED(hresult)) {
      FreeRestriction(prestriction);
      FreeColumnSet(pcolumnset);
    }
    //
    // fall through...
    //
Error:
    if (FAILED(hresult)) {
      RELEASE(pqinfos);
      delete prestriction;
      delete pcolumnset;
    }

    return CreateErrorHelper(hresult, m_ObjectType);
#endif  // 0 
	
}

#if 0
//
//  UNDONE: post-beta2
//

//=--------------------------------------------------------------------------=
// CMSMQQuery::LookupSite
//=--------------------------------------------------------------------------=
//
// Parameters:
// [IN] strName
// [IN] strGuidSite
// [OUT] ppsites
//
// Output:
//    HRESULT       - S_OK, E_NOINTERFACE
//
// Notes:
//

HRESULT CMSMQQuery::LookupSite(
    VARIANT FAR* strName, 
    VARIANT FAR* strGuidSite, 
    IMSMQSites FAR* FAR* ppsites)
{
    return E_NOTIMPL;
    // return ::LookupSite(strName, strGuidSite, ppsites);
}


//=--------------------------------------------------------------------------=
// CMSMQQuery::LookupMachine
//=--------------------------------------------------------------------------=
//
// Parameters:
//
// Output:
//    HRESULT       - S_OK, E_NOINTERFACE
//
// Notes:
//

HRESULT CMSMQQuery::LookupMachine(
    VARIANT FAR* strPathname, 
    VARIANT FAR* strSitename, 
    VARIANT FAR* strGuidSite, 
    VARIANT FAR* strGuidmachine, 
    VARIANT FAR* lService, 
    IMSMQMachines FAR* FAR* ppmachines)
{
    return E_NOTIMPL;
#if 0
    return ::LookupMachine(
              strPathname, 
              strSitename, 
              strGuidSite, 
              strGuidmachine, 
              lService, 
              ppmachines);
#endif // 0
}


//=--------------------------------------------------------------------------=
// CMSMQQuery::LookupCN
//=--------------------------------------------------------------------------=
//
// Parameters:
//
// Output:
//    HRESULT       - S_OK, E_NOINTERFACE
//
// Notes:
//

HRESULT CMSMQQuery::LookupCN(
    VARIANT FAR* strName, 
    VARIANT FAR* lProtocol, 
    VARIANT FAR* strGuidCN, 
    IMSMQCNs FAR* FAR* ppcns)
{
    return E_NOTIMPL;
    // return ::LookupCN(strName, lProtocol, strGuidCN, ppcns);
}

#endif // 0


//=--------------------------------------------------------------------------=
// static CMSMQQuery::FreeRestriction
//=--------------------------------------------------------------------------=
// Frees dynamic memory allocated on behalf of an 
//  MQRESTRICTION struct.  
//
// Parameters:
//  prestriction
//
// Output:
//
// Notes:
//
void CMSMQQuery::FreeRestriction(MQRESTRICTION *prestriction)
{
    MQPROPERTYRESTRICTION *rgPropertyRestriction;
    UINT cRestriction, iProp;
    PROPID prop;

    if (prestriction) {
      cRestriction = prestriction->cRes;
      rgPropertyRestriction = prestriction->paPropRes;
      for (iProp = 0; iProp < cRestriction; iProp++) {
        prop = rgPropertyRestriction[iProp].prop;
        switch (prop) {
        case PROPID_Q_INSTANCE:
        case PROPID_Q_TYPE:
          delete [] rgPropertyRestriction[iProp].prval.puuid;
          break;
        case PROPID_Q_LABEL:
          delete [] rgPropertyRestriction[iProp].prval.pwszVal;
          break;
        } // switch
      } // for
      delete [] rgPropertyRestriction;
      prestriction->paPropRes = NULL;
    }
}


//=--------------------------------------------------------------------------=
// static CMSMQQuery::FreeColumnSet
//=--------------------------------------------------------------------------=
// Frees dynamic memory allocated on behalf of an 
//  MQCOLUMNSET struct.  
//
// Parameters:
//  pcolumnset
//
// Output:
//
// Notes:
//
void CMSMQQuery::FreeColumnSet(MQCOLUMNSET *pcolumnset)
{
    if (pcolumnset) {
      delete [] pcolumnset->aCol;
      pcolumnset->aCol = NULL;
    }
}

