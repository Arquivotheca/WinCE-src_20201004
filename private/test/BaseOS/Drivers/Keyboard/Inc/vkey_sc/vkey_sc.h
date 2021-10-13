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

#pragma once

#include "..\kbddef.h"

    UINT8 Vkey;
    UINT16 Scancode;
    UINT8 kbd;

//                          Virtual Key  Scan Code  Supported Keyboard

const keyinfo  VKtoSCTable[]=
{
     /* Virtual Keys, Standard Set
    */
    /*VK_LBUTTON*/              0x01,   0,      0,
    /*VK_RBUTTON */                 0x02,   0,      0,
    /*VK_CANCEL   */                0x03,   0,      0,
    /*VK_MBUTTON  */                0x04,   0,      0,  /* NOT contiguous with L & RBUTTON */

    /*VK_BACK  */               0x08,   0x0E,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_TAB  */                    0x09,   0x0F,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_CLEAR  */              0x0C,   0x4c,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_RETURN  */             0x0D,   0x1C,   USKBD|JAP1KBD|KORKBD|JAP2KBD,

    /*VK_SHIFT  */              0x10,   0x2a,   USKBD|JAP1KBD|KORKBD|JAP2KBD, //
    /*VK_CONTROL */             0x11,   0x1d,   USKBD|JAP1KBD|KORKBD|JAP2KBD,//
    /*VK_MENU    */             0x12,   0x38,   USKBD|JAP1KBD|KORKBD|JAP2KBD,//
    /*VK_PAUSE  */              0x0,        0x0,    USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_CAPITAL  */                0x14,   0x3a,   USKBD|JAP1KBD|KORKBD|JAP2KBD,//

    /*VK_KANA */                0x15,   0x70,   0|JAP2KBD,//for only jp2
    /*VK_HANGEUL */             0x15 ,  0,      0,/* old name - should be here for compatibility */
    /*VK_HANGUL  */                 0x15,   0x72,   0|KORKBD,
    /*VK_JUNJA */               0x17,   0,      0,
    /*VK_FINAL */                   0x18,   0,      0,
    /*VK_HANJA */               0x19,   0x71,   0|KORKBD,
    /* VK_KANJI */                  0x19,   0x29,   0, //for jp2

    /*VK_ESCAPE  */             0x1B,   0x01,   USKBD|JAP1KBD|KORKBD|JAP2KBD,

    /* VK_CONVERT */                0x1c,   0x79,   0|JAP2KBD,//check
    /*VK_NOCONVERT*/            0x1d,   0x7B,   0|JAP2KBD,//check

    /*VK_SPACE */                   0x20,   0x39,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_PRIOR*/                    0x21,   0xE049, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_NEXT  */                       0x22,   0xE051, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_END    */                  0x23,   0xE04F, USKBD|JAP1KBD|KORKBD|JAP2KBD,       //E04F
    /*VK_HOME  */                   0x24,   0xE047, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_LEFT    */                 0x25,   0xE04B, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_UP    */                   0x26,   0xE048, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_RIGHT     */                   0x27,   0xE04D, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_DOWN */                    0x28,   0xE050, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_SELECT      */             0x29,   0x0,        0,      //check
    /*VK_PRINT */                   0x2A,   0x0,        USKBD|JAP1KBD|KORKBD|JAP2KBD,       //check//how to generate VK_PRINT on XP
    /*VK_EXECUTE    */              0x2B,   0x0,        0  ,        //check
    /*VK_SNAPSHOT  */               0x2C,   0xe037, USKBD|JAP1KBD|KORKBD|JAP2KBD,        //E037
    /*VK_INSERT  */             0x2D,   0xE052, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_DELETE  */             0x2E,   0xE053, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_HELP    */                 0x2F,   0x63,   USKBD|JAP1KBD|KORKBD|JAP2KBD,

    /* VK_0 thru VK_9 are the same as ASCII '0' thru '9' (0x30 - 0x39) */
    /*VK_0*/                        0x30,   0x0B,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_1*/                    0x31,   0x02,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
     /*VK_2*/                       0x32,   0x03,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
     /*VK_3*/                       0x33,   0x04,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
     /*VK_4*/                       0x34,   0x05,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
     /*VK_5*/                       0x35,   0x06,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
     /*VK_6*/                       0x36,   0x07,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
     /*VK_7*/                       0x37,   0x08,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
     /*VK_8*/                       0x38,   0x09,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
     /*VK_9*/                       0x39,   0x0A,   USKBD|JAP1KBD|KORKBD|JAP2KBD,

    /* VK_A thru VK_Z are the same as ASCII 'A' thru 'Z' (0x41 - 0x5A) */

    /*VK_A*/                        0x41,   0x1E,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_B*/                        0x42,   0x30,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_C*/                        0x43,   0x2E,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_D*/                        0x44,   0x20,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_E*/                        0x45,   0x12,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_F*/                        0x46,   0x21,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_G*/                        0x47,   0x22,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_H*/                        0x48,   0x23,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_I*/                        0x49,   0x17,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_J*/                        0x4A,   0x24,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_K*/                        0x4B,   0x25,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_L*/                        0x4C,   0x26,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_M*/                        0x4D,   0x32,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_N*/                        0x4E,   0x31,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_O*/                        0x4F,   0x18,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_P*/                        0x50,   0x19,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_Q*/                        0x51,   0x10,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_R*/                        0x52,   0x13,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_S*/                        0x53,   0x1F,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_T*/                        0x54,   0x14,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_U*/                        0x55,   0x16,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_V*/                        0x56,   0x2F,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_W*/                        0x57,   0x11,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_X*/                        0x58,   0x2D,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_Y*/                        0x59,   0x15,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_Z*/                        0x5A,   0x2C,   USKBD|JAP1KBD|KORKBD|JAP2KBD,

    /*VK_LWIN    */             0x5B,   0xe05B, USKBD|JAP1KBD|KORKBD|JAP2KBD,        //
    /*VK_RWIN    */             0x5C,   0xe05C, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_APPS    */             0x5D,   0xe05D, USKBD|JAP1KBD|KORKBD|JAP2KBD,

    /*VK_SLEEP   */             0x5F,   0x00,   USKBD|JAP1KBD|KORKBD|JAP2KBD,        //we don't support VK_Sleep instead we have VK_OFF so this doesn't apply

    /*VK_NUMPAD0 */             0x60,   0x52,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_NUMPAD1 */             0x61,   0x4F,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_NUMPAD2 */             0x62,   0x50,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_NUMPAD3 */             0x63,   0x51,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_NUMPAD4 */             0x64,   0x4B,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_NUMPAD5 */             0x65,   0x4C,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_NUMPAD6 */             0x66,   0x4D,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_NUMPAD7 */             0x67,   0x47,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_NUMPAD8 */             0x68,   0x48,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_NUMPAD9 */             0x69,   0x49,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_MULTIPLY */                0x6A,   0x37,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_ADD        */              0x6B,   0x4E,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_SEPARATOR*/                0x6C,   0x0,        0,
    /*VK_SUBTRACT*/             0x6D,   0x4A,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_DECIMAL */             0x6E,   0x53,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_DIVIDE    */               0x6F,   0xe035, USKBD|JAP1KBD|KORKBD|JAP2KBD,

    /*VK_F1     */                  0x70,   0x3b,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_F2     */                  0x71,   0x3c,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_F3     */                  0x72,   0x3d,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_F4     */                  0x73,   0x3e,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_F5      */                 0x74,   0x3F,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_F6      */                 0x75,   0x40,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_F7      */                 0x76,   0x41,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_F8      */                 0x77,   0x42,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_F9      */                 0x78,   0x43,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_F10     */                 0x79,   0x44,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_F11     */                 0x7A,   0x57,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_F12     */                 0x7B,   0x58,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_F13     */                 0x7C,   0x64,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_F14     */                 0x7D,   0x65,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_F15     */                 0x7E,   0x66,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_F16     */                 0x7F,   0x67,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_F17     */                 0x80,   0x68,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_F18     */                 0x81,   0x69,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_F19     */                 0x82,   0x6a,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_F20     */                 0x83,   0x6b,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_F21     */                 0x84,   0x6c,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_F22     */                 0x85,   0x6d,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_F23     */                 0x86,   0x6e,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_F24     */                 0x87,   0x76,   USKBD|JAP1KBD|KORKBD|JAP2KBD,

    /*VK_NUMLOCK */             0x90,   0x45,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_SCROLL  */             0x91,   0x46,   USKBD|JAP1KBD|KORKBD|JAP2KBD,

    /*
     * VK_L* & VK_R* - left and right Alt,       Ctrl and Shift virtual keys.
     * Used only as parameters to GetAsyncKeyState() and GetKeyState().
     * No other API or message will distinguish left and right keys in this way.
     */
    /*VK_LSHIFT  */             0xA0,   0x2A,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_RSHIFT  */             0xA1,   0x36,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_LCONTROL*/             0xA2,   0x1D,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_RCONTROL*/             0xA3,   0xe01D, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_LMENU   */             0xA4,   0x38,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_RMENU   */             0xA5,   0xe038, USKBD|JAP1KBD|KORKBD|JAP2KBD,

    /*VK_EXTEND_BSLASH*/            0xE2,   0x73,   0|JAP2KBD,      //check not found
    /*VK_OEM_102 */             0xE2,   0,      0,

    /*VK_PROCESSKEY*/           0xE5,   0,      0,

    /*VK_ATTN    */             0xF6,   0,      0,
    /*VK_CRSEL   */             0xF7,   0,      0,
    /*VK_EXSEL   */             0xF8,   0,      0,
    /*VK_EREOF   */             0xF9,   0,      0,
    /*VK_PLAY    */                 0xFA,   0,      0,
    /*VK_ZOOM    */             0xFB,   0,      0,
    /*VK_NONAME  */             0xFC,   0,      0,
    /*VK_PA1     */                 0xFD,   0,      0,
    /*VK_OEM_CLEAR*/                0xFE,   0,      0,


    /*VK_SEMICOLON*/                0xBA,   0x27,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_EQUAL  */              0xBB,   0x0D,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_COMMA  */              0xBC,   0x33,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_HYPHEN */              0xBD,   0x0C,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_PERIOD */              0xBE,   0x34,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_SLASH  */              0xBF,   0x35,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_BACKQUOTE*/            0xC0,   0x29,   USKBD|JAP1KBD|KORKBD,
    /*VK_BACKQUOTE*/            0xC0,   0x1a,   0|JAP2KBD,

    /*VK_BROWSER_BACK */        0xA6,   0xe06A, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_BROWSER_FORWARD*/      0xA7,   0xe069, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_BROWSER_REFRESH */     0xA8,   0xe067, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_BROWSER_STOP */        0xA9,   0xe068, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_BROWSER_SEARCH */      0xAA,   0xe065, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_BROWSER_FAVORITES*/    0xAB,   0xe066, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_BROWSER_HOME */        0xAC,   0xe032, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_VOLUME_MUTE    */      0xAD,   0xe020, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_VOLUME_DOWN   */       0xAE,   0xe02E, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_VOLUME_UP  */          0xAF,   0xe030, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_MEDIA_NEXT_TRACK*/     0xB0,   0xe019, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_MEDIA_PREV_TRACK */        0xB1,   0xe010, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_MEDIA_STOP */          0xB2,   0xe024, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_MEDIA_PLAY_PAUSE*/     0xB3,   0xe022, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_LAUNCH_MAIL */         0xB4,   0xe06C, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_LAUNCH_MEDIA_SELECT */ 0xB5,   0xe06d, USKBD|JAP1KBD|KORKBD|JAP2KBD,       //check
    /*VK_LAUNCH_APP1            */  0xB6,   0xe06B, USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_LAUNCH_APP2            */  0xB7,   0xe021, USKBD|JAP1KBD|KORKBD|JAP2KBD,

    /*VK_LBRACKET           */  0xDB,   0x1a,   USKBD|JAP1KBD|KORKBD,
    /*VK_LBRACKET           */  0xDB,   0x1b,   0|JAP2KBD,

    /*VK_BACKSLASH      */          0xDC,   0x2b,   USKBD|JAP1KBD|KORKBD,
    /*VK_BACKSLASH      */          0xDC,   0x7D,   0|JAP2KBD,
    /*VK_RBRACKET       */          0xDD,   0x1b,   USKBD|JAP1KBD|KORKBD,
    /*VK_RBRACKET       */          0xDD,   0x2b,   0|JAP2KBD,

    /*VK_APOSTROPHE */                  0xDE,   0x28,   USKBD|JAP1KBD|KORKBD|JAP2KBD,
    /*VK_OFF     */                             0xDF,   0,      0,       //check

    //check these values
    /*VK_DBE_ALPHANUMERIC     */    0x0f0,  0x3a,   0|JAP2KBD,       //jp2
    /*VK_DBE_KATAKANA  */               0x0f1,  0x70,   0|JAP2KBD,
    /*VK_DBE_HIRAGANA  */               0x0f2,  0x70,   0|JAP2KBD,
    /*VK_DBE_SBCSCHAR  */       0x0f3,  0x29,   0|JAP2KBD,
    /*VK_DBE_DBCSCHAR  */       0x0f4,  0x29,   0|JAP2KBD,
    /*VK_DBE_ROMAN     */           0x0f5,  0x70,   0|JAP2KBD,
    /*VK_DBE_NOROMAN   */       0x0f6,  0x70,   0|JAP2KBD,

    /*VK_DBE_ENTERWORDREGISTERMODE*/    0x0f7,  0,      0,
    /*VK_DBE_ENTERIMECONFIGMODE    */   0x0f8,  0,      0,
    /*VK_DBE_FLUSHSTRING */                     0x0f9,  0,      0,
    /*VK_DBE_CODEINPUT */                       0x0fa,  0,      0,
    /*VK_DBE_NOCODEINPUT*/                  0x0fb,  0,      0,
    /*VK_DBE_DETERMINESTRING*/                  0x0fc,  0,      0,
    /*VK_DBE_ENTERDLGCONVERSIONMODE*/   0x0fd,  0,      0
    };
