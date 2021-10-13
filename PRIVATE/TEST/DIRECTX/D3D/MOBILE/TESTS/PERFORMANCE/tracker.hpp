//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
