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

#include "printhandler.h"
#include <clparse.h>
#include "global.h"
#include "commctrl.h"

static const TCHAR  *gszClassName = TEXT("GDIPrint");
static const TCHAR  *gszWindowName = TEXT("GDIPrint test suite");

CPrintHandler::CPrintHandler()
{

    m_tszDeviceName = NULL;
    m_tszDriverName = NULL;
    m_tszOutputName = NULL;

    m_hdcPrint = NULL;
    m_hdcVideoMemory = NULL;
    m_hbmpVideoMemory = NULL;
    m_hbmpVideoMemoryStock = NULL;
    m_nWidth = 0;
    m_nHeight = 0;
    m_nCurrentX = 0;
    m_nCurrentY = 0;
    m_nLeftMargin = 0;
    m_nTopMargin = 0;
    m_nPageWidth = 0;
    m_nPageHeight = 0;
    m_nVidWidth = 0;
    m_nVidHeight = 0;

    m_hwndPrimary = NULL;
    m_hdcPrimary = NULL;
    m_nPrimaryWidth = 0;
    m_nPrimaryHeight = 0;
    m_nPrimaryTop = 0;
    m_nPrimaryLeft = 0;
    
    m_tszTestInstructions = NULL;
    m_nInstructionHeight = 0;
    m_bDrawToPrimary = false;
    m_bInitialized = false;
}

CPrintHandler::~CPrintHandler()
{
    // cleanup will handle the cleanup in the case where
    // it wasn't called normally, calling it twice doesn't hurt anything.
    Cleanup();
}

bool
CPrintHandler::SetPrintHandlerParams(PHPARAMS *php)
{
    int nStringLength;

    m_bDrawToPrimary = php->bDrawToPrimary;

    // copy over our devmode
    memcpy(&m_dm, &(php->dm), sizeof(DEVMODE));

    if(php->tszDriverName)
    {
        // copy over the device strings
        nStringLength = _tcslen(php->tszDriverName);
        // if we have a non-zero string length allocate, otherwise
        // we could end up with a NULL string instead of a NULL pointer.
        // API calls will fail with a NULL string giving little information,
        // however the test outputs useful information
        // for the NULL pointer case.
        if(nStringLength)
        {
            CheckNotRESULT(NULL, m_tszDriverName = new(TCHAR[nStringLength + 1]));
            if(m_tszDriverName)
               _tcsncpy_s(m_tszDriverName, nStringLength + 1, php->tszDriverName, nStringLength);
        }
    }

    if(php->tszDeviceName)
    {
        CheckNotRESULT(NULL, nStringLength = _tcslen(php->tszDeviceName));
        if(nStringLength)
        {
            CheckNotRESULT(NULL, m_tszDeviceName = new(TCHAR[nStringLength + 1]));
            if(m_tszDeviceName)
                _tcsncpy_s(m_tszDeviceName, nStringLength + 1, php->tszDeviceName, nStringLength);
        }
    }

    if(php->tszOutputName)
    {
        nStringLength = _tcslen(php->tszOutputName);
        if(nStringLength)
        {
            CheckNotRESULT(NULL, m_tszOutputName = new(TCHAR[nStringLength + 1]));
            if(m_tszOutputName)
                _tcsncpy_s(m_tszOutputName, nStringLength +1, php->tszOutputName, nStringLength);
        }
    }
    return true;
}

