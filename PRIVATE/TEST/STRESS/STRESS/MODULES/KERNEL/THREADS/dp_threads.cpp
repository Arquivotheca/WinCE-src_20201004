//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*******************************************************************************
DP_THREADS.DLL - stress module for thread creation

This module implements the classic Dining Philosophers problem, using
threads as 'diners' and mutexes as the shared resources (the 'chopsticks').  
The goal is to create a large number of threads, do some nontrivial things
requiring interthread synchronization, then terminate the threads gracefully.

There are two variants supported in this module: 

  create n threads and run; 
  create as many threads as can be spawned in available VM, (optionally) 
  reserving n kB.  

*******************************************************************************/

#include <windows.h>
#include <tchar.h>
#include <kfuncs.h>
#include <stressutils.h>
#include "dp_threads.h"

/*******************************************************************************
DllMain - standard DLL entry point code
*******************************************************************************/

HANDLE			hThisInstance = NULL;

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    hThisInstance = hModule;
	return TRUE;
}

/*******************************************************************************
DP_THREADS GLOBALS

This list will be "circular", as the last diner's "right" pointer will point to
the first diner; this maintains the metaphor of the round dinner table.  
*******************************************************************************/
struct Diner
{
	int DinerIndex;				// arbitrary index of position in circular list
	short nMyPriority;			// priority for this diner thread
	unsigned int nTriedRight;	// how many times the diner tried to pick up the right chopstick
	unsigned int nTriedLeft;	// how many times the diner tried to pick up the left chopstick
	unsigned int nBites;		// how many times the diner owned both chopsticks, ate
	HANDLE DinerThread; 
	DWORD DinerThreadID; 
	HANDLE Chopstick;			// this is the one that this diner "owns"
	struct Diner *right; 
};

struct Diner	*FirstDiner;		// The "head" of the circular list
unsigned int	nNumberOfDiners;	// global to make traversing the list easier
BOOL			bRun;				// lets threads shut down gracefully
DWORD			dwRunTime;			// Test run time in msec

/*******************************************************************************
WalkToNode returns a pointer to a particular node, given its (internal) index
number
*******************************************************************************/

struct Diner *WalkToNode(int nNodeIndex)
{
	struct Diner *CurrentDiner;

	CurrentDiner = FirstDiner;
	do
	{
		if (CurrentDiner->DinerIndex == nNodeIndex)
		{
			return CurrentDiner;
		}
		else
		{
			CurrentDiner = CurrentDiner->right;
		}
	}	while (CurrentDiner != FirstDiner);
	// if we get this far, the index was not found
	return NULL;
}

/*******************************************************************************
DefaultDining is the built-in thread proc;  TODO: allow an external threadproc, 
so that this can be generalized and applied to e.g. other sync objects
*******************************************************************************/
DWORD DefaultDining(LPVOID lpSeatNumber)
{
	HANDLE LeftChopstick, RightChopstick;
	int nMyIndex;
	struct Diner *MySeat, *CurrentDiner;
	
	// Initialization - get your chopsticks
	nMyIndex = (int) lpSeatNumber;
	// this is the node to the left of the current thread's node
	CurrentDiner = WalkToNode((nNumberOfDiners + nMyIndex - 1) % nNumberOfDiners);
	if (CurrentDiner == NULL)
	{
		LogFail(_T("Error in initialization, cannot find node %d"), 
			(nNumberOfDiners + nMyIndex - 1) % nNumberOfDiners);
		return -1L;
	}
	LeftChopstick = CurrentDiner->Chopstick;
	RightChopstick = CurrentDiner->right->Chopstick; 
	MySeat = CurrentDiner->right; 

	// Here is the core functionality of this threadproc
	while(bRun)
	{
		MySeat->nTriedRight++;
		if(WaitForSingleObject(RightChopstick, RANDOM_SECONDS) == WAIT_TIMEOUT)
		{
			Sleep(RANDOM_SECONDS);
			continue;
		}
		MySeat->nTriedLeft++;
		if(WaitForSingleObject(LeftChopstick, RANDOM_SECONDS) == WAIT_TIMEOUT)
		{
			ReleaseMutex(RightChopstick);
			continue;
		}

		LogVerbose(_T("Diner %d is eating..."), nMyIndex); 
		MySeat->nBites++;
		Sleep(RANDOM_SECONDS); 
		LogVerbose(_T("Diner %d is finished eating."), nMyIndex); 
		ReleaseMutex(LeftChopstick);
		ReleaseMutex(RightChopstick);
	}
	return 0;
}

