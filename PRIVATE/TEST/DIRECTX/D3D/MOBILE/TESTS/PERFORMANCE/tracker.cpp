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

