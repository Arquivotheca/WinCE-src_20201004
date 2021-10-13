//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once

typedef enum _D3DQA_PURPOSE
{
	D3DQA_PURPOSE_CLIP_STATUS_TEST = 1,	
	D3DQA_PURPOSE_WORLD_VIEW_TEST,	
	D3DQA_PURPOSE_TEXTURE_TEST,
	D3DQA_PURPOSE_FULLPIPE_TEST,
	D3DQA_PURPOSE_DEPTH_STENCIL_TEST,
	D3DQA_PURPOSE_SMALL_TOPMOST,
	D3DQA_PURPOSE_RAW_TEST
} D3DQA_PURPOSE;
