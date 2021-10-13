//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//=--------------------------------------------------------------------------=
// MSMQMessageObj.H
//=--------------------------------------------------------------------------=
//
// the MSMQMessage object.
//
//
#ifndef _MSMQMessage_H_

#include "AutoObj.H"
#include "lookupx.h"    // UNDONE; to define admin stuff...
#include "mqoa.H"
#include "oautil.h"
#include "mq.h"

// HELPER: describes message type
enum MSGTYPE {
    MSGTYPE_BINARY,
    MSGTYPE_STREAM,
    MSGTYPE_STORAGE
};

//
// HELPER: helper struct for message properties
//
struct PROPREC {
    MSGPROPID m_propid;
    VARTYPE m_vt;
};

class CMSMQMessage : public IMSMQMessage, public CAutomationObject, ISupportErrorInfo {

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

    CMSMQMessage(IUnknown *);
    virtual ~CMSMQMessage();

    // IMSMQMessage methods
    // TODO: copy over the interface methods for IMSMQMessage from
    //       mqInterfaces.H here.
    STDMETHOD(get_Class)(THIS_ long FAR* plClass);
    STDMETHOD(get_PrivLevel)(THIS_ long FAR* plPrivLevel);
    STDMETHOD(put_PrivLevel)(THIS_ long lPrivLevel);
    STDMETHOD(get_AuthLevel)(THIS_ long FAR* plAuthLevel);
    STDMETHOD(put_AuthLevel)(THIS_ long lAuthLevel);
    STDMETHOD(get_IsAuthenticated)(THIS_ VARIANT_BOOL FAR* pisAuthenticated);
    STDMETHOD(get_Delivery)(THIS_ long FAR* plDelivery);
    STDMETHOD(put_Delivery)(THIS_ long lDelivery);
    STDMETHOD(get_Trace)(THIS_ long FAR* plTrace);
    STDMETHOD(put_Trace)(THIS_ long lTrace);
    STDMETHOD(get_Priority)(THIS_ long FAR* plPriority);
    STDMETHOD(put_Priority)(THIS_ long lPriority);
    STDMETHOD(get_Journal)(THIS_ long FAR* plJournal);
    STDMETHOD(put_Journal)(THIS_ long lJournal);
    STDMETHOD(get_ResponseQueueInfo)(THIS_ IMSMQQueueInfo FAR* FAR* ppqinfoResponse);
    STDMETHOD(putref_ResponseQueueInfo)(THIS_ IMSMQQueueInfo FAR* pqinfoResponse);
    STDMETHOD(get_AppSpecific)(THIS_ long FAR* plAppSpecific);
    STDMETHOD(put_AppSpecific)(THIS_ long lAppSpecific);
    STDMETHOD(get_SourceMachineGuid)(THIS_ BSTR FAR* pbstrGuidSrcMachine);
    STDMETHOD(get_BodyLength)(THIS_ long FAR* pcbBody);
    STDMETHOD(get_Body)(THIS_ VARIANT FAR* pvarBody);
    STDMETHOD(put_Body)(THIS_ VARIANT varBody);
    STDMETHOD(get_AdminQueueInfo)(THIS_ IMSMQQueueInfo FAR* FAR* ppqinfoAdmin);
    STDMETHOD(putref_AdminQueueInfo)(THIS_ IMSMQQueueInfo FAR* pqinfoAdmin);
    STDMETHOD(get_Id)(THIS_ VARIANT FAR* pvarMsgId);
    STDMETHOD(get_CorrelationId)(THIS_ VARIANT FAR* pvarMsgId);
    STDMETHOD(put_CorrelationId)(THIS_ VARIANT varMsgId);
    STDMETHOD(get_Ack)(THIS_ long FAR* plAck);
    STDMETHOD(put_Ack)(THIS_ long lAck);
    STDMETHOD(get_Label)(THIS_ BSTR FAR* pbstrLabel);
    STDMETHOD(put_Label)(THIS_ BSTR bstrLabel);
    STDMETHOD(get_MaxTimeToReachQueue)(THIS_ long FAR* plMaxTimeToReachQueue);
    STDMETHOD(put_MaxTimeToReachQueue)(THIS_ long lMaxTimeToReachQueue);
    STDMETHOD(get_MaxTimeToReceive)(THIS_ long FAR* plMaxTimeToReceive);
    STDMETHOD(put_MaxTimeToReceive)(THIS_ long lMaxTimeToReceive);
    STDMETHOD(get_HashAlgorithm)(THIS_ long FAR* plHashAlg);
    STDMETHOD(put_HashAlgorithm)(THIS_ long lHashAlg);
    STDMETHOD(get_EncryptAlgorithm)(THIS_ long FAR* plEncryptAlg);
    STDMETHOD(put_EncryptAlgorithm)(THIS_ long lEncryptAlg);
    STDMETHOD(get_SentTime)(THIS_ VARIANT FAR* pvarSentTime);
    STDMETHOD(get_ArrivedTime)(THIS_ VARIANT FAR* plArrivedTime);
    STDMETHOD(get_DestinationQueueInfo)(THIS_ IMSMQQueueInfo FAR* FAR* ppqinfoDest);
    STDMETHOD(get_SenderCertificate)(THIS_ VARIANT FAR* pvarSenderCert);
    STDMETHOD(put_SenderCertificate)(THIS_ VARIANT varSenderCert);
    STDMETHOD(get_SenderId)(THIS_ VARIANT FAR* pvarSenderId);
    STDMETHOD(get_SenderIdType)(THIS_ long FAR* plSenderIdType);
    STDMETHOD(put_SenderIdType)(THIS_ long lSenderIdType);
    STDMETHOD(Send)(THIS_ IMSMQQueue FAR* pqDest, VARIANT FAR* ptransaction);
    STDMETHOD(AttachCurrentSecurityContext)(THIS);


