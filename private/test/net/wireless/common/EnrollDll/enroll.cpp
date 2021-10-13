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
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>
#include <tchar.h>
#include <string.h>
#include <wininet_bridge.h>
#include <enroll.h>
#include <auto_xxx.hxx>
#include <string.hxx>
#include "NetMain_t.hpp"
#include <assert.h>
#include <cmnprint.h>
#include <netcmn.h>

#include "WiFUtils.hpp"
using namespace ce::qa;

TCHAR *gszMainProgName = TEXT("EnrollDll");
DWORD  gdwMainInitFlags = 0;


//
static CKato *s_pKato = NULL;
static BOOL   s_fNetlogInitted = FALSE;

// ----------------------------------------------------------------------------
//
// Forwards debug and error messages from CmnPrint to Kato.
//
static void
ForwardDebug(const TCHAR *pMessage)
{
    if (s_fNetlogInitted)
    {
        NetLogDebug(TEXT("%s"), pMessage);
    }
    else
    if (s_pKato)
    {
        s_pKato->Log(LOG_COMMENT, TEXT("%s"), pMessage);
    }
    else
    {
        DEBUGMSG(TRUE, (TEXT("%s"), pMessage));
    }
}

static void
ForwardWarning(const TCHAR *pMessage)
{
    if (s_fNetlogInitted)
    {
        NetLogWarning(TEXT("%s"), pMessage);
    }
    else
    if (s_pKato)
    {
        s_pKato->Log(LOG_DETAIL, TEXT("%s"), pMessage);
    }
    else
    {
        RETAILMSG(TRUE, (TEXT("%s"), pMessage));
    }
}

static void
ForwardError(const TCHAR *pMessage)
{
    if (s_fNetlogInitted)
    {
        NetLogError(TEXT("%s"), pMessage);
    }
    else
    if (s_pKato)
    {
        s_pKato->Log(LOG_FAIL, TEXT("%s"), pMessage);
    }
    else
    {
        RETAILMSG(TRUE, (TEXT("%s"), pMessage));
    }
}

static void
ForwardAlways(const TCHAR *pMessage)
{
    if (s_fNetlogInitted)
    {
        NetLogMessage(TEXT("%s"), pMessage);
    }
    else
    if (s_pKato)
    {
        s_pKato->Log(LOG_DETAIL, TEXT("%s"), pMessage);
    }
    else
    {
        RETAILMSG(TRUE, (TEXT("%s"), pMessage));
    }
}



#ifndef UNICODE
#error // We Don't do the imports of non unicode  wininet!!
// Can be fixed easily if needed.
#endif 

// From crypt_web.cpp
HRESULT SetupWinInet(HINSTANCE  *phWinInet);
HRESULT SetOptions (TCHAR *PassedInFileName);
HRESULT GetCryptContextAndCreateKeyIfNotExist(CRYPT_KEY_PROV_INFO *pKeyProvInfo, HCRYPTPROV*  phCryptProv, DWORD dwGenKeyFlags);
HRESULT GeneratePKCS10CertRequest(HCRYPTPROV hCryptProv,CRYPT_KEY_PROV_INFO *pKeyProvInfo,BYTE **ppbPKCS10CertReq, DWORD* pcbPKCS10CertReq);
HRESULT BuildPutRequestFromPKCS10CertReq(const BYTE* pbPKCS10CertReq, const DWORD cbPKCS10CertReq, TCHAR *tCertAttribs,TCHAR *tCertTemplate,ce::string &szPutRequest);
HRESULT SendCertRequestToWebServer(TCHAR *server ,TCHAR *page, TCHAR *userName, TCHAR *password, const char * PutRequest, int * p_iReturnRequestNo);
HRESULT GetWebPageFromServer(WCHAR *szServer, WCHAR  *szPickupPage ,char **ppWebPage);
HRESULT AddCertsToStoreFromWebpage( char *szWebpage, CRYPT_KEY_PROV_INFO* pKeyProvInfo,TCHAR *wszCertStore,HCRYPTPROV hProvWithKey,BOOL bIsCertChain);



