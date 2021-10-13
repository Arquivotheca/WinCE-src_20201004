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
#ifndef _INPUTLANG_H_
#define _INPUTLANG_H_

#include <windows.h>
 
/*
 * Keyboard Shift State defines. These correspond to the bit mask defined
 * by the VkKeyScan() API.
 */
#define KBDBASE        0
#define KBDSHIFT       1
#define KBDCTRL        2
#define KBDALT         4 // Set to enable Alt+NumPad character generation
// three symbols KANA, ROYA, LOYA are for FE
#define KBDKANA        8
#define KBDROYA        0x10
#define KBDLOYA        0x20
#define KBDGRPSELTAP   0x80

#define VK__none_ 0xFF

/***************************************************************************\
* MODIFIER KEYS
*
* All keyboards have "Modifier" keys which are used to alter the behaviour of
* some of the other keys.  These shifter keys are usually:
*   Shift  (left and/or right Shift key)
*   Ctrl   (left and/or right Ctrl key)
*   Alt    (left and/or right Alt key)
*   AltGr  (right Alt key only)
*
* NOTE:
*   All keyboards use the Shift key.
*   All keyboards use a Ctrl key to generate ASCII control characters.
*   All keyboards with a number pad use the Alt key and the NumPad to
*     generate characters by number.
*   Keyboards using AltGr as a Modifier Key usually translate the Virtual
*     ScanCode to Virtual Keys VK_CTRL + VK_ALT at input time: the Modifier
*     tables should be written to treat Ctrl + Alt as a valid shifter
*     key combination in these cases.
*
* By holding down 0 or more of these Modifier keys, a "shift state" is
* obtained : the shift state may affect the translation of Virtual Scancodes
* to Virtual Keys and/or the translation of Virtuals Key to Characters.
*
* EXAMPLES:
*
* Each key on a particular keyboard may be marked with up to five different
* characters in five different positions:
*
*              .-------.
*             /|       |\
*            : | 2   4 | :
*            | |       | |
*            | |       | |
*            | | 1   3 | |
*            | |_______| |
*            | /       \ |
*            |/    5    \|
*            `-----------'
*
* A key may also be able to generate a character that is not marked on it:
* these are ASCII Control chars, lower-case letters and/or "invisible keys".
*                                                  .-------.
*      An example of an "Invisible Key":          /|       |\
*                                                : | >     | :
*  The German M24 keyboard 2 should produce the  | |       | |
*  '|' character when ALT SHIFT is is held down  | |       | |
*  while the '<' key (shown here) is pressed:    | | <   \ | |
*  This keyboard has four other invisible        | |_______| |
*  characters.  France, Italy and Spain also     | /       \ |
*  support invisible characters on the M24       |/         \|
*  Keyboard 2 with ALT SHIFT depressed.          `-----------'
*
* The keyboard table must list the keys that contribute to it's shift state,
* and indicate which combinations are valid.  This is done with
*    aCharModifiers[]  - convert combinations of Modifier Keys to Bitmasks.
* and
*    aModification[];  - convert Modifier Bitmasks to enumerated Modifications
*
* AN EXAMPLE OF VALID AND INVALID MODIFIER KEY COMBINATIONS
*
*    The US English keyboard has 3 Modifier keys:
*      Shift (left or right); Ctrl (left or right); and Alt (left or right).
*
*    The only valid combinations of these Modifier Keys are:
*      none pressed      : Character at position (1) on the key.
*      Shift             : Character at position (2) on the key.
*      Ctrl              : Ascii Control characters
*      Shift + Ctrl      : Ascii Control characters
*      Alt               : Character-by-number on the numpad
*
*    The invalid combinations (that do not generate any characters) are:
*      Shift + Alt
*      Alt + Ctrl
*      Shift + Alt + Ctrl
*
* Something (???) :
* -----------------
*      Modifier keys              Character produced
*      -------------------------  ------------------
*   0  No shifter key depressed   position 1
*   1  Shift key is depressed     position 2
*   2  AltGr (r.h. Alt) depressed position 4 or 5 (whichever is marked)
*
* However, note that 3 shifter keys (SHIFT, can be combined in a
* characters, depending on the Keyboards
* Consider the following keyboards:
*
*     .-------.            STRANGE KBD         PECULIAR KBD
*    /|       |\           ==================  ==================
*   : | 2   4 | :    1   -
*   | |       | |    2   - SHIFT               SHIFT
*   | |       | |    3   - MENU                MENU
*   | | 1   3 | |    4   - SHIFT + MENU        SHIFT + MENU
*   | |_______| |    5   -    no such keys     CTRL  + MENU
*   | /       \ |
*   |/    5    \|
*   `-----------'
* Both STRANGE and PECULIAR keyboards could have aVkToBits[] =
*   { VK_SHIFT  , KBDSHIFT }, // 0x01
*   { VK_CONTROL, KBDCTRL  }, // 0x02
*   { VK_MENU   , KBDALT   }, // 0x04
*   { 0,          0        }
*
* The STRANGE keyboard has 4 distinct shift states, while the PECULIAR kbd
* has 5.  However, note that 3 shifter bits can be combined in a
* total of 2^3 == 8 ways.  Each such combination must be related to one (or
* none) of the enumerated shift states.
* Each shifter key combination can be represented by three binary bits:
*  Bit 0  is set if VK_SHIFT is down
*  Bit 1  is set if VK_CONTROL is down
*  Bit 2  is set if VK_MENU is down
*
* Example: If the STRANGE keyboard generates no characters in combination
* when just the ALT key is held down, nor when the SHIFT, CTRL and ALT keys
* are all held down, then the tables might look like this:
*
*                                VK_MENU,
*                        VK_CTRL,                    0
*    };
*    aModification[] = {
*        0,            //   0       0       0     = 000  <none>
*        1,            //   0       0       1     = 001  SHIFT
*        SHFT_INVALID, //   0       1       0     = 010  ALT
*        2,            //   0       1       1     = 011  SHIFT ALT
*        3,            //   1       0       0     = 100  CTRL
*        4,            //   1       0       1     = 101  SHIFT CTRL
*        5,            //   1       1       0     = 110  CTRL ALT
*        SHFT_INVALID  //   1       1       1     = 111  SHIFT CTRL ALT
*    };
*
*
\***************************************************************************/

