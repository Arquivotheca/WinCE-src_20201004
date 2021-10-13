//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#ifndef INCLUDED_TIMER
#define INCLUDED_TIMER 1


#ifdef QA_API
#undef QA_API
#endif

#ifdef QA_EXPORTS
#define QA_API __declspec(dllexport)
#elif   QA_IMPORTS
#define QA_API __declspec(dllimport)
#else
#define QA_API 
#endif


/*******************************************************************************
        ``For time is the longest distance between two places.''
                    Tennessee Williams, The Glass Menagerie

DESCRIPTION

    The timer class models a stopwatch using the high-performance counter
    available on most machines.  Timer units are double-valued seconds.

    Note that if the machine does not support the high-performance query
    counter, then it will use the lower-resolution system counter (which has
    a resolution of 10-55ms).

    The following operations are supported:


FUNCTIONS

    reset
        Stops the timer and resets the cumulative time to zero.
        This is the initial state of the timer upon contruction.

    start
        Starts (or restarts) the timer.  If the timer is already running,
        it has no effect.

    stop
        Stops the timer.  The timer may be started again to contine timer
        accumulation.

    read
        Reads the current cumulative time.  This does not affect the run-state
        of the timer.


EXAMPLES

    To find the amount of time spent in a block:

        {
            Timer timer;
            timer.start();
            ...
            printf ("Time in block: %lf seconds.\n", timer.read());
        }

    To find time across a function call inside a loop, with multiple start-stop
    cycles:

        Timer timer;

        for (...)
        {
            ...
            timer.start();
            function ();
            timer.stop();
        }

        printf ("Total time in function: %lf seconds.\n", timer.read();

*******************************************************************************/

/*****************************************************************************
The system clock object manages the differences between the high-performance
counter (which may not be supported on some systems) and the lower resolution
GetTickCount (which is used as a fallback).
*****************************************************************************/

class QA_API SysClock
{
  public:
    SysClock (void);
    __int64 GetTicks (void) const;
    double  ElapsedSeconds (__int64 start) const;

  private:
    bool   hpc_supported;
    double frequency;
};

class QA_API Timer
{
  public:

    Timer (void);    // Timer Constructor

    void   reset (void);         // Resets timer to not-running, Zero.
    void   start (void);         // Starts the timer running again.
    void   stop  (void);         // Stops the timer.
    double read  (void) const;   // Current Total Seconds Elapsed

  private:

    double  ttotal;       // Elapsed Time of Prior Laps
    __int64 tstart;       // Current Start Time

    SysClock clock;
};




#endif
