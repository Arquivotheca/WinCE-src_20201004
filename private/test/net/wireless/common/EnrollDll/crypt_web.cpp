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
//
// This file has all the crypto, and web related 
//  functions.
/////////////////////////////////////////////////

#include <windows.h>
#include <wincrypt.h>
#include <intsafe.h>
#include <stdio.h>
#include <tchar.h>
#include <svsutil.hxx>
#include <string.h>
#include <string.hxx>
#include <wininet_bridge.h>
#include <enroll.h>
#include <auto_xxx.hxx>

#include "WiFUtils.hpp"

using namespace ce;
using namespace ce::qa;
 /*The number of times we'll allow the user to fail authentication before bailing.*/
const DWORD  MAX_AUTH_FAILS= 5; 

#define MY_ENCODING_TYPE  (PKCS_7_ASN_ENCODING | X509_ASN_ENCODING)


//
// We'll Dynamically load wininet. These are only valid after you've called SetupWinInet()
// 

PInternetOpen lpInternetOpen;
PInternetErrorDlg lpInternetErrorDlg;
PInternetConnect  lpInternetConnect;
PHttpOpenRequest lpHttpOpenRequest;
PHttpSendRequest lpHttpSendRequest;
PHttpQueryInfo lpHttpQueryInfo;
PInternetCloseHandle lpInternetCloseHandle;
PInternetReadFile lpInternetReadFile;
PInternetSetOption lpInternetSetOption;


// Returns the number of cert to read..
// WebPage Must be null terminiated.
HRESULT ParseWebPageToFindReqNo(char *szWebPage , int *piCertNo)
{
    PREFAST_ASSUME(piCertNo);
    //XXX: If certfnsh.asp changes need to change the following lines.
    // If this worked, we'll see a string like...

    char *pStartReq = strstr(szWebPage,"certnew.cer?"); 
    if (pStartReq) // Don't do unless we're found
    {
        // Search forward till the = and then walk till the & and keep the rest of the string.
        // then copy into tUrl;
        while(*pStartReq != '=') ++pStartReq; 

        char *tFindEndNum=++pStartReq; // Eat the = sign.

        while(*tFindEndNum !='&')++tFindEndNum;
        // Zero tFindEndNum
        *(tFindEndNum) = 0;

        *piCertNo = atoi(pStartReq);
        return S_OK;
    }
    else
    {
        LogError( TEXT("Cert Denied! - Reason Unknown Webpage is:\n%S\n "),szWebPage) ;
    }
    return E_FAIL;
}

// Conversion need to write an URL.
// Returns length of Out.
HRESULT  Base64ToURL(char *B64,DWORD cbB64,ce::string &szURLEncodedB64)
{
    szURLEncodedB64.clear();

    const int estimatedEncodingOverhead=400;
    szURLEncodedB64.reserve(cbB64+estimatedEncodingOverhead);

    for ( unsigned int i=0;i<cbB64;i++)
	{
	      switch  (B64[i])
	     	{
			case '/':
                szURLEncodedB64+="%2F";
				break;
			case '+':
                szURLEncodedB64+="%2B";
				break;
			case '=':
                szURLEncodedB64+="%3D";
				break;
			default:
				szURLEncodedB64.append(&B64[i],1);
				break;
		}
	}

	return S_OK;
}


void HandleError(char *s)
{
    LogError(TEXT("An error occurred in running the program. "));
    // s is ascii copy it over.
    LogError( TEXT("%S\n"),s) ;
    LogError(TEXT("Error number %x.\n"), GetLastError());
    LogError( TEXT("Program terminating. "));
    return;
} // End of HandleError

HRESULT SetSmartCardCert( HCRYPTPROV hProv, CRYPT_KEY_PROV_INFO *pKeyProvInfo,PCCERT_CONTEXT pCert)
{
    // 1 Get the user key
    auto_hcryptkey hCurKey;
    if (!CryptGetUserKey(hProv, pKeyProvInfo->dwKeySpec, &hCurKey))
    {
        return E_FAIL;
    }

    // 2 set the certificate parameter.
    if (!CryptSetKeyParam(hCurKey,KP_CERTIFICATE,pCert->pbCertEncoded, 0))
    {

        int error = GetLastError();
        if (error != NTE_BAD_TYPE) {
            // If error is bad_type then we just can't set the property.
            // Likely means our provider isn't a smart card.
            // If there was another error, we should report it.
            LogError(TEXT("CryptSetKeyParam Failed (0x80090020 usually means no room on card) 0x%x"),error);
        }
        return E_FAIL;
    }
    return S_OK; 
}