// Static Defaults used in SetOptions
static const TCHAR  defCertStore[] = _T("MY");   
static const TCHAR  defServer[] = _T("defserver"); 
static const TCHAR  defCertRequestPage[] = _T("/certsrv/certfnsh.asp"); 
static const TCHAR  defCertPickupTemplate[] = _T("/certsrv/certnew.cer?ReqId=%i&Enc=b64"); 
static const TCHAR  defCertPickupChainTemplate[] = _T("/certsrv/certnew.p7b?ReqId=%i&Enc=b64"); 
static const TCHAR  defUserName[] = _T(""); 
static const TCHAR  defPassword[] = _T(""); 
static const TCHAR  defCertAttribs[] = _T(""); 
static const TCHAR  defCertTemplate[] = _T("ClientAuth"); 


//
// Global Variables used for options, 
// Shared between DoEnroll and SetOptions.
//
#define MAX_SIZE 255
static CRYPT_KEY_PROV_INFO gKeyProvInfo;
static DWORD gdwKeyFlags;
static TCHAR  gCertStore[MAX_SIZE] ; 
static TCHAR  gServer[MAX_SIZE];
static TCHAR  gCertRequestPage[MAX_SIZE] ;
static TCHAR  gCertPickupTemplate[MAX_SIZE];
static TCHAR  gCertPickupChainTemplate[MAX_SIZE];
static TCHAR  gUserName[MAX_SIZE];
static TCHAR  gPassword[MAX_SIZE];
static TCHAR  gCertAttribs[MAX_SIZE];
static TCHAR  gDeprecated[MAX_SIZE]; //Used to store deprecated Values
static TCHAR  gCertTemplate[MAX_SIZE];
static DWORD  gCertPickupChain;

// Used when setting params from file.
// These are pointed to when data is needed for keyProvInfo string
// fields to point at.
static TCHAR  gKeyContainerName[MAX_SIZE]; 
static TCHAR  gKeyProviderName[MAX_SIZE];

////////////////////////////////////////////////////////
// This main procedure does the enrollment.
// The only other thing that may need to be change is 
// the ParseWebPageToFindReqNo
///////////////////////////////////////////////////////

HRESULT Enroll()
{
    ce::auto_hcryptprov hCryptProv;
    HRESULT hr  = 	GetCryptContextAndCreateKeyIfNotExist(&gKeyProvInfo,&hCryptProv, gdwKeyFlags);
    if(FAILED(hr) || !hCryptProv.valid())
    {
        LogError(TEXT("CreateKeyAndGetContext Failed"));
        return hr;
    }

    ce::auto_array_ptr<BYTE> pbPKCS10CertReq;
    DWORD cbPKCS10CertReq;                    

    // GeneratePCKS10CertRequest will allocate.
    hr = GeneratePKCS10CertRequest(hCryptProv,&gKeyProvInfo,&pbPKCS10CertReq,&cbPKCS10CertReq);

    if( FAILED(hr)  || !pbPKCS10CertReq.valid())
    {
        LogError(TEXT("GeneratePKCS10CertRequest Failed"));
        return hr;
    }

    // BuildPutRequest will allocate the szPutRequest
    ce::string szPutRequest; 
    hr =  BuildPutRequestFromPKCS10CertReq(pbPKCS10CertReq,cbPKCS10CertReq,gCertAttribs, gCertTemplate,szPutRequest);
    if (FAILED(hr) || !szPutRequest.length())
    {
        LogError(TEXT("BuildPutRequest Failed"));
        return hr;
    }

    // Send the put request to the CertServer. It includes the cert and certattribs.
    // Will return to us the PickupNumber for the cert.

    int iReqNo = -1;
    hr = SendCertRequestToWebServer(gServer,gCertRequestPage,gUserName,gPassword,szPutRequest.get_buffer(), &iReqNo);

    switch (hr)
    {
        case S_OK:
            {
                // everything is good, iReqNo should have appropriate value
                ASSERT(-1 != iReqNo);
            }break;

        case S_FALSE:
            {
                // user canceled the request.  Not an error but don't have enough info iReqNo should be -1
                ASSERT(-1 == iReqNo);
                return hr;
            }

        default:
            {
                // some sort of error happened.
                LogError(TEXT("SendCertRequestToWebServer Failed"));
                return hr;
            }
    }

    TCHAR certPickupPage[MAX_PATH+1];
    certPickupPage[MAX_PATH]=0; // Terminate the string

    // 4) Fill Template, depends on if we want the chain or the cert.
    if (gCertPickupChain)
    {
        _sntprintf_s(certPickupPage, _countof(certPickupPage),_TRUNCATE,gCertPickupChainTemplate,iReqNo);
    }
    else
    {
        _sntprintf_s(certPickupPage, _countof(certPickupPage),_TRUNCATE,gCertPickupTemplate,iReqNo);
    }

    // Allocates the B64Cert
    ce::auto_array_ptr<char> szWebPage;
    hr  = GetWebPageFromServer(gServer,certPickupPage,&szWebPage);
    if(FAILED(hr) || !szWebPage.valid()) 
    {
        LogError(TEXT("GetCertFromWebServer Failed"));
        return hr;
    }

    // Will Add the Cert to Appropriate Cert Store,
    // If bIsCertChain is set, will also add root and intermediate certs.
    // This also sets the smart card options

    hr = AddCertsToStoreFromWebpage(szWebPage,&gKeyProvInfo,gCertStore,hCryptProv,gCertPickupChain);

    if (FAILED(hr))
    {
        LogError(TEXT("AddCertToStore Failed"));
        return hr;
    }

    return hr;
}

