//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <tchar.h>
#include <windows.h>
#include <math.h>

//
// GetPermutationSpecs
//
//   Given a number of iterators, a ceiling on the total number of steps to take (a sum of
//   maximal number of steps to take for each iterator), this function indicates the maximal
//   number of iterations to execute assuming that the iterators all must take the same
//   same number of iterations.
//
//   Note:  It is possible (likely) that the actual number of iterations resulting from the
//   output of this function will be less than the ceiling provided to this function.
//
// Arguments:
//
//   UINT uiNumIters:  Number of iterators 
//   UINT uiMaxIter:  "Ceiling" for total number of allowable unique permutations 
//   UINT *puiNumStepsTotal:  Total number of steps that would be required to exhaustively iterate,
//                            with the specified number of iterators, over the desired range, with
//                            the recommended step size.
//   UINT *puiNumStepsPerIter:  Number of steps, per iterator.
//
// Return Value:
//
//   HRESULT indicates success or failure
//
HRESULT GetPermutationSpecs(UINT uiNumIters, UINT uiMaxIter, UINT *puiNumStepsTotal, UINT *puiNumStepsPerIter)
{
	UINT uiNumStepsPerIter;
	UINT uiNumStepsTotal;
	
	uiNumStepsPerIter = (UINT)floor(pow((float)uiMaxIter,1.0f/(float)uiNumIters));

	// Must, at a minimum, take two steps per iterator.  Any less is insufficient to cover
	// a non-degenerate range.
	if (uiNumStepsPerIter <= 1)
		return	E_FAIL;

	uiNumStepsTotal = (UINT)pow((float)(uiNumStepsPerIter), (float)uiNumIters);

	//
	// NULL args allowed; to permit the case where the caller only needs *some* of the information
	//
	if (puiNumStepsPerIter) *puiNumStepsPerIter = uiNumStepsPerIter;
	if (puiNumStepsTotal)   *puiNumStepsTotal   = uiNumStepsTotal;

	return S_OK;
}

//
// GetPermutation
//
//   Given three iterators, and an integer range of [0,uiStepsPerIter], generates values
//   for each iterator, based on a "step number".
//
// Arguments:
//
//    UINT uiStep:  
//    UINT uiStepsPerIter:  Iterator constraint
//    UINT *puiIter1:  Output of iterator value
//    UINT *puiIter2:  Output of iterator value
//    UINT *puiIter3:  Output of iterator value
// 
// Return Value:
//
//   HRESULT indicates success or failure
//
HRESULT GetPermutation(UINT uiStep, UINT uiStepsPerIter, UINT *puiIter1, UINT *puiIter2, UINT *puiIter3)
{
	CONST UINT uiNumIters = 3;
	UINT uiRemaining = uiStep;
	UINT uiDivisor = (UINT)pow((float)uiStepsPerIter, (float)uiNumIters);

	if (uiStep >= uiDivisor)
		return E_FAIL;

	if (0 == uiStepsPerIter)
		return E_FAIL;

	if ((NULL == puiIter1) || (NULL == puiIter2) || (NULL == puiIter3))
		return E_FAIL;

	uiDivisor /= uiStepsPerIter;
	*puiIter1 = uiRemaining / uiDivisor;
	uiRemaining -= (*puiIter1 * uiDivisor);

	uiDivisor /= uiStepsPerIter;
	*puiIter2 = uiRemaining / uiDivisor;
	uiRemaining -= (*puiIter2 * uiDivisor);

	uiDivisor /= uiStepsPerIter;
	*puiIter3 = uiRemaining / uiDivisor;
	uiRemaining -= (*puiIter3 * uiDivisor);

	return S_OK;
}

//
// GetPermutation
//
//   Given two iterators, and an integer range of [0,uiStepsPerIter], generates values
//   for each iterator, based on a "step number".
//
// Arguments:
//
//    UINT uiStep:  
//    UINT uiStepsPerIter:  Iterator constraint
//    UINT *puiIter1:  Output of iterator value
//    UINT *puiIter2:  Output of iterator value
// 
// Return Value:
//
//   HRESULT indicates success or failure
//
HRESULT GetPermutation(UINT uiStep, UINT uiStepsPerIter, UINT *puiIter1, UINT *puiIter2)
{
	CONST UINT uiNumIters = 2;
	UINT uiRemaining = uiStep;
	UINT uiDivisor = (UINT)pow((float)uiStepsPerIter, (float)uiNumIters);

	if (uiStep >= uiDivisor)
		return E_FAIL;

	if (0 == uiStepsPerIter)
		return E_FAIL;

	if ((NULL == puiIter1) || (NULL == puiIter2))
		return E_FAIL;

	uiDivisor /= uiStepsPerIter;
	*puiIter1 = uiRemaining / uiDivisor;
	uiRemaining -= (*puiIter1 * uiDivisor);

	uiDivisor /= uiStepsPerIter;
	*puiIter2 = uiRemaining / uiDivisor;
	uiRemaining -= (*puiIter2 * uiDivisor);

	return S_OK;
}