HRESULT AddCAandRootCertsToStores(PCCERT_CONTEXT  pClientCert, const HCERTSTORE hPKCS7ChainCertStore)
{


    ASSERT(pClientCert);
    ASSERT(hPKCS7ChainCertStore);

    // Open the CA And ROOT stores
    //////////////////////////////////////

    ce::auto_hcertstore hRootCertStore = 
        CertOpenStore(CERT_STORE_PROV_SYSTEM,0,0, CERT_SYSTEM_STORE_CURRENT_USER , TEXT("ROOT"));

    if (!hRootCertStore.valid())
    {
        LogError(TEXT(" hRootCertStore = CertOpenStore Failed.  "));
        return E_FAIL;
    }

    // For Intermediate Certs.
    ce::auto_hcertstore hCACertStore = 
        CertOpenStore(CERT_STORE_PROV_SYSTEM,0,0, CERT_SYSTEM_STORE_CURRENT_USER , TEXT("CA"));

    if (!hCACertStore.valid())
    {
        LogError(TEXT(" hCACertStore = CertOpenStore Failed. "));
        return E_FAIL;
    }


    CERT_CHAIN_PARA ChainPara;
    ZeroMemory(&ChainPara, sizeof(ChainPara));
    ChainPara.cbSize = sizeof(ChainPara);
    ce::auto_cert_chain_context pCertChainContext;

    BOOL bSuccess
        = CertGetCertificateChain(NULL, pClientCert, NULL, hPKCS7ChainCertStore, &ChainPara, 0, NULL, &pCertChainContext);

    if (!bSuccess )
    {
        LogError(TEXT("CertGetCertificateChain Failed. \n"));
        return E_FAIL;
    }

    /*///////////////////////////////////////
      Certificate Chain is a list of cElement certs.
      rgpElement[0] is the user cert
      rgpElement[cElement-1] is the root cert.
    /////////////////////////////////////// */

    PCERT_SIMPLE_CHAIN pChain=pCertChainContext->rgpChain[0];


    // We'll allow the following errors.
    const DWORD dwAllowErrors =     CERT_TRUST_IS_UNTRUSTED_ROOT |  // Since we haven't added the root cert expected!
        CERT_TRUST_IS_NOT_TIME_VALID;  // Happens if Time is wrong.

    const DWORD dwChainTrust = pChain->TrustStatus.dwErrorStatus;
    if (dwChainTrust & ~dwAllowErrors)
    {
        LogWarn(TEXT("Warning CertChain is not Trusted dwChainTrust = 0x%x "),dwChainTrust);
    }

    if(pChain->cElement == 0)
    {
        LogError(TEXT(" Chain element underflow"));
        return E_FAIL;
    }
      

    const DWORD dwRootCertTrust =  pChain->rgpElement[pChain->cElement-1]->TrustStatus.dwInfoStatus;
    if (!(dwRootCertTrust & CERT_TRUST_IS_SELF_SIGNED))
    {
        LogWarn(TEXT("Warning Root CERT is not self signed  dwRootCertTrust = 0x%x "),dwRootCertTrust);
    }


    PCCERT_CONTEXT pRootCert=
         pChain->rgpElement[pChain->cElement-1]->pCertContext;

    // Add Root CERT.
    bSuccess = CertAddCertificateContextToStore(
            hRootCertStore,
            pRootCert,
            CERT_STORE_ADD_NEW , // DWORD dwAddDisposition, 
            NULL // Would have been a pCertContext for Added Cert
            );
    DWORD err = GetLastError();

    if (!bSuccess && err != CRYPT_E_EXISTS)
    {
        LogError(TEXT(" Unable to add Root  Cert  to Root Cert store"));
        return E_FAIL;
    }


    // Now Walk rest of the Cert Chain
    // Add every other cert to the Intermediate (CA) Cert Store.

    for (unsigned int i=1;i<pChain->cElement-1;i++)
    {
        PCCERT_CONTEXT  pCACert= pChain->rgpElement[i]->pCertContext;
        ASSERT(pCACert);

        // Add Intermediate Cert
        bSuccess = CertAddCertificateContextToStore(
                hCACertStore,
                pCACert,
                CERT_STORE_ADD_NEW , // DWORD dwAddDisposition, 
                NULL // Would have been a pCertContext for Added Cert
                );
        err = GetLastError();
        if (!bSuccess && err != CRYPT_E_EXISTS)
        {
            LogError(TEXT(" Unable to add CA  Cert  to CA  store"));
            return E_FAIL;
        }
    }
    return S_OK;
}

// Takes  A decoded Cert Chain
// Returns the client cert in it's store *ppCert Upto the caller to free.
BOOL AddCertificateChainToStoresFromPKCS7Blob( HCERTSTORE hClientCertStore, CRYPT_DATA_BLOB *pPKCS7Blob, CRYPT_KEY_PROV_INFO* pKeyProvInfo,HCRYPTPROV hProvWithKey, PCCERT_CONTEXT* ppClientCertInClientCertStore)
{


    ASSERT(hClientCertStore);

    // Open the CertStore with the BASE 64 Decoded Data.
    ce::auto_hcertstore  hPKCS7ChainCertStore= CertOpenStore(CERT_STORE_PROV_PKCS7,
            X509_ASN_ENCODING| PKCS_7_ASN_ENCODING ,
            0, 
            0 ,
            pPKCS7Blob);

    if (!hPKCS7ChainCertStore)
    {
        LogError(TEXT(" hPKCS7ChainCertStore = CertOpenStore failed"));
        return FALSE;
    }

    /////////////////////////////////////////////////////////////
    //Find the user cert in the chain. 
    //Do this by finding the cert associated with our public key.
    /////////////////////////////////////////////////////////////


    // Get the public key. Takes 2 calls, first for getting size to allocate
    // Second To populate the key.

    DWORD cbCertPubKey=0;
    BOOL bSuccess = CryptExportPublicKeyInfo(
            hProvWithKey,
            pKeyProvInfo->dwKeySpec,
            MY_ENCODING_TYPE,
            NULL,
            &cbCertPubKey 
            );
    if (!bSuccess || !cbCertPubKey)
    {
        LogError(TEXT(" Unable to get size of the public key."));
        return FALSE;
    }

    const ce::auto_ptr<CERT_PUBLIC_KEY_INFO> pCertPubKeyInfo = 
        reinterpret_cast<CERT_PUBLIC_KEY_INFO*>( operator new (cbCertPubKey));

    if (!pCertPubKeyInfo.valid())
    {
        LogError(TEXT(" Unable to alloc  the public key."));
        return FALSE;
    }

    // Second Call to get the actual Key.
    bSuccess = CryptExportPublicKeyInfo(
            hProvWithKey,
            pKeyProvInfo->dwKeySpec, 
            MY_ENCODING_TYPE,
            pCertPubKeyInfo,
            &cbCertPubKey
            );
    if (!bSuccess  || !pCertPubKeyInfo.valid() )
    {
        LogError(TEXT(" Unable to get actual PubKeyInfo."));
        return FALSE;
    }

    // Get the client cert from the chain.
    const ce::auto_cert_context pClientCert = 
        CertFindCertificateInStore( hPKCS7ChainCertStore,
            MY_ENCODING_TYPE,
            0,
            CERT_FIND_PUBLIC_KEY,
            pCertPubKeyInfo,
            NULL);
    if(!pClientCert.valid())
    {
        LogError(TEXT(" CertFindCertificateInStore failed. "));
        return FALSE;
    }

    /*/////////////////////////////////////////////////////
      Add our cert to the client cert store which was passed
      caller needs to have the ClientCert so 
      we won't do a free
    //////////////////////////////////////////////////// */

    bSuccess = CertAddCertificateContextToStore(
            hClientCertStore,
            pClientCert,
            CERT_STORE_ADD_ALWAYS , // DWORD dwAddDisposition, 
            ppClientCertInClientCertStore
            );

    if (!bSuccess)
    {
        LogError(TEXT(" CertAddCertificateContextToStore Failed. "));
        return FALSE;
    }

    // Deal with the rest of the intermediate certs.
    HRESULT hr =  AddCAandRootCertsToStores(pClientCert,hPKCS7ChainCertStore);

    if(FAILED(hr))
    {
        LogError(TEXT("AddCAandRootCertsToStores failed - continuing execution! Please Examine your Cert Stores"));
        return FALSE;
    }

    return TRUE;
}


