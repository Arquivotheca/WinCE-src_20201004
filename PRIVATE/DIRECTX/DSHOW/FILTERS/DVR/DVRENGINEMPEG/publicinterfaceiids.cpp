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
#include "stdafx.h"
#include "PipelineInterfaces.h"
#include "DvrInterfaces.h"
#include "InitGuid.h"

using namespace MSDvr;

DEFINE_GUID(IID_IStreamBufferCapture,
    0x34DB9BD0, 0xF185, 0x4334, 0xA2, 0xD7, 0xCD, 0x46, 0x0B, 0x95, 0x0B, 0xDB);

// {50845279-8C98-4e1f-A16A-8C48CE35EC12}
DEFINE_GUID(IID_IStreamBufferPlayback,
0x50845279, 0x8c98, 0x4e1f, 0xa1, 0x6a, 0x8c, 0x48, 0xce, 0x35, 0xec, 0x12);

// (dc61c293-4f18-44d2-a5ed-d4fdac777987)
DEFINE_GUID(IID_IContentRestrictionCapture,
0xdc61c293, 0x4f18, 0x44d2, 0xa5, 0xed, 0xd4, 0xfd, 0xac, 0x77, 0x79, 0x87);

DEFINE_GUID(IID_IDVREngineHelpers,
	0x88F784A7, 0xE9D0, 0x4178, 0xAB, 0x99, 0xA6, 0xC0, 0x26, 0x7D, 0x0D, 0x8F);

// {AF650715-D477-41cd-BE46-30B2BC76E94B}
DEFINE_GUID(IID_ISampleProducer,
    0xaf650715, 0xd477, 0x41cd, 0xbe, 0x46, 0x30, 0xb2, 0xbc, 0x76, 0xe9, 0x4b);

// {96772F01-0767-4fb7-8341-A5497044761F}
DEFINE_GUID(IID_ISampleConsumer,
    0x96772f01, 0x767, 0x4fb7, 0x83, 0x41, 0xa5, 0x49, 0x70, 0x44, 0x76, 0x1f);

// {9EAAB067-68FB-424e-A476-6D76DF6BFDDF}
DEFINE_GUID(IID_IDecoderDriver,
	0x9EAAB067, 0x68FB, 0x424e, 0xA4, 0x76, 0x6D, 0x76, 0xDF, 0x6B, 0xFD, 0xDF);

// {88B94F99-0B04-4c28-8BE8-1678556C15C7}
DEFINE_GUID(IID_IPauseBufferMgr,
    0x88b94f99, 0xb04, 0x4c28, 0x8b, 0xe8, 0x16, 0x78, 0x55, 0x6c, 0x15, 0xc7);

// {4A480BF3-39AF-4802-8784-C05261CC907C}
DEFINE_GUID(IID_IReader,
0x4a480bf3, 0x39af, 0x4802, 0x87, 0x84, 0xc0, 0x52, 0x61, 0xcc, 0x90, 0x7c);

// {150F49F7-6206-4a41-AA71-BCF91DA8B25D}
DEFINE_GUID(IID_IWriter,
    0x150f49f7, 0x6206, 0x4a41, 0xaa, 0x71, 0xbc, 0xf9, 0x1d, 0xa8, 0xb2, 0x5d);
