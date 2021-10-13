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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
// Check to see if bot _TUX_SUITE and _STRESS_SUITE are defined 
#if defined(_STRESS_SUITE) && defined(_TUX_SUITE)
#error _TUX_SUITE and _STRESS_SUITE can not be set at the same time
#endif //_STRESS_SUITE


#ifdef _TUX_SUITE

// Include TUX specific logging macros
#include "logging_tux.h"
#else 

#ifdef _STRESS_SUITE
// Include STRESS specific logging macros
#include "logging_stress.h"
#else
#include "logging_Default.h"
#endif 	//_STRESS_SUTE

#endif	//_TUX_SUITE
