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
#include <windows.h>
#include <d3dm.h>
#include "TestCases.h"
#include "MultiInstTestHelper.h"

MultiInstOptions MultiInstCases[D3DMQA_MULTIINSTTEST_COUNT] = {
//  FVF Format                 | Shade Mode       | VertexAlpha | TableFog | VertexFog | InstanceCount | UseLocalInstances
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, FALSE,        FALSE,     FALSE,      2,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    FALSE,        FALSE,     FALSE,      2,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, FALSE,        FALSE,     FALSE,      2,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    FALSE,        FALSE,     FALSE,      2,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, TRUE,         FALSE,     FALSE,      2,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    TRUE,         FALSE,     FALSE,      2,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, TRUE,         FALSE,     FALSE,      2,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    TRUE,         FALSE,     FALSE,      2,              FALSE,
    
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, FALSE,        TRUE,      FALSE,      2,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    FALSE,        TRUE,      FALSE,      2,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, FALSE,        TRUE,      FALSE,      2,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    FALSE,        TRUE,      FALSE,      2,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, TRUE,         TRUE,      FALSE,      2,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    TRUE,         TRUE,      FALSE,      2,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, TRUE,         TRUE,      FALSE,      2,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    TRUE,         TRUE,      FALSE,      2,              FALSE,
    
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, FALSE,        FALSE,     TRUE,       2,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    FALSE,        FALSE,     TRUE,       2,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, FALSE,        FALSE,     TRUE,       2,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    FALSE,        FALSE,     TRUE,       2,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, TRUE,         FALSE,     TRUE,       2,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    TRUE,         FALSE,     TRUE,       2,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, TRUE,         FALSE,     TRUE,       2,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    TRUE,         FALSE,     TRUE,       2,              FALSE,

    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, FALSE,        FALSE,     FALSE,      3,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    FALSE,        FALSE,     FALSE,      3,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, FALSE,        FALSE,     FALSE,      3,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    FALSE,        FALSE,     FALSE,      3,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, TRUE,         FALSE,     FALSE,      3,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    TRUE,         FALSE,     FALSE,      3,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, TRUE,         FALSE,     FALSE,      3,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    TRUE,         FALSE,     FALSE,      3,              FALSE,

    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, FALSE,        TRUE,      FALSE,      3,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    FALSE,        TRUE,      FALSE,      3,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, FALSE,        TRUE,      FALSE,      3,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    FALSE,        TRUE,      FALSE,      3,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, TRUE,         TRUE,      FALSE,      3,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    TRUE,         TRUE,      FALSE,      3,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, TRUE,         TRUE,      FALSE,      3,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    TRUE,         TRUE,      FALSE,      3,              FALSE,

    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, FALSE,        FALSE,     TRUE,       3,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    FALSE,        FALSE,     TRUE,       3,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, FALSE,        FALSE,     TRUE,       3,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    FALSE,        FALSE,     TRUE,       3,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, TRUE,         FALSE,     TRUE,       3,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    TRUE,         FALSE,     TRUE,       3,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, TRUE,         FALSE,     TRUE,       3,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    TRUE,         FALSE,     TRUE,       3,              FALSE,
    
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, FALSE,        FALSE,     FALSE,      5,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    FALSE,        FALSE,     FALSE,      5,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, FALSE,        FALSE,     FALSE,      5,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    FALSE,        FALSE,     FALSE,      5,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, TRUE,         FALSE,     FALSE,      5,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    TRUE,         FALSE,     FALSE,      5,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, TRUE,         FALSE,     FALSE,      5,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    TRUE,         FALSE,     FALSE,      5,              FALSE,

    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, FALSE,        TRUE,      FALSE,      5,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    FALSE,        TRUE,      FALSE,      5,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, FALSE,        TRUE,      FALSE,      5,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    FALSE,        TRUE,      FALSE,      5,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, TRUE,         TRUE,      FALSE,      5,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    TRUE,         TRUE,      FALSE,      5,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, TRUE,         TRUE,      FALSE,      5,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    TRUE,         TRUE,      FALSE,      5,              FALSE,

    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, FALSE,        FALSE,     TRUE,       5,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    FALSE,        FALSE,     TRUE,       5,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, FALSE,        FALSE,     TRUE,       5,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    FALSE,        FALSE,     TRUE,       5,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, TRUE,         FALSE,     TRUE,       5,              FALSE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    TRUE,         FALSE,     TRUE,       5,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, TRUE,         FALSE,     TRUE,       5,              FALSE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    TRUE,         FALSE,     TRUE,       5,              FALSE,

    
//  FVF Format                 | Shade Mode       | VertexAlpha | TableFog | VertexFog | InstanceCount | UseLocalInstances
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, FALSE,        FALSE,     FALSE,      2,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    FALSE,        FALSE,     FALSE,      2,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, FALSE,        FALSE,     FALSE,      2,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    FALSE,        FALSE,     FALSE,      2,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, TRUE,         FALSE,     FALSE,      2,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    TRUE,         FALSE,     FALSE,      2,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, TRUE,         FALSE,     FALSE,      2,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    TRUE,         FALSE,     FALSE,      2,              TRUE,
    
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, FALSE,        TRUE,      FALSE,      2,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    FALSE,        TRUE,      FALSE,      2,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, FALSE,        TRUE,      FALSE,      2,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    FALSE,        TRUE,      FALSE,      2,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, TRUE,         TRUE,      FALSE,      2,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    TRUE,         TRUE,      FALSE,      2,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, TRUE,         TRUE,      FALSE,      2,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    TRUE,         TRUE,      FALSE,      2,              TRUE,
    
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, FALSE,        FALSE,     TRUE,       2,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    FALSE,        FALSE,     TRUE,       2,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, FALSE,        FALSE,     TRUE,       2,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    FALSE,        FALSE,     TRUE,       2,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, TRUE,         FALSE,     TRUE,       2,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    TRUE,         FALSE,     TRUE,       2,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, TRUE,         FALSE,     TRUE,       2,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    TRUE,         FALSE,     TRUE,       2,              TRUE,

    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, FALSE,        FALSE,     FALSE,      3,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    FALSE,        FALSE,     FALSE,      3,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, FALSE,        FALSE,     FALSE,      3,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    FALSE,        FALSE,     FALSE,      3,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, TRUE,         FALSE,     FALSE,      3,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    TRUE,         FALSE,     FALSE,      3,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, TRUE,         FALSE,     FALSE,      3,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    TRUE,         FALSE,     FALSE,      3,              TRUE,

    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, FALSE,        TRUE,      FALSE,      3,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    FALSE,        TRUE,      FALSE,      3,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, FALSE,        TRUE,      FALSE,      3,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    FALSE,        TRUE,      FALSE,      3,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, TRUE,         TRUE,      FALSE,      3,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    TRUE,         TRUE,      FALSE,      3,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, TRUE,         TRUE,      FALSE,      3,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    TRUE,         TRUE,      FALSE,      3,              TRUE,

    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, FALSE,        FALSE,     TRUE,       3,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    FALSE,        FALSE,     TRUE,       3,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, FALSE,        FALSE,     TRUE,       3,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    FALSE,        FALSE,     TRUE,       3,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, TRUE,         FALSE,     TRUE,       3,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    TRUE,         FALSE,     TRUE,       3,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, TRUE,         FALSE,     TRUE,       3,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    TRUE,         FALSE,     TRUE,       3,              TRUE,
    
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, FALSE,        FALSE,     FALSE,      5,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    FALSE,        FALSE,     FALSE,      5,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, FALSE,        FALSE,     FALSE,      5,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    FALSE,        FALSE,     FALSE,      5,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, TRUE,         FALSE,     FALSE,      5,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    TRUE,         FALSE,     FALSE,      5,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, TRUE,         FALSE,     FALSE,      5,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    TRUE,         FALSE,     FALSE,      5,              TRUE,

    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, FALSE,        TRUE,      FALSE,      5,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    FALSE,        TRUE,      FALSE,      5,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, FALSE,        TRUE,      FALSE,      5,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    FALSE,        TRUE,      FALSE,      5,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, TRUE,         TRUE,      FALSE,      5,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    TRUE,         TRUE,      FALSE,      5,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, TRUE,         TRUE,      FALSE,      5,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    TRUE,         TRUE,      FALSE,      5,              TRUE,

    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, FALSE,        FALSE,     TRUE,       5,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    FALSE,        FALSE,     TRUE,       5,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, FALSE,        FALSE,     TRUE,       5,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    FALSE,        FALSE,     TRUE,       5,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_GOURAUD, TRUE,         FALSE,     TRUE,       5,              TRUE,
    D3DMMULTIINSTTEST_FVF_NOTEX, D3DMSHADE_FLAT,    TRUE,         FALSE,     TRUE,       5,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_GOURAUD, TRUE,         FALSE,     TRUE,       5,              TRUE,
    D3DMMULTIINSTTEST_FVF_TEX,   D3DMSHADE_FLAT,    TRUE,         FALSE,     TRUE,       5,              TRUE,
};
