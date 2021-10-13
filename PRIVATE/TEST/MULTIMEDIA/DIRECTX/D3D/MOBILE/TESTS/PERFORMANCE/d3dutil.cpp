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

    D3DUtil.cpp

Abstract:

   This file contains Direct3D functionality for the Optimal sample.

-------------------------------------------------------------------*/

// ++++ Include Files +++++++++++++++++++++++++++++++++++++++++++++++
#define D3D_OVERLOADS
#include "Optimal.hpp"

// ++++ Global Variables ++++++++++++++++++++++++++++++++++++++++++++
UINT                     g_uiAdapterOrdinal;
HMODULE                  g_hSWDeviceDLL = NULL;
WCHAR                    g_pwszDriver[MAX_PATH + 1] = L"";
D3DMCAPS                 g_devCaps;
LPDIRECT3DMOBILEDEVICE   g_p3ddevice;            // Main Direct3D Device
LPDIRECT3DMOBILE         g_pd3d;                 // Direct3D object
D3DMMATERIAL             g_pmatWhite;            // Materials
D3DMMATRIX          g_matIdent;
int                 g_nLights = 0;

// ++++ Local Variables +++++++++++++++++++++++++++++++++++++++++++++

// Variables for setting up the world, view, and projection matrices.  Arbitrarily
// generic values.
static D3DMVECTOR g_vectUp;
static D3DMVECTOR g_vectFrom;
static D3DMVECTOR g_vectAt;
static float       g_rNear   = 0.001f;
static float       g_rFar    = 100.0f;
static float       g_rAspect = 480.0f / 640.0f;
static float       g_rFov    = PI/4.0f;

#define D3DMDLL _T("D3DM.DLL")

// ++++ Local Functions +++++++++++++++++++++++++++++++++++++++++++++
static BOOL SetInitialState();
static BOOL InitViewport();

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    ForceCleanD3DM

Description:

    Forces the D3DM library to be entirely unloaded.

Arguments:

    None

Return Value:

    S_OK on success, E_FAIL on failure.