bool FindB64CertBoundary(char* szWebpage, char** ppStartB64Msg, char** ppStartTrailer)
{
    // Find start of base 64 encoded cert.
	*ppStartB64Msg = strstr(szWebpage,"\r\n"); 
	if (*ppStartB64Msg == NULL)
	{
		return false;
	}

	*ppStartB64Msg += 2; // Skip the \r\n characters to get to the start of the cert

	// The cert trailer begins at the last but one \r\n combination

	ce::string B64Data = *ppStartB64Msg;

	size_t pos = B64Data.find_last_of("\r\n");
	pos = B64Data.find_last_of("\r\n", pos-2);
	
	if(pos == ce::string::npos)
	{
		return false;
	}

	*ppStartTrailer = *ppStartB64Msg + pos-1;
	return true;
}


void RemoveCRAndLF(char* pStartB64Msg, char* pStartTrailer, char* pB64Data)
{
	char *pStartChunk = pStartB64Msg, *pEndChunk;

	while(pStartChunk < pStartTrailer)
	{
		pEndChunk = strstr(pStartChunk, "\r\n");
		const DWORD cbCurrentChunk = pEndChunk-pStartChunk;
		memcpy(pB64Data, pStartChunk, cbCurrentChunk);
		pB64Data += cbCurrentChunk;
		pStartChunk = pEndChunk + 2;
	}
	*pB64Data = 0;	// Null terminator
}


HRESULT AddCertsToStoreFromWebpage( char *szWebpage, CRYPT_KEY_PROV_INFO* pKeyProvInfo,TCHAR *wszCertStore,HCRYPTPROV hProvWithKey,BOOL bIsCertChain)
{
    // Find start of base 64 encoded cert.
    char *pStartB64Msg, *pStartTrailer;
	if(!FindB64CertBoundary(szWebpage, &pStartB64Msg, &pStartTrailer))
	{
        LogError(TEXT("szWebpage doesn't contain expected data."));
        return E_FAIL;
	}
	
	ce::auto_array_ptr<char> pB64EncodedData = new char[strlen(pStartB64Msg) - strlen(pStartTrailer) + 1];

	if(!pB64EncodedData.valid())
    {
        LogError(TEXT("OOM"));
        return E_OUTOFMEMORY;
    }

	//Remove \r\n characters from encoded data
	
	RemoveCRAndLF(pStartB64Msg, pStartTrailer, pB64EncodedData);

	// XXX: Use better buffer size calculator. 
    const DWORD cbBase64EncodeData = strlen(szWebpage);
    DWORD cbDecodedDataEstimate = cbBase64EncodeData  * 2;

    ce::auto_array_ptr<BYTE> pDecodedData =   new BYTE [cbDecodedDataEstimate];

	if(!pDecodedData.valid())
    {
        LogError(TEXT("OOM"));
        return E_OUTOFMEMORY;
    }

    DWORD cbDecodedDataActual=0;
	BOOL err = svsutil_Base64Decode(pB64EncodedData, pDecodedData, cbDecodedDataEstimate, &cbDecodedDataActual, FALSE);
    if (!err) 
    {
        LogError(TEXT("svsutil_Base64Decode Failed"));
        return E_FAIL;
    }


    // Open the cert store.
    ce::auto_hcertstore hStore = CertOpenStore(CERT_STORE_PROV_SYSTEM,0,0, CERT_SYSTEM_STORE_CURRENT_USER , wszCertStore);

    ce::auto_cert_context pCert; // The Pointer to the created cert.
    BOOL bSuccess=FALSE;

    // Call Correct function based on bIsCertChain
    if (bIsCertChain)
    {
        // Decoded Data is a PKCS7 Blob.
        CRYPT_DATA_BLOB PKCS7Blob = {cbDecodedDataActual,pDecodedData};

        // Upto us to free the pCert
        bSuccess = AddCertificateChainToStoresFromPKCS7Blob( 
                hStore,
                &PKCS7Blob, 
                pKeyProvInfo,
                hProvWithKey,
                &pCert);
    }

    else
    {
        // decoded data is an X509 Cert Import it.

        bSuccess = CertAddEncodedCertificateToStore(hStore,
                X509_ASN_ENCODING,
                (BYTE*)pDecodedData,
                cbDecodedDataActual, 
                CERT_STORE_ADD_NEWER_INHERIT_PROPERTIES, 
                &pCert);
    }

    if (!bSuccess || !pCert.valid())
    {
        LogError(TEXT("CertAdd Failed"));
        return E_FAIL;
    }

    // Associate the private key to the Cert.
    bSuccess = CertSetCertificateContextProperty( pCert,CERT_KEY_PROV_INFO_PROP_ID, 0, pKeyProvInfo); 
    if (!bSuccess )
    {
        LogError(TEXT("CertSetCertificateContextProperty Failed"));
        return E_FAIL;
    }

    // Associate the cert to the key on the smart card.
    SetSmartCardCert(hProvWithKey,pKeyProvInfo,pCert);

    return S_OK;
}