HRESULT DoEnroll(TCHAR* pCmdLine, HANDLE logHandle)
{
    HRESULT hr = E_FAIL;

#ifndef NETLOG_WORKS_PROPERLY
    logHandle = NULL;
#endif    

    if(logHandle)
    {
        NetLogSetInitialize(logHandle);
        s_fNetlogInitted = TRUE;
    }
    else
    {
        s_pKato = (CKato *)KatoGetDefaultObject();
    }


    // Initialize cmnprint.
    CmnPrint_Initialize(gszMainProgName);
    CmnPrint_RegisterPrintFunction(PT_LOG,     ForwardDebug,   FALSE);
    CmnPrint_RegisterPrintFunction(PT_WARN1,   ForwardWarning, FALSE);
    CmnPrint_RegisterPrintFunction(PT_WARN2,   ForwardWarning, FALSE);
    CmnPrint_RegisterPrintFunction(PT_FAIL,    ForwardError,   FALSE);
    CmnPrint_RegisterPrintFunction(PT_VERBOSE, ForwardAlways, FALSE);

    // Must have Wininet
    ce::auto_hlibrary hWinInet;
    hr =  SetupWinInet(&hWinInet);
    if (FAILED(hr) || !hWinInet.valid())
    {
        LogError(TEXT("LoadLibrary failed to load WinInet"));
        goto done;
    }

    if (!_tcsncicmp(pCmdLine,_T("-s"),2))
    {
        // Server Passed In.
        _tcsncpy_s(gServer, _countof(gServer),(pCmdLine+2),MAX_SIZE-1);
        hr = SetOptions(0);
        if (FAILED(hr)) goto done;
    }
    else if (!_tcsncicmp(pCmdLine,_T("-f"),2))
    {
        // FileName Passed in.
        hr = SetOptions((pCmdLine+2));
        if (FAILED(hr)) goto done;
    }
    else if (!_tcsncicmp(pCmdLine,_T("-d"),2))
    {
        // We're in diagnostics mode.
        LogDebug(TEXT("Entering Diagnostics Mode:"));
        LogDebug(TEXT("Checking For NTLMSSP:"));
        if (!(hWinInet = LoadLibrary(_T("ntlmssp.dll"))))
        {
            LogError(TEXT("ERROR nltmssp.dll not found.\n Check for SYSGEN_AUTH_NTLM"));
        }
        else
        {
            LogDebug(TEXT("NTLMSSP found!"));
            LogDebug(TEXT("Everything looks good."));
        }
        goto done;
    }
    else if (!_tcsncicmp(pCmdLine,_T(""),1))
    {
        TCHAR tExecPath[MAX_PATH+1];
        DWORD dwLen;

        tExecPath[MAX_PATH]=0;// Null terminate the string

        // If nothing passed in try to read <exename>.cfg in dir our exe lives in.
        // Usually this will be enroll.cfg

        // Grab the ModuleFileName , change .exe to .cfg
        dwLen = GetModuleFileName(
                0, // hModule 
                tExecPath,
                MAX_PATH
                );
        // Make Sure the . is in the right spot.
        if (dwLen && _T('.') == tExecPath[dwLen -4] )
        {
            // Replace exe with cfg
            _tcscpy_s(&tExecPath[dwLen-3], MAX_PATH - (dwLen-3) ,_T("cfg"));
        }
        else
        {
            if(!dwLen)
            {
                LogError(TEXT("GetModuleFileName Failed: 0x%x or "),GetLastError());
            }
            LogError(TEXT(". in wrong spot %s"),tExecPath);
            _tcscpy_s(tExecPath, MAX_PATH,_T(""));
        }

        hr =  SetOptions(tExecPath);
        if (FAILED(hr)) 
        {
            LogError(TEXT("Error Input must be -d , -sServerName , or -fFileName"));
            goto done;
        }
    }
    else
    {
        // no setttings we understand and enroll.cfg not in current dir.
        LogError(TEXT("Error Input must be -d , -sServerName , or -fFileName\n[%s] - is not acceptable"),pCmdLine);
        goto done;
    }

    hr = Enroll();
    if(S_OK == hr)
        LogDebug(TEXT("Cert Has Been Added Succesfully"));

done:
    return hr;

}


