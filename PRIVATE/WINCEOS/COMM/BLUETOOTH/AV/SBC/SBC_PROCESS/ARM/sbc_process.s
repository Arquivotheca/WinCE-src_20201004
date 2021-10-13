;
; Copyright (c) Microsoft Corporation.  All rights reserved.
;
;
; Use of this source code is subject to the terms of the Microsoft shared
; source or premium shared source license agreement under which you licensed
; this source code. If you did not accept the terms of the license agreement,
; you are not authorized to use this source code. For the terms of the license,
; please see the license agreement between you and Microsoft or, if applicable,
; see the SOURCE.RTF on your install media or the root of your tools installation.
; THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
;
	OPT     2       ; disable listing
	INCLUDE kxarm.h
	OPT     1       ; reenable listing
        


	AREA	|.rdata|, DATA, READONLY
|indexes| DCB	0x0
	DCB	0x10
	DCB	0x20
	DCB	0x30
	DCB	0x40
	DCB	0x8
	DCB	0x18
	DCB	0x28
	DCB	0x38
	DCB	0x48
	DCB	0x8
	DCB	0x18
	DCB	0x28
	DCB	0x38
	DCB	0x48
	DCB	0x10
	DCB	0x20
	DCB	0x30
	DCB	0x40
	DCB	0x0
	DCB	0x10
	DCB	0x20
	DCB	0x30
	DCB	0x40
	DCB	0x0
	DCB	0x18
	DCB	0x28
	DCB	0x38
	DCB	0x48
	DCB	0x8
	DCB	0x18
	DCB	0x28
	DCB	0x38
	DCB	0x48
	DCB	0x8
	DCB	0x20
	DCB	0x30
	DCB	0x40
	DCB	0x0
	DCB	0x10
	DCB	0x20
	DCB	0x30
	DCB	0x40
	DCB	0x0
	DCB	0x10
	DCB	0x28
	DCB	0x38
	DCB	0x48
	DCB	0x8
	DCB	0x18
	DCB	0x28
	DCB	0x38
	DCB	0x48
	DCB	0x8
	DCB	0x18
	DCB	0x30
	DCB	0x40
	DCB	0x0
	DCB	0x10
	DCB	0x20
	DCB	0x30
	DCB	0x40
	DCB	0x0
	DCB	0x10
	DCB	0x20
	DCB	0x38
	DCB	0x48
	DCB	0x8
	DCB	0x18
	DCB	0x28
	DCB	0x38
	DCB	0x48
	DCB	0x8
	DCB	0x18
	DCB	0x28
	DCB	0x40
	DCB	0x0
	DCB	0x10
	DCB	0x20
	DCB	0x30
	DCB	0x40
	DCB	0x0
	DCB	0x10
	DCB	0x20
	DCB	0x30
	DCB	0x48
	DCB	0x8
	DCB	0x18
	DCB	0x28
	DCB	0x38
	DCB	0x48
	DCB	0x8
	DCB	0x18
	DCB	0x28
	DCB	0x38
	DCB	0x0
	DCB	0x10
	DCB	0x20
	DCB	0x30
	DCB	0x40
	%	4
|Proto_8| DCW	0x1
	DCW	0x3
	DCW	0x4
	DCW	0x6
	DCW	0x9
	DCW	0xc
	DCW	0xe
	DCW	0x2e
	DCW	0x42
	DCW	0x55
	DCW	0x68
	DCW	0x78
	DCW	0x82
	DCW	0x85
	DCW	0x7d
	DCW	0x22d
	DCW	0x2a8
	DCW	0x31f
	DCW	0x38f
	DCW	0x3f2
	DCW	0x443
	DCW	0x481
	DCW	0x4a7
	DCW	0xfdd3
	DCW	0xfe4d
	DCW	0xfec0
	DCW	0xff2a
	DCW	0xff88
	DCW	0xffd8
	DCW	0x18
	DCW	0x48
	DCW	0xffd2
	DCW	0xffe4
	DCW	0xfff3
	DCW	0xffff
	DCW	0x7
	DCW	0xd
	DCW	0x10
	DCW	0x11
	DCW	0x10
	DCW	0x11
	DCW	0x10
	DCW	0xd
	DCW	0x1
	DCW	0xd
	DCW	0x1c
	DCW	0x6a
	DCW	0x48
	DCW	0x18
	DCW	0xffd8
	DCW	0xd6
	DCW	0x140
	DCW	0x1b3
	DCW	0x4b4
	DCW	0x4a7
	DCW	0x481
	DCW	0x443
	DCW	0x38f
	DCW	0x31f
	DCW	0x2a8
	DCW	0x6a
	DCW	0x7d
	DCW	0x85
	DCW	0x82
	DCW	0x68
	DCW	0x55
	DCW	0x42
	DCW	0x10
	DCW	0xe
	DCW	0xc
	DCW	0x9
	DCW	0x4
	DCW	0x3
	%	2
	
	TEXTAREA

	LEAF_ENTRY	?process@@YAXPBFPAHAAHPAF@Z

	stmdb       sp!, {r4 - r11, lr}
	sub         sp, sp, #0x24