// mallocs the B64 Cert, upto someone else to free
HRESULT GetWebPageFromServer(WCHAR *szServer, WCHAR  *szPickupPage ,char **ppWebPage)
{
    DWORD cbBytesRead,cbSize,cbWebPage;
    HRESULT ret=S_OK;
    int  error;


    HINTERNET hSession =  NULL;
    HINTERNET hConnect = NULL;
    HINTERNET hRequest = NULL;

    hSession =
        lpInternetOpen(L"capi-GetCert", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);

    if (!hSession)
    {
        error = GetLastError();
        LogError(TEXT("InternetOpen Failed : %d"),error );
        ret = E_FAIL;
        goto done;
    }

    hConnect = 
        lpInternetConnect(hSession, szServer, INTERNET_DEFAULT_HTTP_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 1);

    if (!hConnect)
    {
        error = GetLastError();
        LogError(TEXT("InternetConnect Failed : %d"),error );
        ret = E_FAIL;
        goto done;
    }
    
    hRequest = 
        lpHttpOpenRequest(hConnect, _T("GET"), szPickupPage, _T("HTTP/1.1"), NULL, 0, 0, 1);

    if (!hRequest)
    {
        error = GetLastError();
        LogError( TEXT("HttpOpen Failed : %d"),error );
        ret = E_FAIL;
        goto done;
    }


    BOOL bWorked = lpHttpSendRequest(hRequest, NULL, 0, NULL, 0);
    if (bWorked == FALSE)
    {
        error = GetLastError();
        LogError( TEXT("HttpSendRequest Failed : %d"),error );
        ret = E_FAIL;
        goto done;
    }
    cbWebPage=0;
    cbSize=sizeof(cbWebPage);

    bWorked = lpHttpQueryInfo(hRequest,HTTP_QUERY_CONTENT_LENGTH|HTTP_QUERY_FLAG_NUMBER,&cbWebPage,&cbSize,0);
    if (bWorked == FALSE || cbWebPage == 0)
    {
        error = GetLastError();
        if (error == 12150)
        {
            LogError(TEXT("The host name [%s] could not be found, are you sure it is right?"),szServer);
        }
        else
        {
            LogError(TEXT("HttpQueryInfo Failed %d "),error);
        }
        ret = E_FAIL;
        goto done;
    }

    // Give an extra byte so can null terminate.
    cbWebPage+=1;

    *ppWebPage = new char [ cbWebPage];
    if (!*ppWebPage)
    {
        error = GetLastError();
        LogError(TEXT("malloc failed error:%d "),error);
        ret = E_FAIL;
        goto done;
    }

    (*ppWebPage)[cbWebPage-1] =0x77; // For Debugging 
    bWorked = lpInternetReadFile(hRequest,*ppWebPage,cbWebPage,&cbBytesRead);
    ASSERT(cbBytesRead == cbWebPage-1);
    if (bWorked == FALSE )
    {
        error = GetLastError();
        LogError(TEXT("InternetReadFile Failed : %d "),error);
        ret = E_FAIL;
        goto done;
    }
    // Null Terminate that web page.
    (*ppWebPage)[cbWebPage-1] =0x66; // For Debugging 
    (*ppWebPage)[cbWebPage-1] =0; // Null terminate
done:
    if (hConnect) lpInternetCloseHandle(hConnect);
    if (hRequest) lpInternetCloseHandle(hRequest);
    if (hSession) lpInternetCloseHandle(hSession);
    return ret;
}