/***************************************************************************\
* VK_TO_BIT - associate a Virtual Key with a Modifier bitmask.
*
* Vk        - the Virtual key (eg: VK_SHIFT, VK_RMENU, VK_CONTROL etc.)
*             Special Values:
*                0        null terminator
* ModBits   - a combination of KBDALT, KBDCTRL, KBDSHIFT and kbd-specific bits
*             Any kbd-specific shift bits must be the lowest-order bits other
*             than KBDSHIFT, KBDCTRL and KBDALT (0, 1 & 2)
*
\***************************************************************************/
typedef struct {
    BYTE Vk;
    BYTE ModBits;
} VK_TO_BIT, *PVK_TO_BIT;

/***************************************************************************\
* pModNumber  - a table to map shift bits to enumerated shift states
*
* Table attributes: Ordered table
*
* Maps all possible shifter key combinations to an enumerated shift state.
* The size of the table depends on the value of the highest order bit used
* in aCharModifiers[*].ModBits
*
* Special values for aModification[*]
*   SHFT_INVALID - no characters produced with this shift state.
LATER: (ianja) no SHFT_CTRL - control characters encoded in tables like others
*   SHFT_CTRL    - standard control character production (all keyboards must
*                  be able to produce CTRL-C == 0x0003 etc.)
*   Other        - enumerated shift state (not less than 0)
*
* This table is indexed by the Modifier Bits to obtain an Modification Number.
*
*                        CONTROL MENU SHIFT
*
*    aModification[] = {
*        0,            //   0     0     0     = 000  <none>
*        1,            //   0     0     1     = 001  SHIFT
*        SHFT_INVALID, //   0     1     0     = 010  ALT
*        2,            //   0     1     1     = 011  SHIFT ALT
*        3,            //   1     0     0     = 100  CTRL
*        4,            //   1     0     1     = 101  SHIFT CTRL
*        5,            //   1     1     0     = 110  CTRL ALT
*        SHFT_INVALID  //   1     1     1     = 111  SHIFT CTRL ALT
*    };
*
\***************************************************************************/
// Disable warning C4200: nonstandard extension used : zero-sized array in 
// struct/union
#pragma warning(disable:4200)
typedef struct {
    PVK_TO_BIT pVkToBit;     // Virtual Keys -> Mod bits
    WORD       wMaxModBits;  // max Modification bit combination value
    BYTE       ModNumber[];  // Mod bits -> Modification Number
} MODIFIERS, *PMODIFIERS;
#pragma warning(default:4200)