/*******************************************************************************
SetTable creates the threads and mutexes, and starts off with all threads suspended; 
depending on the DinerStartType, it then starts the diners together or at random
staggered intervals.  DinerDineType selects either a random or fixed time for each
diner; if DINE_RANDOM is chosen, nDineTime is ignored

The assumption is that each diner "owns" the chopstick to his left (the mutex handle
within the Diner struct).  A diner attempts to pick up that chopstick first, then the 
chopstick to his "right" (the next diner in the circular linked list).  

*******************************************************************************/

BOOL SetTable(unsigned int nDinerCount, DWORD DiningFunction, 
							unsigned int nReservedKbMemory,
                            unsigned int nPriority)
{
	unsigned int nContinue;
	char buffer[20];
	struct Diner *PreviousDiner, *CurrentDiner;
	MEMORYSTATUS lpBuffer; 

	if (nDinerCount == 1)	// must have a minimum of two diners
	{
		LogFail(_T("PARAMETER: bad diner count"));
		return false;
	}
	nNumberOfDiners = 0;
	if (nDinerCount == 0)
	{
		// handle limited-by-memory case
		nContinue = 1;
	}
	else
	{
		// handle explicit index case
		nContinue = nDinerCount;
	}

	// for (i = 0; i < nNumberOfDiners; i++)
	while (nContinue)
	{
		if ((CurrentDiner = (struct Diner *) malloc(sizeof Diner)) == NULL)
		{
			if(nDinerCount > 0)
			{
				LogFail(_T("Fail to alloc Diner object in iteration %u"), nNumberOfDiners); 
				return false;
			}
			else break;
		}
		CurrentDiner->DinerThread = CreateThread(NULL, 0L, (LPTHREAD_START_ROUTINE) 
			DiningFunction, (LPVOID) nNumberOfDiners, CREATE_SUSPENDED, 
			(LPDWORD) (&CurrentDiner->DinerThreadID)); 
		if((CurrentDiner->DinerThread) == NULL)
		{
			if(nDinerCount > 0)
			{
				LogFail(_T("Fail on thread create in iteration %u"), nNumberOfDiners);
				free(CurrentDiner);
				return false;
			}
			else break;
		}
        CeSetThreadPriority(CurrentDiner->DinerThread, nPriority);
		CurrentDiner->nMyPriority = CeGetThreadPriority(CurrentDiner->DinerThread);
		sprintf(buffer, "Chopstick%d", nNumberOfDiners);
		CurrentDiner->Chopstick = CreateMutex(NULL, FALSE, (LPCTSTR) buffer);
		if (CurrentDiner->Chopstick == NULL)
		{
			if(nDinerCount > 0)
			{
				LogFail(_T("Fail to create mutex in iteration %u"), nNumberOfDiners);
				CloseHandle(CurrentDiner->DinerThread);
				free(CurrentDiner);
				return false;
			}
			else break;
		}
		CurrentDiner->DinerIndex = nNumberOfDiners; 
		// link into the list
		if (nNumberOfDiners == 0)
		{
			FirstDiner = CurrentDiner;
		}
		else
		{
			PreviousDiner->right = CurrentDiner;
		}
		PreviousDiner = CurrentDiner;
		nNumberOfDiners++;
		if(nDinerCount == 0)
		{
			// evaluate reserved memory limit condition
			lpBuffer.dwLength = sizeof(MEMORYSTATUS);
			GlobalMemoryStatus(&lpBuffer);
			if ( (lpBuffer.dwTotalVirtual - lpBuffer.dwAvailVirtual)/1024 < nReservedKbMemory)
			{
				nContinue = 0; 
			}

		}
		else
		{
			// explicit iteration
			nContinue--;
		}
	}
	// Wrap around to create circular list - CurrentDiner still points to last element
	CurrentDiner->right = FirstDiner; 
	LogComment(_T("%u objects successfully created"), nDinerCount);
	return true;
}

/*******************************************************************************
ResumeDinner starts the threads that were created in SetTable (suspended); it can
either start them as fast as a loop can spin, or staggered with random delay 
between them.  
*******************************************************************************/
BOOL ResumeDinner(enum DinerStartType dstStartType)
{

	struct Diner *CurrentDiner;

	bRun = true;
	CurrentDiner = FirstDiner; 
	do {
		switch(dstStartType)
		{
		case START_TOGETHER:
			{
				break;
			}
		case START_RANDOM:
			{
				Sleep(RANDOM_SECONDS);
				break;
			}
		}
		if (ResumeThread(CurrentDiner->DinerThread) == FAILURE)
		{
			LogFail(_T("Thread could not be started, error: %X"), GetLastError());
			return false; 
		}
		CurrentDiner = CurrentDiner->right;
	} while(CurrentDiner != FirstDiner);
	return true;
}

