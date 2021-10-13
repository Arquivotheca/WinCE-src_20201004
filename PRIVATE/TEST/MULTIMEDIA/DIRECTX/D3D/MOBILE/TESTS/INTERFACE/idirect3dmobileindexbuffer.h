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

