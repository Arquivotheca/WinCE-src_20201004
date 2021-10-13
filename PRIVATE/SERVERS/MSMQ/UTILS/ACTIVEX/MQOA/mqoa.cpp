//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//=--------------------------------------------------------------------------=
// mq.Cpp
//=--------------------------------------------------------------------------=
//
// various routines et all that aren't in a file for a particular automation
// object, and don't need to be in the generic ole automation code.
//
//
#define INITOBJECTS                // define the descriptions for our objects


#include "IPServer.H"
#include "LocalSrv.H"

#include "LocalObj.H"
#include "lookupx.h"    // UNDONE; to define admin stuff...
#include "Globals.H"

#include "Util.H"
#include "Resource.H"

#include "mqoa.H"
#include "AutoObj.H"

#include "oautil.h"
#include "Query.H"
#include "qinfo.H"
#include "q.h"
#include "msg.H"
#include "qinfos.H"
#include "event.h"
#include "mq.h"

//#include "mqsymbls.h"

// No transaction support
//#include "xact.h"
//#include "xdisper.h"

#if defined (winNT)
#include <rtstub.hxx>
#endif

#if DEBUG
extern VOID RemBstrNode(void *pv);
#endif // DEBUG

// needed for ASSERTs and FAIL
//
SZTHISFILE

// debug...
#define new DEBUG_NEW
#if DEBUG
#define SysAllocString DebSysAllocString
#define SysReAllocString DebSysReAllocString
#define SysFreeString DebSysFreeString
#endif // DEBUG


//
// UNDONE: should be in separate header: when VC5
// UNDONE:  is checked in...
// CMSMQApplication: MSMQ application object
//
class CMSMQApplication : public IMSMQApplication, public CAutomationObject, ISupportErrorInfo {

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

    CMSMQApplication(IUnknown *);
    virtual ~CMSMQApplication();

    // IMSMQMessage methods
    // TODO: copy over the interface methods for IMSMQApplication from
    //       mqInterfaces.H here.
    STDMETHOD(MachineIdOfMachineName)(THIS_ BSTR bstrMachineName, BSTR FAR* pbstrGuid);


    // creation method
    //
    static IUnknown *Create(IUnknown *);

  protected:
    virtual HRESULT InternalQueryInterface(REFIID riid, void **ppvObjOut);

    // introduced methods
};

// TODO: modify anything appropriate in this structure, such as the helpfile
//       name, the version number, etc.
//
DEFINE_AUTOMATIONOBJECT(MSMQApplication,
    &CLSID_MSMQApplication,
    L"MSMQApplication",
    CMSMQApplication::Create,
    1,
    &IID_IMSMQApplication,
    L"MSMQApplication.Hlp");



//=--------------------------------------------------------------------------=
// our Libid.  This should be the LIBID from the Type library, or NULL if you
// don't have one.
//
const CLSID *g_pLibid = &LIBID_MSMQ;


//=--------------------------------------------------------------------------=
// Localization Information
//
// We need the following two pieces of information:
//    a. whether or not this DLL uses satellite DLLs for localization.  if
//       not, then the lcidLocale is ignored, and we just always get resources
//       from the server module file.
//    b. the ambient LocaleID for this in-proc server.  Controls calling
//       GetResourceHandle() will set this up automatically, but anybody
//       else will need to be sure that it's set up properly.
//
const VARIANT_BOOL g_fSatelliteLocalization =  FALSE;
LCID               g_lcidLocale = MAKELCID(LANG_USER_DEFAULT, SORT_DEFAULT);


//=--------------------------------------------------------------------------=
// This Table describes all the automatible objects in your automation server.
// See AutomationObject.H for a description of what goes in this structure
// and what it's used for.
//
OBJECTINFO g_ObjectInfo[] = {
    AUTOMATIONOBJECT(MSMQQuery),
    AUTOMATIONOBJECT(MSMQQueueInfo),
    AUTOMATIONOBJECT(MSMQQueue),
    AUTOMATIONOBJECT(MSMQMessage),
    AUTOMATIONOBJECT(MSMQQueueInfos),
    AUTOMATIONOBJECT(MSMQEvent), /*
    AUTOMATIONOBJECT(MSMQTransaction),
    AUTOMATIONOBJECT(MSMQCoordinatedTransactionDispenser),
    AUTOMATIONOBJECT(MSMQTransactionDispenser), */
    AUTOMATIONOBJECT(MSMQApplication),
    EMPTYOBJECT
};