bool
CPrintHandler::InitializeMainWindow()
{

    WNDCLASS wc;
    RECT rcWorkArea;

    wc.style = 0;
    wc.lpfnWndProc = (WNDPROC) DefWindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = globalInst;
    wc.hIcon = 0;
    wc.hCursor = 0;
    wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName = 0;
    wc.lpszClassName = gszClassName;


    RegisterClass(&wc);

    SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWorkArea, 0);

    m_nPrimaryLeft = rcWorkArea.left;
    m_nPrimaryTop = rcWorkArea.top;
    m_nPrimaryWidth = rcWorkArea.right - rcWorkArea.left;
    m_nPrimaryHeight = rcWorkArea.bottom - rcWorkArea.top;

    CheckNotRESULT(NULL, m_hwndPrimary = CreateWindow(gszClassName, gszWindowName, WS_POPUP | WS_VISIBLE | WS_BORDER | WS_CAPTION, 
        m_nPrimaryLeft, m_nPrimaryTop, m_nPrimaryWidth, m_nPrimaryHeight, 
        NULL, NULL, globalInst, NULL));

    CheckNotRESULT(FALSE, SetForegroundWindow(m_hwndPrimary));
    ShowWindow(m_hwndPrimary, SW_SHOWNORMAL);
    CheckNotRESULT(FALSE, UpdateWindow(m_hwndPrimary));

    CheckNotRESULT(NULL, m_hdcPrimary = GetDC(m_hwndPrimary));

    CheckNotRESULT(FALSE, GetWindowRect(m_hwndPrimary, &rcWorkArea));
    m_nPrimaryLeft = rcWorkArea.left;
    m_nPrimaryTop = rcWorkArea.top;
    m_nPrimaryWidth = rcWorkArea.right - rcWorkArea.left;
    m_nPrimaryHeight = rcWorkArea.bottom - rcWorkArea.top;

    CheckNotRESULT(FALSE, PatBlt(m_hdcPrimary, m_nPrimaryLeft, m_nPrimaryTop, m_nPrimaryWidth, m_nPrimaryHeight, WHITENESS));

    return(m_hdcPrimary != NULL);
}

