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
	NTLM_NO_INIT_CONTEXT,		// needs context structures to be initialized.  Per request
	NTLM_PROCESSING,			// in the middle of request, keep structures around.
	NTLM_DONE					// Set after 2nd NTLM pass, it's either failed.  Remove context, not library.
}  NTLM_CONVERSATION;


typedef struct {
	NTLM_CONVERSATION m_Conversation;				// Are we in the middle of a request?

	BOOL m_fHaveCredHandle;					// Is m_hcred initialized?
	CredHandle m_hcred;						

	BOOL m_fHaveCtxtHandle;					// Is m_hctxt initialized?
	struct _SecHandle  m_hctxt;				
} AUTH_NTLM, *PAUTH_NTLM;		

BOOL NTLMInitLib(PAUTH_NTLM pNTLMState);
void FreeNTLMHandles(PAUTH_NTLM pNTLMState);

