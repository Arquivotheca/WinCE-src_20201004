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