const WCHAR g_wszLibName[] = L"MSMQ";
HWND g_hwnd;
WNDPROC g_lpPrevWndFunc;
extern CRITICAL_SECTION g_csCallback;

//=--------------------------------------------------------------------------=
// IntializeLibrary
//=--------------------------------------------------------------------------=
// called from DllMain:DLL_PROCESS_ATTACH.  allows the user to do any sort of
// initialization they want to.
//
// Notes:
//
void InitializeLibrary
(
    void
)
{
    // TODO: initialization here.  control window class should be set up in
    // RegisterClassData.

    //
    // UNDONE: 2121 temporarily fix by unconditionally loading mqrt.dll
    //  so that it won't get unloaded/reloaded and thus trigger
    //  the crypto32 tls leak.  This makes the workaround to 905
    //  superfluous... whatever.
    // 
  #if 0 // !WinCE
    HINSTANCE hLib;
    hLib = LoadLibrary(L"mqrt.dll");
    ASSERT(hLib, L"couldn't load mqrt.dll!");
  #endif
    //
    // Init queue ops/callback lookup critical section
    // NOTE: deleted in UninitializeLibrary.
    //
    InitializeCriticalSection(&g_csCallback);
}

extern void DumpMemLeaks();
extern void DumpBstrLeaks();

//=--------------------------------------------------------------------------=
// UninitializeLibrary
//=--------------------------------------------------------------------------=
// called from DllMain:DLL_PROCESS_DETACH.  allows the user to clean up anything
// they want.
//
// Notes:
//
void UninitializeLibrary
(
    void
)
{
    // TODO: uninitialization here.  control window class will be unregistered
    // for you, but anything else needs to be cleaned up manually.
    // Please Note that the Window 95 DLL_PROCESS_DETACH isn't quite as stable
    // as NT's, and you might crash doing certain things here ...

#if DEBUG
    DumpMemLeaks();
    DumpBstrLeaks();
#endif // DEBUG

#if 0
    //
    // UNDONE: for beta2 let this leak, need to synch
    //  with mqrt.dll unload via a common critsect
    //  so that async thread termination/callback
    //  does the right thing.
    //
#endif // 0

    DeleteCriticalSection(&g_csCallback);
    //
    // release our global transaction manager: if allocated
    //  at all by a call to BeginTransaction
    //
   // RELEASE(CMSMQCoordinatedTransactionDispenser::m_ptxdispenser);
#if 0
    // UNDONE: 2028: DTC can't be unloaded...
    //
    // Free DTC proxy library - if it was loaded
    //
    if (CMSMQCoordinatedTransactionDispenser::m_hLibDtc) {
      FreeLibrary(CMSMQCoordinatedTransactionDispenser::m_hLibDtc);
      CMSMQCoordinatedTransactionDispenser::m_hLibDtc = NULL;
    }
#endif // 0 
}


//=--------------------------------------------------------------------------=
// CheckForLicense
//=--------------------------------------------------------------------------=
// users can implement this if they wish to support Licensing.  otherwise,
// they can just return TRUE all the time.
//
// Parameters:
//    none
//
// Output:
//    BOOL            - TRUE means the license exists, and we can proceed
//                      FALSE means we're not licensed and cannot proceed
//
// Notes:
//    - implementers should use g_wszLicenseKey and g_wszLicenseLocation
//      from the top of this file to define their licensing [the former
//      is necessary, the latter is recommended]
//
BOOL CheckForLicense
(
    void
)
{
    // TODO: you should make sure the machine has your license key here.
    // this is typically done by looking in the registry.
    //
    return TRUE;
}

