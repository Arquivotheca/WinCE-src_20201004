//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: AUTH.H
Abstract: Authentication
--*/

// state information for NTLM auth scheme, 1 per request.  This info is maintained across a session.
typedef enum {
	SEC_STATE_NO_INIT_CONTEXT,		// needs context structures to be initialized.  Per request
	SEC_STATE_PROCESSING,			// in the middle of request, keep structures around.
	SEC_STATE_DONE					// Set after 2nd NTLM pass, it's either failed.  Remove context, not library.
}  SECURITY_CONVERSATION_STATE;

typedef enum {
	AUTHTYPE_NONE = 0,
	AUTHTYPE_BASIC,
	AUTHTYPE_NTLM,
	AUTHTYPE_NEGOTIATE,
	AUTHTYPE_UNKNOWN
}
AUTHTYPE;

typedef struct {
	SECURITY_CONVERSATION_STATE m_Stage;		// Are we in the middle of a request?
	AUTHTYPE                    m_AuthType;

	BOOL m_fHaveCredHandle;					// Is m_hcred initialized?
	CredHandle m_hcred;						

	BOOL m_fHaveCtxtHandle;					// Is m_hctxt initialized?
	struct _SecHandle  m_hctxt;
} AUTH_STATE, *PAUTH_STATE;		

void FreeSecContextHandles(PAUTH_STATE pAuthState);

