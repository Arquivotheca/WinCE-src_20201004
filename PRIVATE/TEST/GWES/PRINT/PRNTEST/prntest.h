//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#define IDC_EDIT_1   400        // QueryDialog: use both for net path and pages #

#define IDC_STATIC1    500      // for PerfTest dialog

#define USE_IRDA         0
#define USE_SERIAL_57600 1
#define USE_SERIAL_9600  2
#define USE_PARALLEL     3
#define USE_NETWORK      4

#define FULL_QUALITY   0x100    // same position as PRN_SELECT_QUALITY    0x00000F00
#define USE_COLOR   0x100000    // same position as PRN_SELECT_COLOR      0x00F00000


#define IDS_String_FIRST       100
#define IDS_String_MS_LONG     100
#define IDS_String_DF_LONG     101
#define IDS_String_2           102
#define IDS_String_3           103
#define IDS_String_4           104
#define IDS_String_5           105
#define IDS_String_PerfTest    106
#define IDS_String_PerfPages   107

#define PRN_SELECT_FUNCTION   0x0000000F
#define PRN_SELECT_LANDSCAPE  0x000000F0
#define PRN_SELECT_QUALITY    0x00000F00
#define PRN_SELECT_CLIPRGN    0x0000F000

#define PRN_SELECT_PAPESIZE   0x000F0000
#define PRN_SELECT_COLOR      0x00F00000
#define PRN_SELECT_PAGE_NUM   0x0F000000
#define PRN_SELECT_DOC_NUM    0xF0000000


typedef int (FAR WINAPI * MYPRINTFARPROC) (LPVOID);


