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
//
// Subband Codec - auxiliary definitions
//

#ifndef _UTILITY_HXX_
#define _UTILITY_HXX_

// minimum of arguments
template<class T> inline T min(const T &a, const T &b) {
	return (a < b) ? a : b;
}

// maximum of arguments
template<class T>
inline T max(const T &a, const T &b) {
	return (a < b) ? b : a;
}
// maximum of arguments
template<class T>
inline T max3(const T &a, const T &b,const T &c) {
	return ((a > b) ? ((a > c) ? a : c) : ((b > c) ? b : c));
}
// absolute value of arguments
template<class T>
inline T abs(const T &a) {
	return max(a, -a);
}

#endif
