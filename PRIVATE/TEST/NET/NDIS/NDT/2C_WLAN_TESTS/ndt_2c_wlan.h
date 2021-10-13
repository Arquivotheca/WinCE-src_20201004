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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#ifndef __TEST_1C_H
#define __TEST_1C_H


//----------------------------------------------------------------------------------------------------------------------

BOOL PrepareToRun();
BOOL FinishRun();

//----------------------------------------------------------------------------------------------------------------------

TEST_FUNCTION(TestWork);

TEST_FUNCTION(TestWlanAdhoc);
TEST_FUNCTION(TestWlanMediastream);
TEST_FUNCTION(TestWlanReloaddefaults);
TEST_FUNCTION(TestWlanStatistics);
TEST_FUNCTION(TestWlanWep);
TEST_FUNCTION(TestWpaAddkey);
TEST_FUNCTION(TestWpaAdhoc);
TEST_FUNCTION(TestWpaAuthentication);
TEST_FUNCTION(TestWpaBssidlist);
TEST_FUNCTION(TestWpaInfrastructure);
TEST_FUNCTION(TestWpaRemovekey);
TEST_FUNCTION(TestWpaSendCheck);
TEST_FUNCTION(TestWpaSendrecvAes);
TEST_FUNCTION(TestWpa2Misc);
TEST_FUNCTION(TestWpa2Radiostate);
TEST_FUNCTION(TestWpa2Sendcheck);
TEST_FUNCTION(TestWpa2Sendrecv);

TEST_FUNCTION(SimpleWEPXfer);
TEST_FUNCTION(SimpleWPAXfer);
TEST_FUNCTION(SimpleWPAWEPXfer);
TEST_FUNCTION(SimpleWPA2Xfer);
TEST_FUNCTION(SimpleWPA2WEPXfer);
//----------------------------------------------------------------------------------------------------------------------

extern TCHAR g_szTestAdapter[256];
extern TCHAR g_szHelpAdapter[256];
extern NDIS_MEDIUM g_ndisMedium;
extern BOOL g_bNoUnbind;
extern DWORD g_dwBeatDelay;
extern DWORD g_dwStrictness;
extern DWORD g_dwDirectedPasspercentage;
extern DWORD g_dwBroadcastPasspercentage;

// Depending on the global strictsness g_dwStrictness
// (which is given a default value if the user does not specify an overriding value)
// individual test failures are masked out as a pass result or flagged as fail
// Default strictness is SevereStrict which the user can change using the 
// strictness command-line option
// All test failures are in log as ERROR, though overall result from tux will be pass or fail
// depending on the global strictness
typedef enum
{
   // Minimal checking for enduring the adapter works with zero config
   MildStrict = 1, 
   // Unused
   MediumStrict = 3,
  // Literally follow desktop 802.11 guideline doc
   SevereStrict = 5
} GLOBAL_TEST_STRICTNESS;


// These assignments are by the logic that for MildStrict, only Severe errors are flagged
// while for SevereStrict all errors are flagged as errors
typedef enum
{
    // Severe errors that need to be fixed to make the driver work with existing systems
    ErrorSevere = MildStrict,  
    // unused
    ErrorMedium = MediumStrict,
    // Errors that if fixed will help for future compliance, in case Windows CE becomes stricter 
    // in following the desktop guidelines
    ErrorMild= SevereStrict
} FAILURE_SEVERITY;


// Depending on the failed test's severity, the overall result may be flagged as failed
inline void FlagError(DWORD TestFailureSeverity, int* pGlobalResult )
{
    // Thus for severity SevereStrict(default) all errors are flagged as errors
    if (TestFailureSeverity <= g_dwStrictness)
        *pGlobalResult = TPR_FAIL;
    // else no change to rc, ie. this failure is being masked out
}

//----------------------------------------------------------------------------------------------------------------------

#endif