// Generates a Key and the  container if necassary
// Behavior:
//   If Default container doesn't exist create it. If it does exist use it.
//   If a key doesn't exist create it. If it does exist use it.
HRESULT  GetCryptContextAndCreateKeyIfNotExist(CRYPT_KEY_PROV_INFO *pKeyProvInfo, HCRYPTPROV*  phCryptProv, DWORD dwGenKeyFlags) 
{

    *phCryptProv = 0; // Start by setting it to 0.

    // 1) Try to open  the container.
    BOOL   bWorked =  CryptAcquireContext(
            phCryptProv,        // Address for handle to be returned.
            pKeyProvInfo->pwszContainerName,               // Use the current user's logon name.
            pKeyProvInfo->pwszProvName,               // Use the default provider.
            pKeyProvInfo->dwProvType,      // Need to both encrypt and sign.
            NULL);

    // 2) If that doesn't work try to create the container.
    if (!bWorked)
    {
        bWorked = CryptAcquireContext(
                phCryptProv,        // Address for handle to be returned.
                pKeyProvInfo->pwszContainerName,               // Use the current user's logon name.
                pKeyProvInfo->pwszProvName,               // Use the default provider.
                pKeyProvInfo->dwProvType,      // Need to both encrypt and sign.
                CRYPT_NEWKEYSET);              // No flags needed.
    }


    // See if it worked.
    if (!bWorked)
    {
        LogError(TEXT("CryptAcquireContext failed. %x"),GetLastError());
        return E_FAIL;
    }

    if (!*phCryptProv)
    {
        LogError(TEXT("No key container selected\n"));
        return E_FAIL;
    }

    //
    // Check if a key already exisis, 
    //If it don't generate a key!
    ///////////////////////////////////////////
    ce::auto_hcryptkey  hCurKey;
    bWorked =  CryptGetUserKey(*phCryptProv,pKeyProvInfo->dwKeySpec,&hCurKey);
    const bool isKeyAlreadyCreatedAndOK = (bWorked != FALSE);

    if (isKeyAlreadyCreatedAndOK)
    {
        return S_OK;
    }

    int error = GetLastError();
    const bool isKeyCreateRequired = (error == NTE_NO_KEY);

    if (!isKeyCreateRequired) 
    {
        LogError(TEXT("CryptGetUserKey Failed- with error other than NTE_NO_KEY: [0x%X]"),error);
        return E_FAIL;
    }

    // Key doesn't exist, and no-errors create it.

    if (!CryptGenKey(*phCryptProv, pKeyProvInfo->dwKeySpec, dwGenKeyFlags, &hCurKey))
    {
        LogError(TEXT("Second Call to Error: CryptGenKey Failed [%x] "),error);
        return E_FAIL;
    }
    return S_OK;
}


// Does a UUEncoding followed by an URL Encoding.
//

HRESULT EncodeCertReqForWebTransport( const BYTE* pbPKCS10CertReq, const DWORD cbPKCS10CertReq, ce::string &szURLEncodedB64CertReq)
{
    szURLEncodedB64CertReq.clear();

    // UUENCODEing expands a string by about 33% + CRLFs.
    DWORD cbB64EncodedCertEstimate = 0;
    HRESULT hr = ULongMult(cbPKCS10CertReq,2, &cbB64EncodedCertEstimate);
    if(FAILED(hr))
    {
        LogError(TEXT("ULongMult failed hr = %d "),hr);
        return hr;
    }      

    hr = ULongAdd(cbB64EncodedCertEstimate, 6, &cbB64EncodedCertEstimate);
    if(FAILED(hr))
    {
        LogError(TEXT("ULongAdd failed hr = %d "),hr);
        return hr;
    }      
    
	DWORD cbB64EncodedCertActual = 0;

    ce::auto_array_ptr<char> pbB64CertReq = new char[cbB64EncodedCertEstimate];
	
	if (!pbB64CertReq.valid())
    {
        LogError(TEXT("malloc failed"));
        return E_OUTOFMEMORY;
    }

	BOOL ret = svsutil_Base64Encode(const_cast<BYTE*>(pbPKCS10CertReq),cbPKCS10CertReq,pbB64CertReq,cbB64EncodedCertEstimate,&cbB64EncodedCertActual);

    if (!ret)
    {
        LogError(TEXT("svsutil_Base64Encode"));
        return E_FAIL;
    }

    // cbB64Len has been set to the end of the string.
    // We have room, so write the 0.
    (pbB64CertReq)[cbB64EncodedCertActual] =0x66; // For Debugging 
    (pbB64CertReq)[cbB64EncodedCertActual] =0; // Zero Terminate the string

    // will update the szURLEncodedB64CertReq
    hr  = Base64ToURL(pbB64CertReq,cbB64EncodedCertActual,szURLEncodedB64CertReq);
    if (FAILED(hr))
    {
        LogError(TEXT("Base64ToURL failed hr = %d "),hr);
        return hr;
    }


    ASSERT(szURLEncodedB64CertReq[0] != '\n');
    ASSERT(szURLEncodedB64CertReq[0] != '\r');

    // Last 2 bytes are: 0D 0A 00
    // or last  4 bytes are:   0D 0A 0D 0A 00

    // Clear the last 2  CR/LF Pair
    for (int i=0;i<4;i++)
    {
        // trim removes one of any of the char's passed in from the start or end.
        // we've already assumed no \r or \n at the start.
        // can have 2 CR/LF pairs => loop 4 times.
        szURLEncodedB64CertReq.trim("\n\r");
    }

    // Our string is properly encoded.
    return S_OK;
}