    // creation method
    //
    static IUnknown *Create(IUnknown *);

    // introduced methods
    HRESULT CreateReceiveMessageProps(
      BOOL wantDestQueue,
      BOOL wantBody,
      MQMSGPROPS *pmsgprops);
    static HRESULT CreateAsyncReceiveMessageProps(
      MQMSGPROPS *pmsgprops);
    HRESULT CreateSendMessageProps(
      MQMSGPROPS *pmsgprops);
    HRESULT SetMessageProps(
      MQMSGPROPS *pmsgprops);
    HRESULT UpdateMsgId(
      MQMSGPROPS *pmsgprops);
    static void FreeMessageProps(
      MQMSGPROPS *pmsgprops);
    HRESULT CreateSendPropIdArray(
        UINT *pcPropRec,
        PROPREC **prgproprec);
    HRESULT GetBinBody(VARIANT FAR* pvarBody);
    HRESULT GetVarBody(VARIANT FAR* pvarBody);
    HRESULT PutBinBody(VARIANT varBody);
    HRESULT PutVarBody(VARIANT varBody);

  protected:
    virtual HRESULT InternalQueryInterface(REFIID riid, void **ppvObjOut);
    HRESULT GetStreamOfBody(ULONG cbBody, void *pvBody, IStream **ppstm);
    HRESULT GetStorageOfBody(ULONG cbBody, void *pvBody, IStorage **ppstg);
    HRESULT UpdateBodyBuffer(ULONG cbBody, void *pvBody);
    HRESULT GetStreamedObject(VARIANT *pvarBody);
    HRESULT GetStoredObject(VARIANT *pvarBody);

  private:
    // member variables that nobody else gets to look at.
    // TODO: add your member variables and private functions here.
    long m_lClass;
    long m_lDelivery;
    long m_lPriority;
    long m_lJournal;
    IMSMQQueueInfo *m_pqinfoResponse;
    long m_lAppSpecific;
    long m_lMaxTimeToReachQueue;
    long m_lMaxTimeToReceive;
    long m_lSentTime;
    long m_lArrivedTime;
    BYTE *m_pbBody;
    VARTYPE m_vtBody;
    HGLOBAL m_hMem;       // optimization: we could always reverse
                          //  engineer from m_pbBody
    long m_cbBody;
    IMSMQQueueInfo *m_pqinfoAdmin;
    IMSMQQueueInfo *m_pqinfoDest;
    BYTE *m_rgbMsgId;
    ULONG m_cbMsgId;
    BYTE *m_rgbCorrelationId;
    ULONG m_cbCorrelationId;
    long m_lAck;
    long m_lTrace;
    BYTE *m_rgbSenderId;
    ULONG m_cbSenderId;
    long m_lSenderIdType;
    BYTE *m_rgbSenderCert;
    ULONG m_cbSenderCert;
    long m_lPrivLevel;
    long m_lAuthLevel;
    BOOL m_fAuthenticated;
    long m_lHashAlg;
    long m_lEncryptAlg;
    long m_lSecurityContext;
public:
    CLSID *m_pguidSrcMachine;
    LPOLESTR m_pwszLabel;
    UINT m_cchLabel;
    LPOLESTR m_pwszDestQueue;
    UINT m_cchDestQueue;
    LPOLESTR m_pwszRespQueue;
    UINT m_cchRespQueue;
    LPOLESTR m_pwszAdminQueue;
    UINT m_cchAdminQueue;
private:
};

// TODO: modify anything appropriate in this structure, such as the helpfile
//       name, the version number, etc.
//
DEFINE_AUTOMATIONOBJECT(MSMQMessage,
    &CLSID_MSMQMessage,
    L"MSMQMessage",
    CMSMQMessage::Create,
    1,
    &IID_IMSMQMessage,
    L"MSMQMessage.Hlp");


#define _MSMQMessage_H_
#endif // _MSMQMessage_H_