bool
CPrintHandler::Initialize()
{
    typedef BOOL (WINAPI * PFNPAGESETUPDLG)(PAGESETUPDLG *lppsd);
    typedef void (WINAPI * PFNINITCOMMONCONTROLS)(void);

    HINSTANCE hinstCommDlg = NULL;
    HINSTANCE hinstCommCtrl = NULL;
    PFNPAGESETUPDLG pfnPageSetupDlg = NULL;
    PFNINITCOMMONCONTROLS pfnInitCommonControls = NULL;
    PAGESETUPDLG psd;
    DOCINFO di;
    bool bSetupVIACommandLine = true;

    memset(&di, 0, sizeof(DOCINFO));
    di.cbSize = sizeof(DOCINFO);
    di.lpszDocName = gszWindowName;

    InitializeMainWindow();

    if(!m_tszDriverName && !m_tszDeviceName && !m_tszOutputName)
    {
        // the information wasn't given in the command line, so we try for the dialog.
        bSetupVIACommandLine = false;
        hinstCommDlg = LoadLibrary(TEXT("CommDlg.dll"));
        if(hinstCommDlg)
        {
            pfnPageSetupDlg = (PFNPAGESETUPDLG) GetProcAddress(hinstCommDlg, TEXT("PageSetupDlgW"));

            if(pfnPageSetupDlg)
            {

                hinstCommCtrl = LoadLibrary(TEXT("CommCtrl.dll"));
                if(hinstCommCtrl)
                {
                    pfnInitCommonControls = (PFNINITCOMMONCONTROLS) GetProcAddress(hinstCommCtrl, TEXT("InitCommonControls"));

                    if(pfnInitCommonControls)
                    {
                        pfnInitCommonControls();

                        memset(&psd, 0, sizeof(PAGESETUPDLG));
                        psd.lStructSize = sizeof(PAGESETUPDLG);
                        psd.hwndOwner = m_hwndPrimary;

                        info(DETAIL, TEXT("Please input print options in the print dialog on the device"));

                        if(pfnPageSetupDlg(&psd))
                        {
                            memcpy(&m_dm, (DEVMODE *) (psd.hDevMode), sizeof(DEVMODE));
                            // if the dev names are set, then they all should be set and valid.
                            if(psd.hDevNames)
                            {
                                int nStringLength = _tcslen((TCHAR *) ((BYTE *) psd.hDevNames + ((DEVNAMES *) psd.hDevNames)->wDriverOffset)) + 1;
                                if(nStringLength > 0)
                                {
                                    CheckNotRESULT(NULL, m_tszDriverName = new(TCHAR[nStringLength]));
                                    if(m_tszDriverName)
                                        _tcsncpy_s(m_tszDriverName, nStringLength, ((TCHAR *) ((BYTE *) psd.hDevNames + ((DEVNAMES *) psd.hDevNames)->wDriverOffset)), nStringLength-1);
                                }

                                nStringLength = _tcslen((TCHAR *) ((BYTE *) psd.hDevNames + ((DEVNAMES *) psd.hDevNames)->wDeviceOffset)) + 1;
                                if(nStringLength > 0)
                                {
                                    CheckNotRESULT(NULL, m_tszDeviceName = new(TCHAR[nStringLength]));
                                    if(m_tszDeviceName)
                                        _tcsncpy_s(m_tszDeviceName, nStringLength, (TCHAR *) ((BYTE *) psd.hDevNames + ((DEVNAMES *) psd.hDevNames)->wDeviceOffset), nStringLength-1);
                                }

                                nStringLength = _tcslen((TCHAR *) ((BYTE *) psd.hDevNames + ((DEVNAMES *) psd.hDevNames)->wOutputOffset)) + 1;
                                if(nStringLength > 0)
                                {
                                    CheckNotRESULT(NULL, m_tszOutputName = new(TCHAR[nStringLength]));
                                    if(m_tszOutputName)
                                        _tcsncpy_s(m_tszOutputName, nStringLength, (TCHAR *) ((BYTE *) psd.hDevNames + ((DEVNAMES *) psd.hDevNames)->wOutputOffset), nStringLength-1);
                                }
                            }
                            else info(FAIL, TEXT("PageSetupDlg failed to configure devnames structure"));
                        }
                        else info(FAIL, TEXT("The page setup dialog call failed, failed to retrieve information about the printer in use."));
                    }
                    else info(FAIL, TEXT("Failed to retrieve the InitCommonControls function pointer, please specify all printer driver parameters via the command line."));

                    FreeLibrary(hinstCommCtrl);
                    hinstCommCtrl = NULL;
                    pfnInitCommonControls = NULL;
                }
                else info(FAIL, TEXT("Failed to load the CommCtrl library, please specify all printer driver parameters via the command line."));
            }
            else info(FAIL, TEXT("Failed to retrieve the page setup dialog, if this is a headless image, please specify all printer driver parameters via the command line."));

            // now that we're completely finished with the dialog, we can free it.
            FreeLibrary(hinstCommDlg);
            hinstCommDlg = NULL;
            pfnPageSetupDlg = NULL;
        }
        else info(FAIL, TEXT("Failed to load the CommDlg library, if this is a headless image, please specify all printer driver parameters via the command line."));
    }

    // otherwise this was all setup during the command line parse and should be correct and usable.

    if(m_tszDriverName && m_tszDeviceName && m_tszOutputName)
    {
        // create our printer dc using the devmode created from either the PageSetupDlg,
        // or given by the user.
        if(NULL == (m_hdcPrint = CreateDC(m_tszDriverName, m_tszDeviceName, m_tszOutputName, &m_dm)))
            info(FAIL, TEXT("Failed to create the printer device."));

        // if we're drawing to the primary, then the start/end test cases will handle starting and
        // stopping the page and document, otherwise there's always a page started and we use a single document.
        if(!m_bDrawToPrimary)
        {
            // start the document
            CheckNotRESULT(FALSE, StartDoc(m_hdcPrint, &di));

            CheckNotRESULT(FALSE, StartPage(m_hdcPrint));
        }

        // retrieve the margins
        m_nLeftMargin = GetDeviceCaps (m_hdcPrint, PHYSICALOFFSETX);
        m_nTopMargin = GetDeviceCaps (m_hdcPrint, PHYSICALOFFSETY);
        m_nCurrentX = m_nLeftMargin;
        m_nCurrentY = m_nTopMargin;

        // set up the physical parameters (compensating for the paper margins).
        m_nPageWidth = GetDeviceCaps (m_hdcPrint, PHYSICALWIDTH);
        m_nPageHeight = GetDeviceCaps (m_hdcPrint, PHYSICALHEIGHT);

        m_nWidth = m_nPageWidth - 2 * m_nLeftMargin;
        m_nHeight = m_nPageHeight - 2 * m_nTopMargin;

        // clear out the rementents of the dialog on the primary
        CheckNotRESULT(FALSE, PatBlt(m_hdcPrimary, m_nPrimaryLeft, m_nPrimaryTop, m_nPrimaryWidth, m_nPrimaryHeight, WHITENESS));

        // create the offscreen dc, this is always used to compose images which are then blitted to the printer dc
        // which results in a different code path for images that show up on the left/right sides of the page.
        CheckNotRESULT(NULL, m_hdcVideoMemory = CreateCompatibleDC(m_hdcPrimary));

        // clear out the video memory surface before use.
        CheckNotRESULT(FALSE, PatBlt(m_hdcVideoMemory, 0, 0, m_nPageWidth/2, m_nPageHeight, WHITENESS));

        // if all of the surfaces were created and everything was set up, then we're initialized.
        // if we're not drawing to the primary, then the creation of the video memory surfaces don't matter.
        // if we are drawing to the primary, then verify that those surfaces were allocated correctly.
        if(m_hdcPrimary && m_hdcPrint && m_hdcVideoMemory)
            m_bInitialized = true;
        else info(FAIL, TEXT("Creation of one or more surfaces needed for testing failed."));
    }

    if(!m_tszDriverName && bSetupVIACommandLine)
        info(FAIL, TEXT("No driver name specified, please specify a driver name"));
    if(!m_tszDeviceName && bSetupVIACommandLine)
        info(FAIL, TEXT("No device name specified, please specify a device name"));
    if(!m_tszOutputName && bSetupVIACommandLine)
        info(FAIL, TEXT("No output name specified, please specify an output name"));

    return m_bInitialized;
}

