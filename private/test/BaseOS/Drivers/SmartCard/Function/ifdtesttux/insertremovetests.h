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
//***************************************************************************
// Purpose:  Performs card reader insertion/removal tests.
//           A similar but more difficult test is the InterruptReaderTest. 
//
// Arguments:
//   DWORD in_dwFullCycles       - How many cycles of the test to perform.
//                               - Specifying 0 implies infinite looping.
//   DWORD in_dwInsRemBeforeTest - How many insertion/removal cycles before
//                                 sanity checking reader with AutoIFD.
//   BOOL in_bAlternateCardInserted - If TRUE, alternate whether the
//                                    CardProviderTest will require removal of
//                                    the test card after each full cycle.
//                               - If FALSE, the test card will always be
//                                 left in the card reader during the test.
//                                 This is useful for unattended stress tests.
//   BOOL in_bUseReaderSwitchBox - TRUE if a USB Reader Switch Box is being 
//                                 used.  Otherwise, this test is manual.
//   DWORD in_dwMillisBeforeReaderAction - Milliseconds to sleep before thread
//                                         inserts/removes reader. (Only valid
//                                         when in_bUseReaderSwitchBox==TRUE).
// 
// Returns:
//   void - actual test result is obtained via ReaderFailed().
//***************************************************************************
void ReaderInsertionRemovalTest(const DWORD in_dwFullCycles,
                                const DWORD in_dwInsRemBeforeTest,
                                const BOOL  in_bAlternateCardInserted,
                                const BOOL  in_bUseReaderSwitchBox,
                                const DWORD in_dwMillisBeforeReaderAction);


//***************************************************************************
// Purpose:  Performs card reader insertion/removal tests, with removal
//           sometimes occuring when the card reader is in use.
//
// Arguments:
//   DWORD in_dwFullCycles       - How many cycles of the test to perform.
//                               - Specifying 0 implies infinite looping.
//   DWORD in_dwInsRemBeforeTest - How many insertion/removal cycles before
//                                 moving on to AutoIFD.
//   BOOL  in_bUseReaderSwitchBox - TRUE if a USB Reader Switch Box is being 
//                                  used.  Otherwise, this test is manual.
//   DWORD in_dwMillisBeforeReaderAction - Milliseconds to sleep before thread
//                                         inserts/removes reader.  (Only valid
//                                         when in_bUseReaderSwitchBox==TRUE)
//   DWORD in_dwMaxMillisSurpriseRemoval - Max milliseconds to sleep before 
//                                         surprise removal of reader while
//                                         AutoIFD is running.  (Only valid
//                                         when in_bUseReaderSwitchBox==TRUE)
//   BOOL  in_bRandomSurpriseRemovalTime - Whether to use a random time between
//                                         [0,in_dwMaxMillisSurpriseRemoval] as
//                                         the time for the random removal.
//                                         (Only valid when
//                                         in_bUseReaderSwitchBox==TRUE)
// Returns:
//   void - actual test result is obtained via ReaderFailed().
//***************************************************************************
void ReaderSurpriseRemovalTest(const DWORD in_dwFullCycles,
                               const DWORD in_dwInsRemBeforeTest,
                               const BOOL  in_bUseReaderSwitchBox,
                               const DWORD in_dwMillisBeforeReaderAction,
                               const DWORD in_dwMaxMillisSurpriseRemoval,
                               const BOOL  in_bRandomSurpriseRemovalTime);


//***************************************************************************
// Purpose:  Performs card reader removal tests when reader is blocked
//           waiting for card insertion/removal.
//
// Arguments:
//   DWORD in_dwFullCycles        - How many cycles of the test to perform.
//                                - Specifying 0 implies infinite looping.
//   BOOL  in_bUseReaderSwitchBox - TRUE if a USB Reader Switch Box is being 
//                                  used. Otherwise, this test is fully manual.
//   DWORD in_dwMillisBeforeReaderAction - Milliseconds to sleep before thread
//                                         inserts/removes reader.  (Only valid
//                                         when in_bUseReaderSwitchBox==TRUE)
//
// Returns:
//   void - actual test result is obtained via ReaderFailed().
//***************************************************************************
void RemoveReaderWhenBlocked(const DWORD in_dwFullCycles, 
                             const BOOL  in_bUseReaderSwitchBox,
                             const DWORD in_dwMillisBeforeReaderAction);


//***************************************************************************
// Purpose:  Card reader insertion/removal tests when device is suspended.
//           In order for this test to work properly, the device should
//           not be configured to wake from suspend on insertion/removal of
//           a card reader.
//
// Arguments:
//   ULONG in_uWaitTime - Number of seconds before device suspends.
//
// Returns:
//   void - actual test result is obtained via ReaderFailed().
//***************************************************************************
void SuspendResumeInsertRemoveTest(const ULONG in_uWaitTime);