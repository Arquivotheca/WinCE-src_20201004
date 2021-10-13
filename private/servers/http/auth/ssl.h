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
/*--
Module Name: SSL.H
Abstract: Secure Sockets Handling for HTTPD.
--*/

typedef struct _SSL_INFO  
{
    CtxtHandle                m_hcred;
    BOOL                      m_fHasCtxt;
    SecPkgContext_StreamSizes m_Sizes;
    BOOL                      m_fSSLInitialized;
    DWORD                     m_dwSessionKeySize;
    DWORD                     m_dwSSLPrivKeySize;
    DWORD                     m_dwClientSSLKeySize;
    CERT_CONTEXT              *m_pClientCertContext;
    const CERT_CHAIN_CONTEXT* m_pCertChainContext;  // client cert
    DWORD                     m_cbSerialNumber;
    DWORD                     m_dwCertFlags;        // CRED_XXX flags queried by ISAPI extension on CERT_FLAGS.

    //
    // Channel Binding Token
    //
    SecPkgContext_Bindings    channelBindingToken;
    
} SSL_INFO;

typedef BOOL           (WINAPI *PFN_CERTFREECERTIFICATECONTEXT)(PCCERT_CONTEXT pCertContext);
typedef DWORD          (WINAPI *PFN_CERTGETNAMESTRINGA)(PCCERT_CONTEXT pCertContext, DWORD dwType, DWORD dwFlags, void *pvTypePara,LPSTR pszNameString,DWORD cchNameString);
typedef DWORD          (WINAPI *PFN_CERTGETNAMESTRINGW)(PCCERT_CONTEXT pCertContext, DWORD dwType, DWORD dwFlags, void *pvTypePara,LPWSTR pszNameString,DWORD cchNameString);
typedef PCCERT_CONTEXT (WINAPI *PFN_CERTFINDCERTIFICATEINSTORE)(HCERTSTORE hCertStore, DWORD dwCertEncodingType, DWORD dwFindFlags, DWORD dwFindType, const void *pvFindPara, PCCERT_CONTEXT pPrevCertContext);
typedef HCERTSTORE     (WINAPI *PFN_CERTOPENSTORE)(LPCSTR lpszStoreProvider, DWORD dwEncodingType, HCRYPTPROV hCryptProv, DWORD dwFlags, const void *pvPara);
typedef BOOL           (WINAPI *PFN_CERTCLOSESTORE)(HCERTSTORE hCertStore, DWORD dwFlags);
typedef BOOL           (WINAPI *PFN_SSLCRACKCERTIFICATE)(PUCHAR pbCertificate, DWORD cbCertificate,DWORD dwFlags,PX509Certificate *  ppCertificate);
typedef BOOL           (WINAPI *PFN_CERTGETCERTIFICATECHAIN)(HCERTCHAINENGINE hChainEngine,PCCERT_CONTEXT pCertContext,LPFILETIME pTime,HCERTSTORE hAdditionalStore,PCERT_CHAIN_PARA pChainPara,DWORD dwFlags,LPVOID pvReserved,PCCERT_CHAIN_CONTEXT* ppChainContext);
typedef void           (WINAPI *PFN_CERTCERTFREECERTIFICATECHAIN)(PCCERT_CHAIN_CONTEXT pChainContext);

extern PFN_CERTGETNAMESTRINGA pCertGetNameStringA;

#define MAX_ISSUER_LEN     (MAX_PATH  / 2)
#define MAX_SERIAL_NUMBER  (MAX_PATH  / 2)
#define MAX_USER_NAME      (MAX_PATH)

typedef struct _SSLUserMap 
{
    struct _SSLUserMap *pNext;
    DWORD              cbSerialNumber;
    BYTE               *pBuffer;  // allocated data.  All other members of this class point into this block.
    WCHAR              *szUserName;
    WCHAR              *szIssuerCN;
    BYTE               *pSerialNumber;
} SSLUserMap;

// Maps SSL client cert paramaters to "user" on the web server.  During authentication,
// user is checked versus a virtual root user list or admin user list to determine whether
// they have access or not.

class CSSLUsers 
{
    SSLUserMap *pUserMapHead;

public:
    void InitializeSSLUsers(CReg *pSSLReg, FixedMemDescr *pUserMem);
    CSSLUsers()  { ; }

    void DeInitUsers(void) 
    {
        SSLUserMap *pTrav = pUserMapHead;

        while (pTrav) 
        {
            MyFree(pTrav->pBuffer);
            pTrav = pTrav->pNext;
        }
    }
    

    ~CSSLUsers() { ; }

    WCHAR *MapUser(BYTE *pSerialNumber, DWORD cbSerialNumber, WCHAR *szIssuerCN);
};