-------------------------------------------------------------------*/
HRESULT ForceCleanD3DM()
{
	CONST UINT uiMaxUnloads = 10;
	UINT uiAttempts = 0;
	HMODULE hD3DMDLL;
	HINSTANCE hLoadLibD3DMDLL;

	while (hD3DMDLL = GetModuleHandle(D3DMDLL))
	{
		FreeLibrary(hD3DMDLL);

		Output(LOG_COMMENT, _T("FreeLibrary(D3DM.DLL)"));

		if (uiMaxUnloads == ++uiAttempts)
			break;
	}

	hLoadLibD3DMDLL = LoadLibrary(D3DMDLL); 
	Output(LOG_COMMENT, _T("LoadLibrary(D3DM.DLL)"));

	if (NULL != hLoadLibD3DMDLL)
		return S_OK;

	return E_FAIL;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    InitDirect3D

Description:

    Initializes the Direct3D object

Arguments:

    None

Return Value:

    TRUE on success, FALSE on failure.

-------------------------------------------------------------------*/
BOOL 
InitDirect3D()
{
    // First, get the D3D Object itself (g_pd3d)
    // Make sure the D3DM library is freshly loaded.
	if (FAILED(ForceCleanD3DM()))
	{
		Output(LOG_ABORT, _T("ForceCleanD3DM failed."));
        return FALSE;
	}
    // Get the Direct3DMobile object
    g_pd3d = Direct3DMobileCreate(D3DM_SDK_VERSION);
    if (NULL == g_pd3d)
        return FALSE;


    // Load the software device (if any)
	if (!g_pwszDriver[0])
	{
		g_uiAdapterOrdinal = D3DMADAPTER_DEFAULT;
	}
	else
	{
	    VOID * pSWDeviceEntry = NULL;
		g_uiAdapterOrdinal = D3DMADAPTER_REGISTERED_DEVICE;

		//
		// This function maps the specified DLL file into the address
		// space of the calling process. 
		//
		g_hSWDeviceDLL = LoadLibrary(g_pwszDriver);

		//
		// NULL indicates failure
		//
		if (NULL == g_hSWDeviceDLL)
		{
		    Output(LOG_ABORT, _T("LoadLibrary failed."));
            return FALSE;
		}

		//
		// This function returns the address of the specified exported DLL function. 
		//
		pSWDeviceEntry = GetProcAddress(g_hSWDeviceDLL,         // HMODULE hModule, 
                                        _T("D3DM_Initialize")); // LPCWSTR lpProcName
		//
		// NULL indicates failure.
		//
 		if (NULL == pSWDeviceEntry)
		{
		    Output(LOG_ABORT, _T("GetProcAddress failed."));
            return FALSE;
		}

		//
		// This method registers pluggable software device with Direct3D Mobile
		//
		g_errLast = g_pd3d->RegisterSoftwareDevice(pSWDeviceEntry);
        if (CheckError(TEXT("Unable to RegisterSoftwareDevice\r\n")))
            return FALSE;
	}

	g_errLast = g_pd3d->GetDeviceCaps(g_uiAdapterOrdinal, D3DMDEVTYPE_DEFAULT, &g_devCaps);
    
    // Create the D3D Device
    // Get the display mode
    D3DMDISPLAYMODE d3ddm;
    g_errLast = g_pd3d->GetAdapterDisplayMode(g_uiAdapterOrdinal, &d3ddm);
    if (CheckError(TEXT("Unable to GetAdapterDisplayMode\r\n"), LOG_SKIP))
        return FALSE;
    

    // Set the device parameters
    D3DMPRESENT_PARAMETERS d3dpp;
    memset(&d3dpp, 0, sizeof(d3dpp));
    d3dpp.BackBufferFormat = d3ddm.Format; //D3DMFMT_UNKNOWN;
    d3dpp.BackBufferCount = 1;
    d3dpp.SwapEffect = D3DMSWAPEFFECT_DISCARD;
    d3dpp.Windowed = FALSE;
    d3dpp.MultiSampleType = D3DMMULTISAMPLE_NONE;
    d3dpp.EnableAutoDepthStencil = FALSE;
    d3dpp.AutoDepthStencilFormat = D3DMFMT_UNKNOWN;
    d3dpp.FullScreen_PresentationInterval = 
        (d3dpp.Windowed || !(g_devCaps.PresentationIntervals & D3DMPRESENT_INTERVAL_IMMEDIATE)) ? 
            D3DMPRESENT_INTERVAL_DEFAULT : D3DMPRESENT_INTERVAL_IMMEDIATE;

//    if (0) //g_options.bAntiAlias)
//    {
//        if (SUCCEEDED(g_pd3d->CheckDeviceMultiSampleType(g_uiAdapterOrdinal,
//                                                         /*g_options.bRefRast ? D3DMDEVTYPE_REF :*/ D3DMDEVTYPE_DEFAULT,
//                                                         d3dpp.BackBufferFormat,
//                                                         d3dpp.Windowed,
//                                                         D3DMMULTISAMPLE_4_SAMPLES)))
//        {
//            RetailOutput(TEXT("Using fullscreen antialiasing\r\n"));
//            d3dpp.MultiSampleType = D3DMMULTISAMPLE_4_SAMPLES;
//            d3dpp.SwapEffect = D3DMSWAPEFFECT_DISCARD;
//        }
//        else
//        {
//            RetailOutput(TEXT("Fullscreen antialiasing not supported\r\n"));
//        }
//    }

    if (!d3dpp.Windowed)
    {
        d3dpp.BackBufferWidth = GetSystemMetrics(SM_CXSCREEN);
        d3dpp.BackBufferHeight = GetSystemMetrics(SM_CYSCREEN);
    }

    if (d3dpp.EnableAutoDepthStencil)
    {
        // Check that we have a valid back-buffer format
        g_errLast = g_pd3d->CheckDeviceFormat(g_uiAdapterOrdinal,
                                            D3DMDEVTYPE_DEFAULT,
                                            d3dpp.BackBufferFormat,
                                            0,
                                            D3DMRTYPE_DEPTHSTENCIL,
                                            d3dpp.AutoDepthStencilFormat);
        if (CheckError(TEXT("Unuspported backbuffer format\r\n")))
            return FALSE;

        // Check that we have a depth stencil format that is supported
        // in conjuction with the current render target format.
        g_errLast = g_pd3d->CheckDepthStencilMatch(g_uiAdapterOrdinal,
                                                  D3DMDEVTYPE_DEFAULT,
                                                  d3ddm.Format,
                                                  d3dpp.BackBufferFormat,
                                                  d3dpp.AutoDepthStencilFormat);
        if (CheckError(TEXT("Depth format incompatable with desired display mode\r\n")))
            return FALSE;
    }

    // Create the device
    g_errLast = g_pd3d->CreateDevice(g_uiAdapterOrdinal,
                                 D3DMDEVTYPE_DEFAULT,
                                 g_hwndApp,
                                 0,
                                 &d3dpp,
                                 &g_p3ddevice);
    if (CheckError(TEXT("Unable to create device\r\n")))
        return FALSE;


    // Create the global materials
    g_pmatWhite = CreateMaterial(1.0f, 1.0f, 1.0f, 1.0f, 10.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);

    // Set the viewport
    if (!InitViewport())
        return FALSE;

    // Set the default global rendering state
    if (!SetInitialState())
        return FALSE;

    // Create the default light sources
    if (g_nUserLights >= 1)
        AddLight(1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
    if (g_nUserLights >= 2)
        AddLight(0.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f);
    if (g_nUserLights >= 3)
        AddLight(0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f);

    return TRUE;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    InitViewport

Description:

    Initializes the Direct3D viewport to standard setup.

Arguments:

    None

Return Value:

    TRUE on success, FALSE on failure.

-------------------------------------------------------------------*/
static BOOL 
InitViewport()
{
/*    D3DVIEWPORT8 viewData = { 20, 20, 620, 460, 0.0f, 1.0f };
    
    // Now, set the global Direct3D viewport's data
    g_errLast = g_p3ddevice->SetViewport(&viewData);
    if (CheckError(TEXT("Set Viewport")))
        return FALSE;
*/
    return TRUE;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    CreateMaterial

Description:

    Creates a material.

Arguments:

    float rRed   - Red value
    float rGreen - Green value
    float rBlue  - Blue value
    float rAlpha - Alpha value
    float rPower - Material power
    float rSpecR - Specular Red
    float rSpecG - Specular Green
    float rSpecB - Specular Blue
    float rEmisR - Emissive Red (Ambient)
    float rEmisB - Emissive Green (Ambient)
    float rEmisG - Emissve Blue (Ambient)

Return Value:

    Pointer to the created material.

-------------------------------------------------------------------*/
D3DMMATERIAL
CreateMaterial(float rRed,   float rGreen, float rBlue, float rAlpha, float rPower, 
               float rSpecR, float rSpecG, float rSpecB, 
               float rEmisR, float rEmisG, float rEmisB)
{
    D3DMMATERIAL mat;        // Holds Material Information

    // Initialize the Material
    memset(&mat, 0, sizeof(mat));
    mat.Diffuse.r  = D3DM_MAKE_D3DMVALUE(rRed);
    mat.Diffuse.g  = D3DM_MAKE_D3DMVALUE(rGreen);
    mat.Diffuse.b  = D3DM_MAKE_D3DMVALUE(rBlue);
    mat.Diffuse.a  = D3DM_MAKE_D3DMVALUE(rAlpha);
    mat.Ambient.r  = D3DM_MAKE_D3DMVALUE(rEmisR);
    mat.Ambient.g  = D3DM_MAKE_D3DMVALUE(rEmisG);
    mat.Ambient.b  = D3DM_MAKE_D3DMVALUE(rEmisB);
    mat.Ambient.a  = D3DM_MAKE_D3DMVALUE(1.0f);
    mat.Specular.r = D3DM_MAKE_D3DMVALUE(rSpecR);
    mat.Specular.g = D3DM_MAKE_D3DMVALUE(rSpecG);
    mat.Specular.b = D3DM_MAKE_D3DMVALUE(rSpecB);
    mat.Power      = rPower;

    return mat;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    AddLight

Description:

    Creates and adds a light to the global viewport

Arguments:

    float rRed   - Red value
    float rGreen - Green value
    float rBlue  - Blue value
    float rX     - X Direction of the light
    float rY     - Y Direction of the light
    float rZ     - Z Direction of the light

Return Value:

    Pointer to the created material.

-------------------------------------------------------------------*/
BOOL
AddLight(float rRed, float rGreen, float rBlue, float rX, float rY, float rZ)
{
    D3DMLIGHT light;
    memset(&light, 0, sizeof(light));
    light.Type          = D3DMLIGHT_DIRECTIONAL;
    light.Diffuse.r     = D3DM_MAKE_D3DMVALUE(rRed);
    light.Diffuse.g     = D3DM_MAKE_D3DMVALUE(rGreen);
    light.Diffuse.b     = D3DM_MAKE_D3DMVALUE(rBlue);
    light.Diffuse.a     = D3DM_MAKE_D3DMVALUE(1.0f);
    light.Direction.x   = D3DM_MAKE_D3DMVALUE(rX);
    light.Direction.y   = D3DM_MAKE_D3DMVALUE(rY);
    light.Direction.z   = D3DM_MAKE_D3DMVALUE(rZ);

    // Create the global light
    g_errLast = g_p3ddevice->SetLight(g_nLights, &light, D3DMFMT_D3DMVALUE_FLOAT);
    if (CheckError(TEXT("Create Light")))
        return FALSE;
    
    g_errLast = g_p3ddevice->LightEnable(g_nLights, TRUE);
    if (CheckError(TEXT("Enable Light")))
        return FALSE;

    g_nLights++;
    
    return TRUE;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    SetInitialState

Description:

    Sets the default render state and sets up the matrices.

Arguments:

    None

Return Value:

    TRUE on success, FALSE on failure.

-------------------------------------------------------------------*/
static BOOL 
SetInitialState()
{
    D3DMMATRIX matWorld;  // World Matrix
    D3DMMATRIX matView;   // View Matrix
    D3DMMATRIX matProj;   // Projection Matrix
    g_matIdent._11 = D3DM_MAKE_D3DMVALUE(1.0f);
    g_matIdent._12 = D3DM_MAKE_D3DMVALUE(0.0f); 
    g_matIdent._13 = D3DM_MAKE_D3DMVALUE(0.0f); 
    g_matIdent._14 = D3DM_MAKE_D3DMVALUE(0.0f);
                                      
    g_matIdent._21 = D3DM_MAKE_D3DMVALUE(0.0f); 
    g_matIdent._22 = D3DM_MAKE_D3DMVALUE(1.0f); 
    g_matIdent._23 = D3DM_MAKE_D3DMVALUE(0.0f); 
    g_matIdent._24 = D3DM_MAKE_D3DMVALUE(0.0f);
    
    g_matIdent._31 = D3DM_MAKE_D3DMVALUE(0.0f);
    g_matIdent._32 = D3DM_MAKE_D3DMVALUE(0.0f); 
    g_matIdent._33 = D3DM_MAKE_D3DMVALUE(1.0f); 
    g_matIdent._34 = D3DM_MAKE_D3DMVALUE(0.0f);
    
    g_matIdent._41 = D3DM_MAKE_D3DMVALUE(0.0f); 
    g_matIdent._42 = D3DM_MAKE_D3DMVALUE(0.0f); 
    g_matIdent._43 = D3DM_MAKE_D3DMVALUE(0.0f); 
    g_matIdent._44 = D3DM_MAKE_D3DMVALUE(1.0f);

    
    g_vectUp.x = D3DM_MAKE_D3DMVALUE(0.0f); 
    g_vectUp.y = D3DM_MAKE_D3DMVALUE(1.0f); 
    g_vectUp.z = D3DM_MAKE_D3DMVALUE(0.0f);

    if (g_fRasterize)
    {
        g_vectFrom.x = D3DM_MAKE_D3DMVALUE(4.0f);
        g_vectFrom.y = D3DM_MAKE_D3DMVALUE(4.0f); 
        g_vectFrom.z = D3DM_MAKE_D3DMVALUE(4.0f);
        
        g_vectAt.x = D3DM_MAKE_D3DMVALUE(0.0f);
        g_vectAt.y = D3DM_MAKE_D3DMVALUE(0.0f);
        g_vectAt.z = D3DM_MAKE_D3DMVALUE(0.0f);
    }
    else
    {
        g_vectAt.x = D3DM_MAKE_D3DMVALUE(4.0f);
        g_vectAt.y = D3DM_MAKE_D3DMVALUE(4.0f); 
        g_vectAt.z = D3DM_MAKE_D3DMVALUE(4.0f);
        
        g_vectFrom.x = D3DM_MAKE_D3DMVALUE(0.0f);
        g_vectFrom.y = D3DM_MAKE_D3DMVALUE(0.0f);
        g_vectFrom.z = D3DM_MAKE_D3DMVALUE(0.0f);
    }

    g_errLast = g_p3ddevice->BeginScene();
    if (CheckError(TEXT("Begin Scene")))
        return FALSE;

    // Enable Writing to the Z buffer
    g_errLast = g_p3ddevice->SetRenderState(D3DMRS_ZWRITEENABLE, TRUE);
    if (CheckError(TEXT("Set Z Write Enable to TRUE")))
        return FALSE;

    // Set the Z comparison function
    g_errLast = g_p3ddevice->SetRenderState(D3DMRS_ZFUNC, D3DMCMP_LESSEQUAL);
    if (CheckError(TEXT("Set Z Function to D3DMCMP_LESS")))
        return FALSE;

    // Enable Z Buffering
    g_errLast = g_p3ddevice->SetRenderState(D3DMRS_ZENABLE, TRUE);
    if (CheckError(TEXT("Set ZEnable to TRUE")))
        return FALSE;

    // Set the shade mode to Gouraud
    g_errLast = g_p3ddevice->SetRenderState(D3DMRS_SHADEMODE, D3DMSHADE_GOURAUD);
    if (CheckError(TEXT("Set shade mode to D3DMSHADE_GOURAUD")))
        return FALSE;

    // Set the global ambient light level
    g_errLast = g_p3ddevice->SetRenderState(D3DMRS_AMBIENT, D3DMCOLOR_XRGB(64, 64, 64));
    if (CheckError(TEXT("Set Ambient Light")))
        return FALSE;

    // Turn on Perspective Texture Mapping
//    g_errLast = g_p3ddevice->SetRenderState(D3DMRS_TEXTUREPERSPECTIVE, TRUE);
//    if (CheckError(TEXT("Set Texture Perspective to TRUE")))
//        return FALSE;

    // Disable specular lights
    g_p3ddevice->SetRenderState(D3DMRS_SPECULARENABLE,    FALSE);

    // No antialiasing
//    g_p3ddevice->SetRenderState(D3DMRS_ANTIALIAS,         FALSE);

    // No alphablending
    g_p3ddevice->SetRenderState(D3DMRS_ALPHABLENDENABLE,  FALSE);

    // Modulate texture with material
//    g_p3ddevice->SetRenderState(D3DMRS_TEXTUREMAPBLEND,   D3DMTBLEND_MODULATE);

    // Bilinear interpolation
    if (g_devCaps.TextureFilterCaps & D3DMPTFILTERCAPS_MINFLINEAR)
        g_p3ddevice->SetTextureStageState(0, D3DMTSS_MINFILTER, D3DMTEXF_LINEAR);
    if (g_devCaps.TextureFilterCaps & D3DMPTFILTERCAPS_MAGFLINEAR)
        g_p3ddevice->SetTextureStageState(0, D3DMTSS_MAGFILTER, D3DMTEXF_LINEAR);

    // Set default material.
//    g_p3ddevice->SetLightState (D3DMLIGHTSTATE_MATERIAL,           g_hmatWhite);

    // Turn on D3D Lighting
    g_p3ddevice->SetRenderState(D3DMRS_LIGHTING, g_nUserLights == 0 ? FALSE : TRUE);

    // Set the World, View, and Projection matrices
    matWorld = g_matIdent;
    SetViewMatrix(&matView, &g_vectFrom, &g_vectAt, &g_vectUp);
    SetProjectionMatrix(&matProj, g_rNear, g_rFar, g_rFov, g_rAspect);

    // Set D3DIM's Projection matrix
    g_errLast = g_p3ddevice->SetTransform(D3DMTS_PROJECTION, &matProj, D3DMFMT_D3DMVALUE_FLOAT);
    if (CheckError(TEXT("Set Projection Matrix")))
        return FALSE;

    // Set D3DIM's World matrix
    g_errLast = g_p3ddevice->SetTransform(D3DMTS_WORLD, &matWorld, D3DMFMT_D3DMVALUE_FLOAT);
    if (CheckError(TEXT("Set World Matrix")))
        return FALSE;
    
    // Set D3DIM's View matrix
    g_errLast = g_p3ddevice->SetTransform(D3DMTS_VIEW, &matView, D3DMFMT_D3DMVALUE_FLOAT);
    if (CheckError(TEXT("Set View Matrix")))
        return FALSE;

    g_errLast = g_p3ddevice->EndScene();
    if (CheckError(TEXT("End Scene")))
        return FALSE;

    return TRUE;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    SetProjectionMatrix

Description:

    Set a projection matrix with the given
    near and far clipping planes in viewing units
    the fov angle in RADIANS
    and aspect ratio

    Destroys the current contents of the destination matrix.

Arguments:

    D3DMMATRIX * pmatxDest   -   Matrix to receive projection matrix

    float  rNear            -  Distance to near clipping plane

    float  rFar             -  Distance to far clipping plane

    float  rFov             -  whole field of view angle in radians

    float  rAspect          -  The aspect ratio Y/X of the viewing plane

Return Value:

    None

-------------------------------------------------------------------*/
void
SetProjectionMatrix(D3DMMATRIX * pmatxDest, float rNear, float rFar, 
                       float rFov, float rAspect)
{
    float rTfov = (float)(tan(rFov/2.0f));
    float rFFN  = rFar / (rFar - rNear);

    pmatxDest->_11 = D3DM_MAKE_D3DMVALUE(1.0f);
    pmatxDest->_12 = D3DM_MAKE_D3DMVALUE(0.0f);
    pmatxDest->_13 = D3DM_MAKE_D3DMVALUE(0.0f);
    pmatxDest->_14 = D3DM_MAKE_D3DMVALUE(0.0f);

    pmatxDest->_21 = D3DM_MAKE_D3DMVALUE(0.0f);
    pmatxDest->_22 = D3DM_MAKE_D3DMVALUE(1.0f/rAspect);
    pmatxDest->_23 = D3DM_MAKE_D3DMVALUE(0.0f);
    pmatxDest->_24 = D3DM_MAKE_D3DMVALUE(0.0f);

    pmatxDest->_31 = D3DM_MAKE_D3DMVALUE(0.0f);
    pmatxDest->_32 = D3DM_MAKE_D3DMVALUE(0.0f);
    pmatxDest->_33 = D3DM_MAKE_D3DMVALUE(rTfov * rFFN);
    pmatxDest->_34 = D3DM_MAKE_D3DMVALUE(rTfov);

    pmatxDest->_41 = D3DM_MAKE_D3DMVALUE(0.0f);
    pmatxDest->_42 = D3DM_MAKE_D3DMVALUE(0.0f);
    pmatxDest->_43 = D3DM_MAKE_D3DMVALUE(-rNear * rTfov * rFFN);
    pmatxDest->_44 = D3DM_MAKE_D3DMVALUE(0.0f);
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    NormalizeVector

Description:

    Normalizes the vector v to unit length.
    If the vector is degenerate nothing is done

Arguments:

    pvect -  Vector to normalize

Return Value:

    None

-------------------------------------------------------------------*/
void
NormalizeVector(D3DMVECTOR * pvect)
{
    float vx, vy, vz, rInvMod;

    vx = MKFT(pvect->x);
    vy = MKFT(pvect->y);
    vz = MKFT(pvect->z);

    if ((vx != 0) || (vy != 0) || (vz != 0)) 
    {
        rInvMod = (float)(1.0f / sqrt(vx * vx + vy * vy + vz * vz));
        pvect->x = D3DM_MAKE_D3DMVALUE(vx * rInvMod);
        pvect->y = D3DM_MAKE_D3DMVALUE(vy * rInvMod);
        pvect->z = D3DM_MAKE_D3DMVALUE(vz * rInvMod);
    }
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    VectorCrossProduct

Description:

    Calculates cross product of vectors 1 and 2.  vectDest = vect1 x vect2

Arguments:

    pvectDest -  Pointer to vector to receive result

    pvect1    -  Pointer to vector 1

    pvect2    -  Pointer to vector 2

Return Value:

    Pointer to result - pD

-------------------------------------------------------------------*/
void
VectorCrossProduct(D3DMVECTOR * pvectDest, D3DMVECTOR * pvect1, D3DMVECTOR * pvect2)
{
    pvectDest->x = D3DM_MAKE_D3DMVALUE(MKFT(pvect1->y) * MKFT(pvect2->z) - MKFT(pvect1->z) * MKFT(pvect2->y));
    pvectDest->y = D3DM_MAKE_D3DMVALUE(MKFT(pvect1->z) * MKFT(pvect2->x) - MKFT(pvect1->x) * MKFT(pvect2->z));
    pvectDest->z = D3DM_MAKE_D3DMVALUE(MKFT(pvect1->x) * MKFT(pvect2->y) - MKFT(pvect1->y) * MKFT(pvect2->x));
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    SetMatrixRotation

Description:

    Set the rotation part of a matrix such that the vector pvectDir is the new
    z-axis and pvectUp is the new y-axis.
    NOTE:  The function only modifies the upper left 3x3 of the matrix.  Row 4 and column 4 are untouched.

Arguments:

    pmatxDest - Pointer to matrix to be modified.

    pvectDir - Pointer to a vector defining the Z-axis (Direction)

    pvectUp - Pointer to a vector defining the Y- Axis (Up)

Return Value:

   None

-------------------------------------------------------------------*/
void
SetMatrixRotation(D3DMMATRIX * pmatxDest, D3DMVECTOR * pvectDir, D3DMVECTOR * pvectUp)
{
    float t;
    D3DMVECTOR d, u, r;

    // Normalize the direction vector.
    d.x = pvectDir->x;
    d.y = pvectDir->y;
    d.z = pvectDir->z;
    NormalizeVector(&d);

    u.x = pvectUp->x;
    u.y = pvectUp->y;
    u.z = pvectUp->z;

    // Project u into the plane defined by d and normalize.
    t = MKFT(u.x) * MKFT(d.x) + MKFT(u.y) * MKFT(d.y) + MKFT(u.z) * MKFT(d.z);
    u.x = D3DM_MAKE_D3DMVALUE(MKFT(u.x) - MKFT(d.x) * t);
    u.y = D3DM_MAKE_D3DMVALUE(MKFT(u.y) - MKFT(d.y) * t);
    u.z = D3DM_MAKE_D3DMVALUE(MKFT(u.z) - MKFT(d.z) * t);
    NormalizeVector(&u);

    // Calculate the vector pointing along the matrix x axis (in a right
    // handed coordinate system) using cross product.
    VectorCrossProduct(&r, &u, &d);

    pmatxDest->_11 = D3DM_MAKE_D3DMVALUE(r.x);
    pmatxDest->_12 = D3DM_MAKE_D3DMVALUE(r.y);
    pmatxDest->_13 = D3DM_MAKE_D3DMVALUE(r.z);

    pmatxDest->_21 = D3DM_MAKE_D3DMVALUE(u.x);
    pmatxDest->_22 = D3DM_MAKE_D3DMVALUE(u.y);
    pmatxDest->_23 = D3DM_MAKE_D3DMVALUE(u.z);

    pmatxDest->_31 = D3DM_MAKE_D3DMVALUE(d.x);
    pmatxDest->_32 = D3DM_MAKE_D3DMVALUE(d.y);
    pmatxDest->_33 = D3DM_MAKE_D3DMVALUE(d.z);
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    SubtractVectors

Description:

    Subtracts point 1 from point 1 giving Vector V.

    NOTE:  Points and vectors should be different types because they are mathematically distinct
           but D3D does not have that concept so D3DMVECTOR is used to represent points.

Arguments:

   D3DMVECTOR * pvectDest  -  vector to reseive result

   D3DMVECTOR *  pvectSrc1  - vector representing point 1.

   D3DMVECTOR *  pvectSrc2  - vector representing point 2.

Return Value:

   None

-------------------------------------------------------------------*/
void
SubtractVectors(D3DMVECTOR * pvectDest, D3DMVECTOR * pvectSrc1, D3DMVECTOR * pvectSrc2)
{
    pvectDest->x = D3DM_MAKE_D3DMVALUE(MKFT(pvectSrc1->x) - MKFT(pvectSrc2->x));
    pvectDest->y = D3DM_MAKE_D3DMVALUE(MKFT(pvectSrc1->y) - MKFT(pvectSrc2->y));
    pvectDest->z = D3DM_MAKE_D3DMVALUE(MKFT(pvectSrc1->z) - MKFT(pvectSrc2->z));
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    TransposeMatrix

Description:

   Transposes a matrix.  Transpose is equivalent to Inverse for pure
   rotation matrices.

Arguments:

  D3DMMATRIX * pmatxDest  - Destination matrix

  D3DMMATRIX * pmatxSrc   - Source matrix

Return Value:

   None

-------------------------------------------------------------------*/
void
TransposeMatrix(D3DMMATRIX * pmatxDest, D3DMMATRIX * pmatxSrc)
{
    D3DMMATRIX matxTemp;

    matxTemp._11 = D3DM_MAKE_D3DMVALUE(pmatxSrc->_11);
    matxTemp._12 = D3DM_MAKE_D3DMVALUE(pmatxSrc->_21);
    matxTemp._13 = D3DM_MAKE_D3DMVALUE(pmatxSrc->_31);
    matxTemp._14 = D3DM_MAKE_D3DMVALUE(pmatxSrc->_41);

    matxTemp._21 = D3DM_MAKE_D3DMVALUE(pmatxSrc->_12);
    matxTemp._22 = D3DM_MAKE_D3DMVALUE(pmatxSrc->_22);
    matxTemp._23 = D3DM_MAKE_D3DMVALUE(pmatxSrc->_32);
    matxTemp._24 = D3DM_MAKE_D3DMVALUE(pmatxSrc->_42);

    matxTemp._31 = D3DM_MAKE_D3DMVALUE(pmatxSrc->_13);
    matxTemp._32 = D3DM_MAKE_D3DMVALUE(pmatxSrc->_23);
    matxTemp._33 = D3DM_MAKE_D3DMVALUE(pmatxSrc->_33);
    matxTemp._34 = D3DM_MAKE_D3DMVALUE(pmatxSrc->_43);

    matxTemp._41 = D3DM_MAKE_D3DMVALUE(pmatxSrc->_14);
    matxTemp._42 = D3DM_MAKE_D3DMVALUE(pmatxSrc->_24);
    matxTemp._43 = D3DM_MAKE_D3DMVALUE(pmatxSrc->_34);
    matxTemp._44 = D3DM_MAKE_D3DMVALUE(pmatxSrc->_44);

    *pmatxDest = matxTemp;
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    SetViewMatrix

Description:

    Sets the viewing matrix such that
    from is at the origin
    at   is on the positive Z axis
    up   is in the Y-Z plane

Arguments:

   D3DMMATRIX * pmatxDest   -  Pointer to matrix to receive view matrix

   D3DMVECTOR * pvectFrom   -  Location of the eye

   D3DMVECTOR * pvectAt     -  Point looked at

   D3DMVECTOR * pvectUp     -  Vector describing the up direction.  Cannot be parallel to (At - From)

Return Value:

   None

-------------------------------------------------------------------*/
void
SetViewMatrix(D3DMMATRIX * pmatxDest, D3DMVECTOR * pvectFrom, D3DMVECTOR * pvectAt, D3DMVECTOR * pvectUp)
{
    D3DMVECTOR los;              // Line of sight

    SubtractVectors(&los, pvectAt, pvectFrom);

    // Generate the rotation part of the matrix
    SetMatrixRotation(pmatxDest, &los, pvectUp);

    // Make sure the matrix is completely initialized
    pmatxDest->_14 = D3DM_MAKE_D3DMVALUE(0.0f);
    pmatxDest->_24 = D3DM_MAKE_D3DMVALUE(0.0f);
    pmatxDest->_34 = D3DM_MAKE_D3DMVALUE(0.0f);
    pmatxDest->_41 = D3DM_MAKE_D3DMVALUE(0.0f);
    pmatxDest->_42 = D3DM_MAKE_D3DMVALUE(0.0f);
    pmatxDest->_43 = D3DM_MAKE_D3DMVALUE(0.0f);
    pmatxDest->_44 = D3DM_MAKE_D3DMVALUE(1.0f);

    TransposeMatrix(pmatxDest, pmatxDest);

    // Preconcatenate translational component
    pmatxDest->_41 = D3DM_MAKE_D3DMVALUE(-(MKFT(pvectFrom->x) * MKFT(pmatxDest->_11) + MKFT(pvectFrom->y) * MKFT(pmatxDest->_21) + MKFT(pvectFrom->z) * MKFT(pmatxDest->_31)));
    pmatxDest->_42 = D3DM_MAKE_D3DMVALUE(-(MKFT(pvectFrom->x) * MKFT(pmatxDest->_12) + MKFT(pvectFrom->y) * MKFT(pmatxDest->_22) + MKFT(pvectFrom->z) * MKFT(pmatxDest->_32)));
    pmatxDest->_43 = D3DM_MAKE_D3DMVALUE(-(MKFT(pvectFrom->x) * MKFT(pmatxDest->_13) + MKFT(pvectFrom->y) * MKFT(pmatxDest->_23) + MKFT(pvectFrom->z) * MKFT(pmatxDest->_33)));
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    D3DMMATRIXSetVRotation

Description:

   Creates a matrix for rotation of angle radians about an arbitrary axis.
   Destroys the current contents of the destination matrix.

Arguments:

   OUT pM      - Matrix to receive rotation matrix

   IN  pAxis   - Axis to rotate about.  Normalized before use.

   IN  angle   - Angle in radians to rotate about the axis.

Return Value:

   pM
-------------------------------------------------------------------*/
void
SetMatrixAxisRotation(D3DMMATRIX * pM, D3DMVECTOR * pvectAxis, float rAngle)
{
    D3DMVECTOR vectAxis = *pvectAxis;

    float c = (float)(cos(rAngle));
    float s = (float)(sin(rAngle));
    float t = 1.0f - c;

    NormalizeVector(&vectAxis);
    pM->_11 = D3DM_MAKE_D3DMVALUE(t * MKFT(vectAxis.x) * MKFT(vectAxis.x) + c);
    pM->_12 = D3DM_MAKE_D3DMVALUE(t * MKFT(vectAxis.x) * MKFT(vectAxis.y) + s * MKFT(vectAxis.z));
    pM->_13 = D3DM_MAKE_D3DMVALUE(t * MKFT(vectAxis.x) * MKFT(vectAxis.z) - s * MKFT(vectAxis.y));
    pM->_14 = D3DM_MAKE_D3DMVALUE(0.0f);

    pM->_21 = D3DM_MAKE_D3DMVALUE(t * MKFT(vectAxis.y) * MKFT(vectAxis.x) - s * MKFT(vectAxis.z));
    pM->_22 = D3DM_MAKE_D3DMVALUE(t * MKFT(vectAxis.y) * MKFT(vectAxis.y) + c);
    pM->_23 = D3DM_MAKE_D3DMVALUE(t * MKFT(vectAxis.y) * MKFT(vectAxis.z) + s * MKFT(vectAxis.x));
    pM->_24 = D3DM_MAKE_D3DMVALUE(0.0f);

    pM->_31 = D3DM_MAKE_D3DMVALUE(t * MKFT(vectAxis.z) * MKFT(vectAxis.x) + s * MKFT(vectAxis.y));
    pM->_32 = D3DM_MAKE_D3DMVALUE(t * MKFT(vectAxis.z) * MKFT(vectAxis.y) - s * MKFT(vectAxis.x));
    pM->_33 = D3DM_MAKE_D3DMVALUE(t * MKFT(vectAxis.z) * MKFT(vectAxis.z) + c);
    pM->_34 = D3DM_MAKE_D3DMVALUE(0.0f);

    pM->_41 = D3DM_MAKE_D3DMVALUE(0.0f);
    pM->_42 = D3DM_MAKE_D3DMVALUE(0.0f);
    pM->_43 = D3DM_MAKE_D3DMVALUE(0.0f);
    pM->_44 = D3DM_MAKE_D3DMVALUE(1.0f);
}
