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
// MSMQQueueInfosObj.H
//=--------------------------------------------------------------------------=
//
// the MSMQQueueInfos object.
//
//
#ifndef _MSMQQueueInfoS_H_

#include "AutoObj.H"
#include "lookupx.h"    // UNDONE; to define admin stuff...
#include "mqoa.H"

#include "oautil.h"
#include "mq.h"
class CMSMQQueueInfos : public IMSMQQueueInfos, public CAutomationObject, ISupportErrorInfo {

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

    CMSMQQueueInfos(IUnknown *);
    virtual ~CMSMQQueueInfos();

    // IMSMQQueueInfos methods
    // TODO: copy over the interface methods for IMSMQQueueInfos from
    //       mqInterfaces.H here.
    STDMETHOD(Reset)(THIS);
    STDMETHOD(Next)(THIS_ IMSMQQueueInfo **ppqinfoNext);

    // creation method
    //
    static IUnknown *Create(IUnknown *);

    // introduced methods...
    HRESULT Init(
      BSTR bstrContext,
      MQRESTRICTION *pRestriction,
      MQCOLUMNSET *pColumns,
      MQSORTSET *pSort);

  protected:
    virtual HRESULT InternalQueryInterface(REFIID riid, void **ppvObjOut);

  private:
    // member variables that nobody else gets to look at.
    // TODO: add your member variables and private functions here.
    HANDLE m_hEnum;
    BSTR m_bstrContext;
    MQRESTRICTION *m_pRestriction;
    MQCOLUMNSET *m_pColumns;
    MQSORTSET *m_pSort;
};

// TODO: modify anything appropriate in this structure, such as the helpfile
//       name, the version number, etc.
//
DEFINE_AUTOMATIONOBJECT(MSMQQueueInfos,
    &CLSID_MSMQQueueInfos,
    L"MSMQQueueInfos",
    CMSMQQueueInfos::Create,
    1,
    &IID_IMSMQQueueInfos,
    L"MSMQQueueInfos.Hlp");


#define _MSMQQueueInfoS_H_
#endif // _MSMQQueueInfoS_H_
