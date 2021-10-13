//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once
#include <windows.h>

HRESULT GetPermutationSpecs(UINT uiNumIters, UINT uiMaxIter, UINT *puiNumStepsTotal, UINT *puiNumStepsPerIter);

HRESULT GetPermutation(UINT uiStep, UINT uiStepsPerIter, UINT *puiIter1, UINT *puiIter2, UINT *puiIter3);

HRESULT GetPermutation(UINT uiStep, UINT uiStepsPerIter, UINT *puiIter1, UINT *puiIter2);

HRESULT ThreeAxisPermutations(UINT uiStep,
                              float fMin, float fMax, UINT uiMaxIter,
                              float *pfX, float *pfY, float *pfZ);

HRESULT TwoAxisPermutations(UINT uiStep,
                            float fMin, float fMax, UINT uiMaxIter,
                            float *pfAxis1, float *pfAxis2);

HRESULT OneAxisPermutations(UINT uiStep,
                            float fMin, float fMax, UINT uiMaxIter,
                            float *pfAxis);