//  Hex numbers start with 0x
//  Handy function.
//    If Line doesn't Starts with strToFind returns false
//    If Line does Starts with strToFind 
//    {
//      cpy option to OptToSet and return OptToSet
//    }
//    
bool getInt(char *Line , char* strToFind ,DWORD *OptToSet) 
{
    int n = strlen(strToFind);

    // Not Found. return 0
    if (_strnicmp(Line,strToFind,n)) return 0;
    if (!OptToSet) 
    {
        ASSERT(0);
        return false;
    }

    // Skip the option itself.
    Line+=n;

    // Lets use sscanf to read the input.
    // this will let us pick up hex numbers.
    int i = sscanf_s(Line,"0x%X",OptToSet);
    if (1 != i)  // sscanf failed.
    {
        // Try scanf again with 0X
        i = sscanf_s(Line,"0X%X",OptToSet);
        if(1 != i ) 
        {
            // The number should be base 10.
            *OptToSet = atoi(Line);
        }
    }
    return true;
}
// 
//  Handy function.
//    If Line doesn't Starts with strToFind returns false
//    If Line does Starts with strToFind an OptToSet 0 then return true
//    If Line does Starts with strToFind an OptToSet valid then 
//    {
//      cpy option to OptToSet and return true
//    }
//    
bool getString(char *Line , char* strToFind ,TCHAR *OptToSet, DWORD OptSizeInCount ) 
{
    int n = strlen(strToFind);

    // Not Found. return 0
    if (_strnicmp(Line,strToFind,n)) return false;
    if (!OptToSet) return true; // used when parsing comments

    // Skip the option itself.
    Line+=n;

    // End the line at first \r or \n
    int LineLen = strlen(Line);
    for(int i=0;i<LineLen;i++)
    {
        if (Line[i] == '\r' || Line[i] == '\n') 
        {
            Line[i] = 0;
            break;
        }
    }

    //XXX: Unicode TO FIX!!
    size_t numberCchConverted = 0;
    mbstowcs_s( &numberCchConverted,OptToSet, OptSizeInCount, Line, strlen(Line) + 1 );
    return true;
}