// Builds the PutRequest that we send to the webserver.
HRESULT  BuildPutRequestFromPKCS10CertReq(const BYTE* pbPKCS10CertReq, const DWORD cbPKCS10CertReq, 
        TCHAR *tCertAttribs,TCHAR *tCertTemplate,ce::string &szPutRequest)
{
    PREFAST_ASSERT(tCertAttribs);
    PREFAST_ASSERT(tCertTemplate);
    PREFAST_ASSERT(pbPKCS10CertReq);
    PREFAST_ASSERT(cbPKCS10CertReq);


    ce::string szURLEncodedB64CertReq;
    HRESULT hr = EncodeCertReqForWebTransport(pbPKCS10CertReq,cbPKCS10CertReq, szURLEncodedB64CertReq);
    if (!SUCCEEDED(hr) || !szURLEncodedB64CertReq.length())
    {
        LogError(TEXT("EncodeCertReqForWebTransport Failed %d"),hr);
        return E_FAIL;
    }

    // Build the CertAttrib Portion of the put request.

    const bool isTemplatePassedIn =  ( _tcslen(tCertTemplate) > 0 );
    ce::string szCertAttrib = "&CertAttrib=";

    if (isTemplatePassedIn)
    {
        // Add the CertificiateTemplate Property if it's passed in.
        // convert input variables to ascii
        ce::string szCertTemplateArg;
        ce::WideCharToMultiByte(CP_UTF8, tCertTemplate, -1, &szCertTemplateArg);

        // OPTIMIZED: Cut down on some spurios allocations/copies.
        szCertAttrib.reserve(szCertTemplateArg.size() + 200 ); 

        szCertAttrib+= "CertificateTemplate%3a";
        szCertAttrib+= szCertTemplateArg;
        szCertAttrib+= "%0D%0A";
    }

    // Add the user passed in Attribs
    {
        ce::string szCertAttribsForCoversion;
        ce::WideCharToMultiByte(CP_UTF8, tCertAttribs, -1, &szCertAttribsForCoversion);
        szCertAttrib+=szCertAttribsForCoversion;
    }

    // The PutRequest is an ansi string.
    szPutRequest =  "TargetStoreFlags=0&Mode=newreq&SaveCert=yes&CertRequest=";
    szPutRequest += szURLEncodedB64CertReq;
    szPutRequest += szCertAttrib;

    return S_OK;
}


