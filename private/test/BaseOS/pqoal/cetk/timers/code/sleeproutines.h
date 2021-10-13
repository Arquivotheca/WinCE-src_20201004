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

#ifndef __SLEEP_ROUTINES_H__
#define __SLEEP_ROUTINES_H__

#include <windows.h>


/* Overview
 * 
 * The tests needs to sleep for different intervals and in different
 * ways.  These functions keep the test from having to special code
 * specific sleep symantics into the general test code.
 * 
 * From the OS perspective, there are two methods of taking up time
 * (sleeping) on the system.
 * 
 * Os sleep - Standard Sleep () call.  The processor swaps out this
 * process.  If another process is ready to run it is marked as
 * runnable.  If not the processor tells the OAL that nothing needs to
 * be done.  The OAL can then, if it wants to, start shutting down
 * peripherals, etc.
 *
 * Busy sleep - while loop watching for the clock to hit a special
 * value.  This is a busy sleep symantic.  The test code tells the
 * proc to spin until a given time value has passed.
 *
 * These two methods of waiting are combined to test different
 * features of the OAL.
 *
 * The sleep routine, as defined by the OS, will never return before a
 * given time.  It can return after the specified, and in some cases
 * along time after a given time.  Our sleep functions need to
 * preserve these symantics.
 *
 * Below are four functions that define how the tests will sleep.
 * 
 * OsSleepFunction - the standard Sleep () call.  We special case this
 * to keep all of the parameters the same.
 *
 * BusyOsSleepFunction - The busy version of sleep.  This is a while
 * loop that waits for the given clock to pass a specified value.
 *
 * littleOsSleepFunction - This function sleeps for small intervals.
 * It wakes the processor every gdwLittleSleepTimeMs interval.  It
 * returns when dwMs has passed.
 *
 * RandomOsSleepFunction - This funciton sleep for small intervals.
 * It wakes on random intervals, with the average of these intervals
 * specified by gdwLittleSleepTimeMs.  It returns when dwMs has
 * passed.
 *
 * Finally, the backwards tests require a method of returning after a
 * random sleep interval.  WaitRandomTime provides this functionality.
 * It is called "wait" because it violates the sleep symantics list
 * above.
 */

/* pass through to sleep */
BOOL OsSleepFunction (DWORD dwMs);

/* tight while loop that returns when gtc has ticked for dwMs */
BOOL BusyOsSleepFunction (DWORD dwMs);

/*
 * setInnerSleepIntervalMs is used to set the time interval for
 * LittleOsSleepFunction and RandomOsSleepFunction.
 */
void setInnerSleepIntervalsMS (DWORD *dwTimeList, DWORD dwNumTimes);

/*
 * This function returns after the given interval has passed.
 * Sleeping is accomplished in steps of size dwMs.  The os function
 * Sleep is used for these intervals.
 *
 * If the sleep intervals set by setInnerSleepIntervalMS doesn't
 * evenly divide the sleep time then the sleep time will return close
 * to the next multiple of this value.
 */
BOOL LittleOsSleepFunction (DWORD dwMs);

/* 
 * Sleep for >= dwMs.  The sleep intervals are random, with the
 * average os sleep time as gdwInnerSleepIntervalMs.
 */
BOOL RandomOsSleepFunction (DWORD dwMs);

/*
 * wait for a random time between 0 ms and dwMaxWait MS.  Note that
 * this violates the sleep semantics.
 */
BOOL
WaitRandomTime (DWORD dwMaxWaitMs);

#endif
