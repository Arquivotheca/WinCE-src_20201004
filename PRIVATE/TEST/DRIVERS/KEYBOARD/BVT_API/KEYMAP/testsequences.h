//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1998  Microsoft Corporation.  All Rights Reserved.

Module Name:

     testsequences.h  
 
Abstract:
 
	This file contains the declaration of the key map test cases.
--*/


#ifndef _testSequences_h_
#define _testSequences_h_ 

#define MAX_SEQUENCE_LENGTH        2

typedef struct {
	int nNumVirtualKeys;
	BYTE byaVirtualKeys[MAX_SEQUENCE_LENGTH];
	WCHAR wcUnicodeChar;
} testSequence;

//  Num  VK1   VK2		Unicode
testSequence USEnglish[] = {
	{1,	{0x20, 0x00},	0x20},		// ' '
	{2,	{0x10, 0x31},	0x21},		// '!'
	{2,	{0x10, 0xDE},	0x22},		// '"'
	{2,	{0x10, 0x33},	0x23},      // '#'
    {2,	{0x10, 0x34},	0x24},      // '$'
    {2,	{0x10, 0x35},	0x25},      // '%'
    {2,	{0x10, 0x37},	0x26},      // '&'
    {1,	{0xDE, 0x00},	0x27},      // '''
    {2,	{0x10, 0x39},	0x28},      // '('
    {2,	{0x10, 0x30},	0x29},      // ')'
    {2,	{0x10, 0x38},	0x2A},	    // '*'
    {2,	{0x10, 0xBB},	0x2B},      // '+'
    {1,	{0xBC, 0x00},	0x2C},      // ','
    {1,	{0xBD, 0x00},	0x2D},      // '-'
    {1,	{0xBE, 0x00},	0x2E},      // '.'
    {1,	{0xBF, 0x00},	0X2F},      // '/'
	{1,	{0x30, 0x00},	0x30},      // '0'
    {1,	{0x31, 0x00},	0x31},      // '1'
    {1,	{0x32, 0x00},	0x32},      // '2'
    {1,	{0x33, 0x00},	0X33},      // '3'
	{1,	{0x34, 0x00},	0x34},      // '4'
    {1,	{0x35, 0x00},	0x35},      // '5'
    {1,	{0x36, 0x00},	0x36},      // '6'
    {1,	{0x37, 0x00},	0X37},      // '7'
	{1,	{0x38, 0x00},	0x38},      // '8'
    {1,	{0x39, 0x00},	0X39},      // '9'
	{2, {0x10, 0xBA},	0X3A},      // ':'
    {1, {0xBA, 0x00},	0X3B},      // ';'
    {2, {0x10, 0xBC},	0X3C},      // '<'
    {1, {0xBB, 0x00},	0X3D},      // '='
    {2, {0x10, 0xBE},	0X3E},      // '>'
    {2, {0x10, 0xBF},	0X3F},      // '?'
	{2,	{0x10, 0x41},	0x41},      // 'A'
    {2,	{0x10, 0x42},	0x42},      // 'B'
    {2,	{0x10, 0x43},	0x43},      // 'C'
    {2,	{0x10, 0x44},	0x44},      // 'D'
	{2,	{0x10, 0x45},	0x45},      // 'E'
    {2,	{0x10, 0x46},	0x46},      // 'F'
    {2,	{0x10, 0x47},	0x47},      // 'G'
    {2,	{0x10, 0x48},	0x48},      // 'H'
	{2,	{0x10, 0x49},	0x49},      // 'I'
    {2,	{0x10, 0x4A},	0x4A},      // 'J'
	{2,	{0x10, 0x4B},	0x4B},      // 'K'
    {2,	{0x10, 0x4C},	0x4C},      // 'L'
    {2,	{0x10, 0x4D},	0x4D},      // 'M'
    {2,	{0x10, 0x4E},	0x4E},      // 'N'
	{2,	{0x10, 0x4F},	0x4F},      // 'O'
    {2,	{0x10, 0x50},	0x50},      // 'P'
    {2,	{0x10, 0x51},	0x51},      // 'Q'
    {2,	{0x10, 0x52},	0x52},      // 'R'
	{2,	{0x10, 0x53},	0x53},      // 'S'
    {2,	{0x10, 0x54},	0x54},      // 'T'
	{2,	{0x10, 0x55},	0x55},      // 'U'
    {2,	{0x10, 0x56},	0x56},      // 'V'
    {2,	{0x10, 0x57},	0x57},      // 'W'
    {2,	{0x10, 0x58},	0x58},      // 'X'
	{2,	{0x10, 0x59},	0x59},      // 'Y'
    {2,	{0x10, 0x5A},	0x5A},      // 'Z'
    {1, {0xDB, 0x00},	0X5B},      // '['
    {1, {0xDC, 0x00},	0X5C},      // '\'
    {1, {0xDD, 0x00},	0X5D},      // ']'
    {2, {0x10, 0x36},	0X5E},      // '^'
    {2, {0x10, 0xBD},	0X5F},      // '_'
    {1, {0xC0, 0x00},	0X60},      // '`'
    {1,	{0x41, 0x00},	0x61},      // 'a'
    {1,	{0x42, 0x00},	0x62},      // 'b'
    {1,	{0x43, 0x00},	0x63},      // 'c'
    {1,	{0x44, 0x00},	0x64},      // 'd'
	{1,	{0x45, 0x00},	0x65},      // 'e'
    {1,	{0x46, 0x00},	0x66},      // 'f'
    {1,	{0x47, 0x00},	0x67},      // 'g'
    {1,	{0x48, 0x00},	0x68},      // 'h'
	{1,	{0x49, 0x00},	0x69},      // 'i'
    {1,	{0x4A, 0x00},	0x6A},      // 'j'
	{1,	{0x4B, 0x00},	0x6B},      // 'k'
    {1,	{0x4C, 0x00},	0x6C},      // 'l'
    {1,	{0x4D, 0x00},	0x6D},      // 'm'
    {1,	{0x4E, 0x00},	0x6E},      // 'n'
	{1,	{0x4F, 0x00},	0x6F},      // 'o'
    {1,	{0x50, 0x00},	0x70},      // 'p'
    {1,	{0x51, 0x00},	0x71},      // 'q'
    {1,	{0x52, 0x00},	0x72},      // 'r'
	{1,	{0x53, 0x00},	0x73},      // 's'
    {1,	{0x54, 0x00},	0x74},      // 't'
	{1,	{0x55, 0x00},	0x75},      // 'u'
    {1,	{0x56, 0x00},	0x76},      // 'v'
    {1,	{0x57, 0x00},	0x77},      // 'w'
    {1,	{0x58, 0x00},	0x78},      // 'x'
	{1,	{0x59, 0x00},	0x79},      // 'y'
    {1,	{0x5A, 0x00},	0x7A},      // 'z'
	{2, {0x10, 0xDB},	0X7B},      // '{'
    {2, {0x10, 0xDC},	0X7C},      // '|'
    {2, {0x10, 0xDD},	0X7D},      // '}'
    {2, {0x10, 0xC0},	0X7E},      // '~'

// Control chars with CAPSLOCK OFF
// Row ZXCVBNM 
 	{2, { 0x11, 90 } , 0x1A },          // Ctrl Z
	{2, { 0x11, 88 },  0x18 },			// Ctrl X
	{2, { 0x11, 67 },  0x03 },			// Ctrl C
	{2, { 0x11, 86 },  0x16 },			// Ctrl V
	{2, { 0x11, 66 },  0x02 },			// Ctrl B
    {2, { 0x11, 78 },  0x0E },			// Ctrl N
	{2, { 0x11, 77 },  0x0D},			// Ctrl M
//Row ASDFG
	{2, { 0x11, 65 }, 0x01 },			// Ctrl A
	{2, { 0x11, 83 }, 0x13 },			// Ctrl S
	{2, { 0x11, 68 }, 0x04 },			// Ctrl D
	{2, { 0x11, 70 }, 0x06 },			// Ctrl F
	{2, { 0x11, 71 }, 0x07 },			// Ctrl G
    {2, { 0x11, 72 }, 0x08 },			// Ctrl H
	{2, { 0x11, 74 }, 0x0A },			// Ctrl J
	{2, { 0x11, 75 }, 0x0B },			// Ctrl K
	{2, { 0x11, 76 }, 0x0C },			// Ctrl L
// Row QWERTY
	{2, {0x11, 81 }, 0x11 },			// Ctrl Q
	{2, {0x11, 87 }, 0x17 },			// Ctrl W
	{2, {0x11, 69 }, 0x05 },			// Ctrl E
	{2, {0x11, 82 }, 0x12 },			// Ctrl R
	{2, {0x11, 84 }, 0x14 },			// Ctrl T
	{2, {0x11, 89 }, 0x19 },			// Ctrl Y
	{2, {0x11, 85 }, 0x15 },			// Ctrl U
	{2, {0x11, 73 }, 0x09 },			// Ctrl I
	{2, {0x11, 79 }, 0x0F },			// Ctrl O
	{2, {0x11, 80 }, 0x10 },			// Ctrl P



//Turn CAPSLOCK ON
    {9, {0x10, 0x14 },  0},

	
// With CAPS ON 

	{1,	{0x20, 0x00},	0x20},		// ' '
	{2,	{0x10, 0x31},	0x21},		// '!'
	{2,	{0x10, 0xDE},	0x22},		// '"'
	{2,	{0x10, 0x33},	0x23},      // '#'
    {2,	{0x10, 0x34},	0x24},      // '$'
    {2,	{0x10, 0x35},	0x25},      // '%'
    {2,	{0x10, 0x37},	0x26},      // '&'
    {1,	{0xDE, 0x00},	0x27},      // '''
    {2,	{0x10, 0x39},	0x28},      // '('
    {2,	{0x10, 0x30},	0x29},      // ')'
    {2,	{0x10, 0x38},	0x2A},	    // '*'
    {2,	{0x10, 0xBB},	0x2B},      // '+'
    {1,	{0xBC, 0x00},	0x2C},      // ','
    {1,	{0xBD, 0x00},	0x2D},      // '-'
    {1,	{0xBE, 0x00},	0x2E},      // '.'
    {1,	{0xBF, 0x00},	0X2F},      // '/'
	{1,	{0x30, 0x00},	0x30},      // '0'
    {1,	{0x31, 0x00},	0x31},      // '1'
    {1,	{0x32, 0x00},	0x32},      // '2'
    {1,	{0x33, 0x00},	0X33},      // '3'
	{1,	{0x34, 0x00},	0x34},      // '4'
    {1,	{0x35, 0x00},	0x35},      // '5'
    {1,	{0x36, 0x00},	0x36},      // '6'
    {1,	{0x37, 0x00},	0X37},      // '7'
	{1,	{0x38, 0x00},	0x38},      // '8'
    {1,	{0x39, 0x00},	0X39},      // '9'
	{2, {0x10, 0xBA},	0X3A},      // ':'
    {1, {0xBA, 0x00},	0X3B},      // ';'
    {2, {0x10, 0xBC},	0X3C},      // '<'
    {1, {0xBB, 0x00},	0X3D},      // '='
    {2, {0x10, 0xBE},	0X3E},      // '>'
    {2, {0x10, 0xBF},	0X3F},      // '?'
	{2,	{0x10, 0x41},	0x61},      // 'A'
    {2,	{0x10, 0x42},	0x62},      // 'B'
    {2,	{0x10, 0x43},	0x63},      // 'C'
    {2,	{0x10, 0x44},	0x64},      // 'D'
	{2,	{0x10, 0x45},	0x65},      // 'E'
    {2,	{0x10, 0x46},	0x66},      // 'F'
    {2,	{0x10, 0x47},	0x67},      // 'G'
    {2,	{0x10, 0x48},	0x68},      // 'H'
	{2,	{0x10, 0x49},	0x69},      // 'I'
    {2,	{0x10, 0x4A},	0x6A},      // 'J'
	{2,	{0x10, 0x4B},	0x6B},      // 'K'
    {2,	{0x10, 0x4C},	0x6C},      // 'L'
    {2,	{0x10, 0x4D},	0x6D},      // 'M'
    {2,	{0x10, 0x4E},	0x6E},      // 'N'
	{2,	{0x10, 0x4F},	0x6F},      // 'O'
    {2,	{0x10, 0x50},	0x70},      // 'P'
    {2,	{0x10, 0x51},	0x71},      // 'Q'
    {2,	{0x10, 0x52},	0x72},      // 'R'
	{2,	{0x10, 0x53},	0x73},      // 'S'
    {2,	{0x10, 0x54},	0x74},      // 'T'
	{2,	{0x10, 0x55},	0x75},      // 'U'
    {2,	{0x10, 0x56},	0x76},      // 'V'
    {2,	{0x10, 0x57},	0x77},      // 'W'
    {2,	{0x10, 0x58},	0x78},      // 'X'
	{2,	{0x10, 0x59},	0x79},      // 'Y'
    {2,	{0x10, 0x5A},	0x7A},      // 'Z'
    {1, {0xDB, 0x00},	0X5B},      // '['
    {1, {0xDC, 0x00},	0X5C},      // '\'
    {1, {0xDD, 0x00},	0X5D},      // ']'
    {2, {0x10, 0x36},	0X5E},      // '^'
    {2, {0x10, 0xBD},	0X5F},      // '_'
    {1, {0xC0, 0x00},	0X60},      // '`'
    {1,	{0x41, 0x00},	0x41},      // 'a'
    {1,	{0x42, 0x00},	0x42},      // 'b'
    {1,	{0x43, 0x00},	0x43},      // 'c'
    {1,	{0x44, 0x00},	0x44},      // 'd'
	{1,	{0x45, 0x00},	0x45},      // 'e'
    {1,	{0x46, 0x00},	0x46},      // 'f'
    {1,	{0x47, 0x00},	0x47},      // 'g'
    {1,	{0x48, 0x00},	0x48},      // 'h'
	{1,	{0x49, 0x00},	0x49},      // 'i'
    {1,	{0x4A, 0x00},	0x4A},      // 'j'
	{1,	{0x4B, 0x00},	0x4B},      // 'k'
    {1,	{0x4C, 0x00},	0x4C},      // 'l'
    {1,	{0x4D, 0x00},	0x4D},      // 'm'
    {1,	{0x4E, 0x00},	0x4E},      // 'n'
	{1,	{0x4F, 0x00},	0x4F},      // 'o'
    {1,	{0x50, 0x00},	0x50},      // 'p'
    {1,	{0x51, 0x00},	0x51},      // 'q'
    {1,	{0x52, 0x00},	0x52},      // 'r'
	{1,	{0x53, 0x00},	0x53},      // 's'
    {1,	{0x54, 0x00},	0x54},      // 't'
	{1,	{0x55, 0x00},	0x55},      // 'u'
    {1,	{0x56, 0x00},	0x56},      // 'v'
    {1,	{0x57, 0x00},	0x57},      // 'w'
    {1,	{0x58, 0x00},	0x58},      // 'x'
	{1,	{0x59, 0x00},	0x59},      // 'y'
    {1,	{0x5A, 0x00},	0x5A},      // 'z'
	{2, {0x10, 0xDB},	0X7B},      // '{'
    {2, {0x10, 0xDC},	0X7C},      // '|'
    {2, {0x10, 0xDD},	0X7D},      // '}'
    {2, {0x10, 0xC0},	0X7E},      // '~'
	{1, { 13 ,  0 }, 13  },      // ENTER
	{1, { 27,   0 }, 27 },       // Escape key

// Control chars  with CAPSLOCK on
// Row ZXCVBNM 
 	{2, { 0x11, 90 } , 0x1A },
	{2, { 0x11, 88 },  0x18 },
	{2, { 0x11, 67 },  0x03 },
	{2, { 0x11, 86 },  0x16 },
	{2, { 0x11, 66 },  0x02 },
    {2, { 0x11, 78 },  0x0E },
	{2, { 0x11, 77 },  0x0D},
//Row ASDFG
	{2, { 0x11, 65 }, 0x01 },
	{2, { 0x11, 83 }, 0x13 },
	{2, { 0x11, 68 }, 0x04 },
	{2, { 0x11, 70 }, 0x06 },
	{2, { 0x11, 71 }, 0x07 },
    {2, { 0x11, 72 }, 0x08 },
	{2, { 0x11, 74 }, 0x0A },
	{2, { 0x11, 75 }, 0x0B },
	{2, { 0x11, 76 }, 0x0C },
// Row QWERTY
	{2, {0x11, 81 }, 0x11 },
	{2, {0x11, 87 }, 0x17 },
	{2, {0x11, 69 }, 0x05 },
	{2, {0x11, 82 }, 0x12 },
	{2, {0x11, 84 }, 0x14 },
	{2, {0x11, 89 }, 0x19 },
	{2, {0x11, 85 }, 0x15 },
	{2, {0x11, 73 }, 0x09 },
	{2, {0x11, 79 }, 0x0F },
	{2, {0x11, 80 }, 0x10 },



};