// Returns CertRequestNo.
// or -1 if error.
HRESULT  SendCertRequestToWebServer(TCHAR *server ,TCHAR *page, TCHAR *userName, TCHAR *password, const char * PutRequest, int * p_iReturnRequestNo)
{
    if (!p_iReturnRequestNo)
    {
        LogError(TEXT("Invalid out param passed into SendCertRequestToWebServer."));
        return E_INVALIDARG;
    }
    *p_iReturnRequestNo=-1;

    DWORD cbBytesRead;
    BOOL bWorked;
    int frmLen =0;
    ce::auto_array_ptr<char> szWebPage=0;
    DWORD cbWebPage=0,cbSize=0;
    int cRetries=0;

    // set the return value to invalid case
    HRESULT hr = E_FAIL;

    // for clarity, error-checking has been removed

    HINTERNET hSession = NULL;
    HINTERNET hConnect = NULL;
    HINTERNET hRequest = NULL;
    hSession = lpInternetOpen(L"capi-Export", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0); 
    if (!hSession)
    {
        LogError(TEXT("Internet Open Failed %d "),GetLastError());
        goto done;
    }
    hConnect = lpInternetConnect(hSession, server, INTERNET_DEFAULT_HTTP_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 1);
    if (!hConnect)
    {
        LogError(TEXT("Internet Connect Failed %d "),GetLastError());
        goto done;
    }
    hRequest = lpHttpOpenRequest(hConnect, _T("POST"), page, _T("HTTP/1.1"), NULL, 0, 0, 1);
    if (!hRequest)
    {
        LogError(TEXT("HttpOpenRequest Failed %d "),GetLastError());
        goto done;
    }

    // If given , send the username and password to the server.
    if (_tcslen(userName))
    {
        // Set username and password.
        lpInternetSetOption(hRequest,INTERNET_OPTION_USERNAME,userName,_tcslen(userName));
        lpInternetSetOption(hRequest,INTERNET_OPTION_PASSWORD,password,_tcslen(password));

    }

    do 
    {
        TCHAR hdrs[] = _T("Content-Type: application/x-www-form-urlencoded"); // Necessary.
        TCHAR accept[] = _T("Accept: */*");
        TCHAR *acArray[2] = {accept,0};
         bWorked = lpHttpSendRequest(hRequest, hdrs, _tcslen(hdrs), const_cast<char*>(PutRequest), strlen(PutRequest));

        if (!bWorked) 
        {
            DWORD err = GetLastError();
            if (err == ERROR_INTERNET_NAME_NOT_RESOLVED)
            {
                LogError(TEXT("Server [%s] Not Found"),server);
            }
            else
            {
                // HttpSendRequest will Succeed even with an AUTH Error.
                // (How silly!)
                // Should never get a fail here.
                LogError(TEXT("HttpSendRequest Failed %d"),GetLastError());
                LogError(TEXT("Is Server[%s] correct?"),server);
                LogError(TEXT("Is Page[%s] correct?"),page);
            }
            goto done;
        }

    } while(FALSE);


    // At this point, we have data.
    cbSize=sizeof(cbWebPage);

    bWorked = lpHttpQueryInfo(hRequest,HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,&cbWebPage,&cbSize,0);
    if (bWorked == FALSE || cbWebPage == 0)
    {
        LogError(TEXT("HttpQueryInfo Failed %d "),GetLastError());
        goto done;
    }

    if(HTTP_STATUS_OK != cbWebPage)
    {
        LogError(TEXT("HttpQueryInfo status is not HTTP_STATUS_OK Failed %d "),GetLastError());
        goto done;
    }

    bWorked = lpHttpQueryInfo(hRequest,HTTP_QUERY_CONTENT_LENGTH|HTTP_QUERY_FLAG_NUMBER,&cbWebPage,&cbSize,0);
    if (bWorked == FALSE || cbWebPage == 0)
    {
        LogError(TEXT("HttpQueryInfo Failed %d\n "),GetLastError());
        goto done;
    }

    cbWebPage+=1; // Add an extra byte so we can null terminate.

    szWebPage = new char [cbWebPage];
    if (!szWebPage)
    {
        LogError(TEXT("malloc failed error:%d "),GetLastError());
        goto done;
    }

    szWebPage[cbWebPage-1] =0x77; // For debug purposes.
    bWorked = lpInternetReadFile(hRequest,szWebPage,cbWebPage,&cbBytesRead);
    if (bWorked == FALSE )
    {
        LogError(TEXT("InternetReadFile Failed : %d "),GetLastError());
        goto done;
    }
    ASSERT(cbBytesRead == cbWebPage-1);
    // 0 terminate our webpage.
    szWebPage[cbWebPage-1] =0x66; // For debug purposes.
    szWebPage[cbWebPage-1] =0;


    hr =  ParseWebPageToFindReqNo(szWebPage,p_iReturnRequestNo);
    if (FAILED(hr))
    {
        LogError(TEXT("Client sent the following Request:"));
        LogError(TEXT("%S\n"),PutRequest);

    }
    // Returns -1 on error.
done:
    // close any valid internet-handles "
    if (hConnect) lpInternetCloseHandle(hConnect);
    if (hRequest) lpInternetCloseHandle(hRequest);
    if (hSession) lpInternetCloseHandle(hSession);
    return hr;
}
#define CERT_SUBJECT_NAME "SubjectName-Should Be OverWritten by CA"
// Generates a Cert Req, which if true will need to be freed
// Somewhere else.
HRESULT GeneratePKCS10CertRequest(HCRYPTPROV hCryptProv,CRYPT_KEY_PROV_INFO *pKeyProvInfo,BYTE **ppbPKCS10CertReq, DWORD* pcbPKCS10CertReq)
{
    //-------------------------------------------------------------------
    // Null out in case of premature failure.

    *pcbPKCS10CertReq=0;
    *ppbPKCS10CertReq=NULL;

    // Declare and initialize a CERT_RDN_ATTR array.
    // In this code, only one array element is used.
    CERT_RDN_ATTR rgNameAttr[] = {
        "2.5.4.3",                             // pszObjId 
        CERT_RDN_PRINTABLE_STRING,             // dwValueType
        strlen(CERT_SUBJECT_NAME),             // value.cbData
        (BYTE*)CERT_SUBJECT_NAME               // value.pbData
    };             

    //-------------------------------------------------------------------
    // Declare and initialize a CERT_RDN array.
    // In this code, only one array element is used.

    CERT_RDN rgRDN[] = 
    {
        1,                 // rgRDN[0].cRDNAttr
        &rgNameAttr[0]
    };   // rgRDN[0].rgRDNAttr

    //-------------------------------------------------------------------
    // Declare and initialize a CERT_NAME_INFO structure.

    CERT_NAME_INFO Name = 
    {
        1,                  // Name.cRDN
        rgRDN
    };             // Name.rgRDN




    //-------------------------------------------------------------------
    //    Begin processing.
    //-------------------------------------------------------------------
    //  Call CryptEncodeObject for size

    DWORD  cbNameEncoded=0;
    if(!CryptEncodeObject(
                MY_ENCODING_TYPE,     // Encoding type
                X509_NAME,            // Structure type
                &Name,                // Address of CERT_NAME_INFO structure
                NULL,                 // pbEncoded
                &cbNameEncoded))      // pbEncoded size
    {
        HandleError("First call to CryptEncodeObject failed.\
                \nA public/private key pair may not exit in the container. \n");
        return E_FAIL;
    }
    //-------------------------------------------------------------------
    //     Allocate memory for the encoded name.

    ce::auto_array_ptr<BYTE>  pbNameEncoded =  new BYTE[cbNameEncoded];

    if (!pbNameEncoded.valid())
    {
        HandleError("pbNamencoded malloc operation failed.\n");
        return E_FAIL;
    }

    //-------------------------------------------------------------------
    //  Call CryptEncodeObject to do the actual encoding of the name.

    if(!CryptEncodeObject(
                MY_ENCODING_TYPE,    // Encoding type
                X509_NAME,           // Structure type
                &Name,               // Address of CERT_NAME_INFO structure
                pbNameEncoded,       // pbEncoded
                &cbNameEncoded))     // pbEncoded size
    {
        HandleError("Second call to CryptEncodeObject failed.\n");
        return E_FAIL;
    }


    //--------------------------------------------------------------------
    // Call CryptExportPublicKeyInfo to get the size of the returned
    // information.

    DWORD  cbPublicKeyInfo=0;
    if(!CryptExportPublicKeyInfo(
                hCryptProv,            // Provider handle
                pKeyProvInfo->dwKeySpec,          // Key spec
                MY_ENCODING_TYPE,      // Encoding type
                NULL,                  // pbPublicKeyInfo
                &cbPublicKeyInfo))     // Size of PublicKeyInfo
    {
        HandleError("First call to CryptExportPublickKeyInfo failed.\
                \nProbable cause: No key pair in the key container. \n");
        return E_FAIL;
    }

    //--------------------------------------------------------------------
    // Allocate the necessary memory.
    ce::auto_ptr<CERT_PUBLIC_KEY_INFO>  pPublicKeyInfo  = 
        reinterpret_cast<CERT_PUBLIC_KEY_INFO*> (operator new (cbPublicKeyInfo));

    if (!pPublicKeyInfo.valid())
    {
        HandleError("Memory allocation failed.");
        return E_OUTOFMEMORY;
    }

    //--------------------------------------------------------------------
    // Call CryptExportPublicKeyInfo to get pbPublicKeyInfo.

    if(!CryptExportPublicKeyInfo(
                hCryptProv,            // Provider handle
                pKeyProvInfo->dwKeySpec,          // Key spec
                MY_ENCODING_TYPE,      // Encoding type
                pPublicKeyInfo,
                &cbPublicKeyInfo))     // Size of PublicKeyInfo
    {
        HandleError("Second call to CryptExportPublicKeyInfo failed.");
        return E_FAIL;
    }

    CERT_REQUEST_INFO  CertReqInfo;

    CertReqInfo.dwVersion = CERT_REQUEST_V1;
    //--------------------------------------------------------------------
    // Set the subject member of CertReqInfo to point to 
    // a CERT_NAME_INFO structure that 
    // has been initialized with the data from cbNameEncoded
    // and pbNameEncoded.
    const CERT_NAME_BLOB subject ={cbNameEncoded,pbNameEncoded}; 
    CertReqInfo.Subject =subject; 

    //--------------------------------------------------------------------
    // Set the SubjectPublicKeyInfo member of the 
    // CERT_REQUEST_INFO structure to point to the CERT_PUBLIC_KEY_INFO 
    // structure created.

    CertReqInfo.SubjectPublicKeyInfo = *pPublicKeyInfo;

    // no extra attributes.
    CertReqInfo.cAttribute = 0; 
    CertReqInfo.rgAttribute = NULL;


    // Setup Signature Algorithon to be sha1RsaSign  and NULL Parmamters.
    CRYPT_ALGORITHM_IDENTIFIER  SigAlg = {szOID_OIWSEC_sha1RSASign,{0,0}};

    //--------------------------------------------------------------------
    // Call CryptSignAndEncodeCertificate to get the size of the
    // returned BLOB.



    if(!CryptSignAndEncodeCertificate(
                hCryptProv,                      // Crypto provider
                pKeyProvInfo->dwKeySpec,                  // Key spec
                MY_ENCODING_TYPE,                // Encoding type
                X509_CERT_REQUEST_TO_BE_SIGNED,  // Structure type
                &CertReqInfo,                    // Structure information
                &SigAlg,                         // Signature algorithm
                NULL,                            // Not used
                NULL,                            // pbSignedEncodedCertReq
                pcbPKCS10CertReq))          // Size of certificate 
        // required
    {
        HandleError("CryptSignandEncode for size failed.");
        return E_FAIL;
    }
    //--------------------------------------------------------------------
    // Allocate memory for the encoded certificate request.

    *ppbPKCS10CertReq = new BYTE[*pcbPKCS10CertReq];

    if(!*ppbPKCS10CertReq )
    {
        HandleError("Malloc operation failed.");
        return E_OUTOFMEMORY;
    }
    //--------------------------------------------------------------------
    // Call CryptSignAndEncodeCertificate to get the 
    // returned BLOB.

    if(!CryptSignAndEncodeCertificate(
                hCryptProv,                     // Crypto provider
                pKeyProvInfo->dwKeySpec,                 // Key spec
                MY_ENCODING_TYPE,               // Encoding type
                X509_CERT_REQUEST_TO_BE_SIGNED, // Struct type
                &CertReqInfo,                   // Struct info        
                &SigAlg,                        // Signature algorithm
                NULL,                           // Not used
                *ppbPKCS10CertReq,         // Pointer
                pcbPKCS10CertReq))         // Length of the message
    {
        HandleError("CryptSignAndEncode for data failed.");
        return E_FAIL;
    }

    return S_OK;
}