bool
CPrintHandler::Cleanup()
{

    // cleanup our allocated character strings
    if(m_tszDriverName)
    {
        delete []m_tszDriverName;
        m_tszDriverName = NULL;
    }
    if(m_tszDeviceName)
    {
        delete[] m_tszDeviceName;
        m_tszDeviceName = NULL;
    }
    if(m_tszOutputName)
    {
        delete[] m_tszOutputName;
        m_tszOutputName = NULL;
    }

    // if the printer dc was created, release it.
    if(m_hdcPrint)
    {
        // if we're drawing to the primary, every test case gets it's own page.
        if(!m_bDrawToPrimary)
        {
            // the last test case doesn't know that it's the last case, so we end the document here.
            CheckNotRESULT(FALSE, EndPage(m_hdcPrint));
            CheckNotRESULT(FALSE, EndDoc(m_hdcPrint));
        }

        CheckNotRESULT(FALSE, DeleteDC(m_hdcPrint));
        m_hdcPrint = NULL;
    }

    // if the video memory selection succeeded, deselect it
    if(m_hbmpVideoMemoryStock)
    {
        CheckNotRESULT(NULL, SelectObject(m_hdcVideoMemory, m_hbmpVideoMemoryStock));
        m_hbmpVideoMemoryStock = NULL;
    }

    // if the video memory bitmap was allocated, free it (it's already been deselected if it was selected)
    if(m_hbmpVideoMemory)
    {
        CheckNotRESULT(FALSE, DeleteObject(m_hbmpVideoMemory));
        m_hbmpVideoMemory = NULL;
    }

    // if the video memory dc was created, free it.
    if(m_hdcVideoMemory)
    {
        CheckNotRESULT(FALSE, DeleteDC(m_hdcVideoMemory));
        m_hdcVideoMemory = NULL;
    }

    CleanupMainWindow();

    return true;
}

bool
CPrintHandler::CleanupMainWindow()
{

    if(m_hdcPrimary)
    {
        CheckNotRESULT(FALSE, ReleaseDC(m_hwndPrimary, m_hdcPrimary));
        m_hdcPrimary = NULL;
    }

    if(m_hwndPrimary)
    {
        CheckNotRESULT(FALSE, DestroyWindow(m_hwndPrimary));
        m_hwndPrimary = NULL;
    }

    UnregisterClass(gszClassName, globalInst);

    return true;
}

