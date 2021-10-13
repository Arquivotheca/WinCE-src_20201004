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

    Optimal.cpp

Abstract:

    This sample demonstrates how to maximum polygon throughput on
    Dreamcast using the Dreamcast sdk.  It creates a set of spheres, each of 
    which is a single triangle strip, and rotates them on screen.  Each sphere
    uses a separate vertex pool, and the world matrix is changed between
    each sphere.  Each sphere is also lit by a single directional light
    source.  These demonstrate that the sample does basically the
    same thing as a game, and doesn't "cheat".

    There are several command line parameters created to allow testing
    other, less-optimal cases, such as using Triangle Lists or non-indexed
    strips.  Run 'Optimal -?' to see these command line params

    Note that although the vertex pools are unique to each strip, a
    single index list is used for all of the strips.  This has no effect
    on performance, and is simply done for clarity of the sample.

-------------------------------------------------------------------*/

// ++++ Include Files +++++++++++++++++++++++++++++++++++++++++++++++
#include "Optimal.hpp"
#ifdef TUXDLL
#include <tux.h>
#include <kato.h>
#include <katoex.h>
#include "ft.h"
#endif

// ++++ Global Variables ++++++++++++++++++++++++++++++++++++++++++++
HWND   g_hwndApp;                                 // HWND of the application
HANDLE g_hInstance = NULL;


#ifdef TUXDLL
CKato    *g_pKato;
TESTPROCAPI getCode(void);
#else
__inline int getCode(void) {return 0;};
#endif


// ++++ Local Variables +++++++++++++++++++++++++++++++++++++++++++++
TCHAR     g_tszAppName[] = TEXT("Optimal");          // The App's Name
LPDIRECT3DMOBILEVERTEXBUFFER *g_ppVerts = NULL;                               // List of strip vertices
LPDIRECT3DMOBILEVERTEXBUFFER *g_ppVerts2 = NULL;                              // List of strip vertices (Used for DrawPrimitive)
LPDIRECT3DMOBILEVERTEXBUFFER *g_ppVertsTransformed = NULL;
D3DLVERTEX **g_ppLVerts;                             // List of strip vertices
D3DLVERTEX **g_ppLVerts2;                            // List of strip vertices (Used for DrawPrimitive)
LPDIRECT3DMOBILEINDEXBUFFER  g_pIndices;                               // List of strip indices
int       g_nVerts, g_nIndices;                      // Number of strip indices and vertices
int       g_nPolys;                                  // Number of polygons per strip
LPDIRECT3DMOBILETEXTURE g_pTexture[10] = { NULL };        // Pointer to the strip's texture
D3DMPOOL g_memorypool = D3DMPOOL_SYSTEMMEM;


BOOL g_fIndexedPrims = false;    // Set to FALSE to use DrawPrimitive, TRUE for DrawIndexedPrimitive
BOOL g_fTriStrips    = true;     // Set to FALSE to use TriLists, TRUE for TriStrips
BOOL g_fPreLit       = false;    // Set to FALSE to use D3DVERTEX, TRUE for D3DLVERTEX
int  g_nUserLights   = 1;        // 0 == Prelit.
int  g_nEndFrame     = 200;       // -1 == 'infinite' (ie no end)
int  g_nOutputFrame  = 300;      // Output info every 300 frames.
BOOL g_fDone         = false;
BOOL g_fTextureMap   = true;     // TRUE == user texture mapping.
BOOL g_fRasterize    = true;
BOOL g_fTransformed  = false;    // Set to true to use ProcessVertices to transform.
BOOL g_fRotating     = true;     // true == spheres are rotating.

// These are the number of quads that appear in eachdimension; ie a width of 16 means
// that there are 32 tris in that dimension.
// Also: StripWidth and height MUST be evenly divisible by two!
int  g_nStripWidth   = 20;
int  g_nStripHeight  = 20;
int  g_nStrips       = 20;

static int g_cFramesDrawn = 0;

bool    fDataXfer = false;

// ++++ Local Functions +++++++++++++++++++++++++++++++++++++++++++++
static BOOL RenderSceneToBackBuffer();
static BOOL AppInit(HINSTANCE hPrev,int nCmdShow);
static BOOL ParseCommandLine(LPTSTR lpCmdLine);
static void DumpInfo(bool fReset = false);

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    CleanUp

Description:

    Cleans up after the application is done.  Frees allocated memory

Arguments:

    None

Return Value:

    None

-------------------------------------------------------------------*/
void
CleanUp()
{
    // Release the textures
    for (int i = 0; i < countof(g_pTexture); i++)
    {
        if (NULL != g_pTexture[i])
        {
            g_pTexture[i]->Release();
            g_pTexture[i] = NULL;
        }
    }
            

    // Release the strips
    if (NULL != g_ppVerts)
    {
        for (i = 0; i < g_nStrips; i++)
            if (g_ppVerts[i]) 
                g_ppVerts[i]->Release();

        delete[] g_ppVerts;
        g_ppVerts = NULL;
    }

    if (NULL != g_ppVerts2)
    {
        for (i = 0; i < g_nStrips; i++)
            if (g_ppVerts2[i]) 
                g_ppVerts2[i]->Release();

        delete[] g_ppVerts2;
        g_ppVerts2 = NULL;
    }

    if (NULL != g_ppVertsTransformed)
    {
        for (i = 0; i < g_nStrips; i++)
            if (g_ppVertsTransformed[i]) 
                g_ppVertsTransformed[i]->Release();

        delete[] g_ppVertsTransformed;
        g_ppVertsTransformed = NULL;
    }

    if (NULL != g_pIndices)
    {
        g_pIndices->Release();
        g_pIndices = NULL;
    }

    // Release the 3D Device
    if (g_p3ddevice)
    {
        g_p3ddevice->Release();
        g_p3ddevice = NULL;
    }

    // Release the D3D object
    if (g_pd3d)
    {
        g_pd3d->Release();
        g_pd3d = NULL;
    }

    if (g_hSWDeviceDLL)
    {
        FreeLibrary(g_hSWDeviceDLL);
        g_hSWDeviceDLL = NULL;
    }

    // Deallocate the strip movement tracker
    if (g_ptrackerStrip)
    {
        delete g_ptrackerStrip;
        g_ptrackerStrip = NULL;
    }
 }

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    WndProc

Description:

    Window message processing routine for the main application window.
    The main purpose of this function is to exit the app when the user
    presses <Escape> or <F12>.