|$M396|
	mov         r6, r3
	str         r6, [sp]
	str         r1, [sp, #0x20]

; 98   :     //int sb2 = 2 * SBCSUBBANDS;
; 99   :     int Y[2 * SBCSUBBANDS];
; 100  : 
; 101  :     int i; 
; 102  :     // Shifting and input
; 103  :     int idx = XStart;

	ldr         lr, [r2]

; 104  : 
; 105  :     pBaseX[idx] = audio_samples[0];

	ldrsh       r4, [r0]

; 106  : 
; 107  :     pBaseX[idx - 1] = audio_samples[2];
; 108  :     pBaseX[idx - 2] = audio_samples[4];
; 109  :     pBaseX[idx - 3] = audio_samples[6];
; 110  :     pBaseX[idx - 4] = audio_samples[8];
; 111  :     pBaseX[idx - 5] = audio_samples[10];
; 112  :     pBaseX[idx - 6] = audio_samples[12];
; 113  :     pBaseX[idx - 7] = audio_samples[14];
; 114  : 
; 115  : 
; 116  :     // Handle index looping around
; 117  :     XStart -= SBCSUBBANDS;
; 118  :     if (XStart < 0) {
; 119  :         XStart = XStart + X_END_PLUS_1;
; 120  :     } 
; 121  : 
; 122  :     int idxStart = XStart + 1;
; 123  : 
; 124  :     short * pX ;
; 125  :     
; 126  :     if(idxStart == 80)
; 127  :         idxStart = 0;
; 128  : 
; 129  :     i = idxStart>>3;
; 130  :     
; 131  :     const short * P8 = Proto_8;
; 132  :     pX = pBaseX + indexes[i][0] + 1;

	ldr         r1, [pc, #0x6A8]
	add         r5, r6, lr, lsl #1
	strh        r4, [r5]
	ldrsh       r7, [r0, #4]

; 133  :     
; 134  :     Y[1] = *pX *  *P8; P8++; pX++;
; 135  :     Y[2] = *pX *  *P8; P8++; pX++;
; 136  :     Y[3] = *pX *  *P8; P8++; pX++;
; 137  :     Y[4] = *pX *  *P8; P8++; pX++;
; 138  :     Y[3] += *pX *  *P8; P8++; pX++;    
; 139  :     Y[2] += *pX *  *P8; P8++; pX++;
; 140  :     Y[1] += *pX *  *P8; P8++;
; 141  :     pX = pBaseX + indexes[i][1];
; 142  :     Y[0] = *pX *  *P8; P8++; pX++;
; 143  :     Y[1] += *pX *  *P8; P8++; pX++;
; 144  :     Y[2] += *pX *  *P8; P8++; pX++;
; 145  :     Y[3] += *pX *  *P8; P8++; pX++;
; 146  :     Y[4] += *pX *  *P8; P8++; pX++;
; 147  :     Y[3] += *pX *  *P8; P8++; pX++;
; 148  :     Y[2] += *pX *  *P8; P8++; pX++;
; 149  :     Y[1] += *pX *  *P8; P8++;
; 150  :     pX = pBaseX + indexes[i][2];
; 151  :     Y[0] += *pX *  *P8; P8++; pX++;
; 152  :     Y[1] += *pX *  *P8; P8++; pX++;
; 153  :     Y[2] += *pX *  *P8; P8++; pX++;
; 154  :     Y[3] += *pX *  *P8; P8++; pX++;

	mov         r11, #0x2E ;mul for r0
	strh        r7, [r5, #-2]
	subs        lr, lr, #8

	ldrsh       r3, [r0, #8]
	addmi       lr, lr, #0x50

	ldrsh       r7, [r0, #0xC]
	str         lr, [r2]

	strh        r3, [r5, #-4]
	strh        r7, [r5, #-6]

; 74   :     pBaseX[idx - 4] = audio_samples[8];
; 75   :     pBaseX[idx - 5] = audio_samples[10];

	ldrsh       r3, [r0, #0x10]
	ldrsh       r7, [r0, #0x14]
	strh        r3, [r5, #-8]
	strh        r7, [r5, #-0xA]

; 76   :     pBaseX[idx - 6] = audio_samples[12];
; 77   :     pBaseX[idx - 7] = audio_samples[14];

	ldrsh       r3, [r0, #0x18]
	ldrsh       r7, [r0, #0x1C]
	strh        r3, [r5, #-0xC]

; 117  :     XStart -= SBCSUBBANDS;
; 118  :     if (XStart < 0) {
; 119  :         XStart = XStart + X_END_PLUS_1;
; 120  :     } 
	

	
	add         lr, lr, #1
	cmp         lr, #0x50
	moveq       lr, #0
	mov         lr, lr, asr #3

	add         lr, lr, lr, lsl #2
	
	ldrb        r2, [r1, +lr, lsl #1]!
	strh        r7, [r5, #-0xE]
	;add         r1, r1, lr, lsl #1
	ldrb        r4, [r1, #1]
	add         r3, r2, #1
	add         r0, r6, r3, lsl #1
	ldrsh       r8, [r0], #2
	ldrb        r10, [r1, #2]
	str         r1, [sp, #4]

; 155  :     Y[4] += *pX *  *P8; P8++; pX++;
; 156  :     Y[3] += *pX *  *P8; P8++; pX++;

	add         r6, r6, r4, lsl #1
	ldrsh       r3, [r0], #2
	ldrsh       r4, [r6], #2
	ldrsh       lr, [r0], #2
	add         r7, r3, r3, lsl #1
	mul         r11, r4, r11
	ldrsh       r3, [r0], #2
	ldrsh       r1, [r0], #2
	add         r3, r3, r3, lsl #1
	mov         r9, r3, lsl #1
	ldrsh       r2, [r0]
	ldrsh       r0, [r0, #2]
	add         r3, r1, r1, lsl #3
	ldrsh       r1, [r6], #2
	add         r5, r3, lr, lsl #2
	rsb         r3, r0, r0, lsl #3
	add         r0, r8, r3, lsl #1
	ldrsh       r3, [r6], #2
	add         r2, r2, r2, lsl #1
	add         lr, r7, r2, lsl #2
	add         r2, r1, r1, lsl #5
	ldrsh       r1, [r6], #2
	add         r3, r3, r3, lsl #4
	add         r3, r3, r3, lsl #2
	add         r7, lr, r3
	ldrsh       r3, [r6], #2
	add         r8, r0, r2, lsl #1
	mov         r2, #0x68
	mla         r4, r1, r2, r5
	rsb         r3, r3, r3, lsl #4
	ldrsh       r2, [r6], #2
	add         r9, r9, r3, lsl #3
	ldr         r3, [sp]
	ldrsh       r0, [r6]
	ldrsh       lr, [r6, #2]
	add         r10, r3, r10, lsl #1
	ldrsh       r6, [r10], #2
	add         r3, r2, r2, lsl #6
	add         r5, r4, r3, lsl #1
	ldrsh       r1, [r10], #2
	rsb         r3, lr, lr, lsl #5
	add         r2, r0, r0, lsl #5
	add         r3, lr, r3, lsl #2
	add         r2, r0, r2, lsl #2
	add         r0, r8, r3
	mov         r8, #0x8B, 30
	ldrsh       r4, [r10], #2
	add         r1, r1, r1, lsl #4
	orr         r8, r8, #1
	add         lr, r7, r2
	add         r2, r1, r1, lsl #2
	mla         r1, r6, r8, r11
	ldrsh       r3, [r10], #2
	str         r1, [sp, #0x10]
	mov         r1, #0xE3, 30
	orr         r1, r1, #3
	mla         r1, r3, r1, r5
	add         r5, r0, r2, lsl #3
	mov         r2, #0xC7, 30
	orr         r2, r2, #3
	ldrsh       r3, [r10], #2
	mla         r7, r4, r2, lr
	mov         r2, #0x3F, 28
	orr         r2, r2, #2
	mla         r2, r3, r2, r9
	ldrsh       r3, [r10], #2

; 157  :     Y[2] += *pX *  *P8; P8++; pX++;

	mov         r0, #0x11, 26
	orr         r0, r0, #3
	str         r2, [sp, #0x14]
	ldrsh       r2, [r10]

; 158  :     Y[1] += *pX *  *P8; P8++;
; 159  :     pX = pBaseX + indexes[i][3];

	ldr         lr, [sp, #4]
	mla         r11, r3, r0, r1
	add         r3, r2, r2, lsl #3
	add         r0, r2, r3, lsl #7
	ldrb        r3, [lr, #3]

; 160  :     Y[0] += *pX *  *P8; P8++; pX++;
; 161  :     Y[1] += *pX *  *P8; P8++; pX++;
; 162  :     Y[2] += *pX *  *P8; P8++; pX++;
; 163  :     Y[3] += *pX *  *P8; P8++; pX++;
; 164  :     Y[4] += *pX *  *P8; P8++; pX++;
; 165  :     Y[3] += *pX *  *P8; P8++; pX++;
; 166  :     Y[2] += *pX *  *P8; P8++; pX++;
; 167  :     Y[1] += *pX *  *P8; P8++;
; 168  :     pX = pBaseX + indexes[i][4];

	ldr         r2, [sp]
	ldrsh       r1, [r10, #2]

; 169  :     Y[0] += *pX *  *P8; P8++; pX++;
; 170  :     Y[1] += *pX *  *P8; P8++; pX++;
; 171  :     Y[2] += *pX *  *P8; P8++; pX++;
; 172  :     Y[3] += *pX *  *P8; P8++; pX++;
; 173  :     Y[4] = (Y[4] + *pX * *P8)>> BITSHIFT_FACTOR2; pX++;P8++;
; 174  :     Y[4] *= (16384/DIVIDE_FACTOR);
; 175  :     Y[3] = (Y[3] + *pX * *P8)>> BITSHIFT_FACTOR2; pX++;P8++;
; 176  :     Y[2] = (Y[2] + *pX * *P8)>> BITSHIFT_FACTOR2; pX++;P8++;
; 177  :     Y[1] = (Y[1] + *pX * *P8)>> BITSHIFT_FACTOR2; P8++;
; 178  : 
; 179  : 
; 180  :     pX = pBaseX + indexes[i][5];

	mov         lr, #0x1B, 28
	add         r10, r2, r3, lsl #1
	ldrsh       r6, [r10], #2
	orr         lr, lr, #3
	mov         r4, #0x4A, 28
	orr         r4, r4, #7
	ldrsh       r3, [r10], #2
	mov         r9, #0xD6
	mla         r5, r1, r4, r5
	mul         lr, r3, lr
	ldrsh       r2, [r10], #2
	add         r4, r7, r0
	ldr         r7, [sp, #0x14]
	ldrsh       r3, [r10], #2
	add         r2, r2, r2, lsl #2
	mul         r1, r3, r9
	mul         r9, r6, r8
	sub         r8, r5, lr
	ldrsh       r0, [r10], #2
	ldr         r5, [sp, #4]
	sub         r6, r4, r2, lsl #6
	sub         lr, r11, r1
	ldrb        r4, [r5, #4]
	ldr         r11, [sp]
	ldrsh       r3, [r10], #2
	rsb         r1, r0, r0, lsl #4
	add         r11, r11, r4, lsl #1
	ldrsh       r5, [r11], #2
	add         r0, r3, r3, lsl #2
	ldrsh       r3, [r10]
	ldrsh       r2, [r10, #2]
	ldrsh       r4, [r11], #2
	add         r3, r3, r3, lsl #1
	sub         r10, r7, r1, lsl #3
	add         r2, r2, r2, lsl #3
	ldrsh       r1, [r11], #2
	add         r6, r6, r3, lsl #3
	sub         r7, lr, r0, lsl #3
	mov         r3, #0x2E
	add         lr, r8, r2, lsl #3
	mul         r8, r5, r3
	add         r3, r1, r1, lsl #1
	ldrsh       r0, [r11], #2
	add         r3, r1, r3, lsl #2
	ldr         r1, [sp, #0x10]
	rsb         r2, r4, r4, lsl #3
	sub         r5, r7, r0
	sub         r4, r1, r9
	ldrsh       r1, [r11], #2
	sub         r7, r4, r8
	ldr         r4, [sp, #4]
	sub         r9, r6, r3
	rsb         r3, r1, r1, lsl #3
	ldr         r6, [sp]
	add         r1, r3, r10
	ldrb        r3, [r4, #5]
	str         r11, [sp, #0x14]
	sub         r11, lr, r2, lsl #2
	ldr         r2, [sp, #0x14]
	add         r8, r6, r3, lsl #1

; 181  :     Y[0] += *pX *  *P8; P8++; pX++;

	ldrsh       r4, [r8], #2
	ldrsh       r0, [r2], #2

; 182  :     Y[9] = *pX *  *P8; P8++; pX++;

	mov         r1, r1, asr #13
	str         r1, [sp, #0x14]
	ldrsh       r6, [r8], #2
	add         r3, r0, r0, lsl #1
	ldrsh       lr, [r2]
	add         r3, r0, r3, lsl #2
	ldrsh       r2, [r2, #2]
	add         r3, r3, r5

; 183  :     Y[10] = *pX *  *P8; P8++; pX++;

	ldrsh       r5, [r8], #2
	mov         r3, r3, asr #13
	add         r1, r9, lr, lsl #4
	add         r2, r2, r2, lsl #4

; 184  :     Y[11] = *pX *  *P8; P8++; pX++;

	ldrsh       r0, [r8], #4
	str         r3, [sp, #8]
	mov         r3, r1, asr #13
	add         r2, r2, r11
	str         r3, [sp, #0x10]
	mov         r3, r2, asr #13

; 185  :     pX++;
; 186  :     Y[11] += *pX *  *P8; P8++; pX++;

	ldrsh       r2, [r8], #2
	str         r3, [sp, #0xC]

; 187  :     Y[10] += *pX *  *P8; P8++; pX++;
; 188  :     Y[9] += *pX *  *P8; P8++;
; 189  :     pX = pBaseX + indexes[i][6];

	ldr         r1, [sp, #4]
	add         r3, r0, r0, lsl #1
	add         r3, r0, r3, lsl #2
	add         r10, r7, r4, lsl #4

; 190  :     Y[0] += *pX *  *P8; P8++; pX++;
; 191  :     Y[9] += *pX *  *P8; P8++; pX++;
; 192  :     Y[10] += *pX *  *P8; P8++; pX++;
; 193  :     Y[11] += *pX *  *P8; P8++; pX++;
; 194  :     pX++;
; 195  :     Y[11] += *pX *  *P8; P8++; pX++;
; 196  :     Y[10] += *pX *  *P8; P8++; pX++;
; 197  :     Y[9] += *pX *  *P8; P8++;
; 198  :     pX = pBaseX + indexes[i][7];

	ldr         r9, [sp]
	add         r7, r3, r2
	ldrb        r3, [r1, #6]
	add         lr, r6, r6, lsl #4
	ldrsh       r0, [r8]
	add         r6, r9, r3, lsl #1
	ldrsh       r3, [r6], #2
	ldrsh       r2, [r8, #2]
	ldrb        r8, [r1, #7]

; 199  :     Y[0] += *pX *  *P8; P8++; pX++;
; 200  :     Y[9] += *pX *  *P8; P8++; pX++;
; 201  :     Y[10] += *pX *  *P8; P8++; pX++;
; 202  :     Y[11] += *pX *  *P8; P8++; pX++;
; 203  :     pX++;
; 204  :     Y[11] -= *pX *  *P8; P8++; pX++;
; 205  :     Y[10] -= *pX *  *P8; P8++; pX++;
; 206  :     Y[9] -= *pX *  *P8; P8++;
; 207  :     pX = pBaseX + indexes[i][8];

	ldrsh       r1, [r6], #2
	rsb         r2, r2, r2, lsl #3
	str         r3, [sp, #0x18]
	add         r3, r0, r0, lsl #1
	add         r2, lr, r2, lsl #2
	ldrsh       lr, [r6], #2
	add         r0, r0, r3, lsl #2
	add         r3, r1, r1, lsl #3
	add         r4, r0, r5, lsl #4
	add         r5, r2, r3, lsl #3
	ldrsh       r3, [r6], #4
	add         r2, lr, lr, lsl #1
	add         r11, r9, r8, lsl #1
	add         lr, r4, r2, lsl #3
	ldrsh       r4, [r11], #2
	ldrsh       r0, [r6], #2
	add         r3, r3, r3, lsl #2
	sub         r1, r7, r3, lsl #3
	str         r4, [sp, #0x1C]
	mov         r4, #0xD6
	ldrsh       r3, [r6]
	ldrsh       r2, [r6, #2]
	mla         r9, r0, r4, r1
	mov         r1, #0x1B, 28
	orr         r1, r1, #3
	ldrsh       r4, [r11], #2
	mla         r7, r2, r1, r5
	add         r3, r3, r3, lsl #2
	ldrsh       r1, [r11], #2
	ldr         r2, [sp, #0x18]
	mov         r0, #0x6A
	add         r8, lr, r3, lsl #6
	add         r3, r1, r1, lsl #3
	mla         r10, r2, r0, r10
	ldrsh       r2, [r11], #4
	add         r0, r1, r3, lsl #7
	mov         r1, #0x11, 26
	orr         r1, r1, #3
	mla         r5, r2, r1, r9
	mov         r2, #0x4A, 28
	orr         r2, r2, #7
	ldrsh       r3, [r11], #2
	mla         r7, r4, r2, r7
	mov         r2, #0xE3, 30
	ldr         r4, [sp, #4]
	orr         r2, r2, #3
	mul         lr, r3, r2
	ldrsh       r3, [r11]
	ldrsh       r2, [r11, #2]
	ldr         r11, [sp]
	ldrb        r1, [r4, #8]

; 208  :     Y[0] += *pX *  *P8; P8++; pX++;

	add         r2, r2, r2, lsl #4

; 209  :     Y[9] += *pX *  *P8; P8++; pX++;

	sub         r5, r5, lr
	add         r11, r11, r1, lsl #1
	ldrsh       r9, [r11], #2
	mov         r1, #0xC7, 30
	orr         r1, r1, #3
	mul         r1, r3, r1
	ldrsh       r4, [r11], #2
	add         r3, r8, r0
	sub         lr, r3, r1
	add         r3, r2, r2, lsl #2

; 210  :     Y[10] += *pX *  *P8; P8++; pX++;

	ldrsh       r2, [r11], #2
	ldr         r0, [sp, #0x1C]
	mov         r6, #0x4B, 28
	orr         r6, r6, #4

; 211  :     Y[11] += *pX *  *P8; P8++; pX++;

	ldrsh       r1, [r11], #4
	mla         r8, r0, r6, r10
	rsb         r0, r4, r4, lsl #5
	sub         r6, r7, r3, lsl #3
	add         r4, r4, r0, lsl #2

; 212  :     pX++;
; 213  :     Y[11] -= *pX *  *P8; P8++; pX++;

	ldrsh       r0, [r11], #2
	add         r3, r2, r2, lsl #5
	add         r2, r2, r3, lsl #2
	add         r3, r1, r1, lsl #6
	add         r1, lr, r2

; 214  :     Y[10] -= *pX *  *P8; P8++; pX++;

	ldrsh       r2, [r11]
	add         lr, r5, r3, lsl #1
	add         r3, r0, r0, lsl #1
	add         r0, r0, r3, lsl #2
	add         r3, r2, r2, lsl #4
	add         r3, r3, r3, lsl #2
	sub         r7, r1, r3

; 215  :     Y[9] -= *pX *  *P8; P8++;
; 216  :     pX = pBaseX + indexes[i][9];

	ldr         r3, [sp, #4]
	mov         r10, #0x6A
	mla         r5, r9, r10, r8
	ldrb        r3, [r3, #9]
	sub         r10, lr, r0, lsl #3

; 217  :     Y[0] = (Y[0] + *pX * *P8)>> BITSHIFT_FACTOR2; P8++; pX++;
; 218  :     Y[0] *= (11585/DIVIDE_FACTOR);
; 219  :     Y[9] += *pX *  *P8; P8++; pX++;
; 220  :     Y[10] += *pX *  *P8; P8++; pX++;
; 221  :     Y[11] += *pX *  *P8; P8++; pX++;
; 222  :     pX++;
; 223  :     Y[11] = (Y[11] + *pX * *P8)>> BITSHIFT_FACTOR2; P8++; pX++;
; 224  :     Y[10] = (Y[10] - (*pX * *P8))>> BITSHIFT_FACTOR2; pX++;
; 225  :     Y[9] = (Y[9] - *pX)>> BITSHIFT_FACTOR2;
; 226  : 
; 227  : 
; 228  : 
; 229  : 
; 230  : 
; 231  : 
; 232  :     int s_same_sign, s_reverse_sign, s_same_sign_next1, s_same_sign_next2;
; 233  :     
; 234  :     //precision loss here, on top of loss on Y[k] and CC2
; 235  :     s_reverse_sign = (Y[1]) * D_6811;
; 236  :     s_same_sign_next1  = (Y[2]) * D_7568;
; 237  :     s_reverse_sign += (Y[3]) * D_8034;

	ldr         lr, [sp]
	add         r1, r6, r4
	mov         r0, #0x7D, 26
	add         r8, lr, r3, lsl #1
	ldrsh       r4, [r8], #2
	ldr         r3, [sp, #8]
	ldrsh       r2, [r11, #2]
	orr         r0, r0, #0x22
	mul         r6, r3, r0
	ldrsh       r3, [r8], #2
	add         r2, r2, r2, lsl #5
	sub         r1, r1, r2, lsl #1
	mov         r11, #0x76, 26
	ldrsh       r2, [r8], #2
	rsb         r3, r3, r3, lsl #3
	add         lr, r1, r3, lsl #1
	orr         r11, r11, #0x10
	add         r3, r2, r2, lsl #1
	ldrsh       r1, [r8], #4
	add         r0, r7, r3, lsl #2
	add         r4, r5, r4, lsl #4
	ldrsh       r9, [r8], #2
	ldr         r3, [sp, #0x10]
	add         r1, r1, r1, lsl #3
	mov         r7, #0x5A, 26
	mul         r5, r3, r11
	ldrsh       r3, [r8]
	ldrsh       r2, [r8, #2]
	ldr         r11, [sp, #0xC]
	add         r3, r3, r3, lsl #1
	mov         r8, #0x6A, 26
	sub         r3, r0, r3
	orr         r8, r8, #0x1B

; 238  :     s_same_sign = Y[4] + Y[0];     
; 239  :     s_reverse_sign += (Y[9]) * D_4551;
; 240  :     s_same_sign_next1 += ((Y[10]) * D_3135);

	mov         r0, #0xC3, 28
	sub         r2, lr, r2
	mla         r6, r11, r8, r6
	orr         r0, r0, #0xF
	mov         r8, r3, asr #13
	mov         lr, r2, asr #13

; 241  :     s_reverse_sign += (Y[11]) * D_1598;
; 242  : 
; 243  : 
; 244  :     s_same_sign += s_same_sign_next1;
; 245  :     subband_samples[0] = ((s_same_sign+s_reverse_sign)>>BITSHIFT_FACTOR2);
; 246  :     subband_samples[7] = ((s_same_sign-s_reverse_sign)>>BITSHIFT_FACTOR2);
; 247  : 
; 248  : 
; 249  : 
; 250  : 
; 251  :     s_reverse_sign = (Y[1]) * D_1598;
; 252  :     s_same_sign_next2  = (Y[2]) * D_3135;
; 253  :     s_reverse_sign -= (Y[3]) * D_6811;
; 254  :     s_same_sign = Y[4] - Y[0];
; 255  :     s_reverse_sign += (Y[9]) * D_8034;

	add         r2, r10, r1
	mla         r1, r8, r0, r5
	str         lr, [sp, #4]
	orr         r7, r7, #0x20
	mov         r3, r4, asr #13
	str         r1, [sp]
	mov         r1, #0x7D, 26
	orr         r1, r1, #0x22
	mul         r5, lr, r1
	ldr         lr, [sp, #8]
	mov         r1, #0x6A, 26
	orr         r1, r1, #0x1B
	mul         r4, lr, r1
	mul         lr, r7, r3
	add         r3, r2, r9, lsl #2
	mov         r7, #0x47, 26
	ldr         r2, [sp, #4]
	orr         r7, r7, #7
	mla         r1, r2, r7, r6

; 256  :     s_same_sign_next2 -= (Y[10]) * D_7568;

	ldr         r2, [sp, #0x10]
	mov         r9, r3, asr #13
	ldr         r3, [sp]
	mov         r11, #0x76, 26
	mul         r0, r2, r0
	mov         r10, #0x63, 28
	orr         r11, r11, #0x10

; 257  :     s_reverse_sign += (Y[11]) * D_4551;

	sub         r2, r5, r4
	ldr         r4, [sp, #0x14]
	str         lr, [sp, #0x1C]
	add         r3, r3, lr
	orr         r10, r10, #0xE
	mul         lr, r8, r11
	mla         r1, r9, r10, r1

; 258  : 
; 259  :     s_same_sign += s_same_sign_next2;

	ldr         r11, [sp, #0x1C]
	add         r3, r3, r4, lsl #13
	sub         r8, r0, lr
	ldr         lr, [sp, #0xC]
	mla         r2, r9, r7, r2
	add         r5, r3, r1
	sub         r6, r3, r1

; 260  :     subband_samples[1] = ((s_same_sign-s_reverse_sign)>>BITSHIFT_FACTOR2);
; 261  :     subband_samples[6] = ((s_same_sign+s_reverse_sign)>>BITSHIFT_FACTOR2);
; 262  : 
; 263  : 
; 264  : 
; 265  : 
; 266  :     s_reverse_sign = ((Y[3]) * D_4551);
; 267  :     s_reverse_sign -= (Y[1]) * D_8034;//3
; 268  :     s_reverse_sign += (Y[9]) * D_1598;//4
; 269  :     s_reverse_sign += ((Y[11]) * D_6811);

	mov         r1, #0x7D, 26
	sub         r3, r8, r11
	orr         r1, r1, #0x22

	
	mla         r2, lr, r10, r2
	add         r7, r3, r4, lsl #13

	;r1 gets used here
	mul         r4, lr, r1
	
	mov         r1, #0x6A, 26
	mov         lr, r6, asr #13
	sub         r3, r7, r2
	orr         r1, r1, #0x1B
	ldr         r6, [sp, #0x20]
	mov         r3, r3, asr #13
	mov         r0, r5, asr #13
	mul         r1, r9, r1
	ldr         r5, [sp, #8]
	stmia       r6, {r0, r3}
	;r0 and r3 free
	mov         r0, #0x47, 26
	add         r2, r7, r2
	sub         r1, r1, r4
	orr         r0, r0, #7
	mov         r2, r2, asr #13
	mla         r1, r5, r0, r1
	add         r12, r6, #0x18
	stmia       r12, {r2, lr}

; 270  : 
; 271  :     s_same_sign -= (s_same_sign_next2<<1);
; 272  :     subband_samples[2] = ((s_same_sign+s_reverse_sign)>>BITSHIFT_FACTOR2);
; 273  :     subband_samples[5] = ((s_same_sign-s_reverse_sign)>>BITSHIFT_FACTOR2);
; 274  : 
; 275  : 
; 276  : 
; 277  :     s_reverse_sign = -(Y[1] * D_4551);
; 278  :     s_reverse_sign += (Y[3]) * D_1598;
; 279  :     s_same_sign = Y[4] + Y[0];     
; 280  :     s_reverse_sign += (Y[9]) * D_6811;
; 281  :     s_reverse_sign -= (Y[11]) * D_8034;

	mov         r3, #0x7D, 26
	orr         r3, r3, #0x22
	ldr         lr, [sp, #4]
	mul         r2, r9, r3

	;free list: r9
	mov         r3, #0x6A, 26
	orr         r3, r3, #0x1B
	mul         r3, lr, r3
	mla         r1, lr, r10, r1
	sub         r2, r3, r2
	ldr         lr, [sp, #0xC]
	mul         r0, lr, r0
	sub         r3, r7, r8, lsl #1
	add         lr, r3, r1
	sub         r4, r3, r1
	sub         r3, r2, r0
	mla         r1, r5, r10, r3

; 282  : 
; 283  :     s_same_sign -= s_same_sign_next1;
; 284  :     subband_samples[3] = ((s_same_sign+s_reverse_sign)>>BITSHIFT_FACTOR2);
; 285  :     subband_samples[4] = ((s_same_sign-s_reverse_sign)>>BITSHIFT_FACTOR2);

	mov         r0, r4, asr #13
	ldr         r2, [sp, #0x14]
	ldr         r3, [sp]
	sub         r3, r11, r3
	add         r3, r3, r2, lsl #13
	add         r2, r3, r1
	sub         r3, r3, r1
	mov         r3, r3, asr #13
	mov         r2, r2, asr #13
	mov         r1, lr, asr #13
	str         r0, [r6, #0x14]
	add         r12, r6, #8
	stmia       r12, {r1 - r3}

; 286  : 
; 287  : 
; 288  : 
; 289  : 
; 290  : 
; 291  : 
; 292  : }

	add         sp, sp, #0x24
	ldmia       sp!, {r4 - r11, lr}
	bx          lr
	DCD         |indexes|	
	ENTRY_END ?process@@YAXPBFPAHAAHPAF@Z


	END