#define SHFT_INVALID 0x0F

/***************************************************************************\
*
* VK_TO_WCHARS<n> - Associate a Virtual Key with <n> UNICODE characters
*
* VirtualKey  - The Virtual Key.
* wch[]       - An array of characters, one for each shift state that
*               applies to the specified Virtual Key.
*
* Special values for VirtualKey:
*    -1        - This entry contains dead chars for the previous entry
*    0         - Terminates a VK_TO_WCHARS[] table
*
* Special values for Attributes:
*    CAPLOK    - The CAPS-LOCK key affects this key like SHIFT
*    SGCAPS    - CapsLock uppercases the unshifted char (Swiss-German)
*
* Special values for wch[*]:
*    WCH_NONE  - No character is generated by pressing this key with the
*                current shift state.
*    WCH_DEAD  - The character is a dead-key: the next VK_TO_WCHARS[] entry
*                will contain the values of the dead characters (diaresis)
*                that can be produced by the Virtual Key.
*    WCH_LGTR  - The character is a ligature.  The characters generated by
*                this keystroke are found in the ligature table.
*
\***************************************************************************/
#define WCH_NONE 0xF000
#define WCH_DEAD 0xF001
#define WCH_LGTR 0xF002

#define CAPLOK      0x01
#define SGCAPS      0x02
#define CAPLOKALTGR 0x04
// KANALOK is for FE
#define KANALOK     0x08
#define GRPSELTAP   0x80

/*
 * Macro for VK to WCHAR with "n" shift states
 */
#define TYPEDEF_VK_TO_WCHARS(n) typedef struct _VK_TO_WCHARS##n {  \
                                    BYTE  VirtualKey;      \
                                    BYTE  Attributes;      \
                                    WCHAR wch[n];          \
                                } VK_TO_WCHARS##n, *PVK_TO_WCHARS##n;

/*
 * To facilitate coding the table scanning routine.
 */

/*
 * Table element types (for various numbers of shift states), used
 * to facilitate static initializations of tables.
 * VK_TO_WCHARS1 and PVK_TO_WCHARS1 may be used as the generic type
 */
TYPEDEF_VK_TO_WCHARS(1) // VK_TO_WCHARS1, *PVK_TO_WCHARS1;
TYPEDEF_VK_TO_WCHARS(2) // VK_TO_WCHARS2, *PVK_TO_WCHARS2;
TYPEDEF_VK_TO_WCHARS(3) // VK_TO_WCHARS3, *PVK_TO_WCHARS3;
TYPEDEF_VK_TO_WCHARS(4) // VK_TO_WCHARS4, *PVK_TO_WCHARS4;
TYPEDEF_VK_TO_WCHARS(5) // VK_TO_WCHARS5, *PVK_TO_WCHARS5;
TYPEDEF_VK_TO_WCHARS(6) // VK_TO_WCHARS6, *PVK_TO_WCHARS5;
TYPEDEF_VK_TO_WCHARS(7) // VK_TO_WCHARS7, *PVK_TO_WCHARS7;
// these three (8,9,10) are for FE
TYPEDEF_VK_TO_WCHARS(8) // VK_TO_WCHARS8, *PVK_TO_WCHARS8;
TYPEDEF_VK_TO_WCHARS(9) // VK_TO_WCHARS9, *PVK_TO_WCHARS9;
TYPEDEF_VK_TO_WCHARS(10) // VK_TO_WCHARS10, *PVK_TO_WCHARS10;