bool
CPrintHandler::SetTestDescription(TCHAR *tszInstructions)
{
    bool bRval = false;

    if(m_bInitialized)
    {
        if(tszInstructions)
        {
            // store the test description
            m_tszTestInstructions = tszInstructions;
            info(DETAIL, TEXT("%s"), m_tszTestInstructions);
            bRval = true;
        }
        else info(FAIL, TEXT("No test description given."));
    }
    else info(ABORT, TEXT("Failed to set the test description, printer not intialized.  Does the image support printing?"));

    return bRval;
}

bool
CPrintHandler::TestStart( int nWidth, int nHeight)
{
    bool bRval = false;

    if(m_bInitialized)
    {
        RECT rc;

        m_nVidWidth = nWidth;
        m_nVidHeight = nHeight;

        if(m_hbmpVideoMemoryStock)
        {
            CheckNotRESULT(NULL, SelectObject(m_hdcVideoMemory, m_hbmpVideoMemoryStock));
            m_hbmpVideoMemoryStock = NULL;
        }

        if(m_hbmpVideoMemory)
            CheckNotRESULT(FALSE, DeleteObject(m_hbmpVideoMemory));

        // create our verification bitmap based on the side the user specified
        CheckNotRESULT(NULL, m_hbmpVideoMemory = CreateCompatibleBitmap(m_hdcPrimary, m_nVidWidth, m_nVidHeight));
        CheckNotRESULT(NULL, m_hbmpVideoMemoryStock = (HBITMAP) SelectObject(m_hdcVideoMemory, m_hbmpVideoMemory));

        // clear out the video memory surface for the next case
        CheckNotRESULT(FALSE, PatBlt(m_hdcVideoMemory, 0, 0, m_nVidWidth, m_nVidHeight, WHITENESS));

        // this is used to calculate how many lines will be needed 
        rc.left = m_nLeftMargin;
        rc.top = m_nTopMargin;
        rc.right = m_nWidth;
        rc.bottom = m_nHeight;
        CheckNotRESULT(0, DrawText(m_hdcPrint, m_tszTestInstructions, -1, &rc, DT_CALCRECT | DT_WORDBREAK));
        m_nInstructionHeight = rc.bottom - rc.top;

        if(m_bDrawToPrimary)
        {
            // A docinfo needed for ending and restarting the document, so the page ejects.
            DOCINFO di;

            memset(&di, 0, sizeof(DOCINFO));
            di.cbSize = sizeof(DOCINFO);
            di.lpszDocName = gszWindowName;

            // restart the doc, because when running with primary verification the doc is started and
            // stopped for each test case.
            CheckNotRESULT(FALSE, StartDoc(m_hdcPrint, &di));

            // When drawing to the primary, every test case gets a separate page.
            CheckNotRESULT(FALSE, StartPage(m_hdcPrint));
        }

        // if the requested width or height are larger than the page, then we cannot do anything.
        // if they're less than or equal to the page size, then see if we need to start a new page.
        if(nWidth <= m_nWidth && nHeight <= m_nHeight)
        {
            // the sizes are acceptable
            bRval = true;

            // if the requested with or height will put the operation outside of the current page,
            // then start a new page.
            if(nWidth + m_nCurrentX > m_nWidth || nHeight + m_nCurrentY + m_nInstructionHeight > m_nHeight)
            {
                // in theory, this will never be hit when we're running on the primary, because
                // each test case has it's own page.
                // so this is the case where our running document needs a new page.
                CheckNotRESULT(FALSE, EndPage(m_hdcPrint));

                // now that we're starting a new test, start a new page.
                CheckNotRESULT(FALSE, StartPage(m_hdcPrint));

                m_nCurrentX = m_nLeftMargin;
                m_nCurrentY = m_nTopMargin;
            }

            // print out the test instructions
            rc.left = m_nCurrentX;
            rc.top = m_nCurrentY;
            rc.bottom = m_nCurrentY + m_nInstructionHeight;
            rc.right = m_nWidth;
            CheckNotRESULT(0, DrawText(m_hdcPrint, m_tszTestInstructions, -1, &rc, DT_WORDBREAK));
            m_nCurrentY += m_nInstructionHeight;
        }
    }

    return bRval;
}

