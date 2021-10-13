//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//=--------------------------------------------------------------------------=
// MSMQQueueInfoObj.H
//=--------------------------------------------------------------------------=
//
// the MSMQQueueInfo object.
//
//
#ifndef _MSMQQueueInfo_H_

#include "AutoObj.H"
#include "lookupx.h"    // UNDONE; to define admin stuff...
#include "mqoa.H"
#include "mq.h"

#include "oautil.h"
class CMSMQQueueInfo : public IMSMQQueueInfo, 
                    public CAutomationObject, 
                    ISupportErrorInfo {

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

    CMSMQQueueInfo(IUnknown *);
    virtual ~CMSMQQueueInfo();

    // IMSMQQueueInfo methods
    // TODO: copy over the interface methods for IMSMQQueueInfo from
    //       mqInterfaces.H here.
    STDMETHOD(get_QueueGuid)(THIS_ BSTR FAR* pbstrGuidQueue);
    STDMETHOD(get_ServiceTypeGuid)(THIS_ BSTR FAR* pbstrGuidServiceType);
    STDMETHOD(put_ServiceTypeGuid)(THIS_ BSTR bstrGuidServiceType);
    STDMETHOD(get_Label)(THIS_ BSTR FAR* pbstrLabel);
    STDMETHOD(put_Label)(THIS_ BSTR bstrLabel);
    STDMETHOD(get_PathName)(THIS_ BSTR FAR* pbstrPathName);
    STDMETHOD(put_PathName)(THIS_ BSTR bstrPathName);
    STDMETHOD(get_FormatName)(THIS_ BSTR FAR* pbstrFormatName);
    STDMETHOD(put_FormatName)(THIS_ BSTR bstrFormatName);
    STDMETHOD(get_IsTransactional)(THIS_ VARIANT_BOOL FAR* pisTransactional);
    STDMETHOD(get_PrivLevel)(THIS_ long FAR* plPrivLevel);
    STDMETHOD(put_PrivLevel)(THIS_ long lPrivLevel);
    STDMETHOD(get_Journal)(THIS_ long FAR* plJournal);
    STDMETHOD(put_Journal)(THIS_ long lJournal);
    STDMETHOD(get_Quota)(THIS_ long FAR* plQuota);
    STDMETHOD(put_Quota)(THIS_ long lQuota);
    STDMETHOD(get_BasePriority)(THIS_ long FAR* plBasePriority);
    STDMETHOD(put_BasePriority)(THIS_ long lBasePriority);
    STDMETHOD(get_CreateTime)(THIS_ VARIANT FAR* pvarCreateTime);
    STDMETHOD(get_ModifyTime)(THIS_ VARIANT FAR* pvarModifyTime);
    STDMETHOD(get_Authenticate)(THIS_ long FAR* plAuthenticate);
    STDMETHOD(put_Authenticate)(THIS_ long lAuthenticate);
    STDMETHOD(get_JournalQuota)(THIS_ long FAR* plJournalQuota);
    STDMETHOD(put_JournalQuota)(THIS_ long lJournalQuota);
    STDMETHOD(get_IsWorldReadable)(THIS_ VARIANT_BOOL FAR* pisWorldReadable);
    STDMETHOD(Create)(THIS_ VARIANT FAR* isTransactional, VARIANT FAR* IsWorldReadable);
    STDMETHOD(Delete)(THIS);
    STDMETHOD(Open)(THIS_ long lAccess, long lShareMode, IMSMQQueue FAR* FAR* ppq);
    STDMETHOD(Refresh)(THIS);
    STDMETHOD(Update)(THIS);

    // creation method
    //
    static IUnknown *Create(IUnknown *);

    // introduced methods
    HRESULT Init(BSTR bstrFormatName);

  protected:
    virtual HRESULT InternalQueryInterface(REFIID riid, void **ppvObjOut);
    HRESULT CreateQueueProps(
      BOOL fUpdate,
      UINT cProp, 
      MQQUEUEPROPS *pqueueprops, 
      BOOL isTransactional,
      ...);
    HRESULT SetQueueProps(MQQUEUEPROPS *pqueueprops);
    void FreeQueueProps(MQQUEUEPROPS *pqueueprops);
    // HRESULT SetProperty(QUEUEPROPID queuepropid);
    HRESULT UpdateFormatName();

#if 0 //  no longer used
    HRESULT PutServiceType(
        BSTR bstrGuidServiceType,
        GUID *pguidServiceType); 
    HRESULT PutLabel(
        BSTR bstrLabel,
        BSTR *pbstrLabel); 
    HRESULT PutPathName(
        BSTR bstrPathName,
        BSTR *pbstrPathName); 
    HRESULT PutFormatName(
        BSTR bstrFormatName,
        BSTR *pbstrFormatName); 
    HRESULT PutPrivLevel(
        long lPrivLevel,
        long *plPrivLevel);
    HRESULT PutJournal(
        long lJournal, 
        long *plJournal);
    HRESULT PutQuota(long lQuota, long *plQuota);
    HRESULT PutBasePriority(
        long lBasePriority, 
        long *plBasePriority);
    HRESULT PutAuthenticate(
        long lAuthenticate, 
        long *plAuthenticate);
#endif // 0
    HRESULT InitProps();

  private:
    // member variables that nobody else gets to look at.
    // TODO: add your member variables and private functions here.
    GUID *m_pguidQueue;
    GUID *m_pguidServiceType;
    BSTR m_bstrLabel;
    BSTR m_bstrFormatName;
    BOOL m_isValidFormatName;   // 2026
    BSTR m_bstrPathName;
    BOOL m_isTransactional;
    long m_lPrivLevel;
    long m_lJournal;
    long m_lQuota;
    long m_lBasePriority;
    long m_lCreateTime;
    long m_lModifyTime;
    long m_lAuthenticate;
    long m_lJournalQuota;
    BOOL m_isRefreshed;         // 2536
};

// TODO: modify anything appropriate in this structure, such as the helpfile
//       name, the version number, etc.
//
DEFINE_AUTOMATIONOBJECT(MSMQQueueInfo,
    &CLSID_MSMQQueueInfo,
    L"MSMQQueueInfo",
    CMSMQQueueInfo::Create,
    1,
    &IID_IMSMQQueueInfo,
    L"MSMQQueueInfo.Hlp");


#define _MSMQQueueInfo_H_
#endif // _MSMQQueueInfo_H_
