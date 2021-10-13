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

