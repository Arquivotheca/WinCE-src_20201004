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

    tracker.cpp

Abstract:

    CTracker implementation

-------------------------------------------------------------------*/

// ++++ Include Files +++++++++++++++++++++++++++++++++++++++++++++++
#include "Optimal.hpp"

CTracker *g_ptrackerStrip = NULL;

void CTracker::Randomize(float rX, float rY, float rZ)
{
    // Randomize translation and rotation info
    m_matRotAndTrans = m_matOrientation =  g_matIdent;
    m_matOrientation._41 = D3DM_MAKE_D3DMVALUE(rX);
    m_matOrientation._42 = D3DM_MAKE_D3DMVALUE(rY);
    m_matOrientation._43 = D3DM_MAKE_D3DMVALUE(rZ);

    m_rXRot = (-1.0f + (float)(rand()%100)/50.0f)/4.0f;
    m_rYRot = (-1.0f + (float)(rand()%100)/50.0f)/4.0f;
    m_rZRot = (-1.0f + (float)(rand()%100)/50.0f)/4.0f;

    m_rXTrans = (-1.0f + (float)(rand()%100)/50.0f)/4.0f;
    m_rYTrans = (-1.0f + (float)(rand()%100)/50.0f)/4.0f;

    SetMatrix();
}

void CTracker::SetMatrix()
{
    D3DMMATRIX matRotate;

    D3DMVECTOR vAxisX = {D3DM_MAKE_D3DMVALUE(1.0f), 
                        D3DM_MAKE_D3DMVALUE(0.0f), 
                        D3DM_MAKE_D3DMVALUE(0.0f)};
    D3DMVECTOR vAxisY = {D3DM_MAKE_D3DMVALUE(0.0f), 
                        D3DM_MAKE_D3DMVALUE(1.0f), 
                        D3DM_MAKE_D3DMVALUE(0.0f)};
    D3DMVECTOR vAxisZ = {D3DM_MAKE_D3DMVALUE(0.0f), 
                        D3DM_MAKE_D3DMVALUE(0.0f), 
                        D3DM_MAKE_D3DMVALUE(1.0f)};

    SetMatrixAxisRotation(&matRotate, &vAxisX, m_rXRot);
    m_matRotAndTrans = matRotate;
    SetMatrixAxisRotation(&matRotate, &vAxisY, m_rYRot);
    m_matRotAndTrans = MatrixMult(&m_matRotAndTrans, &matRotate);
    SetMatrixAxisRotation(&matRotate, &vAxisZ, m_rZRot);
    m_matRotAndTrans = MatrixMult(&m_matRotAndTrans, &matRotate);
}

void CTracker::Move()
{
    if (g_fRotating)
        m_matOrientation = MatrixMult(&m_matOrientation, &m_matRotAndTrans);
    g_p3ddevice->SetTransform(D3DMTS_WORLD, &m_matOrientation, D3DMFMT_D3DMVALUE_FLOAT);
}

