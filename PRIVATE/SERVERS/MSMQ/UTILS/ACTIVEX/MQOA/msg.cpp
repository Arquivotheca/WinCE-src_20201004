//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//--------------------------------------------------------------------------=
// MSMQMessageObj.Cpp
//=--------------------------------------------------------------------------=
// the MSMQMessage object
//
//
//
// Needed for wincrypt.h
//
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif

#include <windows.h>
#include <winreg.h>
#include <wincrypt.h>
#include "IPServer.H"


#include "LocalObj.H"
#include "limits.h"   // for UINT_MAX
#include "utilx.h"


#include "msg.H"
#include "qinfo.h"
#include "q.h"
//#include "txdtc.h"             // no transaction support.
//#include "xact.h"
//#include "mtxdm.h"

//
// some message property buffer sizes
//
#define SENDERIDLEN   32
#define SENDERCERTLEN 4096

#if DEBUG
extern VOID RemBstrNode(void *pv);
#endif // DEBUG

extern IUnknown *GetPunk(VARIANT *pvar);

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

extern UCHAR *AllocateCauVector (CAUB *pcau);
extern HGLOBAL AllocateBodyBuffer(
    MQMSGPROPS *pmsgprops,
    UINT iBody,
    DWORD dwBodySize);
extern void DeleteCauVector (CAUB *pcau);


PROPREC g_rgmsgproprec[] = {
  {PROPID_M_MSGID,                        VT_UI1|VT_VECTOR},
  {PROPID_M_CORRELATIONID,                VT_UI1|VT_VECTOR},
  {PROPID_M_PRIORITY,                     VT_UI1},
  {PROPID_M_DELIVERY,                     VT_UI1},
  {PROPID_M_ACKNOWLEDGE,                  VT_UI1},
  {PROPID_M_JOURNAL,                      VT_UI1},
  {PROPID_M_APPSPECIFIC,                  VT_UI4},
  {PROPID_M_LABEL,                        VT_LPWSTR},
  {PROPID_M_LABEL_LEN,                    VT_UI4},
  {PROPID_M_TIME_TO_BE_RECEIVED,          VT_UI4},
  {PROPID_M_TRACE,                        VT_UI1},
  {PROPID_M_TIME_TO_REACH_QUEUE,          VT_UI4},
  {PROPID_M_SENDERID,                     VT_UI1|VT_VECTOR},
  {PROPID_M_SENDERID_LEN,                 VT_UI4},
  {PROPID_M_SENDERID_TYPE,                VT_UI4},
  {PROPID_M_PRIV_LEVEL,                   VT_UI4}, // must precede ENCRYPTION_ALG
  {PROPID_M_AUTH_LEVEL,                   VT_UI4},
  {PROPID_M_AUTHENTICATED,                VT_UI4}, // must precede HASH_ALG
//  {PROPID_M_HASH_ALG,                     VT_UI4},
//  {PROPID_M_ENCRYPTION_ALG,               VT_UI4},
  {PROPID_M_SENDER_CERT,                  VT_UI1|VT_VECTOR},
  {PROPID_M_SENDER_CERT_LEN,              VT_UI4},
  {PROPID_M_SRC_MACHINE_ID,               VT_CLSID},
  {PROPID_M_SENTTIME,                     VT_UI4},
  {PROPID_M_ARRIVEDTIME,                  VT_UI4},
  {PROPID_M_RESP_QUEUE,                   VT_LPWSTR},
  {PROPID_M_RESP_QUEUE_LEN,               VT_UI4},
  {PROPID_M_ADMIN_QUEUE,                  VT_LPWSTR},
  {PROPID_M_ADMIN_QUEUE_LEN,              VT_UI4},
//  {PROPID_M_SECURITY_CONTEXT,             VT_UI4},
  {PROPID_M_CLASS,                        VT_UI2},
  {PROPID_M_BODY_TYPE,                    VT_UI4}
};

ULONG g_cPropRec = sizeof g_rgmsgproprec / sizeof g_rgmsgproprec[0];

PROPREC g_rgmsgproprecOptional[] = {
  {PROPID_M_DEST_QUEUE,                   VT_LPWSTR},
  {PROPID_M_DEST_QUEUE_LEN,               VT_UI4},
  {PROPID_M_BODY,                         VT_UI1|VT_VECTOR},
  {PROPID_M_BODY_SIZE,                    VT_UI4}
};

ULONG g_cPropRecOptional = sizeof g_rgmsgproprecOptional / sizeof g_rgmsgproprecOptional[0];

#if 0 // not used
PROPREC g_rgproprecAsyncReceive[] = {
    {PROPID_M_BODY,                         VT_UI1|VT_VECTOR},
    {PROPID_M_BODY_SIZE,                    VT_UI4},
};

ULONG g_cPropRecAsyncReceive =
  sizeof g_rgproprecAsyncReceive / sizeof g_rgproprecAsyncReceive[0];
#endif // 0

// typedef HRESULT (STDAPIVCALLTYPE * LPFNGetDispenserManager)(
//                                    IDispenserManager **ppdispmgr);

//=--------------------------------------------------------------------------=
// HELPER: GetOptionalTransaction
//=--------------------------------------------------------------------------=
// Gets optional transaction
//
// Parameters:
//    pvarTransaction   [in]
//    pptransaction     [out]
//    pisRealXact       [out] TRUE if true pointer else enum.
//
// Output:
//
// Notes:
//


HRESULT GetOptionalTransaction(
    VARIANT *pvarTransaction,
    ITransaction **pptransaction,
    BOOL *pisRealXact)
{
//  IMSMQTransaction *pmqtransaction = NULL;
    IDispatch *pdisp = NULL;
    ITransaction *ptransaction = NULL;
    HRESULT hresult = NOERROR;
    //
    // get optional transaction...
    //
    *pisRealXact = FALSE;   // pessimism
    pdisp = GetPdisp(pvarTransaction);
    if (pdisp) {
   
    #if 0 // no real Transaction support
      IfFailGo(pdisp->QueryInterface(
                 IID_IMSMQTransaction,
                 (LPVOID *)&pmqtransaction));
      ASSERT(pmqtransaction, L"should have a transaction.");

      // extract ITransaction interface pointer
      IfFailGo(pmqtransaction->get_Transaction((long*)&ptransaction));
      ADDREF(ptransaction);
      *pisRealXact = TRUE;
    #else  // 0
    	return E_NOTIMPL;
    #endif 
    }
    else {
      //
      // 1890: no explicit transaction supplied: use current
      //  Viper transaction if there is one... unless
      //  Nothing explicitly supplied in Variant.  Note
      //  that if arg is NULL this means that it wasn't
      //  supplied as an optional param so just treat it
      //  like Nothing, i.e. don't user Viper.
      //
      // 1915: support:
      //  MQ_NO_TRANSACTION/VT_ERROR
      //  MQ_MTS_TRANSACTION
      //  MQ_XA_TRANSACTION
      //  MQ_SINGLE_MESSAGE
      //
      if (pvarTransaction) {
        if (V_VT(pvarTransaction) == VT_ERROR) {
 #ifndef _WIN32_CE  // under ce, we use MQ_SINGLE_TRANSACTION only or NULL
          ptransaction = (ITransaction *)MQ_MTS_TRANSACTION;
 #else 
 		  ptransaction = (ITransaction *)MQ_NO_TANSACTION;
 #endif _WIN32_CE
        }
        else {
          UINT uXactType;
          uXactType = GetNumber(pvarTransaction);
          if (uXactType != (UINT)MQ_NO_TRANSACTION &&
            //  uXactType != (UINT)MQ_MTS_TRANSACTION &&
            //  uXactType != (UINT)MQ_XA_TRANSACTION &&
              uXactType != (UINT)MQ_SINGLE_MESSAGE) {
            IfFailGo(hresult = E_INVALIDARG);
          }
          ptransaction = (ITransaction *)uXactType;
        }
      } // if
    }
    *pptransaction = ptransaction;
Error:
#if 0 // no transaction
    RELEASE(pmqtransaction);
#endif // 0
    return hresult;
}



//=--------------------------------------------------------------------------=
// HELPER: VtOfPropIdRgproprec
//=--------------------------------------------------------------------------=
// Looks up propid in global vt array by propid and returns vt
//
// Parameters:
//    propid    [in]  propid to lookup
//
// Output:
//    returns ordinal of propid in global array, VT_ERROR if not found.
// Notes:
//
VARTYPE VtOfPropIdRgproprec(
    MSGPROPID propid,
    PROPREC *rgproprec,
    UINT cPropRec)
{
    UINT iProp;

    for (iProp = 0; iProp < cPropRec; iProp++) {
      if (rgproprec[iProp].m_propid == propid) {
        return rgproprec[iProp].m_vt;
      }
    }
    return VT_ERROR;
}


//=--------------------------------------------------------------------------=
// HELPER: VtOfPropId
//=--------------------------------------------------------------------------=
// Looks up propid in global vt array by propid and returns vt
//
// Parameters:
//    propid    [in]  propid to lookup
//
// Output:
//    returns ordinal of propid in global array, VT_ERROR if not found.
// Notes:
//
VARTYPE VtOfPropId(MSGPROPID propid)
{
    return VtOfPropIdRgproprec(propid, g_rgmsgproprec, g_cPropRec);
}


//=--------------------------------------------------------------------------=
// HELPER: OrdinalOfPropId
//=--------------------------------------------------------------------------=
// Looks up propid in msgprops array and return ordinal
//
// Parameters:
//    pmsgprops [in]  lookup array
//    propid    [in]  propid to lookup
//
// Output:
//    returns ordinal of propid in msgprops array, -1 if not found.
// Notes:
//
UINT OrdinalOfPropId(MQMSGPROPS *pmsgprops, MSGPROPID propid)
{
    UINT iProp;

    for (iProp = 0; iProp < pmsgprops->cProp; iProp++) {
      if (pmsgprops->aPropID[iProp] == propid) {
        return iProp;
      }
    }
    return UINT_MAX;
}


