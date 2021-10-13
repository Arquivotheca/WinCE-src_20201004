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

class VertexBufferTest : public D3DMInitializer {
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

	VertexBufferTest();
	HRESULT Init(LPTEST_CASE_ARGS pTestCaseArgs, LPWINDOW_ARGS pWindowArgs, BOOL bDebug, UINT uiTestCase);
	~VertexBufferTest();

	//
	// A convenience function, that indicates whether the initialization
	// has completed successfully.
	//
	BOOL IsReady();

	////////////////////////////////////////////////////////////////////
	// Verification for individual methods of IDirect3DMobileVertexBuffer
	////////////////////////////////////////////////////////////////////

	//
	// Verify IDirect3DMobileVertexBuffer::QueryInterface
	//
	INT ExecuteQueryInterfaceTest();

	//
	// Verify IDirect3DMobileVertexBuffer::AddRef
	//
	INT ExecuteAddRefTest();

	//
	// Verify IDirect3DMobileVertexBuffer::Release
	//
	INT ExecuteReleaseTest();

	//
	// Verify IDirect3DMobileVertexBuffer::GetDevice
	//
	INT ExecuteGetDeviceTest();

	//
	// Verify IDirect3DMobileVertexBuffer::SetPriority
	//
	INT ExecuteSetPriorityTest();

	//
	// Verify IDirect3DMobileVertexBuffer::GetPriority
	//
	INT ExecuteGetPriorityTest();

	//
	// Verify IDirect3DMobileVertexBuffer::PreLoad
	//
	INT ExecutePreLoadTest();

	//
	// Verify IDirect3DMobileVertexBuffer::GetType
	//
	INT ExecuteGetTypeTest();

	//
	// Verify IDirect3DMobileVertexBuffer::Lock
	//
	INT ExecuteLockTest();

	//
	// Verify IDirect3DMobileVertexBuffer::Unlock
	//
	INT ExecuteUnlockTest();

	//
	// Verify IDirect3DMobileVertexBuffer::GetDesc
	//
	INT ExecuteGetDescTest();


	/////////////////////////////////////////////
	// Additional miscellaneous tests
	/////////////////////////////////////////////

};

