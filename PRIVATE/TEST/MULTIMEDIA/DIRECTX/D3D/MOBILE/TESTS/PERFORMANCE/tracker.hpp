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
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


Module Name:

    Tracker.cxx

Abstract:

    CTracker implementation

-------------------------------------------------------------------*/

class CTracker {
public:
	float m_rXTrans, m_rYTrans, m_rZTrans, m_rXRot, m_rYRot, m_rZRot;
	D3DMMATRIX m_matRotAndTrans, m_matOrientation;
	void Move();
	void Randomize(float rX, float rY, float rZ);
	void SetMatrix();
};