Arguments:

    HWND hWnd           - Window handle

    UINT uMessage       - Message identifier

    WPARAM wParam       - First message parameter

    LPARAM lParam       - Second message parameter

Return Value:

    Zero if the message was processed.  The return value from
    DefWindowProc otherwise.

-------------------------------------------------------------------*/
LRESULT CALLBACK
WndProc(HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
    switch (uMessage)
    {
    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_ESCAPE:
        case VK_F12:
            DestroyWindow(hWnd);
            return 0L;
        } // switch (wParam)
        break;

    case WM_DESTROY:
        if (!g_fDone)
        {
            g_fDone = true;
            DumpInfo();
        }

        // Cleanup DirectX structures
        CleanUp();

        PostQuitMessage(0);
        return 0L;

    } // switch (message)

    return DefWindowProc (hWnd, uMessage, wParam, lParam);
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    CreateStrip

Description:

    Initializes the vertices and indices of the strip that will be
    drawn to the screen.

Arguments:

    None.

Return Value:

    None.

-------------------------------------------------------------------*/
template <class _VERTEX>
void
CreateStrip()
{
    int i, j, iCurStrip, iCurIndex;

    // Allocate space for the list of strips
    g_ppVerts = new LPDIRECT3DMOBILEVERTEXBUFFER[g_nStrips];
    if (!g_ppVerts)
    {
        Output(LOG_ABORT, _T("Out of memory creating vertex buffer pointer array"));
        return;
    }
    memset(g_ppVerts, 0x00, sizeof(LPDIRECT3DMOBILEVERTEXBUFFER) * g_nStrips);

    // Seed the random number initializer with a known value for consistent results
    srand(0);

    for (iCurStrip = 0; iCurStrip < g_nStrips; iCurStrip++)
    {
        // Create the strip vertices.
        g_nVerts = (g_nStripWidth + 1) * (g_nStripHeight + 1);

        g_errLast = g_p3ddevice->CreateVertexBuffer(
            ((sizeof(_VERTEX) == sizeof(D3DLVERTEX)) ? sizeof(LIT_TEXTURED_VERTEX_TYPE) : sizeof(UNLIT_TEXTURED_VERTEX_TYPE)) * g_nVerts, 
            D3DMUSAGE_LOCKABLE, 
            (sizeof(_VERTEX) == sizeof(D3DLVERTEX)) ? LIT_TEXTURED_VERTEX_FVF : UNLIT_TEXTURED_VERTEX_FVF,
            g_memorypool,
            &(g_ppVerts[iCurStrip]));

        _VERTEX *pVertList;
        g_errLast = g_ppVerts[iCurStrip]->Lock(0, 0, (void**)&pVertList, 0);
        float rXOffset, rYOffset, rZOffset;

        // We want to offset each strip to use maximum screen real estate
        rXOffset = -4.0f + (float)(rand()%100)/12.5f;
        rYOffset = -4.0f + (float)(rand()%100)/12.5f;
        rZOffset = -1.0f + (float)(rand()%100)/50.0f;

        // Set the movement tracker for this strip.
        g_ptrackerStrip[iCurStrip].Randomize(rXOffset, rYOffset, rZOffset);

        // Create the strip.
        for (j = 0; j <= g_nStripHeight; j++)
            {
            for (i = 0; i <= g_nStripWidth; i++)
            {
                int nVert = j * (g_nStripWidth + 1) + i;
                _VERTEX *pVert = &pVertList[nVert];

                float rX  = (float)i / (float)(g_nStripWidth);
                float rY  = (float)j / (float)(g_nStripHeight);
                float rXP = rX * 2.0f * PI;
                float rYP = rY * 2.0f * PI;

                pVert->x  = (float)(sin(rXP) * sin(rYP));
                pVert->y  = (float)(cos(rYP));
                pVert->z  = (float)(cos(rXP) * sin(rYP));

                if (sizeof(_VERTEX) == sizeof(D3DLVERTEX))
                {
                    // Prelit vertices - For now, we'll just drop a color in there.  Eventually
                    // this could be rewritten to fake a light source if so desired.
                    D3DLVERTEX *plvert = (D3DLVERTEX*)pVert;
                    plvert->color = 0xFF00FF00;     // Green
                    plvert->specular = 0x00000000;
                }
                else
                {
                    // Not prelit; use normals
                    D3DVERTEX *pulvert = (D3DVERTEX*)pVert;
                    pulvert->nx = pulvert->x;
                    pulvert->ny = pulvert->y;
                    pulvert->nz = pulvert->z;
                    // Normalize the normals of the vertex
                    NormalizeVector((D3DMVECTOR*)(&pulvert->nx));
                }

                pVert->tu = (float)i / (float)(g_nStripWidth) * 4.0f;
                pVert->tv = (float)j / (float)(g_nStripHeight) * 4.0f;

            }
        }
        g_errLast = g_ppVerts[iCurStrip]->Unlock();
    }

    // Create the strip indices.  These are reused for all strips.
    int iIndex = 0;
    WORD * pIndices = NULL;

    if (g_fTriStrips)
    {
        int nIndexWidth = (g_nStripWidth + 1) * 2;

        g_nIndices = (nIndexWidth+1) * g_nStripHeight;

        g_errLast = g_p3ddevice->CreateIndexBuffer(2 * g_nIndices, 0, D3DMFMT_INDEX16, g_memorypool, &g_pIndices);
        g_errLast = g_pIndices->Lock(0, 2 * g_nIndices, (void**)&pIndices, 0);
        for (j = 0; j < g_nStripHeight/2; j++)
        {
            for (i=nIndexWidth/2; i < nIndexWidth; i++)
            {
                pIndices[iIndex++] = i + j * nIndexWidth;
                pIndices[iIndex++] = i - (nIndexWidth/2) + j * nIndexWidth;
            }

            pIndices[iIndex++] = nIndexWidth * (j + 1) - 1;
            pIndices[iIndex++] = nIndexWidth * (j + 1) - 1;

            for (i = nIndexWidth - 1; i >= (nIndexWidth/2); i--)
            {
                pIndices[iIndex++] = i + j * nIndexWidth;
                pIndices[iIndex++] = i - (nIndexWidth / 2) + (j + 1) * nIndexWidth;
            }
        }
        g_errLast = g_pIndices->Unlock();
        // Calculate the number of polygons in the strip
        g_nPolys = (g_nIndices * 2 / g_nStripHeight - 3) * g_nStripHeight / 2;
    }
    else
    {
        // Triangle lists
        g_nIndices = g_nStripWidth * g_nStripHeight * 6;

        g_errLast = g_p3ddevice->CreateIndexBuffer(2 * g_nIndices, 0, D3DMFMT_INDEX16, g_memorypool, &g_pIndices);
        g_errLast = g_pIndices->Lock(0, 2 * g_nIndices, (void**)&pIndices, 0);

        for (j = 0; j < g_nStripHeight; j++)
        {
            for (i = 0; i < g_nStripWidth; i++)
            {
                pIndices[iIndex++] =  i      + (j + 1) * g_nStripWidth;
                pIndices[iIndex++] =  i      +  j      * g_nStripWidth;
                pIndices[iIndex++] = (i + 1) + (j + 1) * g_nStripWidth;

                pIndices[iIndex++] =  i      +  j      * g_nStripWidth;
                pIndices[iIndex++] = (i + 1) +  j      * g_nStripWidth;
                pIndices[iIndex++] = (i + 1) + (j + 1) * g_nStripWidth;
            }
        }
        g_errLast = g_pIndices->Unlock();
        g_nPolys = g_nStripWidth * g_nStripHeight * 2;
    }

    // If the user wants non-indexed prims, then create the lists from the generated vertices above
    if (!g_fIndexedPrims)
    {
        g_nVerts = g_nIndices;

        g_ppVerts2 = new LPDIRECT3DMOBILEVERTEXBUFFER[g_nStrips];
        if (!g_ppVerts2)
        {
            Output(LOG_ABORT, _T("Out of memory creating vertex buffer pointer array"));
            return;
        }
        memset(g_ppVerts2, 0x00, sizeof(LPDIRECT3DMOBILEVERTEXBUFFER) * g_nStrips);

        for (iCurStrip = 0; iCurStrip < g_nStrips; iCurStrip++)
        {
            g_errLast = g_p3ddevice->CreateVertexBuffer(
                ((sizeof(_VERTEX) == sizeof(D3DLVERTEX)) ? sizeof(LIT_TEXTURED_VERTEX_TYPE) : sizeof(UNLIT_TEXTURED_VERTEX_TYPE)) * g_nVerts, 
                D3DMUSAGE_LOCKABLE, 
                (sizeof(_VERTEX) == sizeof(D3DLVERTEX)) ? LIT_TEXTURED_VERTEX_FVF : UNLIT_TEXTURED_VERTEX_FVF,
                g_memorypool,
                &(g_ppVerts2[iCurStrip]));
            
            _VERTEX *pVertListOriginal;
            _VERTEX *pVertList;
            g_errLast = g_ppVerts[iCurStrip]->Lock(0, 0, (void**)&pVertListOriginal, 0);
            g_errLast = g_ppVerts2[iCurStrip]->Lock(0, 0, (void**)&pVertList, 0);
            g_errLast = g_pIndices->Lock(0, 0, (void**)&pIndices, 0);
            for (iCurIndex = 0; iCurIndex < g_nIndices; iCurIndex++)
            {
                pVertList[iCurIndex] = pVertListOriginal[pIndices[iCurIndex]];
            }
            g_ppVerts[iCurStrip]->Unlock();
            g_ppVerts2[iCurStrip]->Unlock();
            g_pIndices->Unlock();
        }
    }

    if (g_fTransformed)
    {
        // Allocate space for the list of transformed vertices
        g_ppVertsTransformed = new LPDIRECT3DMOBILEVERTEXBUFFER[g_nStrips];
        if (!g_ppVertsTransformed)
        {
            Output(LOG_ABORT, _T("Out of memory creating vertex buffer pointer array"));
            return;
        }
        memset(g_ppVertsTransformed, 0x00, sizeof(LPDIRECT3DMOBILEVERTEXBUFFER) * g_nStrips);
        for (iCurStrip = 0; iCurStrip < g_nStrips; iCurStrip++)
        {
            g_errLast = g_p3ddevice->CreateVertexBuffer(
                sizeof(LIT_TRANSFORMED_TEXTURED_VERTEX_TYPE) * g_nVerts, 
                D3DMUSAGE_LOCKABLE, 
                LIT_TRANSFORMED_TEXTURED_VERTEX_FVF,
                g_memorypool,
                &(g_ppVertsTransformed[iCurStrip]));

            if (!g_fRotating)
            {
                // If the spheres are not rotating each frame, and they are transformed,
                // then generate the transformed vertices here.
                g_ptrackerStrip[iCurStrip].Move();
                if (g_fIndexedPrims)
                    g_p3ddevice->SetStreamSource(0, g_ppVerts[iCurStrip], sizeof(_VERTEX));
                else
                    g_p3ddevice->SetStreamSource(0, g_ppVerts2[iCurStrip], sizeof(_VERTEX));

                g_p3ddevice->SetMaterial(&g_pmatWhite, D3DMFMT_D3DMVALUE_FLOAT);
                g_errLast = g_p3ddevice->ProcessVertices(0, 0, g_nVerts, g_ppVertsTransformed[iCurStrip], 0);
                if (CheckError(_T("ProcessVertices, unrotated")))
                {
                    return;
                }
                g_p3ddevice->SetMaterial(NULL, D3DMFMT_D3DMVALUE_FLOAT);
            }
        }
    }
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    RenderSceneToBackBuffer

Description:

    Renders the scene to the back buffer

Arguments:

    None

Return Value:

    TRUE on success, FALSE on failure.

-------------------------------------------------------------------*/
BOOL
RenderSceneToBackBuffer()
{
int i;
    D3DMMATRIX matTemp;
    D3DMFORMAT fmtTemp;

    // Clear the back buffer.
    static RECT s_d3drect = {0, 0, 640, 480};
    g_errLast = g_p3ddevice->Clear(0, NULL, D3DMCLEAR_TARGET | D3DMCLEAR_ZBUFFER, 0, 0.0f, 0);
    if (CheckError(TEXT("Clear Back Buffer")))
        return FALSE;

    // Begin the scene
    g_errLast = g_p3ddevice->BeginScene();
    if (CheckError(TEXT("Begin Scene")))
        goto exitfail;

    g_p3ddevice->GetTransform(D3DMTS_WORLD, &matTemp, &fmtTemp);

    // Set the strip texture
    if (g_fTextureMap)
        g_p3ddevice->SetTexture(0, g_pTexture[0]);

    g_p3ddevice->SetMaterial(&g_pmatWhite, D3DMFMT_D3DMVALUE_FLOAT);

    // Render the Strip(s).
    for (i = 0; i < g_nStrips; i++)
    {
        // Move the strip
        g_ptrackerStrip[i].Move();

        if (g_fTransformed && g_fRotating)
        {
            if (g_fIndexedPrims && !g_fPreLit)
                g_p3ddevice->SetStreamSource(0, g_ppVerts[i], sizeof(D3DVERTEX));
            else if (g_fIndexedPrims)
                g_p3ddevice->SetStreamSource(0, g_ppVerts[i], sizeof(D3DLVERTEX));
            else if (g_fPreLit)
                g_p3ddevice->SetStreamSource(0, g_ppVerts2[i], sizeof(D3DLVERTEX));
            else
                g_p3ddevice->SetStreamSource(0, g_ppVerts2[i], sizeof(D3DVERTEX));
            g_errLast = g_p3ddevice->ProcessVertices(0, 0, g_nVerts, g_ppVertsTransformed[i], 0);
            if (CheckError(_T("Rotating ProcessVertices")))
            {
                goto exitfail;
            }
        }

        // Draw the strip
        if (g_fIndexedPrims)
        {
            if (g_fTriStrips)
            {
                if (g_fTransformed)
                    g_p3ddevice->SetStreamSource(0, g_ppVertsTransformed[i], sizeof(D3DTVERTEX));
                else if (g_fPreLit)
                    g_p3ddevice->SetStreamSource(0, g_ppVerts[i], sizeof(D3DLVERTEX));
                else
                    g_p3ddevice->SetStreamSource(0, g_ppVerts[i], sizeof(D3DVERTEX));
                g_p3ddevice->SetIndices(g_pIndices);

                if (!(g_fTransformed && !g_fRasterize))
                    g_p3ddevice->DrawIndexedPrimitive(D3DMPT_TRIANGLESTRIP, 0, 0, g_nVerts, 0, g_nIndices-2);
            }
            else
            {
                if (g_fTransformed)
                    g_p3ddevice->SetStreamSource(0, g_ppVertsTransformed[i], sizeof(D3DTVERTEX));
                else if (g_fPreLit)
                    g_p3ddevice->SetStreamSource(0, g_ppVerts[i], sizeof(D3DLVERTEX));
                else
                    g_p3ddevice->SetStreamSource(0, g_ppVerts[i], sizeof(D3DVERTEX));
                g_p3ddevice->SetIndices(g_pIndices);
                if (!(g_fTransformed && !g_fRasterize))
                    g_p3ddevice->DrawIndexedPrimitive(D3DMPT_TRIANGLELIST, 0, 0, g_nVerts, 0, g_nIndices/3);
            }
        }
        else
        {
            if (g_fTriStrips)
            {
                // Triangle strips, nonindexed primitive
                if (g_fTransformed)
                    g_p3ddevice->SetStreamSource(0, g_ppVertsTransformed[i], sizeof(D3DTVERTEX));
                else if (g_fPreLit)
                    g_p3ddevice->SetStreamSource(0, g_ppVerts2[i], sizeof(D3DLVERTEX));
                else
                    g_p3ddevice->SetStreamSource(0, g_ppVerts2[i], sizeof(D3DVERTEX));
                if (!(g_fTransformed && !g_fRasterize))
                    g_p3ddevice->DrawPrimitive(D3DMPT_TRIANGLESTRIP, 0, g_nVerts-2);
            }
            else
            {
                // Triangle lists, nonindexed primitves
                if (g_fTransformed)
                    g_p3ddevice->SetStreamSource(0, g_ppVertsTransformed[i], sizeof(D3DTVERTEX));
                else if (g_fPreLit)
                    g_p3ddevice->SetStreamSource(0, g_ppVerts2[i], sizeof(D3DLVERTEX));
                else
                    g_p3ddevice->SetStreamSource(0, g_ppVerts2[i], sizeof(D3DVERTEX));
                if (!(g_fTransformed && !g_fRasterize))
                    g_p3ddevice->DrawPrimitive(D3DMPT_TRIANGLELIST, 0, g_nVerts/3);
            }
        }
    }
    g_p3ddevice->SetStreamSource(0, NULL, sizeof(D3DVERTEX));
    g_p3ddevice->SetIndices(NULL);
    g_p3ddevice->SetTransform(D3DMTS_WORLD, &matTemp, D3DMFMT_D3DMVALUE_FLOAT);

    // Increment frame counter
    g_cFramesDrawn++;

    // Output framerate info to the console
    DumpInfo();

    // End the scene
    g_errLast = g_p3ddevice->EndScene();
    if (CheckError(TEXT("EndScene")))
        goto exitfail;

    return TRUE;

exitfail:
    return FALSE;
}

void
DumpInfo(bool fReset)
{
    static DWORD s_dwTimeLast  = GetTickCount();
    static DWORD s_dwTimeFirst = GetTickCount();
    static DWORD s_dwTimeExclude = 0;

    TCHAR tszBuf[256];

    if (fReset)
    {
        s_dwTimeLast = GetTickCount();
        s_dwTimeFirst = GetTickCount();
        s_dwTimeExclude = 0;
        return;
    }

    // Output polycount info only on the first time
    if (g_cFramesDrawn == 1)
    {
        DWORD dwTime = GetTickCount();

        // Output the type of primitive and the primitive instruction used.
        wsprintf(tszBuf, TEXT("Drawing "));
        if (g_fPreLit)
            _tcscat(tszBuf, TEXT("Prelit "));
        else
            _tcscat(tszBuf, TEXT("Unlit "));

        if (g_fTriStrips)
            _tcscat(tszBuf, TEXT("Triangle Strips "));
        else
            _tcscat(tszBuf, TEXT("Triangle Lists "));

        if (g_fIndexedPrims)
            _tcscat(tszBuf, TEXT("using 'DrawIndexedPrimitive'.\r\n"));
        else
            _tcscat(tszBuf, TEXT("using 'DrawPrimitive'.\r\n"));

        Output(LOG_DETAIL, tszBuf);

        wsprintf(tszBuf, TEXT("Totals: NumStrips: %d, Indices: %d  Vertices: %d  Polygons: %d\r\n"), g_nStrips,
                 g_nIndices * g_nStrips, g_nVerts * g_nStrips, g_nPolys * g_nStrips);

        Output(LOG_DETAIL, tszBuf);
        s_dwTimeExclude += GetTickCount() - dwTime;
    }

    // Output framerate info every 'g_nOutputFrame' frames
    if (g_fDone || ((g_cFramesDrawn % g_nOutputFrame) == 0) && g_nOutputFrame != -1)
    {
        DWORD dwTime              = GetTickCount();
        DWORD dwTimeSinceStart    = dwTime - s_dwTimeFirst - s_dwTimeExclude;
        DWORD dwTimeSincePrevious = dwTime - s_dwTimeLast;

        if (0 == dwTimeSinceStart)
        {
            dwTimeSinceStart = 1;
        }

        if (0 == dwTimeSincePrevious)
        {
            dwTimeSincePrevious = 1;
        }

        // Output fps info
        wsprintf(tszBuf, TEXT("Total Frames Drawn: %d   Time: %d.%03ds   fps: %f  #Polys:%d   p/s:%d\r\n"),
                         g_cFramesDrawn,
                         dwTimeSinceStart/1000, dwTimeSinceStart%1000,
                         (float)(g_cFramesDrawn * 1000) / (float) dwTimeSinceStart,
                         g_nPolys * g_nStrips,
                         (int) ( ((float)g_nPolys * (float)g_nStrips * (float)g_cFramesDrawn * 1000.f) / (float)dwTimeSinceStart) );
        Output(LOG_DETAIL, tszBuf);

        if (!g_fDone)
        {
            wsprintf(tszBuf, TEXT("     Last %d frames: Time: %d.%03ds   fps: %f  p/s:%d\r\n"),
                             g_nOutputFrame,
                             dwTimeSincePrevious/1000, dwTimeSincePrevious%1000, (float) (g_nOutputFrame * 1000) / (float) dwTimeSincePrevious,
                         (int) ( ((float)g_nPolys * (float)g_nStrips * (float)g_nOutputFrame * 1000.f) / (float)dwTimeSincePrevious) );
            Output(LOG_DETAIL, tszBuf);
        }

        s_dwTimeLast = GetTickCount();
        s_dwTimeExclude += s_dwTimeLast - dwTime;
    }
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    UpdateFrame

Description:

    This function is called whenever the CPU is idle.  The application
    should update it's state and rerender to the screen if appropriate.

Arguments:

    None.

Return Value:

    None.

-------------------------------------------------------------------*/
void
UpdateFrame ()
{
    // Render the scene
    RenderSceneToBackBuffer();

    // Flip the buffers
    g_p3ddevice->Present(NULL, NULL, NULL, NULL);

    if (g_nEndFrame != -1 && g_cFramesDrawn >= g_nEndFrame)
    {
        // Last frame drawn!
        g_fDone = true;
        DumpInfo();
    }

}

//******************************************************************************
//
// Function:
//
//     SetDisplayValue
//
// Description:
//
//     Set the given registry key using the given value.
//
// Arguments:
//
//     LPCWSTR szName           - Name of the registry key to set
//
//     DWORD dwValue            - Value to set the key with
//
//     LPDWORD pdwOldValue      - Contains the old value of the registry key
//                                on return
//
// Return Value:
//
//     true on success, false on failure.
//
//******************************************************************************
bool SetDisplayValue(LPCWSTR szName, DWORD dwValue, LPDWORD pdwOldValue) {

    HKEY  hKey;
    DWORD dwType;
    DWORD dwSize;
    bool  bRet = false;

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"DisplaySettings", 0, 0, &hKey)
                                                        == ERROR_SUCCESS)
    {
        if (pdwOldValue) {
            DWORD dwOldValue;
            dwSize = sizeof(DWORD);
            if (RegQueryValueEx(hKey, szName, NULL, &dwType,
                                (LPBYTE)&dwOldValue, &dwSize) == ERROR_SUCCESS)
            {
                *pdwOldValue = dwOldValue;
            }
        }

        if (RegSetValueEx(hKey, szName, 0, REG_DWORD, (LPBYTE)&dwValue,
                            sizeof(DWORD)) == ERROR_SUCCESS)
        {
            bRet = true;
        }

        RegCloseKey(hKey);
    }

    return bRet;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    AppInit

Description:

    This function registers a window class, and creates a window for
    the application.

Arguments:

    hPrev               - Hinstance of another process running the program

    nCmdShow            - Whether the app should be shown (ignored)

Return Value:

    TRUE on success, FALSE on failure.

-------------------------------------------------------------------*/
BOOL
AppInit(HINSTANCE hPrev, int nCmdShow)
{
    WNDCLASS  cls;                                     // Class of the application's window
    static bool fRegistered = false;

    if (!hPrev && !fRegistered)
    {
        //  Register a class for the main application window
        cls.hCursor        = NULL;
        cls.hIcon          = NULL;
        cls.lpszMenuName   = NULL;
        cls.hbrBackground  = NULL;
        cls.hInstance      = (HINSTANCE)g_hInstance;
        cls.lpszClassName  = g_tszAppName;
        cls.lpfnWndProc    = (WNDPROC)WndProc;
        cls.style          = 0;
        cls.cbWndExtra     = 0;
        cls.cbClsExtra     = 0;

        if (!RegisterClass(&cls))
            return FALSE;
        fRegistered = true;
    }

    g_hwndApp = CreateWindowEx (0, g_tszAppName, g_tszAppName, WS_VISIBLE,
                                0, 0, 640, 480, NULL, NULL, (HINSTANCE)g_hInstance, NULL);

#define VERTEX_BUFFER_SIZE 0x001E0000
#define POLYGON_BUFFER_SIZE 0x0030000
    DWORD dwVertexBufferSize;
    DWORD dwPolygonBufferSize;
        SetDisplayValue(L"CommandVertexBufferSize", VERTEX_BUFFER_SIZE,
                        &dwVertexBufferSize);
        SetDisplayValue(L"CommandPolygonBufferSize", POLYGON_BUFFER_SIZE,
                        &dwPolygonBufferSize);

    // Initializes the Direct3D object, viewport, and sets the rendering state.
    if (!InitDirect3D())
        return FALSE;

    // Allocate the strip movement tracker
    g_ptrackerStrip = new CTracker[g_nStrips];
    if (!g_ptrackerStrip)
    {
        Output(LOG_ABORT, _T("Out of memory creating Tracker array"));
        return FALSE;
    }

    // Initialize the Strip's vertices and indices.
    
    if (g_fPreLit)
        CreateStrip<D3DLVERTEX>();
    else
        CreateStrip<D3DVERTEX>();
        
    // Load the texture for the Strip.
    g_pTexture[0] = LoadTexture(TEXT("TEXTURE0"));

    return TRUE;
}

int
GetNumber(TCHAR **ppCmdLine)
{
    int nValue = 0;
    int nNeg = 1;

    while (**ppCmdLine == TEXT(' ') && **ppCmdLine)
        (*ppCmdLine)++;

    if (**ppCmdLine == TEXT('-'))
    {
        nNeg = -1;
        (*ppCmdLine)++;
    }

    while (**ppCmdLine >= TEXT('0') && **ppCmdLine <= TEXT('9') && *ppCmdLine)
    {
        nValue = nValue * 10 + (**ppCmdLine - '0');
        (*ppCmdLine)++;
    }

    return nValue * nNeg;
}

void
DumpUsageInfo()
{
    RetailOutput(TEXT("\r\nOptimal performance application.\r\n"));
    RetailOutput(TEXT("Usage: Optimal [options]\r\n\r\n"));
    RetailOutput(TEXT("-?               This output\r\n"));
    RetailOutput(TEXT("-swdevice <dll>  If present, the softare driver to load. Otherwise the\r\n"));
    RetailOutput(TEXT("                     default device will be used (if present).\r\n"));
    RetailOutput(TEXT("-notex           Disable texture mapping  (default = texture mapping ON)\r\n"));
    RetailOutput(TEXT("-norotate        Disable sphere rotation between frames. This allows for\r\n"));
    RetailOutput(TEXT("                     consistent comparisons between transformed and untransformed\r\n"));
    RetailOutput(TEXT("                     performance runs.\r\n"));
    RetailOutput(TEXT("-procvert        Use ProcessVertices for transformation and lighting.\r\n"));
    RetailOutput(TEXT("-notrans         Use pre-transformed and lit vertices. Equivalent to\r\n"));
    RetailOutput(TEXT("                     using -norotate and -procvert.\r\n"));
    RetailOutput(TEXT("-dip             Use DrawIndexedPrimitive (default = use DrawPrimitive)\r\n"));
    RetailOutput(TEXT("-list            use Triangle Lists       (default = use Triangle Strips)\r\n"));
    RetailOutput(TEXT("-light #         Number of lights to display.  Must be 0, 1, 2, or 3.\r\n"));
    RetailOutput(TEXT("                     Use '0' to use PreLit vertices  (default = 1)\r\n"));
    RetailOutput(TEXT("-frames #        Number of frames to run before exiting.\r\n"));
    RetailOutput(TEXT("                     Use '-1' for Infinite run (default == 200)\r\n"));
    RetailOutput(TEXT("-output #        Number of frames to run between outputting information\r\n"));
    RetailOutput(TEXT("                     use '-1' for no during-run output (default = -1)\r\n"));
    RetailOutput(TEXT("-stripsize # #   Specify size of strip (Width, Height).  Numbers must be\r\n"));
    RetailOutput(TEXT("                     divisible by two! technically, this is the number of \r\n"));
    RetailOutput(TEXT("                     vertices in each dimension.  The number of polygons\r\n"));
    RetailOutput(TEXT("                     displayed will be roughly (2 * Width * Height), since\r\n"));
    RetailOutput(TEXT("                     each 'square' in the strip is 2 polygons (the number \r\n"));
    RetailOutput(TEXT("                     of polygons is 'roughly' that because there are also \r\n"));
    RetailOutput(TEXT("                     extra polygons at the end of each row to allow us to \r\n"));
    RetailOutput(TEXT("                     use a single strip for each sphere.  Look at the output \r\n"));
    RetailOutput(TEXT("                     of the program for the exact number of polygons displayed).\r\n"));
    RetailOutput(TEXT("                     (Default; width = 20, height = 20)\r\n"));
    RetailOutput(TEXT("-stripnum #      Number of strips to display (default = 20)\r\n\r\n"));
}

TCHAR *
GetNextCommand(TCHAR **ppCmdLine)
{
    TCHAR *ptsz = *ppCmdLine;

    // Skip command (ie go until whitespace is hit)
    while (**ppCmdLine != TEXT(' ') && **ppCmdLine)
        (*ppCmdLine)++;

    return ptsz;
}

BOOL
ParseCommandLine(LPTSTR lpCmdLine)
{
    TCHAR tszBuf[256];
    TCHAR *ptszCommand;

    while ((lpCmdLine[0] == TEXT('-')) || (lpCmdLine[0] == TEXT('/')))
    {
        lpCmdLine++;

        ptszCommand = GetNextCommand(&lpCmdLine);

        if (!_tcsnicmp(ptszCommand, TEXT("?"), 1))
        {
            // Dump Usage info
            DumpUsageInfo();
            return false;
        }
        else if (!_tcsnicmp(ptszCommand, TEXT("norast"), 6))
        {
            // User wants to skip rasterization
            g_fRasterize = false;
        }
        else if (!_tcsnicmp(ptszCommand, TEXT("norotate"), 8))
        {
            // When combined with transformed vertices, only rasterization will occur
            g_fRotating = false;
        }
        else if (!_tcsnicmp(ptszCommand, TEXT("procvert"), 8))
        {
            // Use process vertices to transform and light vertices
            g_fTransformed = true;
        }
        else if (!_tcsnicmp(ptszCommand, TEXT("notrans"), 7))
        {
            // Use pre-transformed and lit vertices
            g_fTransformed = true;
            g_fRotating = false;
        }
        else if (!_tcsnicmp(ptszCommand, TEXT("dip"), 3))
        {
            // User wants to use DrawIndexedPrimitive
            g_fIndexedPrims = true;
        }
        else if (!_tcsnicmp(ptszCommand, TEXT("light"), 5))
        {
            // User wants to specify the number of lights to use
            // Get the number
            g_nUserLights = GetNumber(&lpCmdLine);
            if (g_nUserLights == 0)
                g_fPreLit = true;
            if (g_nUserLights > 3)
            {
                // Invalid number of lights!
                wsprintf(tszBuf, TEXT("\r\nERROR: Invalid number of lights! Must be between 0, 1, 2, or 3.\r\nRun 'Optimal -?' for usage info.\r\n"));
                Output(LOG_ABORT, tszBuf);
                return false;
            }
        }
        else if (!_tcsnicmp(ptszCommand, TEXT("list"), 4))
        {
            // User wants to use Triangle lists instead of Triangle Strips
            g_fTriStrips = false;

        }
        else if (!_tcsnicmp(ptszCommand, TEXT("Frames"), 6))
        {
            // User wants to specify the number of frames to run before ending.
            g_nEndFrame = GetNumber(&lpCmdLine);
        }
        else if (!_tcsnicmp(ptszCommand, TEXT("Output"), 6))
        {
            // User wants to specify the number of frames to run between info output
            g_nOutputFrame = GetNumber(&lpCmdLine);
        }
        else if (!_tcsnicmp(ptszCommand, TEXT("stripsize"), 9))
        {
            // User wants to specify the strip size
            g_nStripWidth  = GetNumber(&lpCmdLine);
            g_nStripHeight = GetNumber(&lpCmdLine);
            if ((g_nStripWidth%2) || (g_nStripHeight%2))
            {
                // Invalid size
                wsprintf(tszBuf, TEXT("\r\nERROR: Invalid Strip size specified - width and height *must* be divisible by two\r\nRun 'Optimal -?' for usage info.\r\n"));
                Output(LOG_ABORT, tszBuf);
                return false;
            }
            if ((g_nStripWidth<4) || (g_nStripHeight<4))
            {
                // Invalid size
                wsprintf(tszBuf, TEXT("\r\nERROR: Invalid Strip size specified - width and height *must* be greater than two\r\nRun 'Optimal -?' for usage info.\r\n"));
                Output(LOG_ABORT, tszBuf);
                return false;
            }
        }
        else if (!_tcsnicmp(ptszCommand, TEXT("stripnum"), 8))
        {
            // User wants to specify the number of strips
            g_nStrips      = GetNumber(&lpCmdLine);
        }
        else if (!_tcsnicmp(ptszCommand, TEXT("notex"), 5))
        {
            // User doesn't want textures
            g_fTextureMap = false;
        }
        else if (!_tcsnicmp(ptszCommand, TEXT("swdevice"), 8))
        {
            WCHAR * pwszDriver = g_pwszDriver;
            // Advance to the driver name parameter, or the NULL
            // terminator at the end of the string.
            while (*lpCmdLine == TEXT(' '))
                lpCmdLine++;
            while (*lpCmdLine && *lpCmdLine != TEXT(' ') && pwszDriver < (g_pwszDriver + countof(g_pwszDriver)))
            {
                *pwszDriver = *lpCmdLine;
                ++pwszDriver;
                ++lpCmdLine;
            }
                
        }
        else
        {
            // Unknown command
            wsprintf(tszBuf, TEXT("\r\nERROR: Invalid Command line param '%s' specified - run 'Optimal -?' for usage info.\r\n"), ptszCommand);
            Output(LOG_ABORT, tszBuf);
            return false;
        }

        // Advance to the next command line parameter, or the NULL
        // terminator at the end of the string.
        while (*lpCmdLine == TEXT(' '))
            lpCmdLine++;
    }

    return true;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    WinMain

Description:

    This is the entrypoint for this sample app.  It creates an app
    window and then enters a message loop.

Arguments:

    hInstance           - HInstance of the process

    hPrev               - HInstance of another process running the program

    LPTSTR lpCmdLine    - Pointer to command line string

    nCmdShow            - Whether the app should be shown (ignored)

Return Value:

    We normally return the wParam value from the WM_QUIT message.  If
    there's a failure upon initialization, we just return 0.

-------------------------------------------------------------------*/
#ifdef TUXDLL
TESTPROCAPI TuxTest(UINT uTuxMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
#else
#if !defined(UNDER_CE) && defined(UNICODE)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR _lpCmdLine, int nCmdShow)
#else
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
#endif
#endif
{
    MSG msg;
#ifdef TUXDLL
    TCHAR lpCmdLine[_MAX_PATH] = _T("");
    _tcsncpy(lpCmdLine, g_pShellInfo->szDllCmdLine, countof(lpCmdLine));
    lpCmdLine[countof(lpCmdLine) - 1] = 0;
    if(uTuxMsg != TPM_EXECUTE)    
        return TPR_NOT_HANDLED;

    // Setup the default parameters.
    g_cFramesDrawn  = 0;
    g_fIndexedPrims = false;    // Set to FALSE to use DrawPrimitive, TRUE for DrawIndexedPrimitive
    g_fTriStrips    = true;     // Set to FALSE to use TriLists, TRUE for TriStrips
    g_fPreLit       = false;    // Set to FALSE to use D3DVERTEX, TRUE for D3DLVERTEX
    g_nUserLights   = 1;        // 0 == Prelit.
    g_nEndFrame     = 200;       // -1 == 'infinite' (ie no end)
    g_nOutputFrame  = 300;      // Output info every 300 frames.
    g_fDone         = false;
    g_fTextureMap   = true;     // TRUE == user texture mapping.
    g_fRasterize    = true;
    g_fTransformed  = false;    // Set to true to use ProcessVertices to transform.
    g_fRotating     = true;     // true == spheres are rotating.

    // Reset the timers.
    DumpInfo(true);
    
    switch (lpFTE->dwUniqueID)
    {
    case 100:
        // Use the default settings
        break;
    case 101:
        // Use ProcessVertices, without rasterizing
        g_fTransformed = true;
        g_fRasterize = false;
        break;
    case 102:
        // Don't perform any transformations when drawing frame
        g_fTransformed = true;
        g_fRotating = false;
        break;
    }


    Output(LOG_PASS, _T("Parsing the command line"));
    if (!ParseCommandLine(lpCmdLine))
     return getCode();

    Output(LOG_PASS, _T("Initializing application"));
    if (!AppInit(NULL, 0))
        return getCode();
#else
#if !defined(UNDER_CE) && defined(UNICODE)
    TCHAR lpCmdLine[_MAX_PATH] = TEXT("");
    if (_lpCmdLine && _lpCmdLine[0])
    {
        MultiByteToWideChar(CP_ACP, 0, _lpCmdLine, -1, lpCmdLine, countof(lpCmdLine));
    }
#endif

    // Store Instance handle for use later...
    g_hInstance = hInstance;
    // Check the command line
    if (!ParseCommandLine(lpCmdLine))
        return 0L;

    // Call initialization procedure
    if (!AppInit(hPrevInstance,nCmdShow))
        return 0L;
#endif

    // Main Message loop
    while (TRUE)
    {
        if (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
        {
            // There's a message waiting in the queue for us.  Retrieve
            // it and dispatch it, unless it's a WM_QUIT.
            if (msg.message == WM_QUIT)
                break;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            // If no messages pending, then update the frame.
            UpdateFrame();
            if (g_fDone)
            {
                // Not the correct exit strategy - should send a message to the WndProc and let it deal with this...
                DestroyWindow(g_hwndApp);
            }
        }
    }

    RetailOutput(TEXT("\r\nApp exited...\r\n"));
    return getCode();
}

#ifdef TUXDLL
//
// DllMain
//
//   Main entry point of the DLL. Called when the DLL is loaded or unloaded.
//
// Return Value:
// 
//   BOOL
//
BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(dwReason);
	UNREFERENCED_PARAMETER(lpReserved);

    g_hInstance = (HINSTANCE)hInstance;

    return TRUE;
}

//
// ShellProc
//
//   Processes messages from the TUX shell.
//
// Arguments:
//
//   uMsg            Message code.
//   spParam         Additional message-dependent data.
//
// Return value:
//
//   Depends on the message.
//
SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam)
{
    LPSPS_BEGIN_TEST    pBT;
    LPSPS_END_TEST      pET;

    switch (uMsg)
    {
    case SPM_LOAD_DLL:
        // This message is sent once to the DLL immediately after it is loaded.
        // The ShellProc must set the fUnicode member of this structure to TRUE
		// if the DLL was built with the UNICODE flag set.
        OutputDebugString(TEXT("ShellProc(SPM_LOAD_DLL, ...) called"));

#ifdef UNICODE
        ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
#endif // UNICODE
		g_pKato = (CKato*)KatoGetDefaultObject();
        break;

    case SPM_UNLOAD_DLL:
        // Sent once to the DLL immediately before it is unloaded.
        OutputDebugString(TEXT("ShellProc(SPM_UNLOAD_DLL, ...) called"));
        break;

    case SPM_SHELL_INFO:
        // This message is sent once to the DLL immediately following the
		// SPM_LOAD_DLL message to give the DLL information about its parent shell,
		// via SPS_SHELL_INFO.
        OutputDebugString(TEXT("ShellProc(SPM_SHELL_INFO, ...) called"));

        // Store a pointer to our shell info, in case it is useful later on
        g_pShellInfo = (LPSPS_SHELL_INFO)spParam;
        break;

    case SPM_REGISTER:
        // This message is sent to query the DLL for its function table. 
        OutputDebugString(TEXT("ShellProc(SPM_REGISTER, ...) called"));
        ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;

#ifdef UNICODE
        return SPR_HANDLED | SPF_UNICODE;
#else // UNICODE
        return SPR_HANDLED;
#endif // UNICODE

    case SPM_START_SCRIPT:
        // This message is sent to the DLL immediately before a script is started
        OutputDebugString(TEXT("ShellProc(SPM_START_SCRIPT, ...) called"));
        break;

    case SPM_STOP_SCRIPT:
        // This message is sent to the DLL after the script has stopped.
        OutputDebugString(TEXT("ShellProc(SPM_STOP_SCRIPT, ...) called"));
        break;

    case SPM_BEGIN_GROUP:
        // Sent to the DLL before a group of tests from that DLL is about to
        // be executed. This gives the DLL a time to initialize or allocate
        // data for the tests to follow.
        OutputDebugString(TEXT("ShellProc(SPM_BEGIN_GROUP, ...) called"));
        g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: TUXTEST.DLL"));
		break;

    case SPM_END_GROUP:
        // Sent to the DLL after a group of tests from that DLL has completed
        // running. This gives the DLL a time to cleanup after it has been
        // run. This message does not mean that the DLL will not be called
        // again; it just means that the next test to run belongs to a
        // different DLL. SPM_BEGIN_GROUP and SPM_END_GROUP allow the DLL
        // to track when it is active and when it is not active.
        OutputDebugString(TEXT("ShellProc(SPM_END_GROUP, ...) called"));
        g_pKato->EndLevel(TEXT("END GROUP: TUXTEST.DLL"));
		break;

    case SPM_BEGIN_TEST:
        // This message is sent to the DLL before a single test or group of
		// tests from that DLL is executed. 
       
		OutputDebugString(TEXT("ShellProc(SPM_BEGIN_TEST, ...) called"));
        // Start our logging level.
        pBT = (LPSPS_BEGIN_TEST)spParam;
        g_pKato->BeginLevel(
            pBT->lpFTE->dwUniqueID,
            TEXT("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
            pBT->lpFTE->lpDescription,
            pBT->dwThreadCount,
            pBT->dwRandomSeed);
		break;

    case SPM_END_TEST:
        // This message is sent to the DLL after a single test case from
		// the DLL executes. 
		
        OutputDebugString(TEXT("ShellProc(SPM_END_TEST, ...) called"));
        // End our logging level.
        pET = (LPSPS_END_TEST)spParam;
        g_pKato->EndLevel(
            TEXT("END TEST: \"%s\", %s, Time=%u.%03u"),
            pET->lpFTE->lpDescription,
            pET->dwResult == TPR_SKIP ? TEXT("SKIPPED") :
            pET->dwResult == TPR_PASS ? TEXT("PASSED") :
            pET->dwResult == TPR_FAIL ? TEXT("FAILED") : TEXT("ABORTED"),
            pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);
		break;

    case SPM_EXCEPTION:
        // Sent to the DLL whenever code execution in the DLL causes and
        // exception fault. TUX traps all exceptions that occur while
        // executing code inside a test DLL.
        OutputDebugString(TEXT("ShellProc(SPM_EXCEPTION, ...) called"));
        g_pKato->Log(LOG_EXCEPTION, TEXT("Exception occurred!"));
		break;

    default:
        // Any messages that we haven't processed must, by default, cause us
        // to return SPR_NOT_HANDLED. This preserves compatibility with future
        // versions of the TUX shell protocol, even if new messages are added.
        return SPR_NOT_HANDLED;
    }

    return SPR_HANDLED;
}

TESTPROCAPI getCode(void)
{

    for (int i = 0; i < 15; i++)
        if (g_pKato->GetVerbosityCount((DWORD) i) > 0)
            switch (i)
            {
                case LOG_EXCEPTION:
                    return TPR_HANDLED;
                case LOG_FAIL:
                    return TPR_FAIL;
                case LOG_ABORT:
                    return TPR_ABORT;
                case LOG_SKIP:
                    return TPR_SKIP;
                case LOG_NOT_IMPLEMENTED:
                    return TPR_HANDLED;
                case LOG_PASS:
                    return TPR_PASS;
                default:
                    return TPR_NOT_HANDLED;
            }
    return TPR_PASS;
}

#endif // TUXDLL