/***************************************************************************\
*
* VK_TO_WCHAR_TABLE - Describe a table of VK_TO_WCHARS1
*
* pVkToWchars     - points to the table.
* nModifications  - the number of shift-states supported by this table.
*                   (this is the number of elements in pVkToWchars[*].wch[])
*
* A keyboard may have several such tables: all keys with the same number of
*    shift-states are grouped together in one table.
*
* Special values for pVktoWchars:
*     NULL     - Terminates a VK_TO_WCHAR_TABLE[] list.
*
\***************************************************************************/
typedef struct _VK_TO_WCHAR_TABLE {
    PVK_TO_WCHARS1 pVkToWchars; // Could be any PVK_TO_WCHARS#
    BYTE           nModifications;
    BYTE           cbSize;
} VK_TO_WCHAR_TABLE, *PVK_TO_WCHAR_TABLE;


/***************************************************************************\
*
* Dead Key (diaresis) tables
*
\***************************************************************************/
typedef struct _DEADKEY {
    DWORD  dwBoth;  // diacritic & char
    WCHAR  wchComposed;
    USHORT uFlags;
} DEADKEY, *PDEADKEY;


#define DEADTRANS(ch, accent, comp, flags) { MAKELONG(ch, accent), comp, flags}

/*
 * Bit values for uFlags
 */
#define DKF_DEAD  0x0001 // Not supported


/***************************************************************************\
*
* Ligature table
*
\***************************************************************************/
/*
 * Macro for ligature with "n" characters
 */
#define TYPEDEF_LIGATURE(n) typedef struct _LIGATURE##n {     \
                                    BYTE  VirtualKey;         \
                                    WORD  ModificationNumber; \
                                    WCHAR wch[n];             \
                                } LIGATURE##n, *PLIGATURE##n;

/*
 * To facilitate coding the table scanning routine.
 */

/*
 * Table element types (for various numbers of ligatures), used
 * to facilitate static initializations of tables.
 *
 * LIGATURE1 and PLIGATURE1 are used as the generic type
 */
TYPEDEF_LIGATURE(1) // LIGATURE1, *PLIGATURE1;
TYPEDEF_LIGATURE(2) // LIGATURE2, *PLIGATURE2;
TYPEDEF_LIGATURE(3) // LIGATURE3, *PLIGATURE3;
TYPEDEF_LIGATURE(4) // LIGATURE4, *PLIGATURE4;
TYPEDEF_LIGATURE(5) // LIGATURE5, *PLIGATURE5;


/*
 * Attributes such as AltGr, LRM_RLM, ShiftLock are stored in the the low word
 * of fLocaleFlags (layout specific)
 */
#define KLLF_ALTGR       0x0001
#define KLLF_SHIFTLOCK   0x0002 // Unsupported
#define KLLF_LRM_RLM     0x0004 // Unsupported


typedef struct _VK_TO_SHIFT {
    UINT8           uiVk;
    KEY_STATE_FLAGS KeyStateFlags; 
} VK_TO_SHIFT, *PVK_TO_SHIFT;


enum VKEY_TO_SCAN_CODE_TYPE {
    SC_NORMAL = 0,
    SC_E0,
    SC_E11D,
};

typedef struct _VKEY_TO_SCANCODE {
    UINT8   uiType;
    UINT8   uiVkToSc;
} VKEY_TO_SCANCODE, *PVKEY_TO_SCANCODE;


typedef struct _INPUT_LANGUAGE {
    DWORD   dwSize;
    DWORD   dwType;
    DWORD   dwSubType;

    // Modifier keys
    const MODIFIERS *pCharModifiers;

    // Optional shift key table
    const VK_TO_SHIFT *pVkToShiftState;

    // Optional toggle key table
    const VK_TO_SHIFT *pVkToToggledState;

    // Virtual key to Unicode
    const VK_TO_WCHAR_TABLE *pVkToWcharTable;//ptr to tbl of ptrs to tbl

    // Dead keys
    const DEADKEY *pDeadKey;

    // Virtual key to XT scan code
    const VKEY_TO_SCANCODE *pVkToScanCodeTable;

    // Locale-specific special processing    
    DWORD fLocaleFlags;

    // Ligatures (Not currently supported. Here for compatibility only.)
    BYTE             nLgMax;
    BYTE             cbLgEntry;
    const LIGATURE1 *pLigature;

    BYTE    bcFnKeys;
} INPUT_LANGUAGE, *PINPUT_LANGUAGE;

typedef BOOL (*PFN_INPUT_LANGUAGE_ENTRY)(PINPUT_LANGUAGE 
	pInputLanguage);


#endif // _INPUTLANG_H_