/*******************************************************************************
EndDinner resets the boolean flag in the threadproc to allow each thread to 
naturally complete and terminate.  It also iterates through the threads, to be
sure they do terminate; an arbitrary figure is used as a timeout, and if the
thread does not terminate in that time, a warning is logged.  
*******************************************************************************/
BOOL EndDinner()
{
	struct Diner *CurrentDiner;
	
	bRun = false;	// should cause all threads to terminate naturally
	LogComment(_T("Threads instructed to terminate"));

	// if it doesn't die naturally, kill it
	CurrentDiner = FirstDiner;
	do
	{
		LogVerbose(_T("Waiting for diner #%d to finish..."), CurrentDiner->DinerIndex);
		if(WaitForSingleObject(CurrentDiner->DinerThread, THREAD_DEATH) == WAIT_TIMEOUT)
		{
			// Thread is (by definition) hung
			LogWarn1(_T("Thread #%d did not terminate in %u seconds"), 
				CurrentDiner->DinerIndex, THREAD_DEATH);
		}

		CurrentDiner = CurrentDiner->right;
	
	} while (CurrentDiner != FirstDiner);
	LogComment(_T("All threads should now be finished"));
	return true;
}

/*******************************************************************************
ClearTable walks the linked list, freeing objects and node memory
*******************************************************************************/
void ClearTable()
{
	struct Diner *CurrentDiner, *NextDiner;
	
	CurrentDiner = FirstDiner; 
	do {
		NextDiner = CurrentDiner->right; 
		// if the run is terminated by OOM in SetTable, the pointer to the neighbor
		// will be null - this IF handles that case and avoids a null pointer deref
		if (NextDiner == NULL)
		{
			NextDiner = FirstDiner; 
		}
		CloseHandle(CurrentDiner->Chopstick);
		free(CurrentDiner);
		CurrentDiner = NextDiner;
	} while (CurrentDiner != FirstDiner); 
}

/*******************************************************************************
PrintStats optionally prints statistics on each diner's performance, including:
- how many times a node tried to acquire the 'right' mutex
- how many times a node tried to acquire the 'left' mutex
- how many times a node acquired both and was able to 'eat'
Priority is reported for scenarios where thread priorities have been changed
*******************************************************************************/
void PrintStats()
{
	struct Diner *CurrentDiner;
	
	CurrentDiner = FirstDiner; 
	do {
		LogComment(_T("Diner %d with priority %d: Tried right %d, Tried left %d, Ate %d times"), 
			CurrentDiner->DinerIndex, 
			CurrentDiner->nMyPriority,
			CurrentDiner->nTriedRight, 
			CurrentDiner->nTriedLeft, 
			CurrentDiner->nBites ); 
		CurrentDiner = CurrentDiner->right;
	} while (CurrentDiner != FirstDiner);
}

/*******************************************************************************
MakeCSVFile walks the tree, finds all the perf data collected, and puts it in a CSV
file that can be exported into Excel and charted.  It creates a filename unique to
the process
*******************************************************************************/
BOOL MakeCSVFile()
{
	HANDLE hCSVFile;
	struct Diner *CurrentDiner;
	char buffer[100];
	LPDWORD lpBytesWritten = 0; 
	int nBufferLen; 

	// build a file name that will be unique for the process
	sprintf(buffer, "\\Release\\Diners%x.csv", GetCurrentProcessId());

	hCSVFile = CreateFile( (LPCTSTR) buffer, GENERIC_WRITE, FILE_SHARE_READ, 
		NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hCSVFile == NULL)
	{
		return false; 
	}
	CurrentDiner = FirstDiner; 
	do {
		// columns are diner#, thread, right tries, left tries, successes, priority
		nBufferLen = sprintf(buffer, "%d, %d, %d, %d, %d\n", 
			CurrentDiner->DinerIndex, 
			CurrentDiner->nTriedRight, 
			CurrentDiner->nTriedLeft, 
			CurrentDiner->nBites,  
			CurrentDiner->nMyPriority);
		WriteFile(hCSVFile, (LPCVOID) buffer, nBufferLen, lpBytesWritten, NULL); 
		CurrentDiner = CurrentDiner->right;
	} while (CurrentDiner != FirstDiner);
	CloseHandle(hCSVFile); 
	return true; 
}

