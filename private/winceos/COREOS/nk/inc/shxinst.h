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
// This structure needs to match dmkd\dm.h
//

typedef union _SH3IW {

            //
            // opcode Reg# function
            //  4      4    8 bits
            //

  struct
    {
      USHORT function : 8;        // 8 bit (7-0) function
      USHORT reg      : 4;        // register source/destination
      USHORT opcode   : 4;        // 4 bit (15-12) opcode
    } instr1;

            //
            // opcode Reg# immediate | displacement 
            //  4      4    8 bits
            //

  struct
    {
      USHORT imm_disp : 8;        // immediate data/addressing or displacement
      USHORT reg      : 4;        // register source/destination
      USHORT opcode   : 4;        // 4 bit (15-12) opcode
    } instr2;
  
            //
            // opcode Rn Rm opcode ext | displacement
            //  4     4  4   4 bits
            //

  struct
    {
      USHORT ext_disp : 4;        // 4 bit (3-0) opcode ext or displacement
      USHORT regM     : 4;        // Rm source register
      USHORT regN     : 4;        // Rn destination register
      USHORT opcode   : 4;        // 4 bit (15-12) opcode               
    } instr3;

            //
            // opcode Reg# displacement
            //  8      4    4 bits
            //

  struct
    {
      USHORT disp     : 4;        // 4 bit (3-0) displacement
      USHORT reg      : 4;        // Rn destination register
      USHORT opcode   : 8;        // 8 bit (15-8) opcode
    } instr4;

            //
            // opcode immediate | displacement
            //  8      8 bits
            //

  struct
    {
      USHORT imm_disp  : 8;       // 8 bit (7-0) imm value or displacement
      USHORT opcode    : 8;       // 8 bit (15-8) opcode            
    } instr5;
  
            //
            // opcode
            //  16 bits
            //

  struct 
    { 
      USHORT opcode    : 16;      // 16 bit opcode
    } instr6;

            //
            // opcode displacement
            //  4      12 bits
            //

 struct
    {
      USHORT disp      : 12;      // 12 bit (11-0) displacement
      USHORT opcode    : 4;       // 4 bit  (15-12) opcode
    } instr7;

 struct
   {
     USHORT low        : 8;       // low word  (7-0)
     USHORT high       : 8;       // high word (15-8)
   } data;

 USHORT instruction;

} SH3IW, *PSH3IW;

#define RTS  0xB


