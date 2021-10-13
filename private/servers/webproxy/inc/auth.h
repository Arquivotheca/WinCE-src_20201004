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

#ifndef __AUTH_H__
#define __AUTH_H__

#include "global.h"
#include <sspi.h>

#define MAX_USER_NAME    256

typedef enum {
    AUTH_TYPE_UNKNOWN,
    AUTH_TYPE_NTLM,
    AUTH_TYPE_BASIC    
} AUTH_TYPE;

typedef enum {
    NTLM_NO_INIT_CONTEXT,       // needs context structures to be initialized.  Per request
    NTLM_PROCESSING,            // in the middle of request, keep structures around.
    NTLM_DONE                   // Set after 2nd NTLM pass, it's either failed.  Remove context, not library.
}  NTLM_CONVERSATION;

typedef struct {
    NTLM_CONVERSATION m_Conversation;           // Are we in the middle of a request?

    BOOL m_fHaveCredHandle;                     // Is m_hcred initialized?
    CredHandle m_hcred;                        

    BOOL m_fHaveCtxtHandle;                     // Is m_hctxt initialized?
    struct _SecHandle  m_hctxt;                
} AUTH_NTLM, *PAUTH_NTLM;    


DWORD HandleBasicAuth(const char* szBasicAuth, BOOL* pfAuthSuccess, char* szUser, int cbUser);
DWORD HandleNTLMAuth(const char* szNTLMAuth, char* szNTLMAuthOut, int cbNTLMAuthOut, char* szUser, int cbUser, PAUTH_NTLM pAuth);
void FreeNTLMHandles(PAUTH_NTLM pAuth);
DWORD InitNTLMSecurityLib(void);
DWORD DeinitNTLMSecurityLib(void);
DWORD InitBasicAuth(void);
DWORD DeinitBasicAuth(void);
DWORD GetMaxNTLMBuffBase64Size(DWORD* pdwMaxSecurityBuffSize);


#endif // __AUTH_H__

