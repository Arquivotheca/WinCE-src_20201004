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

class SurfaceTest : public D3DMInitializer {
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

	SurfaceTest();
	HRESULT Init(LPTEST_CASE_ARGS pTestCaseArgs, LPWINDOW_ARGS pWindowArgs, BOOL bDebug, UINT uiTestCase);
	~SurfaceTest();

	//
	// A convenience function, that indicates whether the initialization
	// has completed successfully.
	//
	BOOL IsReady();

	////////////////////////////////////////////////////////////////////
	// Verification for individual methods of IDirect3DMobileSurface
	////////////////////////////////////////////////////////////////////

	//
	// Verify IDirect3DMobileSurface::QueryInterface
	//
	INT ExecuteQueryInterfaceTest();
	
	//
	// Verify IDirect3DMobileSurface::AddRef        
	//
	INT ExecuteAddRefTest();
	
	//
	// Verify IDirect3DMobileSurface::Release       
	//
	INT ExecuteReleaseTest();
	
	//
	// Verify IDirect3DMobileSurface::GetDevice     
	//
	INT ExecuteGetDeviceTest();
	
	//
	// Verify IDirect3DMobileSurface::GetContainer  
	//
	INT ExecuteGetContainerTest();

	//
	// Verify IDirect3DMobileSurface::GetDesc       
	//
	INT ExecuteGetDescTest();

	//
	// Verify IDirect3DMobileSurface::LockRect      
	//
	INT ExecuteLockRectTest();
	
	//
	// Verify IDirect3DMobileSurface::UnlockRect    
	//
	INT ExecuteUnlockRectTest();

	//
	// Verify IDirect3DMobileSurface::GetDC         
	//

	INT ExecuteGetDCTest();
	
	//
	// Verify IDirect3DMobileSurface::ReleaseDC     
	//
	INT ExecuteReleaseDCTest();

	/////////////////////////////////////////////
	// Additional miscellaneous tests
	/////////////////////////////////////////////

};

