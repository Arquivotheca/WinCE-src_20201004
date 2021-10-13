//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once
#include "Initializer.h"

class IndexBufferTest : public D3DMInitializer {
private:

	//
	// Is test executing over debug middleware?
	//
	BOOL m_bDebugMiddleware;
	
	//
	// Indicates whether or not the object is initialized
	//
	BOOL m_bInitSuccess;

public:

	IndexBufferTest();
	HRESULT Init(LPTEST_CASE_ARGS pTestCaseArgs, LPWINDOW_ARGS pWindowArgs, BOOL bDebug, UINT uiTestCase);
	~IndexBufferTest();

	//
	// A convenience function, that indicates whether the initialization
	// has completed successfully.
	//
	BOOL IsReady();


	////////////////////////////////////////////////////////////////////
	// Verification for individual methods of IDirect3DMobileIndexBuffer
	////////////////////////////////////////////////////////////////////

	//
	// Verify IDirect3DMobileIndexBuffer::QueryInterface
	//
	INT ExecuteQueryInterfaceTest();

	//
	// Verify IDirect3DMobileIndexBuffer::AddRef
	//
	INT ExecuteAddRefTest();

	//
	// Verify IDirect3DMobileIndexBuffer::Release
	//
	INT ExecuteReleaseTest();

	//
	// Verify IDirect3DMobileIndexBuffer::GetDevice
	//
	INT ExecuteGetDeviceTest();

	//
	// Verify IDirect3DMobileIndexBuffer::SetPriority
	//
	INT ExecuteSetPriorityTest();

	//
	// Verify IDirect3DMobileIndexBuffer::GetPriority
	//
	INT ExecuteGetPriorityTest();

	//
	// Verify IDirect3DMobileIndexBuffer::PreLoad
	//
	INT ExecutePreLoadTest();

	//
	// Verify IDirect3DMobileIndexBuffer::GetType
	//
	INT ExecuteGetTypeTest();

	//
	// Verify IDirect3DMobileIndexBuffer::Lock
	//
	INT ExecuteLockTest();

	//
	// Verify IDirect3DMobileIndexBuffer::Unlock
	//
	INT ExecuteUnlockTest();

	//
	// Verify IDirect3DMobileIndexBuffer::GetDesc
	//
	INT ExecuteGetDescTest();


	/////////////////////////////////////////////
	// Additional miscellaneous tests
	/////////////////////////////////////////////

};