//=--------------------------------------------------------------------------=
// CheckLicenseKey
//=--------------------------------------------------------------------------=
// when IClassFactory2::CreateInstanceLic is called, a license key is passed
// in, and then passed on to this routine.  users should return a boolean 
// indicating whether it is a valid license key or not
//
// Parameters:
//    LPWSTR          - [in] the key to check
//
// Output:
//    BOOL            - false means it's not valid, true otherwise
//
// Notes:
//
BOOL CheckLicenseKey
(
    LPWSTR pwszKey
)
{
    // TODO: check the license key against your values here and make sure it's
    // valid.
    //
    return TRUE;
}

//=--------------------------------------------------------------------------=
// GetLicenseKey
//=--------------------------------------------------------------------------=
// returns our current license key that should be saved out, and then passed
// back to us in IClassFactory2::CreateInstanceLic
//
// Parameters:
//    none
//
// Output:
//    BSTR                 - key or NULL if Out of memory
//
// Notes:
//
BSTR GetLicenseKey
(
    void
)
{
    // TODO: return your license key here.
    //
    return SysAllocString(L"");
}

//=--------------------------------------------------------------------------=
// RegisterData
//=--------------------------------------------------------------------------=
// lets the inproc server writer register any data in addition to that in
// any other objects.
//
// Output:
//    BOOL            - false means failure.
//
// Notes:
//
BOOL RegisterData
(
    void
)
{
    // TODO: register any additional data here that you might wish to.
    //
	
    //
    // 1501: need to register objects as "safe"
    //
    register int     iObj = 0;

    // loop through all of our creatable objects [those that have a clsid in
    // our global table] and register them as "safe"
    //
    while (!ISEMPTYOBJECT(iObj)) {
      if (!OBJECTISCREATABLE(iObj)) {
        iObj++;
        continue;
      }

      // depending on the object type, register different pieces of information
      //
      switch (g_ObjectInfo[iObj].usType) {
      case OI_AUTOMATION:
        //
        // register object as safe
	//
        if (!RegisterAutomationObjectAsSafe(CLSIDOFOBJECT(iObj))) {
          return FALSE;
        }
        break;
      
      default:
        ASSERT(0, L"we only deal with OA objects here.");
        break;
      }
      iObj++;
    }
    return TRUE;
}

//=--------------------------------------------------------------------------=
// UnregisterData
//=--------------------------------------------------------------------------=
// inproc server writers should unregister anything they registered in
// RegisterData() here.
//
// Output:
//    BOOL            - false means failure.
//
// Notes:
//
BOOL UnregisterData
(
    void
)
{
    // TODO: any additional registry cleanup that you might wish to do.
    //
    return TRUE;
}


//=--------------------------------------------------------------------------=
// CRT stubs
//=--------------------------------------------------------------------------=
// these two things are here so the CRTs aren't needed. this is good.
//
// basically, the CRTs define this to suck in a bunch of stuff.  we'll just
// define them here so we don't get an unresolved external.
//
// TODO: if you are going to use the CRTs, then remove this line.
//
// extern "C" int __cdecl _fltused = 1;

extern "C" int _cdecl _purecall(void)
{
  FAIL(L"Pure virtual function called.");
  return 0;
}


//=--------------------------------------------------------------------------=
// CreateErrorHelper
//=--------------------------------------------------------------------------=
// fills in the rich error info object so that both our vtable bound interfaces
// and calls through ITypeInfo::Invoke get the right error informaiton.
//
// Parameters:
//    HRESULT          - [in] the SCODE that should be associated with this err
//    DWORD            - [in] object type
//
// Output:
//    HRESULT          - the HRESULT that was passed in.
//
// Notes:
//
HRESULT CreateErrorHelper(
    HRESULT hrExcep,
    DWORD dwObjectType)
{
    return SUCCEEDED(hrExcep) ? 
             hrExcep :
             CreateError(
               hrExcep,
               (GUID *)&INTERFACEOFOBJECT(dwObjectType),
               (LPTSTR)NAMEOFOBJECT(dwObjectType));
}




