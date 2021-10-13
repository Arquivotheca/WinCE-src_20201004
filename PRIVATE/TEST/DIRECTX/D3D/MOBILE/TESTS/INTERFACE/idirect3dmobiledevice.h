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

class IDirect3DMobileDeviceTest : public D3DMInitializer {
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
	// Obtain the driver's error status.
	//
	HRESULT GetDriverError(HRESULT *phrDriver);

	//
	// Forces a flush of the Direct3D Mobile command buffer
	//
	HRESULT Flush();

	//
	// Verify that an API call succeeded or failed.
	//
	BOOL SucceededCall(HRESULT hrReturnValue);
	BOOL FailedCall(HRESULT hrReturnValue);

	//
	// Utility for mip-mapping tests; inspects a value and determines
	// whether or not it is a power of two
	//
	BOOL IsPowTwo(UINT uiValue);

public:


	IDirect3DMobileDeviceTest();
	HRESULT Init(LPTEST_CASE_ARGS pTestCaseArgs, LPWINDOW_ARGS pWindowArgs, BOOL bDebug, UINT uiTestCase);
	~IDirect3DMobileDeviceTest();

	//
	// A convenience function, that indicates whether the initialization
	// has completed successfully.
	//
	BOOL IsReady();

	///////////////////////////////////////////////////////////////
	// Verification for the sole export of D3DM.DLL
	///////////////////////////////////////////////////////////////

	INT ExecuteDirect3DMobileCreateTest();


	///////////////////////////////////////////////////////////////
	// Verification for individual methods of IDirect3DMobileDevice
	///////////////////////////////////////////////////////////////

	//
	// Verify IDirect3DMobileDevice::QueryInterface
	//
	INT ExecuteQueryInterfaceTest();

	//
	// Verify IDirect3DMobileDevice::TestCooperativeLevel
	//
	INT ExecuteDeviceTestCooperativeLevelTest();

	//
	// Verify IDirect3DMobileDevice::GetAvailablePoolMem
	//
	INT ExecuteGetAvailablePoolMemTest();

	//
	// Verify IDirect3DMobileDevice::ResourceManagerDiscardBytes
	//
	INT ExecuteResourceManagerDiscardBytesTest();

	//
	// Verify IDirect3DMobileDevice::GetDirect3D
	//
	INT ExecuteGetDirect3DTest();

	//
	// Verify IDirect3DMobileDevice::GetDeviceCaps
	//
	INT ExecuteGetDeviceCapsTest();

	//
	// Verify IDirect3DMobileDevice::GetDisplayMode
	//
	INT ExecuteGetDisplayModeTest();

	//
	// Verify IDirect3DMobileDevice::GetCreationParameters
	//
	INT ExecuteGetCreationParametersTest();

	//
	// Verify IDirect3DMobileDevice::CreateAdditionalSwapChain
	//
	INT ExecuteCreateAdditionalSwapChainTest();

	//
	// Verify IDirect3DMobileDevice::Reset
	//
	INT ExecuteResetTest();

	//
	// Verify IDirect3DMobileDevice::Present
	//
	INT ExecutePresentTest();

	//
	// Verify IDirect3DMobileDevice::GetBackBuffer
	//
	INT ExecuteGetBackBufferTest();

	//
	// Verify IDirect3DMobileDevice::CreateTexture
	//
	INT ExecuteCreateTextureTest();

	//
	// Verify IDirect3DMobileDevice::CreateVertexBuffer
	//
	INT ExecuteCreateVertexBufferTest();

	//
	// Verify IDirect3DMobileDevice::CreateIndexBuffer
	//
	INT ExecuteCreateIndexBufferTest();

	//
	// Verify IDirect3DMobileDevice::CreateRenderTarget
	//
	INT ExecuteCreateRenderTargetTest();

	//
	// Verify IDirect3DMobileDevice::CreateDepthStencilSurface
	//
	INT ExecuteCreateDepthStencilSurfaceTest();

	//
	// Verify IDirect3DMobileDevice::CreateImageSurface
	//
	INT ExecuteCreateImageSurfaceTest();

	//
	// Verify IDirect3DMobileDevice::CopyRects
	//
	INT ExecuteCopyRectsTest();

	//
	// Verify IDirect3DMobileDevice::UpdateTexture
	//
	INT ExecuteUpdateTextureTest();

	//
	// Verify IDirect3DMobileDevice::GetFrontBuffer
	//
	INT ExecuteGetFrontBufferTest();

	//
	// Verify IDirect3DMobileDevice::SetRenderTarget
	//
	INT ExecuteSetRenderTargetTest();

	//
	// Verify IDirect3DMobileDevice::GetRenderTarget
	//
	INT ExecuteGetRenderTargetTest();

	//
	// Verify IDirect3DMobileDevice::GetDepthStencilSurface
	//
	INT ExecuteGetDepthStencilSurfaceTest();

	//
	// Verify IDirect3DMobileDevice::BeginScene
	//
	INT ExecuteBeginSceneTest();

	//
	// Verify IDirect3DMobileDevice::EndScene
	//
	INT ExecuteEndSceneTest();

	//
	// Verify IDirect3DMobileDevice::Clear
	//
	INT ExecuteClearTest();

	//
	// Verify IDirect3DMobileDevice::SetTransform
	//
	INT ExecuteSetTransformTest();

	//
	// Verify IDirect3DMobileDevice::GetTransform
	//
	INT ExecuteGetTransformTest();