// Test function prototypes (TestProc's)
TESTPROCAPI TestIrdaPrint (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestSerialPrint1 (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestSerialPrint2 (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestParallelPrint (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestNetWorkPrint (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestPrintPerf (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

int FAR PASCAL TestPrint (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY, UINT);


// dwUserData
//0xF      F        F         F        F         F          F         F
//=========================================================================
//#Docs   #pages  0=Black  0=Letter    0=NonClip  0=Draft   0=Portrait   Func #
//                1=Color  1=A4        1=ClipRgn  1=Full    1=Landscape
//                         2=LEGAL         QUALITY
//                         3=B5
//
// Our function table that we pass to Tux
FUNCTION_TABLE_ENTRY g_lpFTE[] = {
//  description                             udepth  dwData dwUniqueID lpTestProc
//==================================================================================
    TEXT ("IRDA: FullPageText"),                0, 0x12000010, 1, TestIrdaPrint,
    TEXT ("IRDA: Blt Func "),                   0, 0x21000011, 2, TestIrdaPrint,
    TEXT ("IRDA: Fonts: landscape"),            0, 0x11000012, 3, TestIrdaPrint,
    TEXT ("IRDA: Fonts: Portrit"),              0, 0x11000102, 4, TestIrdaPrint,
    TEXT ("IRDA: Diff Pens"),                   0, 0x11000013, 5, TestIrdaPrint,
    TEXT ("IRDA: Diff Brushes"),                0, 0x16000004, 6, TestIrdaPrint,
    TEXT ("IRDA: >16: bitmaps"),                0, 0x11000005, 7, TestIrdaPrint,
    TEXT ("IRDA: SBlt: diff Rops"),             0, 0x11000006, 8, TestIrdaPrint,
    TEXT ("IRDA: TransparentI"),                0, 0x11000117, 9, TestIrdaPrint,
    TEXT ("IRDA: Rotate Text"),                 0, 0x11000008, 10, TestIrdaPrint,
    TEXT ("IRDA: ClipRegion"),                  0, 0x11000009, 11, TestIrdaPrint,
    TEXT ("IRDA: Color Text"),                  0, 0x1100010A, 12, TestIrdaPrint,


    TEXT ("Serial 57600: FullPageText"),        0, 0x12000010, 101, TestSerialPrint1,
    TEXT ("Serial 57600: Blt Func "),           0, 0x21000011, 102, TestSerialPrint1,
    TEXT ("Serial 57600: Fonts: landscape"),    0, 0x11000012, 103, TestSerialPrint1,
    TEXT ("Serial 57600: Fonts: Portrit"),      0, 0x11000102, 104, TestSerialPrint1,
    TEXT ("Serial 57600: Diff Pens"),           0, 0x11000013, 105, TestSerialPrint1,
    TEXT ("Serial 57600: Diff Brushes"),        0, 0x16000004, 106, TestSerialPrint1,
    TEXT ("Serial 57600: >16: bitmaps"),        0, 0x11000005, 107, TestSerialPrint1,
    TEXT ("Serial 57600: SBlt: diff Rops"),     0, 0x11000006, 108, TestSerialPrint1,
    TEXT ("Serial 57600: TransparentI"),        0, 0x11000117, 109, TestSerialPrint1,
    TEXT ("Serial 57600: Rotate Text"),         0, 0x11000008, 110, TestSerialPrint1,
    TEXT ("Serial 57600: ClipRegion"),          0, 0x11000009, 111, TestSerialPrint1,
    TEXT ("Serial 57600: Color Text"),          0, 0x1100010A, 112, TestSerialPrint1,


    TEXT ("Serial 9600: FullPageText"),         0, 0x12000010, 201, TestSerialPrint2,
    TEXT ("Serial 9600: Blt Func"),             0, 0x21000011, 202, TestSerialPrint2,
    TEXT ("Serial 9600: Fonts: landscape"),     0, 0x11000012, 203, TestSerialPrint2,
    TEXT ("Serial 9600: Fonts: Portrit"),       0, 0x11000102, 204, TestSerialPrint2,
    TEXT ("Serial 9600: Diff Pens"),            0, 0x11000013, 205, TestSerialPrint2,
    TEXT ("Serial 9600: Diff Brushes"),         0, 0x16000004, 206, TestSerialPrint2,
    TEXT ("Serial 9600: >16: bitmaps"),         0, 0x11000005, 207, TestSerialPrint2,
    TEXT ("Serial 9600: SBlt: diff Rops"),      0, 0x11000006, 208, TestSerialPrint2,
    TEXT ("Serial 9600: TransparentI"),         0, 0x11000117, 209, TestSerialPrint2,
    TEXT ("Serial 9600: Rotate Text"),          0, 0x11000008, 210, TestSerialPrint2,
    TEXT ("Serial 9600: ClipRegion"),           0, 0x11000009, 211, TestSerialPrint2,
    TEXT ("Serial 9600: Color Text"),           0, 0x1100010A, 212, TestSerialPrint2,

    TEXT ("Parallel: FullPageText"),            0, 0x12000010, 301, TestParallelPrint,
    TEXT ("Parallel: Blt Func "),               0, 0x21000011, 302, TestParallelPrint,
    TEXT ("Parallel: Fonts: landscape"),        0, 0x11000012, 303, TestParallelPrint,
    TEXT ("Parallel: Fonts: Portrit"),          0, 0x11000102, 304, TestParallelPrint,
    TEXT ("Parallel: Diff Pens"),               0, 0x11000013, 305, TestParallelPrint,
    TEXT ("Parallel: Diff Brushes"),            0, 0x16000004, 306, TestParallelPrint,
    TEXT ("Parallel: >16: bitmaps"),            0, 0x11000005, 307, TestParallelPrint,
    TEXT ("Parallel: SBlt: diff Rops"),         0, 0x11000006, 308, TestParallelPrint,
    TEXT ("Parallel: TransparentI"),            0, 0x11000117, 309, TestParallelPrint,
    TEXT ("Parallel: Rotate Text"),             0, 0x11000008, 310, TestParallelPrint,
    TEXT ("Parallel: ClipRegion"),              0, 0x11000009, 311, TestParallelPrint,
    TEXT ("Parallel: Color Text"),              0, 0x1100010A, 312, TestParallelPrint,

    TEXT ("NetWork: FullPageText"),             0, 0x12000010, 401, TestNetWorkPrint,
    TEXT ("NetWork: Blt Func"),                 0, 0x21000011, 402, TestNetWorkPrint,
    TEXT ("NetWork: Fonts: landscape"),         0, 0x11000012, 403, TestNetWorkPrint,
    TEXT ("NetWork: Fonts: Portrit"),           0, 0x11000102, 404, TestNetWorkPrint,
    TEXT ("NetWork: Diff Pens"),                0, 0x11000013, 405, TestNetWorkPrint,
    TEXT ("NetWork: Diff Brushes"),             0, 0x16000004, 406, TestNetWorkPrint,
    TEXT ("NetWork: >16: bitmaps"),             0, 0x11000005, 407, TestNetWorkPrint,
    TEXT ("NetWork: SBlt: diff Rops"),          0, 0x11000006, 408, TestNetWorkPrint,
    TEXT ("NetWork: TransparentI"),             0, 0x11000117, 409, TestNetWorkPrint,
    TEXT ("NetWork: Rotate Text"),              0, 0x11000008, 410, TestNetWorkPrint,
    TEXT ("NetWork: ClipRegion"),               0, 0x11000009, 411, TestNetWorkPrint,
    TEXT ("NetWork: Color Text"),               0, 0x1100010A, 412, TestNetWorkPrint,

    // Performance test:
    // DRAFT MODE + NON COLOR
    TEXT ("IRDA:   PerfTest"),                  0, USE_IRDA, 700, TestPrintPerf,
    TEXT ("SERAIL (57600): PerfTest"),          0, USE_SERIAL_57600, 701, TestPrintPerf,
    TEXT ("PARALLEL:       PerfTest"),          0, USE_PARALLEL, 702, TestPrintPerf,

    // FULL QUALITY MODE 
    TEXT ("IRDA:   PerfTest"),                  0, FULL_QUALITY | USE_IRDA, 800, TestPrintPerf,
    TEXT ("SERAIL (57600): PerfTest"),          0, FULL_QUALITY | USE_SERIAL_57600, 801, TestPrintPerf,
    TEXT ("PARALLEL:       PerfTest"),          0, FULL_QUALITY | USE_PARALLEL, 802, TestPrintPerf,

    // 2 pages: compare normal font and Clear Font
    TEXT ("Parallel: Fonts: landscape"),        0, 0x12000012, 1303, TestParallelPrint,
    TEXT ("Parallel: Fonts: Portrit"),          0, 0x12000102, 1304, TestParallelPrint,
    NULL, 0, 0, 0, NULL         // marks end of list
};


#define MAX_PATH_LENGTH 256
HINSTANCE g_hInst;
BOOL g_fColorPrint = FALSE;
UINT g_dmPaperSize = 0;         // invalid value
TCHAR g_szDCOutput[MAX_PATH] = {0};

HFONT g_hfont;
RECT g_rcPrint;
HWND ghDlgPrint;
BOOL gfPerformance = FALSE;
UINT gnPages;
BOOL g_fUserAbort, g_fDraftMode;
int g_nJobs = 0;
int g_nTotalPages = 0;
DWORD gdwStartTime;             // Nt set this in side MyGetntPrintDC()

TCHAR gszNetPath[MAX_PATH_LENGTH];
TCHAR *grgszOutput[] = { TEXT ("IRDA:"), TEXT ("COM1: 57600"), TEXT ("COM1: 9600"), TEXT ("LPT1:") };


VOID LogResult (int nLevel, LPCTSTR szFormat, ...);
BOOL PASCAL CallStartPage (HDC hdcPrint, LPDWORD lpdwError);
BOOL PASCAL CallStartDoc (HDC hdcPrint, DOCINFO * lpdocinfo, LPDWORD lpdwError);
BOOL PASCAL CallEndPage (HDC hdcPrint, LPDWORD lpdwError);
BOOL PASCAL CallEndDoc (HDC hdcPrint, LPDWORD lpdwError);
BOOL PASCAL CallEndAndStartNewPage (HDC hdcPrint, LPDWORD lpdwError);

BOOL PASCAL GetUserNetworkPrinterPath (LPTSTR szNetPath);
VOID PASCAL MySetDevMode (DWORD dwUserData, LPDEVMODE lpdm);
BOOL PASCAL MySetClipRegion (HDC hdcPrint, int iPage);
BOOL CALLBACK PrintDlgProc (HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK QueryDlgProc (HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

BOOL CALLBACK AbortProc (HDC hdc, int error);


int PASCAL DrawPageFullText (HDC hdcPrint, int iDoc, int iPage, int nCallAbort);
int PASCAL DrawPageXXXBlt (HDC hdcPrint, int iDoc, int iPage, int nCallAbort);
int PASCAL DrawPageFonts (HDC hdcPrint, int iDoc, int iPage, int nCallAbort);
int PASCAL DrawPagePens (HDC hdcPrint, int iDoc, int iPage, int nCallAbort);
int PASCAL DrawPageBrushes (HDC hdcPrint, int iDoc, int iPage, int nCallAbort);
int PASCAL DrawPageBitmap2 (HDC hdcPrint, int iDoc, int iPage, int nCallAbort);
int PASCAL DrawPageStretchBlt (HDC hdcPrint, int iDoc, int iPage, int nCallAbort);
int PASCAL DrawPageTransparentI (HDC hdcPrint, int iDoc, int iPage, int nCallAbort);
int PASCAL DrawPageFonts2 (HDC hdcPrint, int iDoc, int iPage, int nCallAbort);
int PASCAL DrawPageClipRegion (HDC hdcPrint, int iDoc, int iPage, int nCallAbort);
int PASCAL DrawPageColorText (HDC hdcPrint, int iDoc, int iPage, int nCallAbort);

#ifdef UNDER_NT
HDC PASCAL MyGetPrintDC1 (DEVMODE * lpdm);
HDC PASCAL MyGetNTPrintDC2 (DEVMODE * lpdmUser, LPTSTR szPort);
#endif

VOID DrawPerfTestOnePage (HDC hdcprint, int iPage, int nPort);
BOOL CALLBACK PerfTestDlgProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
VOID DrawPerfTestOnePage_2 (HDC hdcPrint, int iPage, int nPort);

#ifdef UNDER_CE
void FAR PASCAL IncreaseProgramMemory ();
#endif