//    For Japanese keyboard ***************************
//  Num  VK1   VK2		Unicode

testSequence Japanese1[] = {

	{1, { 49 ,0}, '1'},
	{1, { 50, 0}, '2'},
	{1, { 51, 0}, '3'},
	{1, { 52 ,0}, '4'},
	{1, { 53 ,0}, '5'},
	{1, { 54, 0}, '6'},
	{1, { 55, 0}, '7'},
	{1, { 56, 0}, '8'},
	{1, { 57, 0}, '9'},
	{1, { 48, 0}, '0'},
	{1, {189, 0}, '-'},
	{1, {187, 0}, '^'},

	{2, {0x10, 49}, '!'},
	{2, {0x10, 50}, '\"'},
	{2, {0x10, 51}, '#'},
	{2, {0x10, 52}, '$'},
	{2, {0x10, 53}, '%'},
	{2, {0x10, 54}, '&'},
	{2, {0x10, 55}, '\''},
	{2, {0x10, 56}, '('},
	{2, {0x10, 57}, ')'},
	{2, {0x10, 48}, '~'},
	{2, {0x10, 189}, '='},
//    {2, {0x10, 187}, '_'},   // fails for Japanese keyboard

    {1, {8,  0 },   8},


	{1, { 9,  0 },  9},
	{1, { 81, 0 }, 'q'},
	{1, { 87, 0 }, 'w'},
	{1, { 69, 0 }, 'e'},
	{1, { 82, 0 }, 'r'},
	{1, { 84, 0 }, 't'},
	{1, { 89,  0 }, 'y'},
	{1, { 85,  0 }, 'u'},
	{1, { 73,  0 }, 'i'},
	{1, { 79,  0 }, 'o'},
	{1, { 80,  0 }, 'p'},
	{1, {192, 0 }, '@'},
	{1, {219,  0 }, '['},
    {1, {221,  0 }, ']'},

	{2, {0x10,  9 },   9},
	{2, {0x10, 81 }, 'Q'},
	{2, {0x10, 87 }, 'W'},
	{2, {0x10, 69 }, 'E'},
	{2, {0x10, 82 }, 'R'},
	{2, {0x10, 84 }, 'T'},
	{2, {0x10, 89 }, 'Y'},
	{2, {0x10, 85 }, 'U'},
	{2, {0x10, 73 }, 'I'},
	{2, {0x10, 79 }, 'O'},
	{2, {0x10, 80 }, 'P'},
	{2, {0x10,192 }, '`'},
	{2, {0x10,219 }, '{'},
    {2, {0x10,221 }, '}'},

	{1, { 65 , 0 } ,'a' },
	{1, { 83 , 0 }, 's' },
	{1, { 68 , 0 }, 'd' },
	{1, { 70 , 0 }, 'f' },
	{1, { 71 , 0 }, 'g' },
    {1, { 72 , 0 }, 'h' },
	{1, { 74 , 0 }, 'j' },
	{1, { 75 , 0 }, 'k' },
	{1, { 76 , 0 }, 'l' },
	{1, { 186 , 0 }, ';' },
	{1, { 222 , 0 }, ':' },
	{1, { 13 ,  0 }, 13  },      // ENTER

	{2, { 0x10, 65 } ,'A' },
	{2, { 0x10, 83 }, 'S' },
	{2, { 0x10, 68 }, 'D' },
	{2, { 0x10, 70 }, 'F' },
	{2, { 0x10, 71 }, 'G' },
    {2, { 0x10, 72 }, 'H' },
	{2, { 0x10, 74 }, 'J' },
	{2, { 0x10, 75 }, 'K' },
	{2, { 0x10, 76 }, 'L' },
	{2, { 0x10,186 }, '+' },
	{2, { 0x10,222 }, '*' },
	{2, { 0x10, 13 }, 13  },


	{1, {  90, 0 }, 'z' },
	{1, {  88, 0 }, 'x' },
	{1, {  67, 0 }, 'c' },
	{1, {  86, 0 }, 'v' },
	{1, {  66, 0 }, 'b' },
    {1, {  78, 0 }, 'n' },
	{1, {  77, 0 }, 'm' },
	{1, { 188 , 0 }, ',' },
	{1, { 190 , 0 }, '.' },
	{1, { 191 , 0 }, '/' },


	{2, { 0x10, 90 } ,'Z' },
	{2, { 0x10, 88 }, 'X' },
	{2, { 0x10, 67 }, 'C' },
	{2, { 0x10, 86 }, 'V' },
	{2, { 0x10, 66 }, 'B' },
    {2, { 0x10, 78 }, 'N' },
	{2, { 0x10, 77 }, 'M' },
	{2, { 0x10, 188 }, '<'},
	{2, { 0x10, 190}, '>'},
	{2, { 0x10, 191}, '?' },

    {1, {0x1B, 0}, 0x1B},
	{1, { 226,   0 }, '\\' }, // \ key
	{2, { 0x10, 226}, '_'},
	{2, { 0x10, 220}, '|'},

  // Control chars
// Row ZXCVBNM 
 	{2, { 0x11, 90 } , 0x1A },
	{2, { 0x11, 88 },  0x18 },
	{2, { 0x11, 67 },  0x03 },
	{2, { 0x11, 86 },  0x16 },
	{2, { 0x11, 66 },  0x02 },
    {2, { 0x11, 78 },  0x0E },
	{2, { 0x11, 77 },  0x0D},
//Row ASDFG
	{2, { 0x11, 65 }, 0x01 },
	{2, { 0x11, 83 }, 0x13 },
	{2, { 0x11, 68 }, 0x04 },
	{2, { 0x11, 70 }, 0x06 },
	{2, { 0x11, 71 }, 0x07 },
    {2, { 0x11, 72 }, 0x08 },
	{2, { 0x11, 74 }, 0x0A },
	{2, { 0x11, 75 }, 0x0B },
	{2, { 0x11, 76 }, 0x0C },
// Row QWERTY
	{2, {0x11, 81 }, 0x11 },
	{2, {0x11, 87 }, 0x17 },
	{2, {0x11, 69 }, 0x05 },
	{2, {0x11, 82 }, 0x12 },
	{2, {0x11, 84 }, 0x14 },
	{2, {0x11, 89 }, 0x19 },
	{2, {0x11, 85 }, 0x15 },
	{2, {0x11, 73 }, 0x09 },
	{2, {0x11, 79 }, 0x0F },
	{2, {0x11, 80 }, 0x10 },
  
// CAPSLOCK
    {9, {0x10, 0x14 }, 0 },
	
	{1, { 49 ,0}, '1'},
	{1, { 50, 0}, '2'},
	{1, { 51, 0}, '3'},
	{1, { 52 ,0}, '4'},
	{1, { 53 ,0}, '5'},
	{1, { 54, 0}, '6'},
	{1, { 55, 0}, '7'},
	{1, { 56, 0}, '8'},
	{1, { 57, 0}, '9'},
	{1, { 48, 0}, '0'},
	{1, {189, 0}, '-'},
	{1, {187, 0}, '^'},

	{2, {0x10, 49}, '!'},
	{2, {0x10, 50}, '\"'},
	{2, {0x10, 51}, '#'},
	{2, {0x10, 52}, '$'},
	{2, {0x10, 53}, '%'},
	{2, {0x10, 54}, '&'},
	{2, {0x10, 55}, '\''},
	{2, {0x10, 56}, '('},
	{2, {0x10, 57}, ')'},
	{2, {0x10, 48}, '~'},
	{2, {0x10, 189}, '='},
    {1, {8,  0 },   8},


	{1, { 9,  0 },  9},
	{1, { 81, 0 }, 'Q'},
	{1, { 87, 0 }, 'W'},
	{1, { 69, 0 }, 'E'},
	{1, { 82, 0 }, 'R'},
	{1, { 84, 0 }, 'T'},
	{1, { 89,  0 }, 'Y'},
	{1, { 85,  0 }, 'U'},
	{1, { 73,  0 }, 'I'},
	{1, { 79,  0 }, 'O'},
	{1, { 80,  0 }, 'P'},
	{1, {192, 0 }, '@'},
	{1, {219,  0 }, '['},
    {1, {221,  0 }, ']'},
	

	{2, {0x10,  9 },   9},
	{2, {0x10, 81 }, 'q'},
	{2, {0x10, 87 }, 'w'},
	{2, {0x10, 69 }, 'e'},
	{2, {0x10, 82 }, 'r'},
	{2, {0x10, 84 }, 't'},
	{2, {0x10, 89 }, 'y'},
	{2, {0x10, 85 }, 'u'},
	{2, {0x10, 73 }, 'i'},
	{2, {0x10, 79 }, 'o'},
	{2, {0x10, 80 }, 'p'},
	{2, {0x10,192 }, '`'},
	{2, {0x10,219 }, '{'},
    {2, {0x10,221 }, '}'},  

	{1, {  90, 0 }, 'Z' },
	{1, {  88, 0 }, 'X' },
	{1, {  67, 0 }, 'C' },
	{1, {  86, 0 }, 'V' },
	{1, {  66, 0 }, 'B' },
    {1, {  78, 0 }, 'N' },
	{1, {  77, 0 }, 'M' },

	{2, { 0x10, 90 } ,'z' },
	{2, { 0x10, 88 }, 'x' },
	{2, { 0x10, 67 }, 'c' },
	{2, { 0x10, 86 }, 'v' },
	{2, { 0x10, 66 }, 'b' },
    {2, { 0x10, 78 }, 'n' },
	{2, { 0x10, 77 }, 'm' },
	{2, { 0x10, 188 }, '<'},
	{2, { 0x10, 190}, '>'},
	{2, { 0x10, 191}, '?' },

	{1, { 65 , 0 } ,'A' },
	{1, { 83 , 0 }, 'S' },
	{1, { 68 , 0 }, 'D' },
	{1, { 70 , 0 }, 'F' },
	{1, { 71 , 0 }, 'G' },
    {1, { 72 , 0 }, 'H' },
	{1, { 74 , 0 }, 'J' },
	{1, { 75 , 0 }, 'K' },
	{1, { 76 , 0 }, 'L' },
	{1, { 186 , 0 }, ';' },
	{1, { 222 , 0 }, ':' },
	{1, { 13 ,  0 }, 13  },      // ENTER

	{2, { 0x10, 65 } ,'a' },
	{2, { 0x10, 83 }, 's' },
	{2, { 0x10, 68 }, 'd' },
	{2, { 0x10, 70 }, 'f' },
	{2, { 0x10, 71 }, 'g' },
    {2, { 0x10, 72 }, 'h' },
	{2, { 0x10, 74 }, 'j' },
	{2, { 0x10, 75 }, 'k' },
	{2, { 0x10, 76 }, 'l' },
	{2, { 0x10,186 }, '+' },
	{2, { 0x10,222 }, '*' },
	{2, { 0x10, 13 }, 13  },
	{1, { 226,   0 }, '\\' }, // \ key
	{2, { 0x10, 226}, '_'},
	{2, { 0x10, 220}, '|'},
  // Control chars
// Row ZXCVBNM 
 	{2, { 0x11, 90 } , 0x1A },
	{2, { 0x11, 88 },  0x18 },
	{2, { 0x11, 67 },  0x03 },
	{2, { 0x11, 86 },  0x16 },
	{2, { 0x11, 66 },  0x02 },
    {2, { 0x11, 78 },  0x0E },
	{2, { 0x11, 77 },  0x0D},
//Row ASDFG
	{2, { 0x11, 65 }, 0x01 },
	{2, { 0x11, 83 }, 0x13 },
	{2, { 0x11, 68 }, 0x04 },
	{2, { 0x11, 70 }, 0x06 },
	{2, { 0x11, 71 }, 0x07 },
    {2, { 0x11, 72 }, 0x08 },
	{2, { 0x11, 74 }, 0x0A },
	{2, { 0x11, 75 }, 0x0B },
	{2, { 0x11, 76 }, 0x0C },
// Row QWERTY
	{2, {0x11, 81 }, 0x11 },
	{2, {0x11, 87 }, 0x17 },
	{2, {0x11, 69 }, 0x05 },
	{2, {0x11, 82 }, 0x12 },
	{2, {0x11, 84 }, 0x14 },
	{2, {0x11, 89 }, 0x19 },
	{2, {0x11, 85 }, 0x15 },
	{2, {0x11, 73 }, 0x09 },
	{2, {0x11, 79 }, 0x0F },
	{2, {0x11, 80 }, 0x10 },
// Alt keys

//   {2, {0x12, 81 }, 'Q'},


};
#endif _testSequences_h_
