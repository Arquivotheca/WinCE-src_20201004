//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#pragma once

#include <windows.h>
#include <TchStream.h>

// Pass an array of TCH_INPUT_SAMPLE_SET structures in the input buffer and 
// the size of the array in bytes in the input size.
// They will be passed up the touch driver stack.
#define IOCTL_TOUCH_STUB_SEND_TOUCH_SAMPLE_SETS         CTL_CODE(FILE_DEVICE_TOUCH, 2041, METHOD_BUFFERED, FILE_ANY_ACCESS)