HRESULT ThreeAxisPermutations(UINT uiStep,
                              float fMin, float fMax, UINT uiMaxIter,
                              float *pfX, float *pfY, float *pfZ)
{
	CONST UINT uiNumIterators = 3; // 3 axis test
	UINT uiX, uiY, uiZ;
	float fStepSize;
	UINT uiNumStepsTotal, uiNumStepsPerIter;

	if (FAILED(GetPermutationSpecs(uiNumIterators,      // UINT uiNumIters,
	                               uiMaxIter,           // UINT uiMaxIter,
	                               &uiNumStepsTotal,    // UINT *puiNumStepsTotal,
	                               &uiNumStepsPerIter)))// UINT *puiNumStepsPerIter
	{
		OutputDebugString(_T("GetPermutationSpecs failed."));
		return E_FAIL;
	}

	if (FAILED(GetPermutation(uiStep,            // UINT uiStep,
	                          uiNumStepsPerIter, // UINT uiStepsPerIter,
	                          &uiX,              // UINT *puiIter1,
	                          &uiY,              // UINT *puiIter2,
	                          &uiZ)))            // UINT *puiIter3
	{
		OutputDebugString(_T("GetPermutation failed."));
		return E_FAIL;
	}

	fStepSize = (fMax - fMin)/((float)uiNumStepsPerIter - 1.0f);

	*pfX = fMin + uiX * fStepSize;
	*pfY = fMin + uiY * fStepSize;
	*pfZ = fMin + uiZ * fStepSize;

	return S_OK;
}


HRESULT TwoAxisPermutations(UINT uiStep,
                            float fMin, float fMax, UINT uiMaxIter,
                            float *pfAxis1, float *pfAxis2)
{
	CONST UINT uiNumIterators = 2; // 2 axis test
	UINT uiAxis1, uiAxis2;
	float fStepSize;
	UINT uiNumStepsTotal, uiNumStepsPerIter;

	if (FAILED(GetPermutationSpecs(uiNumIterators,      // UINT uiNumIters,
	                               uiMaxIter,           // UINT uiMaxIter,
	                               &uiNumStepsTotal,    // UINT *puiNumStepsTotal,
	                               &uiNumStepsPerIter)))// UINT *puiNumStepsPerIter
	{
		OutputDebugString(_T("GetPermutationSpecs failed."));
		return E_FAIL;
	}

	if (FAILED(GetPermutation(uiStep,            // UINT uiStep,
	                          uiNumStepsPerIter, // UINT uiStepsPerIter,
	                          &uiAxis1,          // UINT *puiIter1,
	                          &uiAxis2)))        // UINT *puiIter2,
	{
		OutputDebugString(_T("GetPermutation failed."));
		return E_FAIL;
	}

	fStepSize = (fMax - fMin)/((float)uiNumStepsPerIter - 1.0f);

	*pfAxis1 = fMin + uiAxis1 * fStepSize;
	*pfAxis2 = fMin + uiAxis2 * fStepSize;

	return S_OK;
}


HRESULT OneAxisPermutations(UINT uiStep,
                            float fMin, float fMax, UINT uiMaxIter,
                            float *pfAxis)
{
	CONST UINT uiNumIterators = 1; // 1 axis test
	float fStepSize;
	UINT uiNumStepsTotal, uiNumStepsPerIter;

	if (uiStep > uiMaxIter)
		return E_FAIL;

	if (FAILED(GetPermutationSpecs(uiNumIterators,      // UINT uiNumIters,
	                               uiMaxIter,           // UINT uiMaxIter,
	                               &uiNumStepsTotal,    // UINT *puiNumStepsTotal,
	                               &uiNumStepsPerIter)))// UINT *puiNumStepsPerIter
	{
		OutputDebugString(_T("GetPermutationSpecs failed."));
		return E_FAIL;
	}

	if (0 == uiNumStepsPerIter)
		return E_FAIL;

	fStepSize = (fMax - fMin)/((float)uiNumStepsPerIter - 1.0f);

	*pfAxis = fMin + uiStep * fStepSize;

	return S_OK;
}

