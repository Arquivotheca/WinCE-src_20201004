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
////////////////////////////////////////////////////////////////////////////////
//
//  Graph Playback Test functions
//
//
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <tux.h>
#include "logging.h"
#include "TuxGraphTest.h"
#include "TestDesc.h"
#include "TestGraph.h"
#include "GraphTest.h"
#include "utility.h"

#define EC_COMPLETE_TOLERANCE 2000

TESTPROCAPI ManualPlaybackTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	DWORD retval = TPR_PASS;
	HRESULT hr = S_OK;

	if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
		return SPR_HANDLED;
	else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

	// Get the test config object from the test parameter
	PlaybackTestDesc *pTestDesc = (PlaybackTestDesc*)lpFTE->dwUserData;

	// From the test desc, determine the media file to be used
	Media* pMedia = pTestDesc->GetMedia(0);
	if (!pMedia)
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to retrieve media object"), __FUNCTION__, __LINE__, __FILE__);
		return TPR_FAIL;
	}

	// Instantiate the test graph
	TestGraph testGraph;
	
	// Build the graph
	hr = testGraph.BuildGraph(pMedia);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to build graph for media %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
		retval = TPR_FAIL;
	}
	else {
		LOG(TEXT("%S: successfully built graph for %s"), __FUNCTION__, pMedia->GetUrl());
	}

	// Get duration and set the playback start and end positions.
	LONGLONG start = pTestDesc->start;
	LONGLONG stop = pTestDesc->stop;
	LONGLONG duration = 0;
	if (retval == TPR_PASS)
	{
		hr = testGraph.GetDuration(&duration);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to get the duration (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
			retval = TPR_FAIL;
		}
	}

	if (retval == TPR_PASS)
	{
		//start and stop were in %, convert to actual values
		start = (pTestDesc->start*duration)/100;
		stop = (pTestDesc->stop*duration)/100;
		if (stop == 0)
			 stop = duration;

		hr = testGraph.SetAbsolutePositions(start, stop);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to set positions to %I64x and %I64x (0x%x)"), __FUNCTION__, __LINE__, __FILE__, start, stop, hr);
			retval = TPR_FAIL;
		}
	}
	
	// Calculate expected duration of playback
	duration = (stop - start);

	// Run the graph
	if (retval == TPR_PASS)
	{
		// Change the state to running - the call doesn't return until the state change completes
		hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to change state to Running for media %s (0x%x)"), 
						__FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
			retval = TPR_FAIL;
		}
	}
	
	// Wait for completion of playback with a duration of twice the expected time
	if (retval == TPR_PASS)
	{
		LOG(TEXT("%S: changed state to Running."), __FUNCTION__);		
		DWORD timeout = 2*TICKS_TO_MSEC(duration);
		LOG(TEXT("%S: waiting for completion with a timeout of %d msec."), __FUNCTION__, timeout);
		hr = testGraph.WaitForCompletion(timeout);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - WaitForCompletion failed (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
			retval = TPR_FAIL;
		}
	}

	if (retval == TPR_PASS)
	{
		// Ask the tester if playback was ok
		int query = MessageBox(NULL, TEXT("Did the playback complete succesfully with no or few glitches?"), TEXT("Manual Playback Test"), MB_YESNO);
		if (query == IDYES)
			LOG(TEXT("%S: playback completed successfully."), __FUNCTION__);
		else if (query == IDNO)
		{
			LOG(TEXT("%S: playback was not ok."), __FUNCTION__);
			retval = TPR_FAIL;
		}
		else {
			LOG(TEXT("%S: unknown response or MessageBox error."), __FUNCTION__);
			retval = TPR_ABORT;
		}
	}

	if (retval == TPR_PASS)
	{
		LOG(TEXT("%S: successfully verified playback."), __FUNCTION__);
	}
	else {
		LOG(TEXT("%S: ERROR %d@%S - failed the playback test."), __FUNCTION__, __LINE__, __FILE__);
		retval = TPR_FAIL;
	}

    // Reset the testGraph
	testGraph.Reset();

	return retval;
}