//=-------------------------------------------------------------------------=
// CMSMQApplication::MachineIdOfMachineName
//=-------------------------------------------------------------------------=
//  Maps machine name to its guid via DS.
//
// Parameters:
//    bstrMachineName  [in] 
//    pbstrGuid        [out]  callee allocated/caller freed
//	
HRESULT CMSMQApplication::MachineIdOfMachineName(
    BSTR bstrMachineName, 
    BSTR FAR* pbstrGuid
    )
{

return E_NOTIMPL;
#if 0 // not implemented in CE
    MQQMPROPS qmprops;
    DWORD cProp;
    int cbStr;
    HRESULT hresult = NOERROR;
	WCHAR awcMachineName[(LENSTRCLSID + 2) * 2];



    qmprops.aPropID = NULL;
    qmprops.aPropVar = NULL;
    qmprops.aStatus = NULL;

    cProp = 1;
    IfNullRet(qmprops.aPropID = new QUEUEPROPID[cProp]);
    IfNullFail(qmprops.aStatus = new HRESULT[cProp]);
    IfNullFail(qmprops.aPropVar = new MQPROPVARIANT[cProp]);
    qmprops.cProp = cProp;
    //
    //  if machine name is NULL, the calls refers to the
    //  local machine
    //
    qmprops.aPropID[0] = PROPID_QM_MACHINE_ID;
    qmprops.aPropVar[0].vt = VT_NULL;
    IfFailGo(MQGetMachineProperties(
               bstrMachineName,
               NULL,
               &qmprops));
    *pbstrGuid = SysAllocStringLen(NULL, LENSTRCLSID - 2);
    if (*pbstrGuid) {
      cbStr = StringFromGUID2(
                *qmprops.aPropVar[0].puuid, 
                awcMachineName, 
                LENSTRCLSID*2);

	  wcsncpy( *pbstrGuid, &awcMachineName[1], LENSTRCLSID - 2 );

#if DEBUG
      RemBstrNode(*pbstrGuid);
#endif // DEBUG
      if (cbStr == 0) {
        IfFailGoTo(hresult = E_OUTOFMEMORY, Error2);
      }
    }
    else {
      IfFailGoTo(hresult = E_OUTOFMEMORY, Error2);
    }
    //
    // fall through...
    //
Error2:
    MQFreeMemory(qmprops.aPropVar[0].puuid);
    //
    // fall through...
    //
Error:
    delete [] qmprops.aPropID;
    delete [] qmprops.aPropVar;
    delete [] qmprops.aStatus;
    return CreateErrorHelper(hresult, m_ObjectType);
	
#endif // 0 
	
}


//=--------------------------------------------------------------------------=
// CMSMQApplication::Create
//=--------------------------------------------------------------------------=
// creates a new MSMQApplication object.
//
// Parameters:
//    IUnknown *        - [in] controlling unkonwn
//
// Output:
//    IUnknown *        - new object.
//
// Notes:
//
IUnknown *CMSMQApplication::Create
(
    IUnknown *pUnkOuter
)
{
    // make sure we return the private unknown so that we support aggegation
    // correctly!
    //
    CMSMQApplication *pNew = new CMSMQApplication(pUnkOuter);
    return pNew ? pNew->PrivateUnknown() : NULL;
}

//=--------------------------------------------------------------------------=
// CMSMQApplication::CMSMQApplication
//=--------------------------------------------------------------------------=
// create the object and initialize the refcount
//
// Parameters:
//    IUnknown *    - [in] controlling unknown
//
// Notes:
//
#pragma warning(disable:4355)  // using 'this' in constructor
CMSMQApplication::CMSMQApplication
(
    IUnknown *pUnkOuter
)
: CAutomationObject(pUnkOuter, OBJECT_TYPE_OBJMQAPPLICATION, (void *)this)
{

    // TODO: initialize anything here
}
#pragma warning(default:4355)  // using 'this' in constructor

//=--------------------------------------------------------------------------=
// CMSMQApplication::CMSMQApplication
//=--------------------------------------------------------------------------=
// "We all labour against our own cure, for death is the cure of all diseases"
//    - Sir Thomas Browne (1605 - 82)
//
// Notes:
//
CMSMQApplication::~CMSMQApplication ()
{
    // TODO: clean up anything here.
}

