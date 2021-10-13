//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#ifndef __RESOURCE_H__
#define __RESOURCE_H__


//Bitmaps
#define IDB_BACKGROUND                  500
#define IDB_BUTTON                      501
#define IDB_BUTTONDOWN                  502
#define IDB_STATUS                      503
#define IDB_TITLE_BAR                   504
#define IDB_TRACKBAR_CHANNEL            505
#define IDB_TRACKBAR_CHANNEL_SEL        506
#define IDB_TRACKBAR_THUMB              507
#define IDB_TRACKBAR_THUMB_SEL          508
#define IDB_LOGO_SMALL                  509
#define IDB_POPUPMENU_RIGHT_ARROW       510
#define IDB_SCROLL_ARROW_UP             511
#define IDB_SCROLL_ARROW_DOWN           512

#define RES_PHCOMMON_BASE           0x1000

//Dialogs
#define IDD_DIALOG_SCREEN           0x1000 //(RES_PHCOMMON_BASE + 0x0000)
#define IDD_MESSAGE_BOX             0x1001 //(RES_PHCOMMON_BASE + 0x0001)

//Menus
#define IDMB_DEFAULT_MESSAGE_BOX    0x2000 //(RES_PHCOMMON_BASE + 0x1000)
#define IDMB_MENU_SCREEN            0x2001 //(RES_PHCOMMON_BASE + 0x1001)

//Strings
#define IDS_MENU_SCREEN_HEADER      (RES_PHCOMMON_BASE + 0x2000)
#define IDS_MENU_SCREEN_TITLE       (RES_PHCOMMON_BASE + 0x2001)
#define IDS_DIALOG_SCREEN_CURRENT_ITEMS (RES_PHCOMMON_BASE + 0x2002)

//Layout Data
#define IDR_METRICS_DATA            0x4000 //(RES_PHCOMMON_BASE + 0x3000)
#define DEFINE_MARGIN(Left, Top, Right, Bottom) \
    (Left), (Top), (Right), (Bottom)

//Colors Data
#define IDR_COLORS_DATA             0x4001 //(RES_PHCOMMON_BASE + 0x3001)
#define DEFINE_RGB_COLOR(Red, Green, Blue) \
    (Red##L + Green##00L + Blue##0000L)

//Font Data
#define IDR_FONTS_DATA              0x4002 //(RES_PHCOMMON_BASE + 0x3002)
#define DEFINE_FONT(FontNameId, Height, Weight) \
    (FontNameId), (Height), (Weight)

#define IDS_FONT_TAHOMA             0x4300 //(RES_PHCOMMON_BASE + 0x3300)
#define IDS_FONT_ARIAL              0x4301 //(RES_PHCOMMON_BASE + 0x3301)
#define IDS_FONT_TREBUCHET          0x4302 //(RES_PHCOMMON_BASE + 0x3302)

#endif // !defined __RESOURCE_H__