/*******************************************************************************
PromoteDiner sets a given diner's thread priority.  
*******************************************************************************/
BOOL PromoteDiner(int SeatNumber, int NewPriority)
{
	struct Diner *CurrentDiner;
	
	CurrentDiner = WalkToNode(SeatNumber);
	CeSetThreadPriority(CurrentDiner->DinerThread, NewPriority); 
	CurrentDiner->nMyPriority = CeGetThreadPriority(CurrentDiner->DinerThread); 
	return true;
}

/*******************************************************************************
BEGINNING OF STRESS HARNESS CODE
*******************************************************************************/

MODULE_PARAMS	mpInput; 
unsigned int    nReservedKbMemory, 
                nThreadCount, 
                nDinerPriority;

BOOL InitializeStressModule(MODULE_PARAMS &mpInput, UINT *pnThreadCount)
{
	TCHAR chCommandLine[255], *pchCurrentToken;
	TCHAR chDelims[] = {_T(" \t")};
	
	InitializeStressUtils(_T("DP_threads"), LOGZONE(SLOG_SPACE_KERNEL, SLOG_DEFAULT), &mpInput);
	dwRunTime = mpInput.dwDuration; // input is in msec
	// Parse the input parameters
/*******************************************************************************
COMMAND LINE PARAMETERS
-n:	number of threads; if set to zero, spins up as many threads as VM will support
	(see -r parameter). CANNOT be 1.  REQUIRED parameter.  
-p: priority to set threads.  If omitted, threads are set at default priority.  
-r:	reserve this many kb of memory when implementing n=0.  In other words, this
	option tells the SetTable function to stop creating new 'diners' when n kb of
	VM remains.  
*******************************************************************************/
	_tcscpy(chCommandLine, mpInput.tszUser); 
	pchCurrentToken = _tcstok(chCommandLine, chDelims);
    nThreadCount = 5;   // a reasonable default, if no value provided
    nDinerPriority = CeGetThreadPriority(GetCurrentThread());    // default priority
	do
	{
		if((*pchCurrentToken == '-') | (*pchCurrentToken == '/'))
		{
			switch(*(pchCurrentToken + 1))
			{
			case 'n':
			case 'N':	nThreadCount = _ttoi(_tcstok(NULL, chDelims));
                        LogComment(_T("Set thread count to %u"), nThreadCount);
						break;
            case 'p':
            case 'P':   nDinerPriority = _ttoi(_tcstok(NULL, chDelims));
                        LogComment(_T("Set thread priority to %u"), nDinerPriority);
                        break;
			case 'r':
			case 'R':	nReservedKbMemory = _ttoi(_tcstok(NULL, chDelims));
                        LogComment(_T("Reserve %u kb memory"), nDinerPriority);
						break;
			default:	LogFail(_T("Error in command line"));
						return false;
			}
		}
	} while((pchCurrentToken = _tcstok(NULL, chDelims)) != NULL);
	return true;
}

UINT DoStressIteration(HANDLE hMyThread, DWORD dwMyThreadID, LPVOID pvUnused)
{
	
	// Set the table
    // DebugBreak();
	if(! SetTable(nThreadCount, (DWORD) DefaultDining, nReservedKbMemory, nDinerPriority))
    {
        return CESTRESS_FAIL;
    } 
    // if successful, be sure main thread's priority == diner threads' priority, or
    // the main thread will not be able to shut down the diners
    CeSetThreadPriority(GetCurrentThread(), nDinerPriority); 
	// Start the dinner
	if(! ResumeDinner(START_RANDOM))
	{
		return CESTRESS_FAIL;
	}
	// After prescribed time, end dinner
	LogComment(_T("Main thread sleeping for %u msec"), dwRunTime);
	Sleep(dwRunTime);
	if(! EndDinner())
	{
    	LogFail(_T("Failure in EndDinner"));
		return CESTRESS_FAIL;
	}
	LogComment(_T("Returning from DoStressIteration"));
	return CESTRESS_PASS; 
}

DWORD TerminateStressModule()
{
	// This is effectively a 'no op' unless SLOG_COMMENT is set
	PrintStats();
	// Clear the table, i.e. release all resources
	ClearTable();
    return 0;
}