//=--------------------------------------------------------------------------=
// HELPER: AddPropRecRgproprec
//=--------------------------------------------------------------------------=
// Appends a propid to array
//
// Parameters:
//    propid     [in]     propid to add
//    prgproprec [out]    array
//    pcPropRec   [in/out] array size
//
// Output:
//
// Notes:
//  Reallocs array as needed.
//
HRESULT AddPropRecOfRgproprec(
    MSGPROPID propid,
    PROPREC *rgproprec,
    UINT cPropRec,
    PROPREC **prgproprecOut,
    UINT *pcPropRecOut)
{
    UINT cPropRecOut = *pcPropRecOut;
    PROPREC proprec;
    PROPREC *rgproprecOut = *prgproprecOut;

    proprec.m_propid = propid;
    proprec.m_vt = VtOfPropIdRgproprec(
                     propid,
                     rgproprec,
                     cPropRec);
    IfNullRet(rgproprecOut =
      (PROPREC *)realloc(rgproprecOut,
                         (cPropRecOut + 1) * sizeof(PROPREC)));
    rgproprecOut[cPropRecOut++] = proprec;
    *pcPropRecOut = cPropRecOut;
    *prgproprecOut = rgproprecOut;
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// HELPER: AddPropRecOptional
//=--------------------------------------------------------------------------=
// Appends a propid to array
//
// Parameters:
//    propid     [in]     propid to add
//    prgproprec [out]    array
//    pcPropRec   [in/out] array size
//
// Output:
//
// Notes:
//  Reallocs array as needed.
//
HRESULT AddPropRecOptional(
    MSGPROPID propid,
    PROPREC **prgproprec,
    UINT *pcPropRec)
{
    return AddPropRecOfRgproprec(
             propid,
             g_rgmsgproprecOptional,
             g_cPropRecOptional,
             prgproprec,
             pcPropRec);
}


//=--------------------------------------------------------------------------=
// HELPER: AddPropRec
//=--------------------------------------------------------------------------=
// Appends a propid to array
//
// Parameters:
//    propid     [in]     propid to add
//    prgproprec [out]    array
//    pcPropRec   [in/out] array size
//
// Output:
//
// Notes:
//  Reallocs array as needed.
//
HRESULT AddPropRec(
    MSGPROPID propid,
    PROPREC **prgproprec,
    UINT *pcPropRec)
{
    return AddPropRecOfRgproprec(
             propid,
             g_rgmsgproprec,
             g_cPropRec,
             prgproprec,
             pcPropRec);
}


//=--------------------------------------------------------------------------=
// HELPER: AllocateVectorOfProperty
//=--------------------------------------------------------------------------=
// creates a new MSMQMessage object.
//
// Parameters:
//    pmsgprops
//    iProp
//    cb
//
// Output:
//
// Notes:
//
UCHAR *AllocateVectorOfProperty(
    MQMSGPROPS *pmsgprops,
    UINT iProp,
    UINT cb)
{
    pmsgprops->aPropVar[iProp].caub.cElems = cb;
    pmsgprops->aPropVar[iProp].caub.pElems = NULL;
    return AllocateCauVector(&pmsgprops->aPropVar[iProp].caub);
}


//=--------------------------------------------------------------------------=
// HELPER: AllocateMessageProps
//=--------------------------------------------------------------------------=
// Allocate and init message prop arrays
//
HRESULT AllocateMessageProps(
    MQMSGPROPS *pmsgprops,
    PROPREC *rgproprec,
    UINT cProp)
{
    UINT i;
    HRESULT hresult = NOERROR;

    pmsgprops->aPropID = NULL;
    pmsgprops->aPropVar = NULL;
    pmsgprops->aStatus = NULL;
    pmsgprops->cProp = cProp;
    IfNullRet(pmsgprops->aPropID = new MSGPROPID[cProp]);
    IfNullFail(pmsgprops->aPropVar = new MQPROPVARIANT[cProp]);
    IfNullFail(pmsgprops->aStatus = new HRESULT[cProp]);
    for (i = 0; i < cProp; i++) {
      pmsgprops->aPropID[i] = rgproprec[i].m_propid;
      pmsgprops->aPropVar[i].vt = rgproprec[i].m_vt;
    }
Error:
    return hresult;
}


//=--------------------------------------------------------------------------=
// HELPER: AllocateReceiveMessageProps
//=--------------------------------------------------------------------------=
// Allocate and init message prop arrays for receive.
//
HRESULT AllocateReceiveMessageProps(
    BOOL wantDestQueue,
    BOOL wantBody,
    MQMSGPROPS *pmsgprops,
    PROPREC *rgproprec,
    UINT cProp,
    UINT *pcPropOut)
{
    MSGPROPID propid;
    UINT i, cPropOut = cProp, cPropOld = cProp;
    HRESULT hresult = NOERROR;

    pmsgprops->aPropID = NULL;
    pmsgprops->aPropVar = NULL;
    pmsgprops->aStatus = NULL;
    if (wantDestQueue) {
      cPropOut++;      // PROPID_M_DEST_QUEUE
      cPropOut++;      // PROPID_M_DEST_QUEUE_LEN
    }
    if (wantBody) {
      cPropOut++;      // PROPID_M_BODY
      cPropOut++;      // PROPID_M_BODY_SIZE
    }
    pmsgprops->cProp = cPropOut;
    IfNullRet(pmsgprops->aPropID = new MSGPROPID[cPropOut]);
    IfNullFail(pmsgprops->aPropVar = new MQPROPVARIANT[cPropOut]);
    IfNullFail(pmsgprops->aStatus = new HRESULT[cPropOut]);
    for (i = 0; i < cPropOld; i++) {
      pmsgprops->aPropID[i] = rgproprec[i].m_propid;
      pmsgprops->aPropVar[i].vt = rgproprec[i].m_vt;
      
    }
    if (wantDestQueue) {
      pmsgprops->aPropID[cPropOld] = propid = PROPID_M_DEST_QUEUE;
      pmsgprops->aPropVar[cPropOld++].vt = VtOfPropIdRgproprec(
                                             propid,
                                             g_rgmsgproprecOptional,
                                             g_cPropRecOptional);
      pmsgprops->aPropID[cPropOld] = propid = PROPID_M_DEST_QUEUE_LEN;
      pmsgprops->aPropVar[cPropOld++].vt = VtOfPropIdRgproprec(
                                             propid,
                                             g_rgmsgproprecOptional,
                                             g_cPropRecOptional);
    }
    if (wantBody) {
      pmsgprops->aPropID[cPropOld] = propid = PROPID_M_BODY;
      pmsgprops->aPropVar[cPropOld++].vt = VtOfPropIdRgproprec(
                                             propid,
                                             g_rgmsgproprecOptional,
                                             g_cPropRecOptional);
      pmsgprops->aPropID[cPropOld] = propid = PROPID_M_BODY_SIZE;
      pmsgprops->aPropVar[cPropOld++].vt = VtOfPropIdRgproprec(
                                             propid,
                                             g_rgmsgproprecOptional,
                                             g_cPropRecOptional);
    }
    ASSERT(cPropOld == cPropOut, L"property count mismatch.");
    *pcPropOut = cPropOut;
    // fall through...

Error:
    return hresult;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::Create
//=--------------------------------------------------------------------------=
// creates a new MSMQMessage object.
//
// Parameters:
//    IUnknown *        - [in] controlling unkonwn
//
// Output:
//    IUnknown *        - new object.
//
// Notes:
//
IUnknown *CMSMQMessage::Create
(
    IUnknown *pUnkOuter
)
{
    // make sure we return the private unknown so that we support aggegation
    // correctly!
    //
    CMSMQMessage *pNew = new CMSMQMessage(pUnkOuter);
    return pNew ? pNew->PrivateUnknown() : NULL;
}

//=--------------------------------------------------------------------------=
// CMSMQMessage::CMSMQMessage
//=--------------------------------------------------------------------------=
// create the object and initialize the refcount
//
// Parameters:
//    IUnknown *    - [in] controlling unknown
//
// Notes:
//
#pragma warning(disable:4355)  // using 'this' in constructor
CMSMQMessage::CMSMQMessage
(
    IUnknown *pUnkOuter
)
: CAutomationObject(pUnkOuter, OBJECT_TYPE_OBJMQMESSAGE, (void *)this)
{

    // TODO: initialize anything here
    //
    m_lClass = -1;      // illegal value...
    m_lDelivery = DEFAULT_M_DELIVERY;
    m_lPriority = DEFAULT_M_PRIORITY;
    m_lJournal = DEFAULT_M_JOURNAL;
    m_pqinfoResponse = NULL;
    m_lAppSpecific = DEFAULT_M_APPSPECIFIC;
    m_pbBody = NULL;
    m_vtBody = VT_ARRAY | VT_UI1; // default: safearray of bytes
    m_hMem = NULL;
    m_cbBody = 0;
    m_pqinfoAdmin = NULL;
    m_rgbMsgId = NULL;
    m_cbMsgId = 0;
    m_rgbCorrelationId = NULL;
    m_cbCorrelationId = 0;
    m_lAck = DEFAULT_M_ACKNOWLEDGE;
    m_pwszLabel = new WCHAR[MQ_MAX_MSG_LABEL_LEN + 1];
    memset(m_pwszLabel, 0, sizeof(WCHAR));
    m_cchLabel = 0;           // empty
    m_lTrace = MQMSG_TRACE_NONE;
    m_rgbSenderId = NULL;
    m_cbSenderId = 0;
    m_lSenderIdType = DEFAULT_M_SENDERID_TYPE;
    m_rgbSenderCert = NULL;
    m_cbSenderCert = 0;
    m_lPrivLevel = DEFAULT_M_PRIV_LEVEL;
    m_lAuthLevel = DEFAULT_M_AUTH_LEVEL;
    m_fAuthenticated = FALSE;
    m_lHashAlg = CALG_MD5;
    m_lEncryptAlg = CALG_RC2;
    m_lMaxTimeToReachQueue = -1;
    m_lMaxTimeToReceive = -1;
    m_lSentTime = -1;
    m_lArrivedTime = -1;
    m_pqinfoDest = NULL;
    m_pwszDestQueue  = new WCHAR[FORMAT_NAME_INIT_BUFFER];
    memset(m_pwszDestQueue, 0, sizeof(WCHAR));
    m_cchDestQueue = 0;            // empty
    m_pwszRespQueue  = new WCHAR[FORMAT_NAME_INIT_BUFFER];
    memset(m_pwszRespQueue, 0, sizeof(WCHAR));
    m_cchRespQueue = 0;            // empty
    m_pwszAdminQueue  = new WCHAR[FORMAT_NAME_INIT_BUFFER];
    memset(m_pwszAdminQueue, 0, sizeof(WCHAR));
    m_cchAdminQueue = 0;            // empty
    m_lSecurityContext = 0;   // ignored...
    m_pguidSrcMachine = new GUID(GUID_NULL);
}
#pragma warning(default:4355)  // using 'this' in constructor

//=--------------------------------------------------------------------------=
// CMSMQMessage::CMSMQMessage
//=--------------------------------------------------------------------------=
// "We all labour against our own cure, for death is the cure of all diseases"
//    - Sir Thomas Browne (1605 - 82)
//
// Notes:
//
CMSMQMessage::~CMSMQMessage ()
{
    // TODO: clean up anything here.
    GLOBALFREE(m_hMem);
    m_cbBody = 0;
    delete [] m_rgbMsgId;
    delete [] m_rgbCorrelationId;
    delete [] m_pwszLabel;
    m_cchLabel = (UINT)-1; // uninitialized
    delete [] m_rgbSenderId;
    delete [] m_rgbSenderCert;
    RELEASE(m_pqinfoResponse);
    RELEASE(m_pqinfoAdmin);
    RELEASE(m_pqinfoDest);
    delete [] m_pwszDestQueue;
    m_cchDestQueue = (UINT)-1; // uninitialized
    delete [] m_pwszRespQueue;
    m_cchRespQueue = (UINT)-1; // uninitialized
    delete [] m_pwszAdminQueue;
    m_cchAdminQueue = (UINT)-1; // uninitialized
    delete m_pguidSrcMachine;
}

//=--------------------------------------------------------------------------=
// CMSMQMessage::InternalQueryInterface
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
HRESULT CMSMQMessage::InternalQueryInterface
(
    REFIID riid,
    void **ppvObjOut
)
{
    CHECK_POINTER(ppvObjOut);

    // we support IMSMQMessage and ISupportErrorInfo
    //
    if (DO_GUIDS_MATCH(riid, IID_IMSMQMessage)) {
        *ppvObjOut = (void *)(IMSMQMessage *)this;
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
//

//=--------------------------------------------------------------------------=
// CMSMQMessage::get_Class
//=--------------------------------------------------------------------------=
// Gets message class of message
//
// Parameters:
//    plClass - [out] message's class
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::get_Class(long FAR* plClass)
{
    *plClass = m_lClass;
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::get_Delivery
//=--------------------------------------------------------------------------=
// Gets message's delivery option
//
// Parameters:
//    pdelivery - [in] message's delivery option
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::get_Delivery(long FAR* plDelivery)
{
    *plDelivery = m_lDelivery;
    return NOERROR;
}

//=--------------------------------------------------------------------------=
// CMSMQMessage::put_Delivery
//=--------------------------------------------------------------------------=
// Sets delivery option for message
//
// Parameters:
//    delivery - [in] message's delivery option
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::put_Delivery(long lDelivery)
{
    switch (lDelivery) {
    case MQMSG_DELIVERY_EXPRESS:
    case MQMSG_DELIVERY_RECOVERABLE:
EnterCriticalSection(&m_CSlocal);
      m_lDelivery = lDelivery;
LeaveCriticalSection(&m_CSlocal);
      return NOERROR;
    default:
      return CreateErrorHelper(MQ_ERROR_ILLEGAL_PROPERTY_VALUE, m_ObjectType);
    }
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::get_HashAlgorithm
//=--------------------------------------------------------------------------=
// Gets message's hash algorithm
//
// Parameters:
//    plHashAlg - [in] message's hash alg
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::get_HashAlgorithm(long FAR* plHashAlg)
{
    *plHashAlg = m_lHashAlg;
    return NOERROR;
}

//=--------------------------------------------------------------------------=
// CMSMQMessage::put_HashAlgorithm
//=--------------------------------------------------------------------------=
// Sets message's hash alg
//
// Parameters:
//    lHashAlg - [in] message's hash alg
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::put_HashAlgorithm(long lHashAlg)
{

EnterCriticalSection(&m_CSlocal);
    m_lHashAlg = lHashAlg;
LeaveCriticalSection(&m_CSlocal);
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::get_EncryptAlgorithm
//=--------------------------------------------------------------------------=
// Gets message's encryption algorithm
//
// Parameters:
//    plEncryptAlg - [in] message's encryption alg
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::get_EncryptAlgorithm(long FAR* plEncryptAlg)
{
    *plEncryptAlg = m_lEncryptAlg;
    return NOERROR;
}

//=--------------------------------------------------------------------------=
// CMSMQMessage::put_EncryptAlgorithm
//=--------------------------------------------------------------------------=
// Sets message's encryption alg
//
// Parameters:
//    lEncryptAlg - [in] message's crypt alg
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::put_EncryptAlgorithm(long lEncryptAlg)
{

EnterCriticalSection(&m_CSlocal);
    m_lEncryptAlg = lEncryptAlg;
LeaveCriticalSection(&m_CSlocal);
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::get_SenderIdType
//=--------------------------------------------------------------------------=
// Gets message's sender id type
//
// Parameters:
//    plSenderIdType - [in] message's sender id type
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::get_SenderIdType(long FAR* plSenderIdType)
{
    *plSenderIdType = m_lSenderIdType;
    return NOERROR;
}

//=--------------------------------------------------------------------------=
// CMSMQMessage::put_SenderIdType
//=--------------------------------------------------------------------------=
// Sets sender id type for message
//
// Parameters:
//    lSenderIdType - [in] message's sender id type
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::put_SenderIdType(long lSenderIdType)
{
    switch (lSenderIdType) {
    case MQMSG_SENDERID_TYPE_NONE:
    case MQMSG_SENDERID_TYPE_SID:
   	  EnterCriticalSection(&m_CSlocal);
      	m_lSenderIdType = lSenderIdType;
      LeaveCriticalSection(&m_CSlocal);
      return NOERROR;
    default:
      return CreateErrorHelper(
               MQ_ERROR_ILLEGAL_PROPERTY_VALUE,
               m_ObjectType);
    }
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::get_PrivLevel
//=--------------------------------------------------------------------------=
// Gets message's privlevel
//
// Parameters:
//    plPrivLevel - [in] message's privlevel
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::get_PrivLevel(long FAR* plPrivLevel)
{
    *plPrivLevel = m_lPrivLevel;
    return NOERROR;
}

//=--------------------------------------------------------------------------=
// CMSMQMessage::put_PrivLevel
//=--------------------------------------------------------------------------=
// Sets privlevel for message
//
// Parameters:
//    lPrivLevel - [in] message's sender id type
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::put_PrivLevel(long lPrivLevel)
{
    switch (lPrivLevel) {
    case MQMSG_PRIV_LEVEL_NONE:
    case MQMSG_PRIV_LEVEL_BODY:
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
// CMSMQMessage::get_AuthLevel
//=--------------------------------------------------------------------------=
// Gets message's authlevel
//
// Parameters:
//    plAuthLevel - [in] message's authlevel
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::get_AuthLevel(long FAR* plAuthLevel)
{
    *plAuthLevel = m_lAuthLevel;
    return NOERROR;
}

//=--------------------------------------------------------------------------=
// CMSMQMessage::put_AuthLevel
//=--------------------------------------------------------------------------=
// Sets authlevel for message
//
// Parameters:
//    lAuthLevel - [in] message's authlevel
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::put_AuthLevel(long lAuthLevel)
{
    switch (lAuthLevel) {
    case MQMSG_AUTH_LEVEL_NONE:
    case MQMSG_AUTH_LEVEL_ALWAYS:
EnterCriticalSection(&m_CSlocal);
      m_lAuthLevel = lAuthLevel;
LeaveCriticalSection(&m_CSlocal);
      return NOERROR;
    default:
      return CreateErrorHelper(
               MQ_ERROR_ILLEGAL_PROPERTY_VALUE,
               m_ObjectType);
    }
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::get_IsAuthenticated
//=--------------------------------------------------------------------------=
//
// Parameters:
//    pisAuthenticated  [out]
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::get_IsAuthenticated(VARIANT_BOOL *pisAuthenticated)
{
    *pisAuthenticated = m_fAuthenticated;
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::get_Trace
//=--------------------------------------------------------------------------=
// Gets message's trace option
//
// Parameters:
//    plTrace - [in] message's trace option
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::get_Trace(long FAR* plTrace)
{
    *plTrace = m_lTrace;
    return NOERROR;
}

//=--------------------------------------------------------------------------=
// CMSMQMessage::put_Trace
//=--------------------------------------------------------------------------=
// Sets trace option for message
//
// Parameters:
//    trace - [in] message's trace option
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::put_Trace(long lTrace)
{
    switch (lTrace) {
    case MQMSG_TRACE_NONE:
    case MQMSG_SEND_ROUTE_TO_REPORT_QUEUE:
      EnterCriticalSection(&m_CSlocal);
      	m_lTrace = lTrace;
      LeaveCriticalSection(&m_CSlocal);
      return NOERROR;
    default:
      return CreateErrorHelper(
               MQ_ERROR_ILLEGAL_PROPERTY_VALUE,
               m_ObjectType);
    }
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::get_Priority
//=--------------------------------------------------------------------------=
// Gets message's priority
//
// Parameters:
//    plPriority - [out] message's priority
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::get_Priority(long FAR* plPriority)
{
    *plPriority = m_lPriority;
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::put_Priority
//=--------------------------------------------------------------------------=
// Sets message's priority
//
// Parameters:
//    lPriority - [in] message's priority
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::put_Priority(long lPriority)
{
  if ((lPriority >= MQ_MIN_PRIORITY) &&
      (lPriority <= MQ_MAX_PRIORITY)) {
    EnterCriticalSection(&m_CSlocal);
    	m_lPriority = lPriority;
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
// CMSMQMessage::get_Journal
//=--------------------------------------------------------------------------=
// Gets message's journaling option
//
// Parameters:
//    plJournal - [out] message's journaling option
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::get_Journal(long FAR* plJournal)
{
    *plJournal = m_lJournal;
    return NOERROR;
}

#define MQMSG_JOURNAL_MASK  (MQMSG_JOURNAL_NONE | \
                              MQMSG_DEADLETTER | \
                              MQMSG_JOURNAL)

//=--------------------------------------------------------------------------=
// CMSMQMessage::put_Journal
//=--------------------------------------------------------------------------=
// Sets journaling option for message
//
// Parameters:
//    lJournal - [in] message's admin queue
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::put_Journal(long lJournal)
{
    //
    // ensure that no bits are set in incoming lJournal
    //  flags word other than our mask.
    //
    if (lJournal & ~MQMSG_JOURNAL_MASK) {
      return CreateErrorHelper(
               MQ_ERROR_ILLEGAL_PROPERTY_VALUE,
               m_ObjectType);
    }
    else {
    EnterCriticalSection(&m_CSlocal);
      m_lJournal = lJournal;
    LeaveCriticalSection(&m_CSlocal);
      return NOERROR;
    }
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::get_ResponseQueueInfo
//=--------------------------------------------------------------------------=
// Gets Response queue for message
//
// Parameters:
//    ppqResponse - [out] message's Response queue
//
// Output:
//
// Notes:
//    caller must Release returned obj pointer.
//
HRESULT CMSMQMessage::get_ResponseQueueInfo(
    IMSMQQueueInfo FAR* FAR* ppqinfoResponse)
{
    *ppqinfoResponse = m_pqinfoResponse;
    ADDREF(*ppqinfoResponse);
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::putref_ResponseQueueInfo
//=--------------------------------------------------------------------------=
// Sets Response queue for message
//
// Parameters:
//    pqResponse - [in] message's Response queue
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::putref_ResponseQueueInfo(
    IMSMQQueueInfo FAR* pqinfoResponse)
{
    BSTR bstrFormatName = NULL;
    HRESULT hresult = NOERROR;


	EnterCriticalSection(&m_CSlocal);
	
    RELEASE(m_pqinfoResponse);
    m_pqinfoResponse = pqinfoResponse;
    ADDREF(m_pqinfoResponse);
    //
    // Update our resp formatname buffer
    //
    if (m_pqinfoResponse) {
      IfFailRet(m_pqinfoResponse->get_FormatName(&bstrFormatName));
      ASSERT(SysStringByteLen(bstrFormatName) < FORMAT_NAME_INIT_BUFFER,
             L"resp queue buffer too short.");
      memcpy(m_pwszRespQueue,
             bstrFormatName,
             SysStringByteLen(bstrFormatName));
      m_cchRespQueue = SysStringLen(bstrFormatName);
      //
      // null terminate
      //
      memset(&m_pwszRespQueue[m_cchRespQueue], 0, sizeof(WCHAR));
    }

    LeaveCriticalSection(&m_CSlocal);
    
    SysFreeString(bstrFormatName);
    return hresult;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::get_AppSpecific
//=--------------------------------------------------------------------------=
// Gets message's app specific info of message
//
// Parameters:
//    plAppSpecific - [out] message's app specific info
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::get_AppSpecific(long FAR* plAppSpecific)
{
    *plAppSpecific = m_lAppSpecific;
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::put_AppSpecific
//=--------------------------------------------------------------------------=
// Sets app specific info for message
//
// Parameters:
//    lAppSpecific - [in] message's app specific info
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::put_AppSpecific(long lAppSpecific)
{
EnterCriticalSection(&m_CSlocal);
    m_lAppSpecific = lAppSpecific;
LeaveCriticalSection(&m_CSlocal);
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::get_SentTime
//=--------------------------------------------------------------------------=
// Gets message's sent time
//
// Parameters:
//    pvarSentTime - [out] time message was sent
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::get_SentTime(VARIANT FAR* pvarSentTime)
{
    return GetVariantTimeOfTime(m_lSentTime, pvarSentTime);
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::get_ArrivedTime
//=--------------------------------------------------------------------------=
// Gets message's arrival time
//
// Parameters:
//    pvarArrivedTime - [out] time message arrived
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::get_ArrivedTime(VARIANT FAR* pvarArrivedTime)
{
    return GetVariantTimeOfTime(m_lArrivedTime, pvarArrivedTime);
}



//=--------------------------------------------------------------------------=
// CMSMQMessage::AttachCurrentSecurityContext
//=--------------------------------------------------------------------------=
// Sets message's security context from current security context.
//
// Parameters:
//
// Output:
 //
// Notes:
//
HRESULT CMSMQMessage::AttachCurrentSecurityContext()
{
    //
    // pass binSenderCert property if set otherwise
    //  use default cert
    //
return E_NOTIMPL;
#if 0
#if DEBUG
    if (m_cbSenderCert) {
      ASSERT(m_rgbSenderCert, L"should have sender cert buffer.");
    }
#endif // DEBUG
    return MQGetSecurityContext(
               m_rgbSenderCert,
               m_cbSenderCert,
               (HANDLE *)&m_lSecurityContext);
#endif // 0
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::get_MaxTimeToReachQueue
//=--------------------------------------------------------------------------=
// Gets message's lifetime
//
// Parameters:
//    plMaxTimeToReachQueue - [out] message's lifetime
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::get_MaxTimeToReachQueue(long FAR* plMaxTimeToReachQueue)
{
    *plMaxTimeToReachQueue = m_lMaxTimeToReachQueue;
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::put_MaxTimeToReachQueue
//=--------------------------------------------------------------------------=
// Sets message's lifetime
//
// Parameters:
//    lMaxTimeToReachQueue - [in] message's lifetime
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::put_MaxTimeToReachQueue(long lMaxTimeToReachQueue)
{
EnterCriticalSection(&m_CSlocal);
    m_lMaxTimeToReachQueue = lMaxTimeToReachQueue;
LeaveCriticalSection(&m_CSlocal);
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::get_MaxTimeToReceive
//=--------------------------------------------------------------------------=
// Gets message's lifetime
//
// Parameters:
//    plMaxTimeToReceive - [out] message's lifetime
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::get_MaxTimeToReceive(long FAR* plMaxTimeToReceive)
{
    *plMaxTimeToReceive = m_lMaxTimeToReceive;
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::put_MaxTimeToReceive
//=--------------------------------------------------------------------------=
// Sets message's lifetime
//
// Parameters:
//    lMaxTimeToReceive - [in] message's lifetime
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::put_MaxTimeToReceive(long lMaxTimeToReceive)
{
EnterCriticalSection(&m_CSlocal);
    m_lMaxTimeToReceive = lMaxTimeToReceive;
LeaveCriticalSection(&m_CSlocal);
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::GetVarBody
//=--------------------------------------------------------------------------=
// Gets message body
//
// Parameters:
//    pvarBody - [out] pointer to message body variant
//
// Output:
//
// Notes:
//    Supports intrinsic variant types
//
HRESULT CMSMQMessage::GetVarBody(VARIANT FAR* pvarBody)
{
    VARTYPE vt = m_vtBody;
    WCHAR *wszTmp = NULL;
    UINT cchBody;
    HRESULT hresult = NOERROR;

    //
    // UNDONE: VT_BYREF?
    //
    switch (vt) {
    case VT_I2:
    case VT_UI2:
      pvarBody->iVal = *(short *)m_pbBody;
      break;
    case VT_I4:
    case VT_UI4:
      pvarBody->lVal = *(long *)m_pbBody;
      break;
    case VT_R4:
      pvarBody->fltVal = *(float *)m_pbBody;
      break;
    case VT_R8:
      pvarBody->dblVal = *(double *)m_pbBody;
      break;
    case VT_CY:
      pvarBody->cyVal = *(CY *)m_pbBody;
      break;
    case VT_DATE:
      pvarBody->date = *(DATE *)m_pbBody;
      break;
    case VT_BOOL:
      pvarBody->boolVal = *(VARIANT_BOOL *)m_pbBody;
      break;
    case VT_I1:
    case VT_UI1:
      pvarBody->bVal = *(UCHAR *)m_pbBody;
      break;
    case VT_LPSTR:
      //
      // coerce ansi to unicode
      //
      // alloc large enough unicode buffer
      IfNullGo(wszTmp = new WCHAR[m_cbBody * 2]);
      cchBody = MultiByteToWideChar(CP_ACP,
                                    0,
                                    (LPCSTR)m_pbBody,
                                    -1,
                                    wszTmp,
                                    m_cbBody * 2);
      if (cchBody != 0) {
        IfNullGo(pvarBody->bstrVal = SysAllocString(wszTmp));
      }
      else {
        IfFailGo(hresult = E_OUTOFMEMORY);
      }
      // map string to BSTR
      vt = VT_BSTR;
#if DEBUG
      RemBstrNode(pvarBody->bstrVal);
#endif // DEBUG
      break;
    case VT_LPWSTR:
      // map wide string to BSTR
      vt = VT_BSTR;
      //
      // fall through...
      //
    case VT_BSTR:
      // Construct bstr to return
      IfNullFail(pvarBody->bstrVal =
                   SysAllocStringByteLen(
                     (const char *)m_pbBody,
                     m_cbBody));
#if DEBUG
      RemBstrNode(pvarBody->bstrVal);
#endif // DEBUG
      break;
    default:
      IfFailGo(hresult = E_INVALIDARG);
      break;
    } // switch
    pvarBody->vt = vt;
    // fall through...

Error:
    delete [] wszTmp;
    return CreateErrorHelper(hresult, m_ObjectType);
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::UpdateBodyBuffer
//=--------------------------------------------------------------------------=
// Sets message body
//
// Parameters:
//  cbBody    [in]    body length
//  pvBody    [in]    pointer to body buffer
//
// Output:
//
// Notes:
//    updates m_hMem, m_pbBody, m_cbBody: might produce
//     empty message.
//
HRESULT CMSMQMessage::UpdateBodyBuffer(ULONG cbBody, void *pvBody)
{
    HRESULT hresult = NOERROR;

	EnterCriticalSection(&m_CSlocal);
    GLOBALFREE(m_hMem);
    m_pbBody = NULL;
    m_cbBody = 0;
    IfNullRet(m_hMem = GLOBALALLOC_MOVEABLE_NONDISCARD(cbBody));
    IfNullGo(m_pbBody = (BYTE *)GlobalLock(m_hMem));
    memcpy(m_pbBody, pvBody, cbBody);
    GLOBALUNLOCK(m_hMem);
    m_cbBody = cbBody;
    return hresult;

Error:
    GLOBALFREE(m_hMem);
	LeaveCriticalSection(&m_CSlocal);
    return hresult;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::GetStreamOfBody
//=--------------------------------------------------------------------------=
// Sets message body
//
// Parameters:
//  cbBody    [in]    body length
//  pvBody    [in]    pointer to body buffer
//  hMem      [in]    handle to body buffer
//  ppstm     [out]   points to stream inited with body
//
// Output:
//
// Notes:
//    Creates a new in-memory stream and inits it
//     with incoming buffer, resets the seek pointer
//     and returns the stream.
//

HRESULT CMSMQMessage::GetStreamOfBody(
    ULONG cbBody,
    void *pvBody,
    // HGLOBAL hMem,
    IStream **ppstm)
{
#if 0 // not yet implemented	
	HRESULT hresult;
    LARGE_INTEGER li;
    IStream *pstm = NULL;
    

    // pessimism
    *ppstm = NULL;
    HGLOBAL hMem = GlobalHandle(pvBody);
    ASSERT(hMem, L"bad handle.");

#if DEBUG
//    DWORD cbSize;
//    cbSize = GlobalSize(hMem);
#endif // DEBUG

    IfFailRet(CreateStreamOnHGlobal(
                  hMem,  // NULL,   // hGlobal
                  FALSE, // TRUE,   // fDeleteOnRelease
                  &pstm));

#if DEBUG
//    cbSize = GlobalSize(hMem);
#endif // DEBUG

    // reset stream seek pointer
    LISet32(li, 0);
    IfFailGo(pstm->Seek(li, STREAM_SEEK_SET, NULL));

#if DEBUG
    STATSTG statstg;

    IfFailGo(pstm->Stat(&statstg, STATFLAG_NONAME));
//    cbSize = GlobalSize(hMem);
//    ASSERT(cbSize >= cbBody, L"stream not big enough...");
#endif // DEBUG
#if 0
    ULARGE_INTEGER ulibSize;

    // set stream size
    ULISet32(ulibSize, cbBody);
    IfFailGo(pstm->SetSize(ulibSize));

    ULONG cbWritten;

    // write bytes to stream
    IfFailGo(pstm->Write((void const*)pvBody,
                          cbBody,
                          &cbWritten));
    ASSERT(cbBody == cbWritten, L"not all bytes written.");

      // reset stream seek pointer
    LISet32(li, 0);
    IfFailGo(pstm->Seek(li, STREAM_SEEK_SET, NULL));
#endif // 0
    *ppstm = pstm;
    return hresult;

Error:
    RELEASE(pstm);
    return hresult;
#endif // not yet implemented
    return E_NOTIMPL;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::GetStorageOfBody
//=--------------------------------------------------------------------------=
// Sets message body
//
// Parameters:
//  cbBody    [in]    body length
//  pvBody    [in]    pointer to body buffer
//  hMem      [in]    handle to body buffer
//  ppstg     [out]   pointer to storage inited with buffer
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::GetStorageOfBody(
    ULONG cbBody,
    void *pvBody,
    // HGLOBAL hMem,
    IStorage **ppstg)
{
#if 0 // not yet implemented
	HRESULT hresult;
    ULARGE_INTEGER ulibSize;
    ILockBytes *plockbytes = NULL;
    IStorage *pstg = NULL;
    

    // pessimism
    *ppstg = NULL;

    HGLOBAL hMem = GlobalHandle(pvBody);
    ASSERT(hMem, L"bad handle.");

    // have to create and init ILockBytes before stg creation
    IfFailRet(CreateILockBytesOnHGlobal(
                hMem,  // NULL,  // hGlobal
                FALSE, // TRUE,  // fDeleteOnRelease
                &plockbytes));

    // set ILockBytes size
    ULISet32(ulibSize, cbBody);
    IfFailGo(plockbytes->SetSize(ulibSize));

#if 0
    // write bytes to ILockBytes
    ULONG cbWritten;
    ULARGE_INTEGER uliOffset;
    ULISet32(uliOffset, 0);
    IfFailGo(plockbytes->WriteAt(
                           uliOffset,
                           (void const *)pvBody,
                           cbBody,
                           &cbWritten));
    ASSERT(cbBody == cbWritten, L"not all bytes written.");
#endif // 0
    hresult = StgOpenStorageOnILockBytes(
               plockbytes,
               NULL,    // pstPriority
               STGM_READWRITE | STGM_SHARE_EXCLUSIVE,
               NULL,    // SNB
               0,       //Reserved; must be zero
               &pstg);
    //
    // 1415: map FILEALREADYEXISTS to E_INVALIDARG
    // OLE returns former when bytearray exists (as it does
    //  since we just created one) but the contents aren't
    //  a storage -- e.g. when the message buffer isn't an
    //  object.
    //
    if (hresult == STG_E_FILEALREADYEXISTS) {
      IfFailGo(hresult = E_INVALIDARG);
    }

#if DEBUG
    STATSTG statstg, statstg2;
    IfFailGo(pstg->Stat(&statstg2, STATFLAG_NONAME));
    IfFailGo(plockbytes->Stat(&statstg, STATFLAG_NONAME));
#endif // DEBUG
    *ppstg = pstg;
    RELEASE(plockbytes);
    return hresult;

Error:
    RELEASE(plockbytes);
    RELEASE(pstg);
    return hresult;
#endif // not yet implemented
    return E_NOTIMPL;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::get_lenBody
//=--------------------------------------------------------------------------=
// Gets message body len
//
// Parameters:
//    pcbBody - [out] pointer to message body len
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::get_BodyLength(long *pcbBody)
{
    *pcbBody = m_cbBody;
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::get_Body
//=--------------------------------------------------------------------------=
// Gets message body
//
// Parameters:
//    pvarBody - [out] pointer to message body
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::get_Body(VARIANT FAR* pvarBody)
{
    HRESULT hresult = NOERROR;

    //
    // inspect PROPID_M_BODY_TYPE to learn how
    //  to interpret message
    //
    if (m_vtBody & VT_ARRAY) {
      hresult = GetBinBody(pvarBody);
    }
    else if (m_vtBody == VT_STREAMED_OBJECT) {

#if 0 // not implemented...yet?
      hresult = GetStreamedObject(pvarBody);
#else
	  hresult = E_NOTIMPL;
#endif
    }
    else if (m_vtBody == VT_STORED_OBJECT) {
#if 0 // not implemented...yet?   
      hresult = GetStoredObject(pvarBody);
#else
	  hresult = E_NOTIMPL;
#endif

    }
    else {
      hresult = GetVarBody(pvarBody);
    }
    return CreateErrorHelper(hresult, m_ObjectType);
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::GetBinBody
//=--------------------------------------------------------------------------=
// Gets binary message body
//
// Parameters:
//    pvarBody - [out] pointer to binary message body
//
// Output:
//
// Notes:
//  produces a 1D array of BYTEs in a variant.
//
HRESULT CMSMQMessage::GetBinBody(VARIANT FAR* pvarBody)
{
    SAFEARRAY *psa;
    SAFEARRAYBOUND rgsabound[1];
    long rgIndices[1];
    long i;
    HRESULT hresult = NOERROR, hresult2 = NOERROR;

    ASSERT(pvarBody, L"bad variant.");
    VariantClear(pvarBody);

    // create a 1D byte array
    rgsabound[0].lLbound = 0;
    rgsabound[0].cElements = m_cbBody;
    IfNullRet(psa = SafeArrayCreate(VT_UI1, 1, rgsabound));

    // if (m_pbBody) {
    if (m_hMem) {
      ASSERT(m_pbBody, L"should have pointer to body.");
      ASSERT(m_hMem == GlobalHandle(m_pbBody),
             L"bad handle.");
      //
      // now copy array
      //
      // BYTE *pbBody;
      // IfNullFail(pbBody = (BYTE *)GlobalLock(m_hMem));
      for (i = 0; i < m_cbBody; i++) {
        rgIndices[0] = i;
        IfFailGo(SafeArrayPutElement(psa, rgIndices, (VOID *)&m_pbBody[i]));
        // IfFailGo(SafeArrayPutElement(psa, rgIndices, (VOID *)&pbBody[i]));
      }
    }

    // set variant to reference safearray of bytes
    V_VT(pvarBody) = VT_ARRAY | VT_UI1;
    pvarBody->parray = psa;
    GLOBALUNLOCK(m_hMem);
    return hresult;

Error:
    hresult2 = SafeArrayDestroy(psa);
    if (FAILED(hresult2)) {
      return CreateErrorHelper(
               hresult2,
               m_ObjectType);
    }
    GLOBALUNLOCK(m_hMem);
    return CreateErrorHelper(
             hresult,
             m_ObjectType);
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::put_Body
//=--------------------------------------------------------------------------=
// Sets message body
//
// Parameters:
//    varBody - [in] message body
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::put_Body(VARIANT varBody)
{
    VARTYPE vtBody = varBody.vt;
    HRESULT hresult = NOERROR;

    if ((vtBody & VT_ARRAY) ||
        (vtBody == VT_UNKNOWN) ||
        (vtBody == VT_DISPATCH))  {

 #if 1 // not yet implemented
      hresult = PutBinBody(varBody);
 #else 
 		hresult = E_NOTIMPL;
 #endif
 
    }
    else {
      hresult = PutVarBody(varBody);
    }
    return CreateErrorHelper(
             hresult,
             m_ObjectType);
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::PutVarBody
//=--------------------------------------------------------------------------=
// Sets message body
//
// Parameters:
//    varBody [in]
//
// Output:
//
// Notes:
//    Supports intrinsic variant types
//
HRESULT CMSMQMessage::PutVarBody(VARIANT varBody)
{
    VARTYPE vt = V_VT(&varBody);
    void *pvBody;
    HRESULT hresult = NOERROR;
  
    
	EnterCriticalSection(&m_CSlocal);

    // UNDONE: VT_BYREF?
    switch (vt) {
    case VT_I2:
    case VT_UI2:
      m_cbBody = 2;
      pvBody = &varBody.iVal;
      break;
    case VT_I4:
    case VT_UI4:
      m_cbBody = 4;
      pvBody = &varBody.lVal;
      break;
    case VT_R4:
      m_cbBody = 4;
      pvBody = &varBody.fltVal;
    case VT_R8:
      m_cbBody = 8;
      pvBody = &varBody.dblVal;
      break;
    case VT_CY:
      m_cbBody = sizeof(CY);
      pvBody = &varBody.cyVal;
      break;
    case VT_DATE:
      m_cbBody = sizeof(DATE);
      pvBody = &varBody.date;
      break;
    case VT_BOOL:
      m_cbBody = sizeof(VARIANT_BOOL);
      pvBody = &varBody.boolVal;
      break;
    case VT_I1:
    case VT_UI1:
      m_cbBody = 1;
      pvBody = &varBody.bVal;
      break;
    case VT_BSTR:
      BSTR bstrBody;

      IfFailGo(GetTrueBstr(&varBody, &bstrBody));
      m_cbBody = SysStringByteLen(bstrBody);
      pvBody = bstrBody;
      break;
    default:
      IfFailGo(hresult = E_INVALIDARG);
      break;
    } // switch
    m_vtBody = vt;
    hresult = UpdateBodyBuffer((ULONG)m_cbBody, pvBody ? pvBody : L"");
    // fall through...

Error:
	
	LeaveCriticalSection(&m_CSlocal);
    return CreateErrorHelper(hresult, m_ObjectType);
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::PutBinBody
//=--------------------------------------------------------------------------=
// Sets message body
//
// Parameters:
//    psaBody - [in] binary message body
//
// Output:
//
// Notes:
//    Supports arrays of any type
//     and persistent ActiveX objects:
//     i.e. objects that support IPersistStream | IPersistStorage
//           and IDispatch.
//

HRESULT CMSMQMessage::PutBinBody(VARIANT varBody)
{

    SAFEARRAY *psa = NULL;
    UINT nDim, i, cbElem, cbBody;
    long lLBound, lUBound;
    VOID *pvData;
    VARTYPE vt = varBody.vt;
    IPersistStream *ppersstm = NULL;
    IPersistStorage *ppersstg = NULL;
    
#if 0 // not used outside of "unimplemented" areas
    IUnknown *punk = NULL;
    IDispatch *pdisp = NULL;
 	LARGE_INTEGER li;
    ULARGE_INTEGER ulibMax;
    STATSTG statstg;
#endif // 0 

    MSGTYPE msgtype;
    ILockBytes *plockbytes = NULL;
    IStream *pstm = NULL;
    IStorage *pstg = NULL;
    BOOL	bLocked = FALSE;
    HRESULT hresult = NOERROR;

    cbBody = 0;
    GLOBALFREE(m_hMem);
    m_pbBody = NULL;

    switch (vt) {
    case VT_DISPATCH:

		return E_INVALIDARG;
#if 0 // not implemented
      //
      // QI to IPersistStream
      //
      pdisp = varBody.pdispVal;
      if (pdisp == NULL) {
        return E_INVALIDARG;
      }
      hresult = pdisp->QueryInterface(IID_IPersistStream,
                                     (LPVOID *)&ppersstm);

      // try IPersistStorage...
      if (FAILED(hresult)) {
        IfFailGo(pdisp->QueryInterface(IID_IPersistStorage,
                                       (LPVOID *)&ppersstg));
        msgtype = MSGTYPE_STORAGE;
      }
      else {
        msgtype = MSGTYPE_STREAM;
      }
 #endif // 0 not implemented
      break;
    case VT_UNKNOWN:
		return E_INVALIDARG;
#if 0 // not implemnented
      //
      // QI to IPersistStream
      //
      punk = varBody.punkVal;
      if (punk == NULL) {
        return E_INVALIDARG;
      }
      //
      hresult = punk->QueryInterface(IID_IPersistStream,
                                     (LPVOID *)&ppersstm);

      // try IPersistStorage...
      if (FAILED(hresult)) {
        IfFailGo(punk->QueryInterface(IID_IPersistStorage,
                                      (LPVOID *)&ppersstg));
        msgtype = MSGTYPE_STORAGE;
      }
      else {
        msgtype = MSGTYPE_STREAM;
      }
#endif // 0 not implemented
      break;
    default:
      msgtype = MSGTYPE_BINARY;
    } // switch

	
	// Lock us down so other threads won't write a message behind our backs
	EnterCriticalSection(&m_CSlocal);
	bLocked = TRUE;
    //
    // allocate new global handle of size 0
    //
    IfNullFail(m_hMem = GLOBALALLOC_MOVEABLE_NONDISCARD(0));
    switch (msgtype) {
    case MSGTYPE_STREAM:
#if 0 // not implemented

      hresult = CreateStreamOnHGlobal(
                  m_hMem, // NULL,   // hGlobal
                  FALSE,  // TRUE,   // fDeleteOnRelease
                  &pstm);

      // reset stream seek pointer
      LISet32(li, 0);
      IfFailGo(pstm->Seek(li, STREAM_SEEK_SET, NULL));

      // save
      IfFailGo(OleSaveToStream(ppersstm, pstm));

      // How big is our stream?
      IfFailGo(pstm->Stat(&statstg, STATFLAG_NONAME));
      ulibMax = statstg.cbSize;
      if (ulibMax.HighPart != 0) {
        IfFailGo(hresult = E_INVALIDARG);
      }
      cbBody = ulibMax.LowPart;
      m_vtBody = VT_STREAMED_OBJECT;
 #endif // 0
      break;
    case MSGTYPE_BINARY:
      //
      // array: compute byte count
      //
      psa = varBody.parray;
      if (psa) {
        nDim = SafeArrayGetDim(psa);
        cbElem = SafeArrayGetElemsize(psa);
        for (i = 1; i <= nDim; i++) {
          IfFailGo(SafeArrayGetLBound(psa, i, &lLBound));
          IfFailGo(SafeArrayGetUBound(psa, i, &lUBound));
          cbBody += (lUBound - lLBound + 1) * cbElem;
        }
        IfFailGo(SafeArrayAccessData(psa, &pvData));
        IfFailGo(UpdateBodyBuffer(cbBody, pvData));
      }
      m_vtBody = VT_ARRAY | VT_UI1;
      break;
    case MSGTYPE_STORAGE:
#if 0 // not implemented
      //
      // Always create a new storage object.
      // REVIEW: Be nice if, as we do for streams, we could
      //  cache an in-memory storage and reuse --
      //  but I know of no way to reset a storage.
      //
      IfFailGo(CreateILockBytesOnHGlobal(
                  m_hMem, // NULL,  // hGlobal
                  FALSE,  // TRUE,  // fDeleteOnRelease
                  &plockbytes));
      IfFailGo(StgCreateDocfileOnILockBytes(
                 plockbytes,
                 STGM_CREATE |
                  STGM_READWRITE |
                  STGM_SHARE_EXCLUSIVE,
                 0,             //Reserved; must be zero
                 &pstg));
      IfFailGo(OleSave(ppersstg, pstg, FALSE /* fSameAsLoad */));
#if DEBUG
      STATSTG statstg2;

      IfFailGo(pstg->Stat(&statstg2, STATFLAG_NONAME));
      DWORD cbSize;
	  // use LocalSize on WinCE
      cbSize = LocalSize(m_hMem);
#endif // DEBUG

      // now get underlying ILockBytes stats
      IfFailGo(plockbytes->Stat(&statstg, STATFLAG_NONAME));
      ulibMax = statstg.cbSize;
      ASSERT(ulibMax.HighPart == 0, L"storage too large.");
      cbBody = ulibMax.LowPart;
      m_vtBody = VT_STORED_OBJECT;
#endif // 0
      break;
    default:
      ASSERT(0, L"unreachable?");
      break;
    } // switch

    m_cbBody = cbBody;
    m_pbBody = (BYTE *)GlobalLock(m_hMem);
    ASSERT(m_pbBody, L"should have valid pointer.");
    GLOBALUNLOCK(m_hMem);

    // fall through...

Error:
	if(bLocked)
		LeaveCriticalSection(&m_CSlocal);
		
    if (psa) {
      SafeArrayUnaccessData(psa);
    }
    RELEASE(ppersstm);
    RELEASE(ppersstg);
    RELEASE(plockbytes);
    RELEASE(pstm);
    RELEASE(pstg);
    return CreateErrorHelper(
               hresult,
               m_ObjectType);
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::GetStreamedObject
//=--------------------------------------------------------------------------=
// Produce streamed object from binary message body
//
// Parameters:
//    pvarBody - [out] pointer to object
//
// Output:
//
// Notes:
//  Object must implement IPersistStream
//

#if 0 // Not yet implemented
HRESULT CMSMQMessage::GetStreamedObject(VARIANT FAR* pvarBody)
{
	
	HRESULT hresult = NOERROR;

    IUnknown *punk = NULL;
    IDispatch *pdisp = NULL;
    IStream *pstm = NULL;
    IPersistStream *ppersstm = NULL;
    

    ASSERT(pvarBody, L"bad variant.");
    VariantClear(pvarBody);

    // Attempt to load from an in-memory stream
    if (m_hMem) {
      ASSERT(m_hMem == GlobalHandle(m_pbBody), L"bad handle.");
      IfFailGo(GetStreamOfBody(m_cbBody, m_pbBody, &pstm));

      // load
      IfFailGo(OleLoadFromStream(pstm,
                                 IID_IPersistStream,
                                 (void **)&ppersstm));
      //
      // Supports IDispatch? if not, return IUnknown
      //
      IfFailGo(ppersstm->QueryInterface(IID_IUnknown,
                                        (LPVOID *)&punk));
      RELEASE(ppersstm);
      hresult = punk->QueryInterface(IID_IDispatch,
                                     (LPVOID *)&pdisp);
      if (SUCCEEDED(hresult)) {
        //
        // Setup returned object
        //
        V_VT(pvarBody) = VT_DISPATCH;
        pvarBody->pdispVal = pdisp;
        ADDREF(pvarBody->pdispVal);   // ownership transfers
      }
      else {
        //
        // return IUnknown interface
        //
        V_VT(pvarBody) = VT_UNKNOWN;
        pvarBody->punkVal = punk;
        ADDREF(pvarBody->punkVal);   // ownership transfers
      }
    }
    else {
      V_VT(pvarBody) = VT_ERROR;
    }

    // fall through...

Error:
    RELEASE(punk);
    RELEASE(pstm);
    RELEASE(pdisp);
    RELEASE(ppersstm);

    return CreateErrorHelper(hresult, m_ObjectType);
}
#endif // 0

//=--------------------------------------------------------------------------=
// CMSMQMessage::GetStoredObject
//=--------------------------------------------------------------------------=
// Produce stored object from binary message body
//
// Parameters:
//    pvarBody - [out] pointer to object
//
// Output:
//
// Notes:
//  Object must implement IPersistStorage
//
#if 0 // not yet implemented
HRESULT CMSMQMessage::GetStoredObject(VARIANT FAR* pvarBody)
{
    IUnknown *punk = NULL;
    IDispatch *pdisp = NULL;
    IStorage *pstg = NULL;
    IPersistStorage *ppersstg = NULL;
    HRESULT hresult = NOERROR;
#if DEBUG
    LPOLESTR pwszGuid;
#endif // DEBUG
    ASSERT(pvarBody, L"bad variant.");
    VariantClear(pvarBody);

    // Attempt to load from an in-memory storage
    if (m_hMem) {
      ASSERT(m_hMem == GlobalHandle(m_pbBody), L"bad handle.");
      //
      // try to load as a storage
      //
      IfFailGo(GetStorageOfBody(m_cbBody, m_pbBody, &pstg));
#if 0
      //
      // UNDONE: for some reason this doesn't work -- i.e. the returned
      //  object doesn't support IDispatch...
      //
      IfFailGo(OleLoad(pstg,
                       IID_IPersistStorage,
                       NULL,  //Points to the client site for the object
                       (void **)&ppersstg));
#else
      CLSID clsid;
      IfFailGo(ReadClassStg(pstg, &clsid))
      IfFailGo(CoCreateInstance(
                 clsid,
                 NULL,
                 CLSCTX_SERVER,
                 IID_IPersistStorage,
                 (LPVOID *)&ppersstg));
      IfFailGo(ppersstg->Load(pstg));
#endif // 0
#if DEBUG
      // get clsid
      STATSTG statstg;

      IfFailGo(pstg->Stat(&statstg, STATFLAG_NONAME));
      StringFromCLSID(statstg.clsid, &pwszGuid);
#endif // DEBUG
#if 0
      //
      // Now setup returned object
      //
      V_VT(pvarBody) = VT_DISPATCH;
      pvarBody->pdispVal = pdisp;
      ADDREF(pvarBody->pdispVal);   // ownership transfers
#else
      //
      // Supports IDispatch? if not, return IUnknown
      //
      IfFailGo(ppersstg->QueryInterface(IID_IUnknown,
                                        (LPVOID *)&punk));
      hresult = punk->QueryInterface(IID_IDispatch,
                                     (LPVOID *)&pdisp);
      if (SUCCEEDED(hresult)) {
        //
        // Setup returned object
        //
        V_VT(pvarBody) = VT_DISPATCH;
        pvarBody->pdispVal = pdisp;
        ADDREF(pvarBody->pdispVal);   // ownership transfers
      }
      else {
        //
        // return IUnknown interface
        //
        V_VT(pvarBody) = VT_UNKNOWN;
        pvarBody->punkVal = punk;
        ADDREF(pvarBody->punkVal);   // ownership transfers
      }
#endif // 0
    }
    else {
      V_VT(pvarBody) = VT_ERROR;
    }

    // fall through...

Error:
    RELEASE(punk);
    RELEASE(pstg);
    RELEASE(ppersstg);
    RELEASE(pdisp);
    return CreateErrorHelper(hresult, m_ObjectType);
}
#endif


//=--------------------------------------------------------------------------=
// CMSMQMessage::get_SenderId
//=--------------------------------------------------------------------------=
// Gets binary sender id
//
// Parameters:
//    pvarSenderId - [out] pointer to binary sender id
//
// Output:
//
// Notes:
//  produces a 1D array of BYTEs in a variant.
//
HRESULT CMSMQMessage::get_SenderId(VARIANT FAR* pvarSenderId)
{
    return PutSafeArrayOfBuffer(
             m_rgbSenderId,
             m_cbSenderId,
             pvarSenderId);
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::get_SenderCertificate
//=--------------------------------------------------------------------------=
// Gets binary sender id
//
// Parameters:
//    pvarSenderCert - [out] pointer to binary sender certificate
//
// Output:
//
// Notes:
//  produces a 1D array of BYTEs in a variant.
//
HRESULT CMSMQMessage::get_SenderCertificate(VARIANT FAR* pvarSenderCert)
{
    return PutSafeArrayOfBuffer(
             m_rgbSenderCert,
             m_cbSenderCert,
             pvarSenderCert);
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::put_SenderCertificate
//=--------------------------------------------------------------------------=
// Sets binary sender certificate
//
// Parameters:
//    psaSenderCert - [in] binary sender certificate
//
// Output:
//
// Notes:
//    Supports arrays of any type.
//
HRESULT CMSMQMessage::put_SenderCertificate(VARIANT varSenderCert)
{
	HRESULT hr;
	EnterCriticalSection(&m_CSlocal);
	
    hr =  GetSafeArrayOfVariant(
             &varSenderCert,
             &m_rgbSenderCert,
             &m_cbSenderCert);
             
    LeaveCriticalSection(&m_CSlocal);
    return hr;
             
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::get_AdminQueueInfo
//=--------------------------------------------------------------------------=
// Gets admin queue for message
//
// Parameters:
//    ppqinfoAdmin - [out] message's admin queue
//
// Output:
//
// Notes:
//    caller must Release returned obj pointer.
//
HRESULT CMSMQMessage::get_AdminQueueInfo(
    IMSMQQueueInfo FAR* FAR* ppqinfoAdmin)
{
    *ppqinfoAdmin = m_pqinfoAdmin;
    ADDREF(*ppqinfoAdmin);
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::putref_AdminQueueInfo
//=--------------------------------------------------------------------------=
// Sets admin queue for message
//
// Parameters:
//    pqinfoAdmin - [in] message's admin queue
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::putref_AdminQueueInfo(
    IMSMQQueueInfo FAR* pqinfoAdmin)
{
    BSTR bstrFormatName = NULL;
    HRESULT hresult = NOERROR;


	EnterCriticalSection(&m_CSlocal);
	
    RELEASE(m_pqinfoAdmin);
    m_pqinfoAdmin = pqinfoAdmin;
    ADDREF(m_pqinfoAdmin);
    //
    // Update our admin formatname buffer
    //
    if (m_pqinfoAdmin) {
      IfFailRet(m_pqinfoAdmin->get_FormatName(&bstrFormatName));
      ASSERT(SysStringByteLen(bstrFormatName) < FORMAT_NAME_INIT_BUFFER,
             L"admin queue buffer too short.");
      memcpy(m_pwszAdminQueue,
             bstrFormatName,
             SysStringByteLen(bstrFormatName));
      m_cchAdminQueue = SysStringLen(bstrFormatName);
      //
      // null terminate
      //
      memset(&m_pwszAdminQueue[m_cchAdminQueue], 0, sizeof(WCHAR));
    }
 	LeaveCriticalSection(&m_CSlocal);
 	
    SysFreeString(bstrFormatName);
    return hresult;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::get_DestinationQueueInfo
//=--------------------------------------------------------------------------=
// Gets destination queue for message
//
// Parameters:
//    ppqinfoDest - [out] message's destination queue
//
// Output:
//
// Notes:
//    caller must Release returned obj pointer.
//
HRESULT CMSMQMessage::get_DestinationQueueInfo(
    IMSMQQueueInfo FAR* FAR* ppqinfoDest)
{
    *ppqinfoDest = m_pqinfoDest;
    ADDREF(*ppqinfoDest);
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::get_Id
//=--------------------------------------------------------------------------=
// Gets message id
//
// Parameters:
//    pvarId - [out] message's id
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::get_Id(VARIANT *pvarMsgId)
{
    return PutSafeArrayOfBuffer(
             m_rgbMsgId,
             m_cbMsgId,
             pvarMsgId);
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::get_CorrelationId
//=--------------------------------------------------------------------------=
// Gets message correlation id
//
// Parameters:
//    pvarCorrelationId - [out] message's correlation id
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::get_CorrelationId(VARIANT *pvarCorrelationId)
{
    return PutSafeArrayOfBuffer(
             m_rgbCorrelationId,
             m_cbCorrelationId,
             pvarCorrelationId);
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::put_CorrelationId
//=--------------------------------------------------------------------------=
// Sets message's correlation id
//
// Parameters:
//    varCorrelationId - [in] message's correlation id
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::put_CorrelationId(VARIANT varCorrelationId)
{
	HRESULT hr;
	
	EnterCriticalSection(&m_CSlocal);
    hr = GetSafeArrayOfVariant(
             &varCorrelationId,
             &m_rgbCorrelationId,
             &m_cbCorrelationId);
	LeaveCriticalSection(&m_CSlocal);

    return hr;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::get_Ack
//=--------------------------------------------------------------------------=
// Gets message's acknowledgement
//
// Parameters:
//    pmsgack - [out] message's acknowledgement
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::get_Ack(long FAR* plAck)
{
    *plAck = m_lAck;
    return NOERROR;
}

#define MQMSG_ACK_MASK     (MQMSG_ACKNOWLEDGMENT_NONE | \
                            MQMSG_ACKNOWLEDGMENT_FULL_REACH_QUEUE | \
                            MQMSG_ACKNOWLEDGMENT_FULL_RECEIVE | \
                            MQMSG_ACKNOWLEDGMENT_NACK_REACH_QUEUE | \
                            MQMSG_ACKNOWLEDGMENT_NACK_RECEIVE)

//=--------------------------------------------------------------------------=
// CMSMQMessage::put_acknowledge
//=--------------------------------------------------------------------------=
// Sets message's acknowledgement
//
// Parameters:
//    msgack - [in] message's acknowledgement
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::put_Ack(long lAck)
{
    //
    // ensure that no bits are set in incoming lAck
    //  flags word other than our mask.
    //
    if (lAck & ~MQMSG_ACK_MASK) {
      return CreateErrorHelper(
               MQ_ERROR_ILLEGAL_PROPERTY_VALUE,
               m_ObjectType);
    }
    else {

    EnterCriticalSection(&m_CSlocal);
      m_lAck = lAck;
    LeaveCriticalSection(&m_CSlocal);
      return NOERROR;
    }
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::get_Label
//=--------------------------------------------------------------------------=
// Gets message label
//
// Parameters:
//    pbstrLabel - [out] pointer to message label
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::get_Label(BSTR FAR* pbstrLabel)
{

    if (m_cchLabel) {
      IfNullRet(*pbstrLabel = SysAllocStringLen(m_pwszLabel, m_cchLabel));
    }
    else {
      IfNullRet(*pbstrLabel = SysAllocString(L""));
    }
    
#if DEBUG
    RemBstrNode(*pbstrLabel);
#endif // DEBUG
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::put_Label
//=--------------------------------------------------------------------------=
// Sets message label
//
// Parameters:
//    bstrLabel - [in] message label
//
// Output:
//
// Notes:
//
HRESULT CMSMQMessage::put_Label(BSTR bstrLabel)
{
    UINT cch = SysStringLen(bstrLabel);

    if (cch > MQ_MAX_MSG_LABEL_LEN) {
      return CreateErrorHelper(MQ_ERROR_LABEL_TOO_LONG, m_ObjectType);
    }
    EnterCriticalSection(&m_CSlocal);
    memcpy(m_pwszLabel, bstrLabel, SysStringByteLen(bstrLabel));
    m_cchLabel = cch;
    //
    // null terminate
    //
    memset(&m_pwszLabel[m_cchLabel], 0, sizeof(WCHAR));
    LeaveCriticalSection(&m_CSlocal);
    return NOERROR;
}

//=--------------------------------------------------------------------------=
// CMSMQMessage::get_guidSrcMachine
//=--------------------------------------------------------------------------=
//
// Parameters:
//    pbstrGuidSrcMachine  [out] message's source machine s guid
//
// Output:
//    HRESULT       - S_OK, E_NOINTERFACE
//
// Notes:
//
HRESULT CMSMQMessage::get_SourceMachineGuid(BSTR *pbstrGuidSrcMachine)
{
    int cbStr;
   WCHAR awcMachineName[(LENSTRCLSID + 2) * 2];

    *pbstrGuidSrcMachine = SysAllocStringLen(NULL, LENSTRCLSID - 2);
    if (*pbstrGuidSrcMachine) {
      cbStr = StringFromGUID2(*m_pguidSrcMachine, awcMachineName, LENSTRCLSID*2);
     wcsncpy( *pbstrGuidSrcMachine, &awcMachineName[1], LENSTRCLSID - 2 );

#if DEBUG
      RemBstrNode(*pbstrGuidSrcMachine);
#endif // DEBUG
      return cbStr == 0 ? E_OUTOFMEMORY : NOERROR;
    }
    else {
      return E_OUTOFMEMORY;
    }
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::Send
//=--------------------------------------------------------------------------=
// Sends this message to a queue.
//
// Parameters:
//    pqDest        - [in] destination queue
//    varTransaction  [in, optional]
//
// Output:
//
// Notes:
// 
HRESULT CMSMQMessage::Send(
    IMSMQQueue FAR* pqDest,
    VARIANT *pvarTransaction)
{
    MQMSGPROPS msgprops;
    QUEUEHANDLE lHandleDest;
    ITransaction *ptransaction = NULL;
    BOOL isRealXact = NULL;
    HRESULT hresult = NOERROR;

    if (pqDest == NULL) {
      return E_INVALIDARG;
    }
    pqDest->get_Handle((long *)&lHandleDest);

    // update msgprops with contents of data members
    IfFailGo(CreateSendMessageProps(&msgprops));
    //
    // get optional transaction...
    //

    IfFailGo(GetOptionalTransaction(
               pvarTransaction,
               &ptransaction,
               &isRealXact));

    //
    // and finally send the message...
    //
    IfFailGo(MQSendMessage(lHandleDest,
                           (MQMSGPROPS *)&msgprops,
                           NULL));   // was ptransaction
    IfFailGo(UpdateMsgId(&msgprops));
    // fall through...

Error:
    FreeMessageProps(&msgprops);
	/*
    if (isRealXact) {
      RELEASE(ptransaction);
    }
	*/
    return CreateErrorHelper(hresult, m_ObjectType);
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::CreateSendPropIdArray
//=--------------------------------------------------------------------------=
// Creates and populates a propid array from the current
//  non-default message props before sending a message.
//
// Parameters:
//    pcPropRec  [out]
//    prgproprec [out]
//
// Output:
//    HRESULT       - S_OK, E_OUTOFMEMORY
//
// Notes:
//
HRESULT CMSMQMessage::CreateSendPropIdArray(
    UINT *pcPropRec,
    PROPREC **prgproprec)
{
    HRESULT hresult = NOERROR;

    // The must haves...some commented out (as yet unsupported in CE)
    IfFailRet(AddPropRec(PROPID_M_DELIVERY, prgproprec, pcPropRec));
    IfFailRet(AddPropRec(PROPID_M_PRIORITY, prgproprec, pcPropRec));
    IfFailRet(AddPropRec(PROPID_M_JOURNAL, prgproprec, pcPropRec));
    IfFailRet(AddPropRec(PROPID_M_ACKNOWLEDGE, prgproprec, pcPropRec));
    IfFailRet(AddPropRec(PROPID_M_TRACE, prgproprec, pcPropRec));
    // IfFailRet(AddPropRec(PROPID_M_PRIV_LEVEL, prgproprec, pcPropRec));
    IfFailRet(AddPropRec(PROPID_M_AUTH_LEVEL, prgproprec, pcPropRec));
    // IfFailRet(AddPropRec(PROPID_M_AUTHENTICATED, prgproprec, pcPropRec));
    // IfFailRet(AddPropRec(PROPID_M_HASH_ALG, prgproprec, pcPropRec));
    // IfFailRet(AddPropRec(PROPID_M_ENCRYPTION_ALG, prgproprec, pcPropRec));
 // IfFailRet(AddPropRec(PROPID_M_MSGID, prgproprec, pcPropRec));
    IfFailRet(AddPropRec(PROPID_M_APPSPECIFIC, prgproprec, pcPropRec));
    IfFailRet(AddPropRec(PROPID_M_BODY_TYPE, prgproprec, pcPropRec));
    // IfFailRet(AddPropRec(PROPID_M_SENDERID_TYPE, prgproprec, pcPropRec));

    // not necessarily...
    if (m_hMem) {
      IfFailRet(AddPropRecOptional(PROPID_M_BODY, prgproprec, pcPropRec));
      // IfFailRet(AddPropRecOptional(PROPID_M_BODY_SIZE, prgproprec, pcPropRec));
    }
    if (m_cchRespQueue) {
      IfFailRet(AddPropRec(PROPID_M_RESP_QUEUE, prgproprec, pcPropRec));
      // IfFailRet(AddPropRec(PROPID_M_RESP_QUEUE_LEN, prgproprec, pcPropRec));
    }
    if (m_cchAdminQueue) {
      IfFailRet(AddPropRec(PROPID_M_ADMIN_QUEUE, prgproprec, pcPropRec));
      // IfFailRet(AddPropRec(PROPID_M_ADMIN_QUEUE_LEN, prgproprec, pcPropRec));
    }
    if (m_cchDestQueue) {
      IfFailRet(AddPropRecOptional(PROPID_M_DEST_QUEUE, prgproprec, pcPropRec));
     // IfFailRet(AddPropRecOptional(PROPID_M_DEST_QUEUE_LEN, prgproprec, pcPropRec));
    }
    if (m_rgbCorrelationId) {
      IfFailRet(AddPropRec(PROPID_M_CORRELATIONID, prgproprec, pcPropRec));
    }
    if (m_cchLabel) {
      IfFailRet(AddPropRec(PROPID_M_LABEL, prgproprec, pcPropRec));
    //  IfFailRet(AddPropRec(PROPID_M_LABEL_LEN, prgproprec, pcPropRec));
    }
#if 0 // unsupported
    if (m_rgbSenderCert) {
      IfFailRet(AddPropRec(PROPID_M_SENDER_CERT, prgproprec, pcPropRec));
      IfFailRet(AddPropRec(PROPID_M_SENDER_CERT_LEN, prgproprec, pcPropRec));
    }
#endif // 0
    if (m_lMaxTimeToReachQueue != -1) {
      IfFailRet(AddPropRec(PROPID_M_TIME_TO_REACH_QUEUE, prgproprec, pcPropRec));
    }
    if (m_lMaxTimeToReceive != -1) {
      IfFailRet(AddPropRec(PROPID_M_TIME_TO_BE_RECEIVED, prgproprec, pcPropRec));
    }
#if 0 // unsupported
    if (m_lSecurityContext != 0) {
      IfFailRet(AddPropRec(PROPID_M_SECURITY_CONTEXT, prgproprec, pcPropRec));
    }
    if (*m_pguidSrcMachine != GUID_NULL) {
      IfFailRet(AddPropRec(PROPID_M_SRC_MACHINE_ID, prgproprec, pcPropRec));
    }
#endif // 0
    return hresult;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::CreateReceiveMessageProps
//=--------------------------------------------------------------------------=
// Creates and updates an MQMSGPROPS struct before receiving
//  a message.
//  NOTE: only used in SYNCHRONOUS receive case.
//   Use CreateAsyncReceiveMessage props otherwise.
//
// Parameters:
//    rgproprec    [in]  array of propids
//    cPropRec     [in]  array size
//    pmsgprops    [out]
//
// Output:
//    HRESULT       - S_OK, E_OUTOFMEMORY
//
// Notes:
//   Used only *before* Receive
//
HRESULT CMSMQMessage::CreateReceiveMessageProps(
    BOOL wantDestQueue,
    BOOL wantBody,
    MQMSGPROPS *pmsgprops)
{
    MSGPROPID propid;
    UINT iBodySize = (UINT)-1;
    UINT i, cbBody = 0;
    UINT cPropRec = 0;
    HRESULT hresult = NOERROR;
    
    IfFailGo(AllocateReceiveMessageProps(
               wantDestQueue,
               wantBody,
               pmsgprops,
               g_rgmsgproprec,
               g_cPropRec,
               &cPropRec));
    //
    // Note: we reuse buffers for each dynalloced property.
    //
    for (i = 0; i < cPropRec; i++) {
      propid = pmsgprops->aPropID[i];
      switch (propid) {
      case PROPID_M_MSGID:
      case PROPID_M_CORRELATIONID:
        IfNullRet(AllocateVectorOfProperty(
                   pmsgprops,
                   i,
                   sizeof(GUID) + sizeof(ULONG)));
        break;
      case PROPID_M_BODY_SIZE:
        iBodySize = i;
        break;
      case PROPID_M_BODY:
        HGLOBAL hMem;
        pmsgprops->aPropVar[i].caub.pElems = NULL;
        //
        // REVIEW: unfortunately can't optimize by reusing current buffer
        //  if same size as the default buffer since this message object
        //  is always new since we're creating one prior to receiving a new
        //  message.
        //
        IfNullRet(hMem = AllocateBodyBuffer(
                           pmsgprops,
                           i,
                           BODY_INIT_SIZE));
		// use LocalSize on WinCE
        cbBody = LocalSize(hMem);
        break;
      case PROPID_M_LABEL:
        pmsgprops->aPropVar[i].pwszVal = m_pwszLabel;
        break;
      case PROPID_M_LABEL_LEN:
        pmsgprops->aPropVar[i].lVal = MQ_MAX_MSG_LABEL_LEN + 1;
        break;
      case PROPID_M_SENDERID:
        IfNullRet(AllocateVectorOfProperty(pmsgprops, i, SENDERIDLEN));
        break;
      case PROPID_M_SENDER_CERT:
        IfNullRet(AllocateVectorOfProperty(pmsgprops, i, SENDERCERTLEN));
        break;
      case PROPID_M_SRC_MACHINE_ID:
        if (m_pguidSrcMachine) {
          //
          // per-instance buffer: freed in dtor
          //
          pmsgprops->aPropVar[i].puuid = m_pguidSrcMachine;
          pmsgprops->aPropVar[i].vt = VT_CLSID;
        }
        break;
      case PROPID_M_RESP_QUEUE_LEN:
      case PROPID_M_ADMIN_QUEUE_LEN:
      case PROPID_M_DEST_QUEUE_LEN:
        pmsgprops->aPropVar[i].lVal = FORMAT_NAME_INIT_BUFFER;
        break;
      case PROPID_M_RESP_QUEUE:
        pmsgprops->aPropVar[i].pwszVal = m_pwszRespQueue;
        break;
      case PROPID_M_ADMIN_QUEUE:
        pmsgprops->aPropVar[i].pwszVal = m_pwszAdminQueue;
        break;
      case PROPID_M_DEST_QUEUE:
        pmsgprops->aPropVar[i].pwszVal = m_pwszDestQueue;
        break;
      case PROPID_M_PRIORITY:
      	pmsgprops->aPropVar[i].bVal = 0;
      	break;
      	
      } // switch
    } // for
    if (iBodySize != (UINT)-1) {
      pmsgprops->aPropVar[iBodySize].lVal = cbBody;
    }

Error:
	
    if (FAILED(hresult)) {
      FreeMessageProps(pmsgprops);
    }
    return hresult;

}


//=--------------------------------------------------------------------------=
// static CMSMQMessage::CreateAsyncReceiveMessageProps
//=--------------------------------------------------------------------------=
// Creates and updates an MQMSGPROPS struct before async receiving
//  a message.
//  NOTE: only used in ASYNCHRONOUS receive case.
//   Use CreateReceiveMessage props otherwise.
//
// Parameters:
//    pmsgprops    [out]
//
// Output:
//    HRESULT       - S_OK, E_OUTOFMEMORY
//
// Notes:
//   Used only *before* async Receive
//
HRESULT CMSMQMessage::CreateAsyncReceiveMessageProps(
    MQMSGPROPS *pmsgprops)
{
    UINT cPropRec = 0;
    HRESULT hresult = NOERROR;

    pmsgprops->aPropID = NULL;
    pmsgprops->aPropVar = NULL;
    pmsgprops->aStatus = NULL;
    pmsgprops->cProp = cPropRec;
    IfNullRet(pmsgprops->aPropID = new MSGPROPID[cPropRec]);
    IfNullFail(pmsgprops->aPropVar = new MQPROPVARIANT[cPropRec]);
    IfNullFail(pmsgprops->aStatus = new HRESULT[cPropRec]);
    return hresult;

Error:
    FreeMessageProps(pmsgprops);
    return hresult;
}


//=--------------------------------------------------------------------------=
// HELPER: SetBinaryMessageProp
//=--------------------------------------------------------------------------=
// Sets data members from binary message props after a receive or send.
//
// Parameters:
//    pmsgprops - [in] pointer to message properties struct
//
// Output:
//
HRESULT SetBinaryMessageProp(
    MQMSGPROPS *pmsgprops,
    UINT iProp,
    // ULONG *pcbBuffer,
    ULONG cbBuffer,
    BYTE **prgbBuffer)
{
    BYTE *rgbBuffer = *prgbBuffer;
    // ULONG cbBuffer;

    delete [] rgbBuffer;
    // cbBuffer = pmsgprops->aPropVar[iProp].caub.cElems;
    IfNullRet(rgbBuffer = new BYTE[cbBuffer]);
    memcpy(rgbBuffer,
           pmsgprops->aPropVar[iProp].caub.pElems,
           cbBuffer);
    *prgbBuffer= rgbBuffer;
    // *pcbBuffer = cbBuffer;
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// HELPER: GetQueueInfoOfFormatNameProp
//=--------------------------------------------------------------------------=
// Converts string message prop to bstr after send/receive.
//
// Parameters:
//    pmsgprops - [in] pointer to message properties struct
//    ppqinfo     [out]
//
// Output:
//
HRESULT GetQueueInfoOfFormatNameProp(
    MQMSGPROPS *pmsgprops,
    UINT iProp,
    WCHAR *pwsz,
    IMSMQQueueInfo **ppqinfo)
{
    BSTR bstr = NULL;
    UINT cString;
    CMSMQQueueInfo *pqinfo = NULL;
    HRESULT hresult = NOERROR;

    ASSERT(ppqinfo, L"bad param.");
    if (cString = (UINT)pmsgprops->aPropVar[iProp].lVal) {
      IfNullRet(bstr = SysAllocStringLen(pwsz, cString));
      IfNullFail(pqinfo = new CMSMQQueueInfo(NULL));
      IfFailGoTo(pqinfo->Init(bstr), Error2);
      *ppqinfo = pqinfo;
      goto Error;         // 2657: fix memleak
    }
    return NOERROR;

Error2:
    delete pqinfo;
    // fall through...

Error:
    SysFreeString(bstr);
    return hresult;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::UpdateMsgId
//=--------------------------------------------------------------------------=
// Sets message id after send
//
// Parameters:
//    pmsgprops - [in] pointer to message properties struct
//
// Output:
//
HRESULT CMSMQMessage::UpdateMsgId(
    MQMSGPROPS *pmsgprops)
{
    UINT i;
    UINT cProp = pmsgprops->cProp;
    MSGPROPID msgpropid;
	HRESULT hr;
	
    for (i = 0; i < cProp; i++) {
      msgpropid = pmsgprops->aPropID[i];
      //
      // skip ignored props
      //
      if (pmsgprops->aStatus[i] == MQ_INFORMATION_PROPERTY_IGNORED) {
        continue;
      }
      if (msgpropid == PROPID_M_MSGID) {
      	EnterCriticalSection(&m_CSlocal);
        	m_cbMsgId = pmsgprops->aPropVar[i].caub.cElems;
        	hr =  SetBinaryMessageProp(pmsgprops,
                                    i,
                                    m_cbMsgId,
                                    &m_rgbMsgId);
        LeaveCriticalSection(&m_CSlocal);
        return hr;
      }
    } // for
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::SetMessageProps
//=--------------------------------------------------------------------------=
// Sets data members from message props after a receive.
//
// Parameters:
//    pmsgprops - [in] pointer to message properties struct
//
// Output:
//
HRESULT CMSMQMessage::SetMessageProps(
    MQMSGPROPS *pmsgprops)
{
    UINT i, cch;
    UINT cProp = pmsgprops->cProp;
    MSGPROPID msgpropid;
    UCHAR *pucElemsBody = NULL;  // used to save pointer to body until we know its size
    BOOL fProcessedRespQueue = FALSE;
    BOOL fProcessedAdminQueue = FALSE;
    BOOL fProcessedDestQueue = FALSE;
    BOOL fProcessedSenderCert = FALSE;
    BOOL fProcessedSenderId = FALSE;
    BOOL fProcessedPrivLevel = FALSE;
    BOOL fProcessedAuthenticated = FALSE;
    UINT iSenderCert = UINT_MAX, iSenderId = UINT_MAX;
    HRESULT hresult = NOERROR;


	// lock us down whil we update our members
	EnterCriticalSection(&m_CSlocal);

    for (i = 0; i < cProp; i++) {
      msgpropid = pmsgprops->aPropID[i];
      //
      // skip ignored props
      //
      if (pmsgprops->aStatus[i] == MQ_INFORMATION_PROPERTY_IGNORED) {
        continue;
      }
      switch (msgpropid) {
      case PROPID_M_CLASS:
        m_lClass = (MQMSGCLASS)pmsgprops->aPropVar[i].uiVal;
        break;
      case PROPID_M_MSGID:
        m_cbMsgId = pmsgprops->aPropVar[i].caub.cElems;
        IfFailGo(SetBinaryMessageProp(pmsgprops,
                                      i,
                                      // &m_cbMsgId,
                                      m_cbMsgId,
                                      &m_rgbMsgId));
        break;
      case PROPID_M_CORRELATIONID:
        m_cbCorrelationId = pmsgprops->aPropVar[i].caub.cElems;
        IfFailGo(SetBinaryMessageProp(pmsgprops,
                                      i,
                                      // &m_cbCorrelationId,
                                      m_cbCorrelationId,
                                      &m_rgbCorrelationId));
        break;
      case PROPID_M_PRIORITY:
        m_lPriority = (long)pmsgprops->aPropVar[i].bVal;
        break;
      case PROPID_M_DELIVERY:
        m_lDelivery = (MQMSGDELIVERY)pmsgprops->aPropVar[i].bVal;
        break;
      case PROPID_M_ACKNOWLEDGE:
        m_lAck = (MQMSGACKNOWLEDGEMENT)pmsgprops->aPropVar[i].bVal;
        break;
      case PROPID_M_JOURNAL:
        m_lJournal = (long)pmsgprops->aPropVar[i].bVal;
        break;
      case PROPID_M_APPSPECIFIC:
        m_lAppSpecific = (long)pmsgprops->aPropVar[i].lVal;
        break;
      case PROPID_M_BODY:
        //
        // We can't allocate the body yet because we might
        //  not know its size -- cElems is *not* the actual size
        //  rather it's obtained via the BODY_SIZE property which
        //  is processed separately, so we simply save away a
        //  pointer to the returned buffer.
        //
        pucElemsBody = pmsgprops->aPropVar[i].caub.pElems;
        m_hMem = GlobalHandle(pucElemsBody);
        ASSERT(m_hMem, L"bad handle.");
        m_pbBody = (BYTE *)pucElemsBody;
        break;
      case PROPID_M_BODY_SIZE:
        m_cbBody = (UINT)pmsgprops->aPropVar[i].lVal;
        break;
      case PROPID_M_BODY_TYPE:
        //
        // 1645: if the sending app hasn't bothered
        //  to set this property (i.e. it's still 0)
        //  let's default it to a binary bytearray.
        //
        m_vtBody = (USHORT)pmsgprops->aPropVar[i].lVal;
        if (m_vtBody == 0) {
          m_vtBody = VT_ARRAY | VT_UI1;
        }
        break;
      case PROPID_M_LABEL:
        // m_pwszLabel = pmsgprops->aPropVar[i].pwszVal;
        ASSERT(m_pwszLabel == pmsgprops->aPropVar[i].pwszVal,
               L"should reuse same buffer.");
        break;
      case PROPID_M_LABEL_LEN:
        cch = pmsgprops->aPropVar[i].lVal;
        //
        // remove trailing null if necessary
        //
        m_cchLabel = cch ? cch - 1 : 0;
        break;
      case PROPID_M_TIME_TO_BE_RECEIVED:
        m_lMaxTimeToReceive = pmsgprops->aPropVar[i].lVal;
        break;
      case PROPID_M_TIME_TO_REACH_QUEUE:
        m_lMaxTimeToReachQueue = pmsgprops->aPropVar[i].lVal;
        break;
      case PROPID_M_TRACE:
        m_lTrace = (long)pmsgprops->aPropVar[i].bVal;
        break;
      case PROPID_M_SENDERID:
        fProcessedSenderId = TRUE;
        iSenderId = i;
        break;
      case PROPID_M_SENDERID_LEN:
        ASSERT(fProcessedSenderId,
               L"should have processed SENDERID.");
        m_cbSenderId = pmsgprops->aPropVar[i].caub.cElems;
        IfFailGo(SetBinaryMessageProp(pmsgprops,
                                      iSenderId,
                                      // &m_cbSenderId,
                                      m_cbSenderId,
                                      &m_rgbSenderId));
        break;
      case PROPID_M_SENDERID_TYPE:
        m_lSenderIdType = pmsgprops->aPropVar[i].lVal;
        break;
      case PROPID_M_SENDER_CERT:
        //
        // don't know buffer size yet
        //
        iSenderCert = i;
        fProcessedSenderCert = TRUE;
        break;
      case PROPID_M_SENDER_CERT_LEN:
        ASSERT(fProcessedSenderCert,
               L"should have processed SENDER_CERT already.");
        m_cbSenderCert = pmsgprops->aPropVar[i].caub.cElems;
        IfFailGo(SetBinaryMessageProp(pmsgprops,
                                      iSenderCert,
                                      // &m_cbSenderCert,
                                      m_cbSenderCert,
                                      &m_rgbSenderCert));
        break;
      case PROPID_M_PRIV_LEVEL:
        m_lPrivLevel = pmsgprops->aPropVar[i].lVal;
        fProcessedPrivLevel = TRUE;
        break;
      case PROPID_M_AUTH_LEVEL:
        m_lAuthLevel = pmsgprops->aPropVar[i].lVal;
        break;
      case PROPID_M_AUTHENTICATED:
        m_fAuthenticated = (BOOL)pmsgprops->aPropVar[i].bVal;
        fProcessedAuthenticated = TRUE;
        break;
      case PROPID_M_HASH_ALG:
        //
        // hashalg only valid after receive if message was
        //  authenticated
        //
        ASSERT(fProcessedAuthenticated,
               L"should have processed authenticated.");
        if (m_fAuthenticated) {
          m_lHashAlg = pmsgprops->aPropVar[i].lVal;
        }
        break;
      case PROPID_M_ENCRYPTION_ALG:
        //
        // encryptionalg only valid after receive is privlevel
        //  is body
        //
        ASSERT(fProcessedPrivLevel,
               L"should have processed privlevel.");
        if (m_lPrivLevel == MQ_PRIV_LEVEL_BODY) {
          m_lEncryptAlg = pmsgprops->aPropVar[i].lVal;
        }
        break;
      case PROPID_M_SRC_MACHINE_ID:
        // already allocated buffer in ctor
        // *m_pguidSrcMachine = *pmsgprops->aPropVar[i].puuid;
        ASSERT(m_pguidSrcMachine == pmsgprops->aPropVar[i].puuid,
              L"should reuse same buffer.");
        break;
      case PROPID_M_SENTTIME:
        m_lSentTime = pmsgprops->aPropVar[i].lVal;
        break;
      case PROPID_M_ARRIVEDTIME:
        m_lArrivedTime = pmsgprops->aPropVar[i].lVal;
        break;
      case PROPID_M_DEST_QUEUE:
        // save away dest queue name -- we still
        //  don't know its real length.
        //
        // m_pwszDestQueue = pmsgprops->aPropVar[i].pwszVal;
        ASSERT(m_pwszDestQueue == pmsgprops->aPropVar[i].pwszVal,
               L"should reuse same buffer.");
        fProcessedDestQueue = TRUE;
        break;
      case PROPID_M_DEST_QUEUE_LEN:
        // we assume that we've processed DEST_QUEUE already
        ASSERT(fProcessedDestQueue,
               L"whoops! should have processed dest queue already.");
        cch = pmsgprops->aPropVar[i].lVal;
        //
        // remove trailing null if necessary
        //
        m_cchDestQueue = cch ? cch - 1 : 0;
        RELEASE(m_pqinfoDest);
        IfFailGo(GetQueueInfoOfFormatNameProp(pmsgprops,
                                              i,
                                              m_pwszDestQueue,
                                              &m_pqinfoDest));
        break;
      case PROPID_M_RESP_QUEUE:
        // save away resp queue name -- we still
        //  don't know its real length.
        //
        // m_pwszRespQueue = pmsgprops->aPropVar[i].pwszVal;
        ASSERT(m_pwszRespQueue == pmsgprops->aPropVar[i].pwszVal,
              L"should reuse same buffer.");
        fProcessedRespQueue = TRUE;
        break;
      case PROPID_M_RESP_QUEUE_LEN:
        // we assume that we've processed RESP_QUEUE already
        ASSERT(fProcessedRespQueue,
               L"whoops! should have processed resp queue already.");
        cch = pmsgprops->aPropVar[i].lVal;
        //
        // remove trailing null if necessary
        //
        m_cchRespQueue = cch ? cch - 1 : 0;
        RELEASE(m_pqinfoResponse);
        IfFailGo(GetQueueInfoOfFormatNameProp(pmsgprops,
                                              i,
                                              m_pwszRespQueue,
                                              &m_pqinfoResponse));
        break;
      case PROPID_M_ADMIN_QUEUE:
        // save away admin queue name -- we still
        //  don't know its real length.
        //
        // m_pwszAdminQueue = pmsgprops->aPropVar[i].pwszVal;
        ASSERT(m_pwszAdminQueue == pmsgprops->aPropVar[i].pwszVal,
              L"should reuse same buffer.");
        fProcessedAdminQueue = TRUE;
        break;
      case PROPID_M_ADMIN_QUEUE_LEN:
        // we assume that we've processed ADMIN_QUEUE already
        ASSERT(fProcessedAdminQueue,
               L"whoops! should have processed admin queue already.");
        cch = pmsgprops->aPropVar[i].lVal;
        //
        // remove trailing null if necessary
        //
        m_cchAdminQueue = cch ? cch - 1 : 0;
        RELEASE(m_pqinfoAdmin);
        IfFailGo(GetQueueInfoOfFormatNameProp(pmsgprops,
                                              i,
                                              m_pwszAdminQueue,
                                              &m_pqinfoAdmin));
        break;
      case PROPID_M_SECURITY_CONTEXT:
        m_lSecurityContext = pmsgprops->aPropVar[i].lVal;
        break;
      default:
        ASSERT(0, L"unrecognized msgpropid.");
        break;
      } // switch
    } // for
#if DEBUG
    if (m_hMem) {
 //     ASSERT(m_pbBody, L"no body.");
 //     ASSERT(m_cbBody <= (long)GlobalSize(m_hMem), L"bad size.");
    }
#endif // DEBUG

Error:
	LeaveCriticalSection(&m_CSlocal);
    return hresult;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::FreeMessageProps
//=--------------------------------------------------------------------------=
// Frees dynamically allocated memory allocated on
//  behalf of a msgprops struct.
//
// Parameters:
//    pmsgprops - [in] pointer to message properties struct
//
// Output:
//
// Notes:
//
void CMSMQMessage::FreeMessageProps(
    MQMSGPROPS *pmsgprops)
{
    UINT cProp = pmsgprops->cProp;
    UINT i;
    MSGPROPID msgpropid;

    for (i = 0; i < cProp; i++) {
      msgpropid = pmsgprops->aPropID[i];
      switch (msgpropid) {
      case PROPID_M_MSGID:
      case PROPID_M_CORRELATIONID:
      case PROPID_M_SENDERID:
      case PROPID_M_SENDER_CERT:
        DeleteCauVector(&pmsgprops->aPropVar[i].caub);
        break;
      case PROPID_M_LABEL:
      case PROPID_M_RESP_QUEUE:
      case PROPID_M_ADMIN_QUEUE:
      case PROPID_M_DEST_QUEUE:
      case PROPID_M_SRC_MACHINE_ID:
        // we allocate a single per-instance buffer
        //  which is freed by the dtor
        //
        break;
      case PROPID_M_BODY:
        break;
      } // switch
    } // for
    delete [] pmsgprops->aPropID;
    delete [] pmsgprops->aPropVar;
    delete [] pmsgprops->aStatus;
    return;
}


//
// HELPER: AllocateStringProperty
//  Purpose: allocate and init a null-terminated
//   unicode string message property
//  Returns OOM or S_OK
//
HRESULT AllocateStringProperty(
    MQMSGPROPS *pmsgprops,
    UINT iProp,
    BSTR bstrProperty)
{
    UINT cb;

    IfNullRet(pmsgprops->aPropVar[iProp].pwszVal =
      new WCHAR[SysStringLen(bstrProperty)+1]);
    cb = SysStringByteLen(bstrProperty) + sizeof(WCHAR);
    memcpy(pmsgprops->aPropVar[iProp].pwszVal, (LPVOID)bstrProperty, cb);
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// CMSMQMessage::CreateSendMessageProps
//=--------------------------------------------------------------------------=
// Updates message props with contents of data members
//  before sending a message.
//
// Parameters:
//    pmsgprops - [in] pointer to message properties struct
//
// Output:
//
// Notes:
//    Only used before Send.
//
HRESULT CMSMQMessage::CreateSendMessageProps(
    MQMSGPROPS *pmsgprops)
{
    UINT i;
    UINT cPropRec = 0;
    MSGPROPID msgpropid;
    PROPREC *rgproprec = NULL;
    HRESULT hresult = NOERROR;

    IfFailRet(CreateSendPropIdArray(&cPropRec, &rgproprec));
    IfFailGo(AllocateMessageProps(pmsgprops, rgproprec, cPropRec));
    for (i = 0; i < cPropRec; i++) {
      msgpropid = pmsgprops->aPropID[i];
      switch (msgpropid) {
      case PROPID_M_CLASS:
        ASSERT(FALSE, L"can't specify class on Send.");
        break;
      case PROPID_M_MSGID:
      case PROPID_M_CORRELATIONID:
        IfNullRet(AllocateVectorOfProperty(
                   pmsgprops,
                   i,
                   sizeof(GUID) + sizeof(ULONG)));
        // 1963: update correlation id
        if (msgpropid == PROPID_M_CORRELATIONID) {
          memcpy(pmsgprops->aPropVar[i].caub.pElems,
                 m_rgbCorrelationId,
                 m_cbCorrelationId);
        }
        break;
      case PROPID_M_PRIORITY:
        pmsgprops->aPropVar[i].bVal = (UCHAR)m_lPriority;
        break;
      case PROPID_M_DELIVERY:
        pmsgprops->aPropVar[i].bVal = (UCHAR)m_lDelivery;
        break;
      case PROPID_M_ACKNOWLEDGE:
        pmsgprops->aPropVar[i].bVal = (UCHAR)m_lAck;
        break;
      case PROPID_M_JOURNAL:
        pmsgprops->aPropVar[i].bVal = (UCHAR)m_lJournal;
        break;
      case PROPID_M_APPSPECIFIC:
        pmsgprops->aPropVar[i].lVal = m_lAppSpecific;
        break;
      case PROPID_M_BODY:
        ASSERT(m_hMem, L"should have buffer!");
        ASSERT(m_hMem == GlobalHandle(m_pbBody), L"bad handle.");
        ASSERT(m_cbBody <= (long)LocalSize(m_hMem), L"bad body size.");
        pmsgprops->aPropVar[i].caub.cElems = m_cbBody;
        pmsgprops->aPropVar[i].caub.pElems = m_pbBody;
        break;

#if 0 // unsupported
      case PROPID_M_BODY_SIZE:
        ASSERT(m_hMem, L"should have buffer!");
        pmsgprops->aPropVar[i].lVal = m_cbBody;
        break;
#endif // 0
      case PROPID_M_BODY_TYPE:
        pmsgprops->aPropVar[i].lVal = m_vtBody;
        break;
      case PROPID_M_LABEL:
        pmsgprops->aPropVar[i].pwszVal = m_pwszLabel;
        break;

#if 0 // unsupported
      case PROPID_M_LABEL_LEN:
        pmsgprops->aPropVar[i].lVal = m_cchLabel;
        break;
#endif // 0
      case PROPID_M_TIME_TO_BE_RECEIVED:
        pmsgprops->aPropVar[i].lVal = m_lMaxTimeToReceive;
        break;
      case PROPID_M_TIME_TO_REACH_QUEUE:
        pmsgprops->aPropVar[i].lVal = m_lMaxTimeToReachQueue;
        break;
      case PROPID_M_TRACE:
        pmsgprops->aPropVar[i].bVal = (UCHAR)m_lTrace;
        break;

#if 0 // unsupported
      case PROPID_M_SENDERID:
        pmsgprops->aPropVar[i].caub.cElems = m_cbSenderId;
        pmsgprops->aPropVar[i].caub.pElems = NULL;
        IfNullFail(AllocateCauVector(&pmsgprops->aPropVar[i].caub));
        break;
#endif // 0

      case PROPID_M_SENDERID_LEN:
        // REVIEW: safe to ignore?
        break;

#if 0 // unsupported
      case PROPID_M_SENDER_CERT:
        pmsgprops->aPropVar[i].caub.cElems = m_cbSenderCert;
        pmsgprops->aPropVar[i].caub.pElems = NULL;
        IfNullFail(AllocateCauVector(&pmsgprops->aPropVar[i].caub));
        memcpy(pmsgprops->aPropVar[i].caub.pElems,
               m_rgbSenderCert,
               m_cbSenderCert);
        break;
#endif //  0

      case PROPID_M_SENDER_CERT_LEN:
        // REVIEW: safe to ignore?
        break;
        
#if 0 // unsupported
      case PROPID_M_SENDERID_TYPE:
        pmsgprops->aPropVar[i].lVal = m_lSenderIdType;
        break;
      case PROPID_M_PRIV_LEVEL:
        pmsgprops->aPropVar[i].lVal = m_lPrivLevel;
        break;
#endif // 0
      case PROPID_M_AUTH_LEVEL:
        pmsgprops->aPropVar[i].lVal = m_lAuthLevel;
        break;
      case PROPID_M_AUTHENTICATED:
        pmsgprops->aPropVar[i].bVal = (UCHAR)m_fAuthenticated;
        break;
        
#if 0 // unsupported
      case PROPID_M_HASH_ALG:
        pmsgprops->aPropVar[i].lVal = m_lHashAlg;
        break;
      case PROPID_M_ENCRYPTION_ALG:
        pmsgprops->aPropVar[i].lVal = m_lEncryptAlg;
        break;
      case PROPID_M_SRC_MACHINE_ID:
        ASSERT(m_pguidSrcMachine, L"should always be non-null");
        pmsgprops->aPropVar[i].vt = VT_CLSID;
        pmsgprops->aPropVar[i].puuid = m_pguidSrcMachine;
        break;
#endif // 0
      case PROPID_M_SENTTIME:
        // ignored...
        break;
      case PROPID_M_ARRIVEDTIME:
        // ignored...
        break;
      case PROPID_M_DEST_QUEUE:
        pmsgprops->aPropVar[i].pwszVal = m_pwszDestQueue;
        break;
#if 0 // unsupported
      case PROPID_M_DEST_QUEUE_LEN:
        pmsgprops->aPropVar[i].lVal = m_cchDestQueue;
        break;
#endif // 0
      case PROPID_M_RESP_QUEUE:
        pmsgprops->aPropVar[i].pwszVal = m_pwszRespQueue;
        break;
#if 0 // unsupported
      case PROPID_M_RESP_QUEUE_LEN:

        pmsgprops->aPropVar[i].lVal = m_cchRespQueue;
        break;
#endif // 0
      case PROPID_M_ADMIN_QUEUE:
        ASSERT(m_pqinfoAdmin, L"should have admin queue.");
        pmsgprops->aPropVar[i].pwszVal = m_pwszAdminQueue;
        break;
#if 0 // unsupported
      case PROPID_M_ADMIN_QUEUE_LEN:

        pmsgprops->aPropVar[i].lVal = m_cchAdminQueue;

        break;
      case PROPID_M_SECURITY_CONTEXT:
 
        pmsgprops->aPropVar[i].lVal = m_lSecurityContext;
        break;
#endif // 0
      default:
        ASSERT(0, L"unrecognized msgpropid.");
        break;
      } // switch
    } // for
Error:
    delete [] rgproprec;
    return hresult;
}