#define MAX_OPTION_LINE 200
HRESULT  SetOptions (TCHAR *PassedInFileName)
{
    TCHAR *tFileName ;

    ///////////////////////////////
    // Set the default options.
    //////////////////////////////
    gKeyProvInfo.pwszContainerName = L"enroll"; 
    gKeyProvInfo.pwszProvName = 0; 
    gKeyProvInfo.dwProvType = PROV_RSA_FULL;
    gKeyProvInfo.dwFlags = 0; 
    gKeyProvInfo.cProvParam = 0;
    gKeyProvInfo.rgProvParam = NULL;
    gKeyProvInfo.dwKeySpec = AT_SIGNATURE; 

    gCertPickupChain=1; // By Default Pickup Cert Chain.
    _tcscpy_s(gCertStore, _countof(gCertStore),defCertStore);
    _tcscpy_s(gCertRequestPage, _countof(gCertRequestPage),defCertRequestPage);
    _tcscpy_s(gCertPickupTemplate, _countof(gCertPickupTemplate),defCertPickupTemplate);
    _tcscpy_s(gCertPickupChainTemplate, _countof(gCertPickupChainTemplate),defCertPickupChainTemplate);
    _tcscpy_s(gUserName, _countof(gUserName),defUserName);
    _tcscpy_s(gPassword, _countof(gPassword),defPassword);
    _tcscpy_s(gCertAttribs, _countof(gCertAttribs),defCertAttribs);
    _tcscpy_s(gCertTemplate, _countof(gCertTemplate),defCertTemplate);


    if (PassedInFileName && _tcslen(PassedInFileName))  tFileName = PassedInFileName;
    else return S_OK;

    errno_t err;
    FILE* fOpt    = NULL;  

    err = _tfopen_s(&fOpt, tFileName,_T("r"));    
    if(!fOpt || err !=0)
    {
        LogError(TEXT("Unable to open Option File[%s] -Fatal since Can't Get a Server"),tFileName);
        return E_FAIL;
    }

    // While we have data.
    char MaxLine[MAX_OPTION_LINE];

    MaxLine[sizeof(MaxLine)-1]=0;// Null Terminate the buffer

    bool  isServerFound=false; // Set to true when server field is set.

    while ( fgets (MaxLine,sizeof(MaxLine)-1,fOpt))
    {
        if (strlen(MaxLine) < 3) continue; // Skip Blanks
        if (getString(MaxLine,"//",NULL,0)) continue; // Skip comments
        else if (getString(MaxLine,"%",NULL,0)) continue; // Skip comments
        else if (getString(MaxLine,";",NULL,0)) continue; // Skip comments

        // Get the String Options.
        else if (getString(MaxLine,"USERNAME=",gUserName, _countof(gUserName)));
        else if (getString(MaxLine,"PASSWORD=",gPassword, _countof(gPassword))) ;
        else if (getString(MaxLine,"CERT_STORE=",gCertStore, _countof(gCertStore)));
        else if (getString(MaxLine,"SERVER=",gServer, _countof(gServer))) isServerFound=true;
        else if (getString(MaxLine,"CERT_REQ_PAGE=",gCertRequestPage, _countof(gCertRequestPage)));
        else if (getString(MaxLine,"CERT_PICKUP_TEMPLATE=",gCertPickupTemplate, _countof(gCertPickupTemplate)));
        else if (getString(MaxLine,"CERT_PICKUP_CHAIN_TEMPLATE=",gCertPickupChainTemplate, _countof(gCertPickupChainTemplate)));
        else if (getString(MaxLine,"CERT_ATTRIBS=",gCertAttribs, _countof(gCertAttribs)));
        else if (getString(MaxLine,"CERT_OTHER_ATTRIBS=",gDeprecated, _countof(gDeprecated)))
        {
            LogDebug(TEXT("CERT_OTHER_ATTRIBS has been deprecated"));
            LogDebug(TEXT("Please update your .cfg file to use "));
            LogDebug(TEXT("CERT_ATTRIBS and/or CERT_TEMPLATE instead"));
        }
        else if (getString(MaxLine,"CERT_TEMPLATE=",gCertTemplate, _countof(gCertTemplate)));

        else if (getString(MaxLine,"KEY_PROVIDER_NAME=",gKeyProviderName, _countof(gKeyProviderName)))
        {
            gKeyProvInfo.pwszProvName = gKeyProviderName; 
        }
        else if (getString(MaxLine,"KEY_CONTAINER_NAME=",gKeyContainerName, _countof(gKeyContainerName)))
        {
            gKeyProvInfo.pwszContainerName = gKeyContainerName; 
        }
        // Get the integer options.
        else if (getInt(MaxLine,"DW_KEY_SPEC=",&gKeyProvInfo.dwKeySpec));
        else if (getInt(MaxLine,"DW_PROV_TYPE=",&gKeyProvInfo.dwProvType));
        else if (getInt(MaxLine,"DW_FLAGS=",&gKeyProvInfo.dwFlags));
        else if (getInt(MaxLine,"CERT_CHAIN=",&gCertPickupChain));
        else if (getInt(MaxLine,"DW_KEY_FLAGS=",&gdwKeyFlags));
        else
        {
            LogError(TEXT("Unable to process line [%S]"),MaxLine );
        }
    }

    if (!isServerFound) 
    {
        LogError(TEXT("SERVER not set in config file. This is a Fatal Error "));
        return E_FAIL;
    }
    fclose(fOpt);
    return S_OK;
}



/////////////////////
// Entry Point.
//////////////////

int WINAPI WinMain(
        HINSTANCE    hInstance, HINSTANCE hPrevInstance,
        TCHAR *pCmdLine, int    nShowCmd)
{

    DoEnroll(pCmdLine, NULL);  

    return 0;
}
