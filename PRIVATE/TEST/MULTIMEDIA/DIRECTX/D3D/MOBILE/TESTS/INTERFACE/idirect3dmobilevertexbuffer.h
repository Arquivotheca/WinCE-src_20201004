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

