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

class TextureTest : public D3DMInitializer {
private:
	
	//
	// Indicates whether or not the object is initialized
	//
	BOOL m_bInitSuccess;

	//
	// Is test executing over debug middleware?
	//
	BOOL m_bDebugMiddleware;

	//
	// Caps verification helpers
	//
	HRESULT SupportsMipMap();
	HRESULT SupportsManagedPool();
	HRESULT SupportsTextures();
	HRESULT SupportsLockableTexture();

	//
	// Resource creation helpers
	//
	HRESULT CreateSimpleTexture(IDirect3DMobileTexture **ppTexture, BOOL bLockable);
	HRESULT CreateSimpleMipMap(IDirect3DMobileTexture **ppTexture, BOOL bLockable);
	HRESULT CreateAutoFormatTexture(IDirect3DMobileTexture **ppTexture, D3DMPOOL TexturePool, BOOL bLockable);
	HRESULT CreateAutoFormatMipMap(IDirect3DMobileTexture **ppTexture, D3DMPOOL TexturePool, BOOL bLockable);
	HRESULT CreateManagedTexture(IDirect3DMobileTexture **ppTexture, BOOL bLockable);
	HRESULT CreateManagedMipMap(IDirect3DMobileTexture **ppTexture);

public:

	TextureTest();
	HRESULT Init(LPTEST_CASE_ARGS pTestCaseArgs, LPWINDOW_ARGS pWindowArgs, BOOL bDebug, UINT uiTestCase);
	~TextureTest();

	//
	// A convenience function, that indicates whether the initialization
	// has completed successfully.
	//
	BOOL IsReady();

	////////////////////////////////////////////////////////////////////
	// Verification for individual methods of IDirect3DMobileTexture
	////////////////////////////////////////////////////////////////////

	//
	// Verify IDirect3DMobileTexture::QueryInterface  
	//
	INT ExecuteQueryInterfaceTest();

	//
	// Verify IDirect3DMobileTexture::AddRef          
	//
	INT ExecuteAddRefTest();

	//
	// Verify IDirect3DMobileTexture::Release         
	//
	INT ExecuteReleaseTest();

	//
	// Verify IDirect3DMobileTexture::GetDevice       
	//
	INT ExecuteGetDeviceTest();

	//
	// Verify IDirect3DMobileTexture::SetPriority     
	//
	INT ExecuteSetPriorityTest();

	//
	// Verify IDirect3DMobileTexture::GetPriority     
	//
	INT ExecuteGetPriorityTest();

	//
	// Verify IDirect3DMobileTexture::PreLoad         
	//
	INT ExecutePreLoadTest();

	//
	// Verify IDirect3DMobileTexture::GetType         
	//
	INT ExecuteGetTypeTest();

	//
	// Verify IDirect3DMobileTexture::SetLOD          
	//
	INT ExecuteSetLODTest();

	//
	// Verify IDirect3DMobileTexture::GetLOD          
	//
	INT ExecuteGetLODTest();

	//
	// Verify IDirect3DMobileTexture::GetLevelCount   
	//
	INT ExecuteGetLevelCountTest();

	//
	// Verify IDirect3DMobileTexture::GetLevelDesc    
	//
	INT ExecuteGetLevelDescTest();

	//
	// Verify IDirect3DMobileTexture::GetSurfaceLevel 
	//
	INT ExecuteGetSurfaceLevelTest();

	//
	// Verify IDirect3DMobileTexture::LockRect        
	//
	INT ExecuteLockRectTest();

	//
	// Verify IDirect3DMobileTexture::UnlockRect      
	//
	INT ExecuteUnlockRectTest();

	//
	// Verify IDirect3DMobileTexture::AddDirtyRect    
	//
	INT ExecuteAddDirtyRectTest();

	/////////////////////////////////////////////
	// Texture surface tests
	/////////////////////////////////////////////

	//
	// Texture Surface Level IDirect3DMobileSurface::QueryInterface
	//
	INT ExecuteTexSurfQueryInterfaceTest();

	//
	// Texture Surface Level IDirect3DMobileSurface::AddRef
	//
	INT ExecuteTexSurfAddRefTest();

	//
	// Texture Surface Level IDirect3DMobileSurface::Release
	//
	INT ExecuteTexSurfReleaseTest();

	//
	// Texture Surface Level IDirect3DMobileSurface::GetDevice
	//
	INT ExecuteTexSurfGetDeviceTest();

	//
	// Texture Surface Level IDirect3DMobileSurface::GetContainer
	//
	INT ExecuteTexSurfGetContainerTest();

	//
	// Texture Surface Level IDirect3DMobileSurface::GetDesc
	//
	INT ExecuteTexSurfGetDescTest();

	//
	// Texture Surface Level IDirect3DMobileSurface::LockRect
	//
	INT ExecuteTexSurfLockRectTest();

	//
	// Texture Surface Level IDirect3DMobileSurface::UnlockRect
	//
	INT ExecuteTexSurfUnlockRectTest();

	//
	// Texture Surface Level IDirect3DMobileSurface::GetDC
	//
	INT ExecuteTexSurfGetDCTest();

	//
	// Texture Surface Level IDirect3DMobileSurface::ReleaseDC
	//
	INT ExecuteTexSurfReleaseDCTest();

	//
	// Texture Surface Level IDirect3DMobileDevice::StretchRect
	//
	INT ExecuteTexSurfStretchRectTest();

	//
	// Texture Surface Level IDirect3DMobileDevice::ColorFill
	//
	INT ExecuteTexSurfColorFillTest();
	
};