	//
	// Verify IDirect3DMobileDevice::SetViewport
	//
	INT ExecuteSetViewportTest();

	//
	// Verify IDirect3DMobileDevice::GetViewport
	//
	INT ExecuteGetViewportTest();

	//
	// Verify IDirect3DMobileDevice::SetMaterial
	//
	INT ExecuteSetMaterialTest();

	//
	// Verify IDirect3DMobileDevice::GetMaterial
	//
	INT ExecuteGetMaterialTest();

	//
	// Verify IDirect3DMobileDevice::SetLight
	//
	INT ExecuteSetLightTest();

	//
	// Verify IDirect3DMobileDevice::GetLight
	//
	INT ExecuteGetLightTest();

	//
	// Verify IDirect3DMobileDevice::LightEnable
	//
	INT ExecuteLightEnableTest();

	//
	// Verify IDirect3DMobileDevice::GetLightEnable
	//
	INT ExecuteGetLightEnableTest();

	//
	// Verify IDirect3DMobileDevice::SetRenderState
	//
	INT ExecuteSetRenderStateTest();

	//
	// Verify IDirect3DMobileDevice::GetRenderState
	//
	INT ExecuteGetRenderStateTest();

	//
	// Verify IDirect3DMobileDevice::SetClipStatus
	//
	INT ExecuteSetClipStatusTest();

	//
	// Verify IDirect3DMobileDevice::GetClipStatus
	//
	INT ExecuteGetClipStatusTest();

	//
	// Verify IDirect3DMobileDevice::GetTexture
	//
	INT ExecuteGetTextureTest();

	//
	// Verify IDirect3DMobileDevice::SetTexture
	//
	INT ExecuteSetTextureTest();

	//
	// Verify IDirect3DMobileDevice::GetTextureStageState
	//
	INT ExecuteGetTextureStageStateTest();

	//
	// Verify IDirect3DMobileDevice::SetTextureStageState
	//
	INT ExecuteSetTextureStageStateTest();

	//
	// Verify IDirect3DMobileDevice::ValidateDevice
	//
	INT ExecuteValidateDeviceTest();

	//
	// Verify IDirect3DMobileDevice::GetInfo
	//
	INT ExecuteGetInfoTest();

	//
	// Verify IDirect3DMobileDevice::SetPaletteEntries
	//
	INT ExecuteSetPaletteEntriesTest();

	//
	// Verify IDirect3DMobileDevice::GetPaletteEntries
	//
	INT ExecuteGetPaletteEntriesTest();

	//
	// Verify IDirect3DMobileDevice::SetCurrentTexturePalette
	//
	INT ExecuteSetCurrentTexturePaletteTest();

	//
	// Verify IDirect3DMobileDevice::GetCurrentTexturePalette
	//
	INT ExecuteGetCurrentTexturePaletteTest();

	//
	// Verify IDirect3DMobileDevice::DrawPrimitive
	//
	INT ExecuteDrawPrimitiveTest();

	//
	// Verify IDirect3DMobileDevice::DrawIndexedPrimitive
	//
	INT ExecuteDrawIndexedPrimitiveTest();

	//
	// Verify IDirect3DMobileDevice::ProcessVertices
	//
	INT ExecuteProcessVerticesTest();

	//
	// Verify IDirect3DMobileDevice::SetStreamSource
	//
	INT ExecuteSetStreamSourceTest();

	//
	// Verify IDirect3DMobileDevice::GetStreamSource
	//
	INT ExecuteGetStreamSourceTest();

	//
	// Verify IDirect3DMobileDevice::SetIndices
	//
	INT ExecuteSetIndicesTest();

	//
	// Verify IDirect3DMobileDevice::GetIndices
	//
	INT ExecuteGetIndicesTest();

	//
	// Verify IDirect3DMobileDevice::StretchRect
	//
	INT ExecuteStretchRectTest();

	//
	// Verify IDirect3DMobileDevice::ColorFill
	//
	INT ExecuteColorFillTest();


	/////////////////////////////////////////////
	// Additional miscellaneous tests
	/////////////////////////////////////////////

	//
	// Verify automatic mip-map level generation
	//
	INT ExecuteVerifyAutoMipLevelExtentsTest();

	//
	// Attempt to use multisampling when disallowed by swap effect
	//
	INT ExecuteSwapEffectMultiSampleMismatchTest();

	//
	// Attempt to create a swap chain with lockable multisampled back buffers
	//
	INT ExecuteAttemptPresentFlagMultiSampleMismatchTest();

	//
	// Verify that multiple back-buffers are disallowed, when using the copy swap effect
	//
	INT ExecuteVerifyBackBufLimitSwapEffectCopyTest();

	//
	// Attempt to manipulate swap chain after a failed reset
	//
	INT ExecuteSwapChainManipDuringResetTest();

	//
	// Verify presentation interval functionality
	//
	INT ExecuteDevicePresentationIntervalTest();

	//
	// Test command-buffer ref counting
	//
	INT ExecuteInterfaceCommandBufferRefTest();

	//
	// Test PSL security
	//
	INT ExecutePSLRandomizerTest();
};


//
// Simple vertex structure for vert buffer testing
//
struct D3DVERTEXSIMPLE
{
    FLOAT x, y, z;   // The untransformed position for the vertex
};

// Flexible Vertex Format bitmask for D3DVERTEXSIMPLE
#define D3DFVF_D3DVERTEXSIMPLE (D3DMFVF_XYZ_FLOAT)