//=--------------------------------------------------------------------------=
// CMSMQApplication::InternalQueryInterface
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
HRESULT CMSMQApplication::InternalQueryInterface
(
    REFIID riid,
    void **ppvObjOut
)
{
    CHECK_POINTER(ppvObjOut);

    // we support IMSMQApplication and ISupportErrorInfo
    //
    if (DO_GUIDS_MATCH(riid, IID_IMSMQApplication)) {
        *ppvObjOut = (void *)(IMSMQApplication *)this;
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


//=-------------------------------------------------------------------------=
// DLLGetDocumentation
//=-------------------------------------------------------------------------=
// Called by OLEAUT32.DLL for ITypeInfo2::GetDocumentation2.  This gives us
// a chance to return a localized string for a given help context value.
//
// Parameters:
//	ptlib	[in] - TypeLib associated w/ help context
//	ptinfo  [in]-  TypeInfo associated w/ help context
//      dwHelpStringContext - [in] Cookie value representing the help context
//				   id being looked for.
//	pbstrHelpString - [out] localized help string associated with the
//				context id passed in.
//	
STDAPI DLLGetDocumentation
(
  ITypeLib * ptlib,
  ITypeInfo * ptinfo,
  LCID lcid,
  DWORD dwCtx,
  BSTR * pbstrHelpString
)
{
  
   
	LPTSTR szDllFile;
    BOOL fTriedEnglish = FALSE, fFound = FALSE;

    if (pbstrHelpString == NULL)
      return E_POINTER;
    *pbstrHelpString = NULL;

 
    //
    // UNDONE: switch on lcid to work out what dll to load
    //  the doc string from.  For now we just use this dll.
    //

    switch (PRIMARYLANGID(lcid)) {
    case LANG_FRENCH: // French  
      szDllFile = L"MQOAfr.DLL";
      break;
    case LANG_GERMAN: // German 
      szDllFile = L"MQOAde.DLL";
      break;
    case LANG_JAPANESE: // Japanese 
      szDllFile = L"MQOAjp.DLL";
      break;
    case LANG_ENGLISH: // English 
    case LANG_NEUTRAL:
    default:
      szDllFile = L"MQOA.DLL";
      fTriedEnglish = TRUE;
      break;
    } // switch
    if (!GetMessageOfId(dwCtx, 
                        szDllFile, 
                        FALSE, // fUseDefaultLcid 
                        pbstrHelpString)) {
      //
      // try english as a last resort
      //
      if (!fTriedEnglish) {      
        fFound = GetMessageOfId(
                   dwCtx, 
                   szDllFile, 
                   FALSE, // fUseDefaultLcid 
                   pbstrHelpString);
      }
      return fFound ? NOERROR : TYPE_E_ELEMENTNOTFOUND;
    }
#if DEBUG
    RemBstrNode(*pbstrHelpString);  
#endif // DEBUG

	return NOERROR;
}



//=--------------------------------------------------------------------------=
// Helper: SetStreamPosition
//=--------------------------------------------------------------------------=
// Gets current seek position in stream
//
HRESULT SetStreamPosition(
    IStream *pstm, 
    ULARGE_INTEGER libPosition)
{
    LARGE_INTEGER dlibMove;

    LISet32(dlibMove, libPosition.LowPart); // UNDONE: hi word?
    return pstm->Seek(dlibMove,
                      STREAM_SEEK_SET,
                      &libPosition);
}


//=--------------------------------------------------------------------------=
// Helper: GetCurrentStreamPosition
//=--------------------------------------------------------------------------=
// Gets current seek position in stream
//
HRESULT GetCurrentStreamPosition(
    IStream *pstm, 
    ULARGE_INTEGER *plibCurPosition)
{
    LARGE_INTEGER dlibMove;

    LISet32(dlibMove, 0);
    return pstm->Seek(dlibMove,
                      STREAM_SEEK_CUR,
                      plibCurPosition);
}


