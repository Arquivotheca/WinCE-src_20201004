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
//=--------------------------------------------------------------------------=
// MSMQQueryObj.H
//=--------------------------------------------------------------------------=
//
// the MSMQQuery object.
//
//
#ifndef _MQQUERY_H_

#include "AutoObj.H"
//#include "lookupx.h"   // for mqadminx stuff??
#include "mqoa.H"

#include "oautil.h"
#include "qinfos.h"
class CMSMQQuery : public IMSMQQuery, public CAutomationObject, ISupportErrorInfo {

  public:
    // IUnknown methods
    //
    DECLARE_STANDARD_UNKNOWN();

    // IDispatch methods
    //
    DECLARE_STANDARD_DISPATCH();

    //  ISupportErrorInfo methods
    //
    DECLARE_STANDARD_SUPPORTERRORINFO();

    CMSMQQuery(IUnknown *);
    virtual ~CMSMQQuery();

    // IMSMQQuery methods
    // TODO: copy over the interface methods for IMSMQQuery from
    //       mqInterfaces.H here.

    STDMETHOD(LookupQueue)(THIS_ VARIANT *strGuidQueue, 
                           VARIANT *strGuidServiceType, 
                           VARIANT *strLabel, 
                           VARIANT *dateCreateTime, 
                           VARIANT *dateModifyTime, 
                           VARIANT *relServiceType, 
                           VARIANT *relLabel, 
                           VARIANT *relCreateTime, 
                           VARIANT *relModifyTime, 
                           IMSMQQueueInfos **pqinfos);
#if 0
    // UNDONE: post-beta2
    STDMETHOD(LookupSite)(THIS_ VARIANT FAR* strName, VARIANT FAR* strGuidSite, IMSMQSites FAR* FAR* ppsites);
    STDMETHOD(LookupMachine)(THIS_ VARIANT FAR* strPathname, VARIANT FAR* strSitename, VARIANT FAR* strGuidSite, VARIANT FAR* strGuidmachine, VARIANT FAR* lService, IMSMQMachines FAR* FAR* ppmachines);
    STDMETHOD(LookupCN)(THIS_ VARIANT FAR* strName, VARIANT FAR* lProtocol, VARIANT FAR* strGuidCN, IMSMQCNs FAR* FAR* ppcns);
#endif // 0

    // creation method
    //
    static IUnknown *Create(IUnknown *);

    // introduced publics
    static void FreeColumnSet(MQCOLUMNSET *pColumnSet);
    static void FreeRestriction(MQRESTRICTION *pRestriction);

  protected:
    virtual HRESULT InternalQueryInterface(REFIID riid, void **ppvObjOut);

    // introduced methods...
    HRESULT CreateRestriction(
      VARIANT *pstrGuidQueue, 
      VARIANT *pstrGuidServiceType, 
      VARIANT *pstrLabel, 
      VARIANT *pdateCreateTime,
      VARIANT *pdateModifyTime,
      VARIANT *prelServiceType, 
      VARIANT *prelLabel, 
      VARIANT *prelCreateTime,
      VARIANT *prelModifyTime,
      MQRESTRICTION *pRestriction,
      MQCOLUMNSET *pColumnSet);

  private:
    // member variables that nobody else gets to look at.
    // TODO: add your member variables and private functions here.
    // CMSMQQueueInfos *m_pqinfos;
    // CLSID *m_pguidServiceType;
    // CLSID *m_pguidQueue;
    // BSTR m_bstrPathName;
    // BSTR m_bstrLabel;
    // BSTR m_bstrFormatName;
};

// TODO: modify anything appropriate in this structure, such as the helpfile
//       name, the version number, etc.
//
DEFINE_AUTOMATIONOBJECT(MSMQQuery,
    &CLSID_MSMQQuery,
    L"MSMQQuery",
    CMSMQQuery::Create,
    1,
    &IID_IMSMQQuery,
    L"MSMQQuery.Hlp");


#define _MQQUERY_H_
#endif // _MQQUERY_H_
