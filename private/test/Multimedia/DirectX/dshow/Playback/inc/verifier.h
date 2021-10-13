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
////////////////////////////////////////////////////////////////////////////////
//
//  Verifier defines from compiler flags
//
////////////////////////////////////////////////////////////////////////////////


#include "Verification.h"

#ifdef AUDIO_VERIFIERS
extern BOOL InitializeAudioVerifications();
extern VOID FreeAudioVerifications();
extern VerificationMap *GetAudioVerifications();
#endif

#ifdef BASIC_VERIFIERS
extern BOOL InitializeBasicVerifications();
extern VOID FreeBasicVerifications();
extern VerificationMap *GetBasicVerifications();
#endif

#ifdef LATENCY_VERIFIERS
extern BOOL InitializeLatencyVerifications();
extern VOID FreeLatencyVerifications();
extern VerificationMap *GetLatencyVerifications();
#endif

#ifdef MISC_VERIFIERS
extern BOOL InitializeMiscVerifications();
extern VOID FreeMiscVerifications();
extern VerificationMap *GetMiscVerifications();
#endif

#ifdef SEEK_VERIFIERS
extern BOOL InitializeSeekVerifications();
extern VOID FreeSeekVerifications();
extern VerificationMap *GetSeekVerifications();
#endif

#ifdef VIDEO_VERIFIERS
extern BOOL InitializeVideoVerifications();
extern VOID FreeVideoVerifications();
extern VerificationMap *GetVideoVerifications();
#endif

#ifdef RS_VERIFIERS
extern BOOL InitializeRSVerifications();
extern VOID FreeRSVerifications();
extern VerificationMap *GetRSVerifications();
#endif

BOOL    
CVerificationMgr::InitializeVerifications()
{
    BOOL rtn = TRUE;

#ifdef AUDIO_VERIFIERS
    rtn = InitializeAudioVerifications();
    if ( !rtn ) return FALSE;
    rtn = PopulateMap( GetAudioVerifications() );
    if ( !rtn ) return FALSE;
#endif

#ifdef BASIC_VERIFIERS
    rtn = InitializeBasicVerifications();
    if ( !rtn ) return FALSE;
    rtn = PopulateMap( GetBasicVerifications() );
    if ( !rtn ) return FALSE;
#endif

#ifdef LATENCY_VERIFIERS
    rtn = InitializeLatencyVerifications();
    if ( !rtn ) return FALSE;
    rtn = PopulateMap( GetLatencyVerifications() );
    if ( !rtn ) return FALSE;
#endif

#ifdef MISC_VERIFIERS
    rtn = InitializeMiscVerifications();
    if ( !rtn ) return FALSE;
    rtn = PopulateMap( GetMiscVerifications() );
    if ( !rtn ) return FALSE;
#endif

#ifdef SEEK_VERIFIERS
    rtn = InitializeSeekVerifications();
    if ( !rtn ) return FALSE;
    rtn = PopulateMap( GetSeekVerifications() );
    if ( !rtn ) return FALSE;
#endif

#ifdef VIDEO_VERIFIERS
    rtn = InitializeVideoVerifications();
    if ( !rtn ) return FALSE;
    rtn = PopulateMap( GetVideoVerifications() );
    if ( !rtn ) return FALSE;
#endif

#ifdef RS_VERIFIERS
    rtn = InitializeRSVerifications();
    if ( !rtn ) return FALSE;
    rtn = PopulateMap( GetRSVerifications() );
    if ( !rtn ) return FALSE;
#endif

    if ( m_TestVerifications.size() > 0 )
        m_bInitialized = TRUE;

    return rtn;
}
