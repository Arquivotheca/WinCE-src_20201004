//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <d3dm.h>
#include <tchar.h>
#include "VerifTestCases.h"
#include "CreationArgs.h"
#include "ResUtils.h"
#include <tux.h>

//
// During resource creation, limit memory usage from filling all available memory
//
#define D3DQA_NUM_RESERVED_BYTES 100000

//
// Inclusive range of priorities to use during resource manager test
//
#define D3DQA_PRIORITY_MIN 0  
#define D3DQA_PRIORITY_MAX 5 


// 
// ResourceManagerEvictionTest
// 
//   Although the specification for resource management allows flexibility on most
//   design and implementation details (e.g., LRU algorithm is recommended but not
//   required), this test attempts to create circumstances in which the resource
//   manager is forced to evict some resources due to video memory scarcity, and
//   use priority levels to do so.
//
//   This test is most effective when:
//
//       * Video memory is more scarce than system memory
//       * The eviction algorithm is LRU or a variant thereof
//       * A minimum of 1MB of managed pool allocations can be made
//         to facilitate testing.
// 
//   The test will execute even if these conditions due not exist, although the value
//   of the test may be diminished.
//
//   Because the resource manager is not required to implement any single particular
//   eviction algorithm, this test cannot ascertain whether or not the resource
//   manager is selecting the correct action under any particular circumstance.
//   Therefore, this test generally cannot indicate failure as a result.  Regardless,
//   it is useful for exercising resource management code.
//   
// Arguments:
// 
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying Direct3D device
//   D3DMRESOURCETYPE ResType:  Resource type that should be exercised
// 
// Return Value:
// 
//   TPR_PASS, TPR_SKIP, or TPR_ABORT
// 
INT ResourceManagerEvictionTest(LPDIRECT3DMOBILEDEVICE pDevice, D3DMRESOURCETYPE ResType)
{
	//
	// Useful for querying mem availability
	//
	MEMORYSTATUS MemStatus;

	//
	// Resource creation specifications for this test
	//
	CREATEINDEXBUFFER_ARGS CreateArgsIB;
	CREATEVERTEXBUFFER_ARGS CreateArgsVB;
	CREATETEXTURE_ARGS CreateArgsTex;
	SIZE_T EstimatedSize;
	UINT uiResourceCount;

	//
	// Array of interface pointers; array length depends on factors computed
	// at runtime
	//
	LPDIRECT3DMOBILERESOURCE *ppInterfaces = NULL;

	//
	// Iterator for manipulating interface array
	//
	UINT uiResourceIter;

	//
	// Storage for intermediate priority computations
	//
	float fCurrentPriority;

	//
	// Test result
	//
	INT Result = TPR_PASS;

	//
	// Format that is valid for texture creation
	//
	D3DMFORMAT TexFormat;

	//
	// Number of bytes to create in resources
	//
	SIZE_T NumResourceBytes;

	//
	// This test is only useful if the managed pool is supported
	//
	if (FAILED(SupportsManagedPool(pDevice)))
	{
		OutputDebugString(_T("SupportsManagedPool failed."));
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Screen for validity of resource type argument
	//
	switch(ResType)
	{
	case D3DMRTYPE_VERTEXBUFFER:
	case D3DMRTYPE_INDEXBUFFER:
		//
		// Device must support VB, IB
		//
		break;

	case D3DMRTYPE_SURFACE:
		//
		// CreateImageSurface does not take a pool argument
		//
		Result = TPR_SKIP;
		goto cleanup;

	case D3DMRTYPE_TEXTURE:
		//
		// Textures can be tested with the managed pool only
		// if the device supports textures.
		//
		
		if (FAILED(SupportsTextures(pDevice)))
		{
			Result = TPR_SKIP;
			goto cleanup;
		}

		// 
		// Find a supported format for textures
		//
		if (FAILED(GetValidTextureFormat( pDevice,      // LPDIRECT3DMOBILEDEVICE pDevice,
		                                  &TexFormat))) // D3DMFORMAT *pFormat
		{
			OutputDebugString(_T("GetValidTextureFormat failed."));
			Result = TPR_FAIL;
			goto cleanup;
		}
		break;

	default:
		OutputDebugString(_T("Specified resource type is invalid for this test case."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Retrieve available memory
	//
	ZeroMemory(&MemStatus, sizeof(MEMORYSTATUS));
	MemStatus.dwLength = sizeof(MEMORYSTATUS);
	GlobalMemoryStatus(&MemStatus);

	//
	// Limit resource creation to avoid consuming all available memory
	//
	NumResourceBytes = MemStatus.dwAvailPhys - D3DQA_NUM_RESERVED_BYTES;


	//
	// Create resources of specified type
	//
	switch(ResType)
	{
	case D3DMRTYPE_VERTEXBUFFER:

		//
		// Obtain valid arguments for resource creation call
		//
		if (FAILED(GetValidVertexBufferArgs(pDevice,              // LPDIRECT3DMOBILEDEVICE pDevice
		                                   &CreateArgsVB,         // CREATETEXTURE_ARGS *pCreateTexArgs
		                                   &EstimatedSize)))      // SIZE_T *pEstimatedSize
		{
			OutputDebugString(_T("GetValidVertexBufferArgs failed."));
			Result = TPR_SKIP;
			goto cleanup;
		}

		//
		// Compute desired resource count
		//
		uiResourceCount = (UINT)(NumResourceBytes / EstimatedSize);

		//
		// Nearly fill system memory with resources
		//
		if (FAILED(CreateVertexBufferResources(pDevice,                    // LPDIRECT3DMOBILEDEVICE pDevice
		                                       uiResourceCount,            // UINT uiResourceCount
		                                       &CreateArgsVB,              // CREATEVERTEXBUFFER_ARGS *pCreateVertexBufferArgs
		                                       &ppInterfaces)))            // IDirect3DMobileResource ***pppInterfaces
		{
			OutputDebugString(_T("CreateVertexBufferResources failed."));
			Result = TPR_SKIP;
			goto cleanup;
		}
		
		break;
	case D3DMRTYPE_INDEXBUFFER:

		//
		// Obtain valid arguments for resource creation call
		//
		if (FAILED(GetValidIndexBufferArgs(pDevice,          // LPDIRECT3DMOBILEDEVICE pDevice
		                                   &CreateArgsIB,    // CREATETEXTURE_ARGS *pCreateTexArgs
		                                   &EstimatedSize))) // SIZE_T *pEstimatedSize
		{
			OutputDebugString(_T("GetValidIndexBufferArgs failed."));
			Result = TPR_SKIP;
			goto cleanup;
		}

		//
		// Compute desired resource count
		//
		uiResourceCount = (UINT)(NumResourceBytes / EstimatedSize);

		//
		// Nearly fill system memory with resources
		//
		if (FAILED(CreateIndexBufferResources(pDevice,                    // LPDIRECT3DMOBILEDEVICE pDevice
		                                      uiResourceCount,            // UINT uiResourceCount
		                                      &CreateArgsIB,              // CREATEINDEXBUFFER_ARGS *pCreateIndexBufferArgs
		                                      &ppInterfaces)))            // IDirect3DMobileResource ***pppInterfaces
		{
			OutputDebugString(_T("CreateIndexBufferResources failed."));
			Result = TPR_SKIP;
			goto cleanup;
		}
		break;
	case D3DMRTYPE_TEXTURE:

		//
		// Obtain valid arguments for resource creation call
		//
		if (FAILED(GetValidTextureArgs(pDevice,          // LPDIRECT3DMOBILEDEVICE pDevice
		                               &CreateArgsTex,   // CREATETEXTURE_ARGS *pCreateTexArgs
		                               &EstimatedSize))) // SIZE_T *pEstimatedSize
		{
			OutputDebugString(_T("GetValidTextureArgs failed."));
			Result = TPR_SKIP;
			goto cleanup;
		}

		//
		// Compute desired resource count
		//
		uiResourceCount = (UINT)(NumResourceBytes / EstimatedSize);

		//
		// Nearly fill system memory with resources
		//
		if (FAILED(CreateTextureResources(pDevice,                    // LPDIRECT3DMOBILEDEVICE pDevice
		                                  uiResourceCount,            // UINT uiResourceCount
		                                  &CreateArgsTex,             // CREATETEXTURE_ARGS *pCreateTextureArgs
		                                  &ppInterfaces)))            // IDirect3DMobileResource ***pppInterfaces
		{
			OutputDebugString(_T("CreateTextureResources failed."));
			Result = TPR_SKIP;
			goto cleanup;
		}
		break;
	}

	//
	// System memory should be nearly filled with resources at this point.
	//

	//
	// Attach priorities to each resource; with many identical priorities.  General rule about priorities:
	//
	//   * "A resource assigned a low priority is removed before a resource with a high priority."
	//
	// This loop assigns priorities in an order of increasing importance
	//
	for (uiResourceIter = 0; uiResourceIter < uiResourceCount; uiResourceIter++)
	{
		fCurrentPriority = ((float)uiResourceIter / (float)(uiResourceCount-1)); // Operation range [0.0f, 1.0f]
		fCurrentPriority *= (float)(D3DQA_PRIORITY_MAX - D3DQA_PRIORITY_MIN);   // Scale
		fCurrentPriority += D3DQA_PRIORITY_MIN;                                 // Offset

		ppInterfaces[uiResourceIter]->SetPriority((DWORD)fCurrentPriority);      // DWORD PriorityNew
	}
	
	//
	// Preload resources in a manner that is likely to force texture eviction due to priority
	//
	for (uiResourceIter = 0; uiResourceIter < uiResourceCount; uiResourceIter++)
	{
		ppInterfaces[uiResourceIter]->PreLoad();
	}

cleanup:

	if (ppInterfaces)
	{
		for (uiResourceIter = 0; uiResourceIter < uiResourceCount; uiResourceIter++)
		{
			ppInterfaces[uiResourceIter]->Release();
		}
		free(ppInterfaces);
	}

	return Result;
}

TESTPROCAPI ResManTest(LPVERIFTESTCASEARGS pTestCaseArgs)
{
	OutputDebugString(_T("Beginning ResManTest."));

	//
	// Index into table of permutations
	//
	DWORD dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_RESMANTEST_BASE;

	switch(dwTableIndex)
	{
	case 0:
		return ResourceManagerEvictionTest(pTestCaseArgs->pDevice,  // LPDIRECT3DMOBILEDEVICE pDevice,
		                                   D3DMRTYPE_VERTEXBUFFER); // D3DMRESOURCETYPE ResType)
	case 1:
		return ResourceManagerEvictionTest(pTestCaseArgs->pDevice,  // LPDIRECT3DMOBILEDEVICE pDevice,
		                                   D3DMRTYPE_INDEXBUFFER);  // D3DMRESOURCETYPE ResType)
	case 2:
		return ResourceManagerEvictionTest(pTestCaseArgs->pDevice,  // LPDIRECT3DMOBILEDEVICE pDevice,
		                                   D3DMRTYPE_TEXTURE);      // D3DMRESOURCETYPE ResType)
	default:
		OutputDebugString(_T("Unknown test index."));
		return TPR_ABORT;
	}

}