bool
CPrintHandler::GetDCData(DCDATA *dcd)
{
    bool bRval = false;

    if(m_bInitialized)
    {
        dcd->hdcPrint = m_hdcPrint;
        dcd->hdcVideoMemory = m_hdcVideoMemory;
        dcd->nCurrentX = m_nCurrentX;
        dcd->nCurrentY = m_nCurrentY;
        dcd->nHeight = m_nHeight;
        dcd->nWidth = m_nWidth / 2;
        // currently, the vid memory surface is 0 based
        dcd->nVidCurrentX = 0;
        dcd->nVidCurrentY = 0;
        dcd->nVidWidth = m_nVidWidth;
        dcd->nVidHeight = m_nVidHeight;
        bRval = true;
    }

    return bRval;
}

bool
CPrintHandler::TestFinished()
{
    bool bRval = false;
    RECT rcText = {m_nPrimaryLeft, m_nPrimaryTop, 
                            m_nPrimaryWidth + m_nPrimaryLeft, m_nPrimaryHeight + m_nPrimaryTop};
    int nDestHeight = m_nPrimaryHeight;
    int nMBResult;

    if(m_bInitialized)
    {
        // Everything here succeeds, unless otherwise set to false.
        bRval = true;

        // this test is finished, so send out it's verification bitmap data.
        CheckNotRESULT(FALSE, BitBlt(m_hdcPrint, m_nPageWidth/2, m_nCurrentY, m_nVidWidth, m_nVidHeight, m_hdcVideoMemory, 0, 0, SRCCOPY));

        // set the new current location
        // don't modify the current x location, just the y location
        //m_nCurrentX = nCurrentLocationX;
        // our new y location is the old location plus the test that just finished height.
        m_nCurrentY += m_nVidHeight;

        // if drawing to the primary
        if(m_bDrawToPrimary)
        {
            // clear out the data from the previous test.
            CheckNotRESULT(FALSE, PatBlt(m_hdcPrimary, 0, 0, m_nPrimaryWidth, m_nPrimaryHeight, WHITENESS));

            // draw the instructions
            CheckNotRESULT(0, DrawText(m_hdcPrimary, m_tszTestInstructions, -1, &rcText, DT_CALCRECT | DT_WORDBREAK));
            // no need to manipulate left and top.
            rcText.top = m_nPrimaryHeight + m_nPrimaryTop - rcText.bottom;
            rcText.bottom = m_nPrimaryHeight + m_nPrimaryTop;

            CheckNotRESULT(0, DrawText(m_hdcPrimary, m_tszTestInstructions, -1, &rcText, DT_WORDBREAK));

            // calculate the available space
            nDestHeight = m_nPrimaryHeight - (rcText.bottom - rcText.top);

            // use the smaller of the height of the primary minus the text comments at the bottom, or the height of the
            // bitmap from the user.
            CheckNotRESULT(FALSE, StretchBlt(m_hdcPrimary, 0, 0, min(m_nVidWidth, m_nPrimaryWidth), min(m_nVidHeight, nDestHeight), m_hdcVideoMemory, 0, 0, m_nVidWidth, m_nVidHeight, SRCCOPY));

            // When we're drawing to the primary, every test case gets it's own page, so we're finished with this page.
            CheckNotRESULT(FALSE, EndPage(m_hdcPrint));

            // end the doc and restart it so the page is ejected.
            // the next case will start a new doc if we're running on the primary,
            // otherwise the test only uses 1 doc and pages only as needed.
            CheckNotRESULT(FALSE, EndDoc(m_hdcPrint));

            // now that the EndPage has been called, the page should be printed.  Ask the user if it printed correctly.
            nMBResult = MessageBox(m_hwndPrimary, TEXT("Does the description match the result?"), TEXT("Test input"), MB_YESNO);

            if(nMBResult == IDYES)
                info(DETAIL, TEXT("The user indicated the test passed."));
            else if(nMBResult == IDNO)
                info(FAIL, TEXT("The user indicated the test failed."));
            else info(ABORT, TEXT("The message box call failed."));

            m_nCurrentX = m_nLeftMargin;
            m_nCurrentY = m_nTopMargin;
        }
    }
    return bRval;
}