TESTPROCAPI PlaybackTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	DWORD retval = TPR_PASS;
	HRESULT hr = S_OK;

	if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
		return SPR_HANDLED;
	else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

	// Get the test config object from the test parameter
	PlaybackTestDesc *pTestDesc = (PlaybackTestDesc*)lpFTE->dwUserData;

	// From the test desc, determine the media file to be used
	Media* pMedia = pTestDesc->GetMedia(0);
	if (!pMedia)
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to retrieve media object"), __FUNCTION__, __LINE__, __FILE__);
		return TPR_FAIL;
	}

	// Instantiate the test graph
	TestGraph testGraph;
	
	// Build the graph
	hr = testGraph.BuildGraph(pMedia);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to build graph for media %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
		retval = TPR_FAIL;
	}
	else {
		LOG(TEXT("%S: successfully built graph for %s"), __FUNCTION__, pMedia->GetUrl());
	}

	// Check the verification required
	bool bGraphVerification = false;
	PlaybackDurationTolerance* pDurationTolerance = NULL;
	VerificationList verificationList = pTestDesc->GetVerificationList();
	VerificationList::iterator iterator = verificationList.begin();
	while (iterator != verificationList.end())
	{
		VerificationType type = (VerificationType)iterator->first;
		if (type == VerifyPlaybackDuration) {
			pDurationTolerance = (PlaybackDurationTolerance*)iterator->second;
			// We need the startup latency to measure playback duration accurately - so enable measurement
			// Keep the tolerance as INFINITE since we just want to measure it
			LONGLONG startupLatencyTolerance = INFINITE_TIME;
			hr = testGraph.EnableVerification(StartupLatency, (void*)&startupLatencyTolerance);
			if (FAILED(hr))
			{
				LOG(TEXT("%S: ERROR %d@%S - failed to enable startup latency measurement)"), __FUNCTION__, __LINE__, __FILE__);
				retval = TPR_FAIL;
				break;
			}
		}
		else if (testGraph.IsCapable(type)) {
			hr = testGraph.EnableVerification(type, iterator->second);
			if (FAILED_F(hr))
			{
				LOG(TEXT("%S: ERROR %d@%S - failed to enable verification %S)"), __FUNCTION__, __LINE__, __FILE__, GetVerificationString(type));
				break;
			}
			bGraphVerification = true;
		}
		else {
			LOG(TEXT("%S: WARNING %d@%S - unrecognized verification requested %S)"), __FUNCTION__, __LINE__, __FILE__, GetVerificationString(type));
		}
		iterator++;
	}

	// Set the playback start and end positions.
	LONGLONG start = pTestDesc->start;
	LONGLONG stop = pTestDesc->stop;
	LONGLONG duration = 0;
	LONGLONG actualDuration = 0;

	if (retval == TPR_PASS)
	{		
		hr = testGraph.GetDuration(&duration);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to get the duration (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
			retval = TPR_FAIL;
		}
	}
	
	if (retval == TPR_PASS)
	{		
		//start and stop were in %, convert to actual values
		start = (pTestDesc->start*duration)/100;
		stop = (pTestDesc->stop*duration)/100;
		if (stop == 0)
			 stop = duration;

		hr = testGraph.SetAbsolutePositions(start, stop);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to set positions to %I64x and %I64x (0x%x)"), __FUNCTION__, __LINE__, __FILE__, start, stop, hr);
			retval = TPR_FAIL;
		}

		// Calculate expected duration of playback
		duration = (stop - start);
	}

	// Run the graph and wait for completion
	if (retval == TPR_PASS)
	{
		// Change the state to running - the call doesn't return until the state change completes
		LOG(TEXT("%S: changing state to Running and waiting for EC_COMPLETE."), __FUNCTION__);	

		// Instantiate timer for measurement
		Timer timer;

		DWORD timeout = 2*TICKS_TO_MSEC(duration);

		// Start the playback timer
		timer.Start();

		// Signal the start of verification
		hr = testGraph.StartVerification();
		if (FAILED_F(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to start the verification (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
			retval = TPR_FAIL;
		}

		// Change state
		if (retval == TPR_PASS)
		{
			hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS);
			if (FAILED(hr))
			{
				LOG(TEXT("%S: ERROR %d@%S - failed to change state to Running (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
				retval = TPR_FAIL;
			}
		}
		
		// Wait for the playback to complete
		if (retval == TPR_PASS)
		{
			// Wait for completion of playback with a timeout of twice the expected time
			hr = testGraph.WaitForCompletion(timeout);
			
			// Measure the actual duration
			actualDuration = timer.Stop();

			// Did the wait succeed?
			if (FAILED(hr))
			{
				// If the duration is very short, the WaitForCompletion will return E_ABORT since it is 
				// possible that before it gets called the playback already completed.
				// BUGBUG: from the test, it seems 86.375ms made it through. Hard coded it to 100 ms
				if ( E_ABORT == hr && duration < DURATION_THRESHOLD_IN_MS * 10000 ) 
				{
					LOG( TEXT("%S: WARN %d@%S - WaitForCompletion aborted since the duraton is very short (%d ms)."), 
								__FUNCTION__, __LINE__, __FILE__, duration / 10000 );
				}
				else
				{
					LOG(TEXT("%S: ERROR %d@%S - WaitForCompletion failed (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
					retval = TPR_FAIL;
				}
			}
		}
	}

	// Check the results of verification if there was any specified
	if (bGraphVerification && (retval == TPR_PASS))
	{
		hr = testGraph.GetVerificationResults();
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - verification failed"), __FUNCTION__, __LINE__, __FILE__);
			retval = TPR_FAIL;
		}
		else {
			LOG(TEXT("%S: verification succeeded"), __FUNCTION__);
		}
	}

	if (retval == TPR_PASS)
	{
		LOG(TEXT("%S: playback completed successfully."), __FUNCTION__);

		// Verification of playback time
		if (pDurationTolerance)
		{
			// Measure time from RenderFile to getting EC_COMPLETE notification
			LOG(TEXT("%S: total playback time (Run to EC_COMPLETE): %d msec"), __FUNCTION__, TICKS_TO_MSEC(actualDuration));

			// Measure the startup latency 
			LONGLONG startupLatency = 0;
			hr = testGraph.GetVerificationResult(StartupLatency, &startupLatency, NULL);
			if (FAILED(hr))
			{
				LOG(TEXT("%S: ERROR %d@%S - failed to calculate startup latency"), __FUNCTION__, __LINE__, __FILE__);
			}
			else {
				LOG(TEXT("%S: startup latency %d msec"), __FUNCTION__, TICKS_TO_MSEC(startupLatency));
			}

			// Account for startup latency
			actualDuration -= startupLatency;

			// Check the percentage tolerance
			if (!ToleranceCheck(duration, actualDuration, pDurationTolerance->pctDurationTolerance) &&
				!ToleranceCheckAbs(duration, actualDuration, MSEC_TO_TICKS(pDurationTolerance->pctDurationTolerance)))
			{
				LOG(TEXT("%S: ERROR %d@%S - expected duration: %d does not match actual duration %d msec. Tolerance: %d%%,%d msec: failed"), __FUNCTION__, __LINE__, __FILE__, TICKS_TO_MSEC(duration), TICKS_TO_MSEC(actualDuration), pDurationTolerance->pctDurationTolerance, pDurationTolerance->absDurationTolerance);
				retval = TPR_FAIL;
			}
			else {
				LOG(TEXT("%S: expected duration: %d matches actual duration: %d msec. Tolerance: %d%%,%d msec: suceeded"), __FUNCTION__, TICKS_TO_MSEC(duration), TICKS_TO_MSEC(actualDuration), pDurationTolerance->pctDurationTolerance, pDurationTolerance->absDurationTolerance);
			}
		}
	}

	if (retval == TPR_PASS)
	{
		LOG(TEXT("%S: successfully verified playback."), __FUNCTION__);
	}
	else {
		LOG(TEXT("%S: ERROR %d@%S - failed the playback test."), __FUNCTION__, __LINE__, __FILE__);
		retval = TPR_FAIL;
	}

    // Reset the testGraph
	testGraph.Reset();

	return retval;
}

TESTPROCAPI PlaybackDurationTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	DWORD retval = TPR_PASS;
	HRESULT hr = S_OK;

	if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
		return SPR_HANDLED;
	else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

	// Get the test config object from the test parameter
	PlaybackTestDesc *pTestDesc = (PlaybackTestDesc*)lpFTE->dwUserData;

	// From the test desc, determine the media file to be used
	Media* pMedia = pTestDesc->GetMedia(0);
	if (!pMedia)
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to retrieve media object"), __FUNCTION__, __LINE__, __FILE__);
		return TPR_FAIL;
	}

	// Instantiate the test graph
	TestGraph testGraph;
	
	// Build the graph
	hr = testGraph.BuildGraph(pMedia);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to build graph for media %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
		retval = TPR_FAIL;
	}
	else {
		LOG(TEXT("%S: successfully built graph for %s"), __FUNCTION__, pMedia->GetUrl());
	}

	// This test doesn not handle generic verifications - so don't check for them
	// But this test needs the playback duration verifier
	PlaybackDurationTolerance* pDurationTolerance = NULL;
	VerificationList verificationList = pTestDesc->GetVerificationList();
	VerificationList::iterator iterator = verificationList.begin();
	VerificationType type = (VerificationType)iterator->first;
	if (type == VerifyPlaybackDuration)
	{
		// Get the duration tolerance
		pDurationTolerance = (PlaybackDurationTolerance*)iterator->second;
		hr = testGraph.EnableVerification(VerifyPlaybackDuration, NULL);
		if (FAILED(hr))
			retval = TPR_FAIL;
	}
	else
	{
		retval = TPR_FAIL;
	}
	
	// Set the playback start and end positions.
	LONGLONG start = pTestDesc->start;
	LONGLONG stop = pTestDesc->stop;
	// Expected duration of media
	LONGLONG duration = 0;
	// Acutal time that it took
	LONGLONG actualDuration = 0;
	// Latency of getting EC_COMPLETE
	LONGLONG ecCompletionLatency = 0;
	
	// Get the duration
	if (retval == TPR_PASS)
	{
		hr = testGraph.GetDuration(&duration);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to get duration (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
			retval = TPR_FAIL;
		}
	}

	// Set the positions requested
	if (retval == TPR_PASS)
	{
		start = pTestDesc->start*duration/100;
		stop = pTestDesc->stop*duration/100;
		if (stop == 0)
			 stop = duration;

		hr = testGraph.SetAbsolutePositions(start, stop);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to set positions to %I64x and %I64x (0x%x)"), __FUNCTION__, __LINE__, __FILE__, start, stop, hr);
			retval = TPR_FAIL;
		}

		// Calculate expected duration of playback
		duration = (stop - start);
	}

	// Instantiate timer for measurement
	Timer timer;

	// Run the graph and wait for completion
	if (retval == TPR_PASS)
	{
		// Change the state to running - the call doesn't return until the state change completes
		LOG(TEXT("%S: changing state to Running and waiting for EC_COMPLETE."), __FUNCTION__);	

		DWORD timeout = 2*TICKS_TO_MSEC(duration);

		// Signal the start of verification
		hr = testGraph.StartVerification();
		if (FAILED_F(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to start the verification (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
			retval = TPR_FAIL;
		}

		// Start the playback timer
		timer.Start();

		// Change state
		if (retval == TPR_PASS)
		{
			hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS);
			if (FAILED(hr))
			{
				LOG(TEXT("%S: ERROR %d@%S - failed to change state to Running (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
				retval = TPR_FAIL;
			}
		}
		
		// Wait for the clip to playback
		if (retval == TPR_PASS)
		{
			hr = testGraph.WaitForCompletion(timeout);
			
			// Measure the playback duration and the current time in ticks
			ecCompletionLatency = MSEC_TO_TICKS(GetTickCount());
			actualDuration = timer.Stop();

			// Did the wait succeed?
			if (FAILED(hr))
			{
				// If the duration is very short, the WaitForCompletion will return E_ABORT since it is 
				// possible that before it gets called the playback already completed.
				// BUGBUG: from the test, it seems 86.375ms made it through. Hard coded it to 100 ms
				if ( E_ABORT == hr && duration < DURATION_THRESHOLD_IN_MS * 10000 ) 
				{
					LOG( TEXT("%S: WARN %d@%S - WaitForCompletion aborted since the duraton is very short (%d ms)."), 
								__FUNCTION__, __LINE__, __FILE__, duration / 10000 );
				}
				else
				{
					LOG(TEXT("%S: ERROR %d@%S - WaitForCompletion failed (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
					retval = TPR_FAIL;
				}
			}
		}
	}

	// Check the results of playback duration verification
	if (retval == TPR_PASS)
	{
		PlaybackDurationData data;
		hr = testGraph.GetVerificationResult(VerifyPlaybackDuration, &data, NULL);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - playback duration verification failed"), __FUNCTION__, __LINE__, __FILE__);
			retval = TPR_FAIL;
		}
		else {
			LOG(TEXT("%S: first sample time - %d msec, EOS time - %d msec, playback duration - %d msec"), __FUNCTION__, TICKS_TO_MSEC(data.firstSampleTime), TICKS_TO_MSEC(data.eosTime), TICKS_TO_MSEC(actualDuration));

			// Take out the first sample time
			actualDuration -= data.firstSampleTime;

			// Check with the pct and tolerance absolute tolerances
			if (!ToleranceCheck(duration, actualDuration, pDurationTolerance->pctDurationTolerance) &&
				!ToleranceCheckAbs(duration, actualDuration, MSEC_TO_TICKS(pDurationTolerance->absDurationTolerance)))
			{
				LOG(TEXT("%S: ERROR %d@%S - expected duration: %d does not match actual duration %d msec. Tolerance: %d%%,%d msec: failed"), __FUNCTION__, __LINE__, __FILE__, TICKS_TO_MSEC(duration), TICKS_TO_MSEC(actualDuration), TICKS_TO_MSEC(pDurationTolerance->pctDurationTolerance), TICKS_TO_MSEC(pDurationTolerance->absDurationTolerance));
				retval = TPR_FAIL;
			}
			else {
				LOG(TEXT("%S: expected duration: %d matches actual duration: %d msec. Tolerance: %d%%,%d msec: suceeded"), __FUNCTION__, TICKS_TO_MSEC(duration), TICKS_TO_MSEC(actualDuration), TICKS_TO_MSEC(pDurationTolerance->pctDurationTolerance), TICKS_TO_MSEC(pDurationTolerance->absDurationTolerance));
			}

			// Calculate the time taken to send the EC_COMPLETE after it first flowed through the graph
			ecCompletionLatency = (ecCompletionLatency > data.eosTime) ? (ecCompletionLatency - data.eosTime) : (0xffffffff - data.eosTime + ecCompletionLatency);

			// If the time taken to send the EC_COMPLETE is greater than the tolerance
			if (ecCompletionLatency > MSEC_TO_TICKS(EC_COMPLETE_TOLERANCE))
			{
				LOG(TEXT("%S: ERROR %d@%S - EC_COMPLETE arrived %d msec after the EOS flowed through graph. Failing."), __FUNCTION__, __LINE__, __FILE__, TICKS_TO_MSEC(ecCompletionLatency));
				retval = TPR_FAIL;
			}
			else {
				LOG(TEXT("%S: EC Completion latency - %d msec"), __FUNCTION__, TICKS_TO_MSEC(ecCompletionLatency));
			}
		}
	}

    // Reset the testGraph
	testGraph.Reset();

	return retval;
}

TESTPROCAPI MultiplePlaybackTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	DWORD retval = TPR_PASS;
	HRESULT hr = S_OK;

	if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
		return SPR_HANDLED;
	else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

	// Get the test config object from the test parameter
	PlaybackTestDesc *pTestDesc = (PlaybackTestDesc*)lpFTE->dwUserData;

	// From the test desc, determine the media file to be used
	int nMedia = pTestDesc->GetNumMedia();

	// Instantiate the multiple test graphs
	TestGraph * testGraphArray = new TestGraph[nMedia];
	if (!testGraphArray)
	{
		LOG(TEXT("%S: ERROR %d@%s - failed to allocate multiple TestGraph objects."), __FUNCTION__, __LINE__, __FILE__);
		return TPR_FAIL;
	}

	
	// Build the graphs
	for(int i = 0; i < nMedia; i++)
	{
		Media* pMedia = pTestDesc->GetMedia(i);
		if (!pMedia)
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to retrieve media object"), __FUNCTION__, __LINE__, __FILE__);
			retval = TPR_FAIL;
			break;
		}

		hr = testGraphArray[i].BuildGraph(pMedia);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to build graph for media %d - %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, i, pMedia->GetUrl(), hr);
			retval = TPR_FAIL;
		}
	}

	if (retval == TPR_PASS)
	{
		LOG(TEXT("%S: successfully built graph for the %d media files."), __FUNCTION__, nMedia);

		Media* pMedia = pTestDesc->GetMedia(i);
		for(int i = 0; i < nMedia; i++)
		{
			// Change the state to running - the call doesn't return until the state change completes and the graph starts rendering
			hr = testGraphArray[i].SetState(State_Running, TestGraph::SYNCHRONOUS);
			if (FAILED(hr))
			{
				LOG(TEXT("%S: ERROR %d@%S - failed to change state to Running (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
				retval = TPR_FAIL;
			}
		}
	}

	HANDLE* eventArray = NULL;
	if (retval == TPR_PASS)
	{
		LOG(TEXT("%S: changed state to Running"), __FUNCTION__);

		eventArray = new HANDLE[nMedia];
		if (eventArray)
		{
			Media* pMedia = pTestDesc->GetMedia(i);
			for(int i = 0; i < nMedia; i++)
			{
				hr = testGraphArray[i].GetEventHandle(&eventArray[i]);
				if (FAILED(hr))
				{
					LOG(TEXT("%S: ERROR %d@%S - failed to get event handle for media %d - %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, i, pMedia->GetUrl(), hr);
					retval = TPR_FAIL;
				}
			}
		}
		else {
			LOG(TEXT("%S: ERROR %d@%s - failed to allocate event handle array."), __FUNCTION__, __LINE__, __FILE__);
			retval = TPR_FAIL;
		}
	}

	if (retval == TPR_PASS)
	{
		LOG(TEXT("%S: got event handles"), __FUNCTION__);
		
		// Wait for completion on all events
		hr = WaitForMultipleObjects(nMedia, eventArray, true, INFINITE);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - wait failed on event handles (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
			LOG(TEXT("%S: ERROR %d@%S - failed to receive EC_COMPLETE (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
			retval = TPR_FAIL;
		}
	}
		
	if (retval == TPR_FAIL)	{
		LOG(TEXT("%S: ERROR %d@%S - failed the multiple playback test"), __FUNCTION__, __LINE__, __FILE__);
	}
	else {
		LOG(TEXT("%S: successfully verified multiple playback."), __FUNCTION__);
	}

	if (eventArray)
		delete [] eventArray;

    // Reset the testGraph
	if (testGraphArray)
	{
		for(int i = 0; i < nMedia; i++)
		{
			testGraphArray[i].Reset();
		}
		delete [] testGraphArray;
	}

	return retval;
}

HRESULT RandomStateChangeTest(TestGraph& testGraph, int nStateChanges, FILTER_STATE initialState, int seed, DWORD dwTimeBetwenStates)
{
	HRESULT hr = S_OK;

	FILTER_STATE currState = initialState;
	FILTER_STATE nextState = currState;

	// Set the initial state
	testGraph.SetState(currState, TestGraph::SYNCHRONOUS);

	// Now randomly switch states
	LOG(TEXT("%S: switching randomly between states"), __FUNCTION__);

	for(int i = 0; i < nStateChanges; i++)
	{
		// Get the next state
		nextState = testGraph.GetRandomState();

		// Change to the new state
		hr = testGraph.SetState(nextState, TestGraph::SYNCHRONOUS);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to change state from %s to %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, GetStateName(currState), GetStateName(nextState), hr);
			break;
		}
		
		currState = nextState;

		Sleep(dwTimeBetwenStates);
	}

	return hr;
}

HRESULT GetNextState(StateChangeSequence sequence, FILTER_STATE current, FILTER_STATE* pNextState)
{
	FILTER_STATE state;

	if (!pNextState)
		return E_FAIL;

	switch(sequence)
	{
	case PlayPause:
		state = (current != State_Running) ? State_Running : State_Paused;
		break;

	case PlayStop:
		state = (current != State_Running) ? State_Running : State_Stopped;
		break;

	case PauseStop:
		state = (current != State_Paused) ? State_Paused : State_Stopped;
		break;

	case RandomSequence:
		state = (FILTER_STATE)(rand()%3);
		break;
	default:
		return E_FAIL;
	}

	*pNextState = state;
	return S_OK;
}

TESTPROCAPI StateChangeTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	DWORD retval = TPR_PASS;
	HRESULT hr = S_OK;

	if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
		return SPR_HANDLED;
	else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

	// Get the test config object from the test parameter
	StateChangeTestDesc *pTestDesc = (StateChangeTestDesc*)lpFTE->dwUserData;

	// From the test desc, determine the media file to be used
	Media* pMedia = pTestDesc->GetMedia(0);
	if (!pMedia)
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to retrieve media object"), __FUNCTION__, __LINE__, __FILE__);
		return TPR_FAIL;
	}


	// Instantiate the test graph
	TestGraph testGraph;

	// Build the graph
	hr = testGraph.BuildGraph(pMedia);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to build graph (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
		retval = TPR_FAIL;
	}
	else {
		LOG(TEXT("%S: successfully built graph for %s"), __FUNCTION__, pMedia->GetUrl());
	}

	if (retval == TPR_PASS)
	{
		DWORD* pLatency = NULL;
		
		// If the latency has to be verified
		bool bVerifyLatency = pTestDesc->GetVerification(VerifyStateChangeLatency, (void**)&pLatency);

		// Now run through the state changes
		Timer timer;
		LONGLONG latency = 0;
		FILTER_STATE currState = pTestDesc->state;
		FILTER_STATE nextState;
		LONGLONG llAvgLatency = 0;
		for(int i = 0; (retval == TPR_PASS) && (i < pTestDesc->nStateChanges); i++)
		{
			LOG(TEXT("%S: setting graph state to %s"), __FUNCTION__, GetStateName(currState));

			// Set the state
			timer.Start();
			hr = testGraph.SetState(currState, TestGraph::SYNCHRONOUS);
			latency = timer.Stop();
			if (FAILED(hr))
			{
				LOG(TEXT("%S: ERROR %d@%S - failed to set state to %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr, GetStateName(currState));
				retval = TPR_FAIL;
			}

			// Check latency if needed
			if ((retval == TPR_PASS) && bVerifyLatency)
			{
				if (TICKS_TO_MSEC(latency) > *pLatency)
				{
					LOG(TEXT("%S: ERROR %d@%S - state change latency (%d) ms exceeded tolerance (%d) msec (0x%x)"), __FUNCTION__, __LINE__, __FILE__, TICKS_TO_MSEC(latency), *pLatency);
					retval = TPR_FAIL;
				}
				else {
					LOG(TEXT("%S: latency %u"), __FUNCTION__, TICKS_TO_MSEC(latency));
				}
				llAvgLatency = llAvgLatency + latency;
			}

			// Get the next state
			if (retval == TPR_PASS)
			{
				// Get the next state to set
				hr = GetNextState(pTestDesc->sequence, currState, &nextState);
				if (FAILED(hr))
				{
					LOG(TEXT("%S: ERROR %d@%S - failed to get next state (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
					retval = TPR_FAIL;
				}
				currState = nextState;
			}

			if (retval == TPR_FAIL)
				break;

			Sleep(pTestDesc->dwTimeBetweenStates);
		}

		//calculate avg latency
		if(i!=0) 
		{
			llAvgLatency = llAvgLatency / i;
			LOG(TEXT("%S: Average latency for this run: %u"), __FUNCTION__, TICKS_TO_MSEC(llAvgLatency));
		}
		
		// Verification of state change
		if (retval == TPR_PASS)
		{
			LOG(TEXT("%S: successfully verified state changes."), __FUNCTION__);
		}
		else
		{
			LOG(TEXT("%S: ERROR %d@%S - failed the state change test (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
			retval = TPR_FAIL;
		}
	}

    // Reset the testGraph
	testGraph.Reset();

	return retval;
}


TESTPROCAPI Run_Pause_Run_Test(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	DWORD retval = TPR_PASS;
	HRESULT hr = S_OK;

	if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
		return SPR_HANDLED;
	else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

	// Get the test config object from the test parameter
	PlaybackTestDesc *pTestDesc = (PlaybackTestDesc*)lpFTE->dwUserData;

	// From the test desc, determine the media file to be used
	Media* pMedia = pTestDesc->GetMedia(0);
	if (!pMedia)
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to retrieve media object"), __FUNCTION__, __LINE__, __FILE__);
		return TPR_FAIL;
	}

	// Instantiate the test graph
	TestGraph testGraph;
	
	// Build the graph
	hr = testGraph.BuildGraph(pMedia);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to build graph for media %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
		retval = TPR_FAIL;
	}
	else {
		LOG(TEXT("%S: successfully built graph for %s"), __FUNCTION__, pMedia->GetUrl());
	}

	// Enable the verification required
	bool bGraphVerification = false;
	LONGLONG posTolerance = 0;
	VerificationList verificationList = pTestDesc->GetVerificationList();
	VerificationList::iterator iterator = verificationList.begin();
	while (iterator != verificationList.end())
	{
		VerificationType type = (VerificationType)iterator->first;
		if (testGraph.IsCapable(type)) {
			hr = testGraph.EnableVerification(type, iterator->second);
			if (FAILED_F(hr))
			{
				LOG(TEXT("%S: ERROR %d@%S - failed to enable verification %S)"), __FUNCTION__, __LINE__, __FILE__, GetVerificationString(type));
				break;
			}
			bGraphVerification = true;
		}
		else {
			LOG(TEXT("%S: WARNING %d@%S - unrecognized verification requested %S)"), __FUNCTION__, __LINE__, __FILE__, GetVerificationString(type));
		}
		iterator++;
	}

	// Get the duration
	LONGLONG duration = 0;
	if (retval == TPR_PASS)
	{
		hr = testGraph.GetDuration(&duration);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to get the duration (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
			retval = TPR_FAIL;
		}
	}

	// The test consists of the following steps
	// 1. Set the graph in running state
	// 2. Pause the graph
	// 3. Start verification
	// 4. Run the graph
	// 3. Get the verification results

	// Set the graph in running state
	if (retval == TPR_PASS)
	{
		hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to change state to Running (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
			retval = TPR_FAIL;
		}
		else {
			LOG(TEXT("%S: changed state to running."), __FUNCTION__);
		}
	}

	// Sleep some time
	if (retval == TPR_PASS)
	{
		// BUGBUG: add the delay specification to the config file
		Sleep(WAIT_BEFORE_NEXT_OPERATION);
	}

	// Set the graph in pause state
	if (retval == TPR_PASS)
	{
		hr = testGraph.SetState(State_Paused, TestGraph::SYNCHRONOUS);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to change state to Paused (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
			retval = TPR_FAIL;
		}
		else {
			LOG(TEXT("%S: changed state to Pause."), __FUNCTION__);
		}
	}

	// Sleep some more time
	if (retval == TPR_PASS)
	{
		// BUGBUG: add the delay specification to the config file
		Sleep(WAIT_BEFORE_NEXT_OPERATION);
	}

	// Signal the start of verification
	if ((retval == TPR_PASS) && bGraphVerification)
	{
		hr = testGraph.StartVerification();
		if (FAILED_F(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to start the verification (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
		}
	}

	// Sleep some more time
	if (retval == TPR_PASS)
	{
		// BUGBUG: add the delay specification to the config file
		Sleep(WAIT_BEFORE_NEXT_OPERATION);
	}


	if (retval == TPR_PASS)
	{
		// Set the graph in running state
		if (retval == TPR_PASS)
		{
			hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS);
			if (FAILED(hr))
			{
				LOG(TEXT("%S: ERROR %d@%S - failed to change state to Running (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
				retval = TPR_FAIL;
			}
			else {
				LOG(TEXT("%S: changed state to running."), __FUNCTION__);
			}
		}
	}

	// Sleep some more time
	if (retval == TPR_PASS)
	{
		// BUGBUG: add the delay specification to the config file
		Sleep(WAIT_FOR_VERIFICATION);
	}

	// Check the results of verification
	if (bGraphVerification && (retval == TPR_PASS))
	{
		hr = testGraph.GetVerificationResults();
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - verification failed"), __FUNCTION__, __LINE__, __FILE__);
			retval = TPR_FAIL;
		}
		else {
			LOG(TEXT("%S: verification succeeded"), __FUNCTION__);
		}
	}

	// Set the state back to stop
	hr = testGraph.SetState(State_Stopped, TestGraph::SYNCHRONOUS);
	if ((retval == TPR_PASS) && FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to change state to stopped (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
		retval = TPR_FAIL;
	}

	// Reset the testGraph
	testGraph.Reset();

	// Reset the test
    pTestDesc->Reset();

	return retval;
}