// Does the link to Wininet and sets up the global variables.
// It's up to the application to free the HINSTANCE
// returns  0 if error.
HRESULT SetupWinInet(HINSTANCE  *phWinInet)
{
    *phWinInet =  LoadLibrary(_T("WININET.DLL"));
    // Do some GetProcAddress for our stuffs.
    if (!*phWinInet)
    {
        LogError(TEXT("LoadLibrary failed to load WinInet"));
        return E_FAIL;
    }

    //XXX: Change these to use unicodable constants..
    lpInternetOpen =
        (PInternetOpen ) GetProcAddress(*phWinInet,_T("InternetOpenW"));
    lpHttpQueryInfo =
        (PHttpQueryInfo ) GetProcAddress(*phWinInet,_T("HttpQueryInfoW"));
    lpInternetConnect =
        ( PInternetConnect ) GetProcAddress(*phWinInet,_T("InternetConnectW"));
    lpHttpOpenRequest =
        (PHttpOpenRequest ) GetProcAddress(*phWinInet,_T("HttpOpenRequestW"));
    lpHttpSendRequest =
        (PHttpSendRequest ) GetProcAddress(*phWinInet,_T("HttpSendRequestW"));
    lpInternetSetOption = 
        (PInternetSetOption) GetProcAddress(*phWinInet,_T("InternetSetOptionW"));

    lpInternetCloseHandle =
        (PInternetCloseHandle ) GetProcAddress(*phWinInet,_T("InternetCloseHandle"));
    lpInternetErrorDlg =
        (PInternetErrorDlg ) GetProcAddress(*phWinInet,_T("InternetErrorDlg"));
    lpInternetReadFile =
        (PInternetReadFile ) GetProcAddress(*phWinInet,_T("InternetReadFile"));

    if ( !lpInternetOpen || !lpInternetErrorDlg || !lpInternetConnect || 
            !lpHttpOpenRequest || !lpHttpSendRequest || !lpHttpQueryInfo  ||
            !lpInternetCloseHandle || !lpInternetReadFile || !lpInternetSetOption)
    {
        LogError(TEXT("Get ProcAddress Failed"));
        return E_FAIL ;
    }
    return S_OK;
}
