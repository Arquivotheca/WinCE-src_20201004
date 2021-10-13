//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once

//
//
//   |      Test Data      | Description
//   +---------------------+----------------------------------------------------------------
//   | D3DMFVFTEST_ORNT01  | Floating point primitives (position 1) with 1D tex coords
//   | D3DMFVFTEST_ORNT02  | Floating point primitives (position 2) with 1D tex coords
//   | D3DMFVFTEST_ORNT03  | Floating point primitives (position 3) with 1D tex coords
//   | D3DMFVFTEST_ORNT04  | Floating point primitives (position 4) with 1D tex coords
//   | D3DMFVFTEST_ORNT05  | Floating point primitives (position 5) with 1D tex coords
//   | D3DMFVFTEST_ORNT06  | Floating point primitives (position 1) with 2D tex coords
//   | D3DMFVFTEST_ORNT07  | Floating point primitives (position 2) with 2D tex coords
//   | D3DMFVFTEST_ORNT08  | Floating point primitives (position 3) with 2D tex coords
//   | D3DMFVFTEST_ORNT09  | Floating point primitives (position 4) with 2D tex coords
//   | D3DMFVFTEST_ORNT10  | Floating point primitives (position 5) with 2D tex coords
//

//
//             POSITION 1
//                                     
//                  (2)                
//                                     
//                 *  *                
//               ***  ***              
//             *****  *****            
//           *******  *******          
//         *********  *********        
//       ***********  ***********      
//  (1) ************  ************ (4) 
//       ***********  ***********      
//         *********  *********        
//           *******  *******          
//             *****  *****            
//               ***  ***              
//                 *  *                
//                                     
//                  (3)                
//                                     



#define POS1X1    0.0f
#define POS1Y1    0.5f 
#define POS1Z1    0.0f

#define POS1X2    0.5f 
#define POS1Y2    0.0f
#define POS1Z2    0.0f

#define POS1X3    0.5f 
#define POS1Y3    1.0f 
#define POS1Z3    0.0f

#define POS1X4    1.0f 
#define POS1Y4    0.5f 
#define POS1Z4    0.0f



//
//         POSITION 2
//                                     
//             (2)                
//                                     
//            *                        
//           **  **                     
//          ***  ****                   
//          ***  ******                 
//         ****  ********               
//         ****  **********             
//        *****  ************           
//        *****  **************         
//       ******  ****************       
//       ******  *****************  (4)
//  (1) *******  ************           
//        *****  ******                 
//           **                         
//            (3)                
//                     


#define POS2X1    0.0f
#define POS2Y1    0.75f
#define POS2Z1    0.0f

#define POS2X2    0.25f
#define POS2Y2    0.0f
#define POS2Z2    0.0f

#define POS2X3    0.25f
#define POS2Y3    1.0f 
#define POS2Z3    0.0f

#define POS2X4    1.0f 
#define POS2Y4    0.75f
#define POS2Z4    0.0f



//
//         POSITION 3
//                                     
//                             
//                    (2)             
//                       **            
//               ******  *****       
//         ************  ******* (4)
// (1)*****************  ******     
//     ****************  ******      
//       **************  *****      
//         ************  *****      
//           **********  ****      
//             ********  ****         
//               ******  ***         
//                 ****  ***         
//                   **  **             
//                       *                 
//                                 
//                      (3)            
//                               
//                                     



#define POS3X1    0.0f
#define POS3Y1    0.25f
#define POS3Z1    0.0f

#define POS3X2    0.75f
#define POS3Y2    0.0f
#define POS3Z2    0.0f

#define POS3X3    0.75f
#define POS3Y3    1.0f 
#define POS3Z3    0.0f

#define POS3X4    1.0f 
#define POS3Y4    0.25f
#define POS3Z4    0.0f



//
//         POSITION 4
//                                     
//                             
//                    (2) 
//                                     
//                       *                 
//                   **  **             
//                 ****  ***         
//               ******  ***         
//             ********  ****         
//           **********  ****      
//         ************  *****      
//       **************  *****      
//     ****************  ******      
// (1)*****************  ******     
//         ************  ******* (4)
//               ******  *****       
//                       **            
//                      (3)            
//                               
//                                     

#define POS4X1    0.0f
#define POS4Y1    0.75f
#define POS4Z1    0.0f
				
#define POS4X2    0.75f
#define POS4Y2    0.0f
#define POS4Z2    0.0f
				
#define POS4X3    0.75f
#define POS4Y3    1.0f 
#define POS4Z3    0.0f
				
#define POS4X4    1.0f 
#define POS4Y4    0.75f
#define POS4Z4    0.0f


//
//         POSITION 5
//                                     
//             (2)                
//                                     
//           **                         
//        *****  ******                 
//  (1) *******  ************           
//       ******  ***************** (4) 
//       ******  ****************       
//        *****  **************         
//        *****  ************           
//         ****  **********             
//         ****  ********               
//          ***  ******                 
//          ***  ****                   
//           **  **                     
//            *                        
//                                     
//            (3)                
//                                     

#define POS5X1    0.0f
#define POS5Y1    0.25f
#define POS5Z1    0.0f

#define POS5X2    0.25f
#define POS5Y2    0.0f
#define POS5Z2    0.0f

#define POS5X3    0.25f
#define POS5Y3    1.0f 
#define POS5Z3    0.0f

#define POS5X4    1.0f 
#define POS5Y4    0.25f
#define POS5Z4    0.0f



//
// Primitive Orientation - Case 01
//
#define D3DMFVFTEST_ORNT01_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE1(0))

typedef struct _D3DMFVFTEST_ORNT01 {
	float x, y, z, rhw;
	float u;
} D3DMFVFTEST_ORNT01;

static D3DMFVFTEST_ORNT01 FVFOrnt01[D3DQA_NUMVERTS] = {
//  |       |       |       |       | TEX SET #1|
//  |   X   |   Y   |   Z   |  RHW  |     u     |
//  +-------+-------+-------+-------+-----------+
    { POS1X1, POS1Y1, POS1Z1,   1.0f,       0.0f},
    { POS1X2, POS1Y2, POS1Z2,   1.0f,       1.0f},
    { POS1X3, POS1Y3, POS1Z3,   1.0f,       0.0f},
    { POS1X4, POS1Y4, POS1Z4,   1.0f,       1.0f}
};


//
// Primitive Orientation - Case 02
//
#define D3DMFVFTEST_ORNT02_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE1(0))

typedef struct _D3DMFVFTEST_ORNT02 {
	float x, y, z, rhw;
	float u;
} D3DMFVFTEST_ORNT02;

static D3DMFVFTEST_ORNT02 FVFOrnt02[D3DQA_NUMVERTS] = {
//  |       |       |       |       |TEX SET #1 |
//  |   X   |   Y   |   Z   |  RHW  |     u     |
//  +-------+-------+-------+-------+-----------+
    { POS2X1, POS2Y1, POS2Z1,   1.0f,       0.0f},
    { POS2X2, POS2Y2, POS2Z2,   1.0f,       1.0f},
    { POS2X3, POS2Y3, POS2Z3,   1.0f,       0.0f},
    { POS2X4, POS2Y4, POS2Z4,   1.0f,       1.0f}
};


//
// Primitive Orientation - Case 03
//
#define D3DMFVFTEST_ORNT03_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE1(0))

typedef struct _D3DMFVFTEST_ORNT03 {
	float x, y, z, rhw;
	float u;
} D3DMFVFTEST_ORNT03;

static D3DMFVFTEST_ORNT03 FVFOrnt03[D3DQA_NUMVERTS] = {
//  |       |       |       |       |TEX SET #1 |
//  |   X   |   Y   |   Z   |  RHW  |     u     |
//  +-------+-------+-------+-------+-----------+
    { POS3X1, POS3Y1, POS3Z1,   1.0f,       0.0f},
    { POS3X2, POS3Y2, POS3Z2,   1.0f,       1.0f},
    { POS3X3, POS3Y3, POS3Z3,   1.0f,       0.0f},
    { POS3X4, POS3Y4, POS3Z4,   1.0f,       1.0f}
};


//
// Primitive Orientation - Case 04
//
#define D3DMFVFTEST_ORNT04_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE1(0))

typedef struct _D3DMFVFTEST_ORNT04 {
	float x, y, z, rhw;
	float u;
} D3DMFVFTEST_ORNT04;

static D3DMFVFTEST_ORNT04 FVFOrnt04[D3DQA_NUMVERTS] = {
//  |       |       |       |       |TEX SET #1 |
//  |   X   |   Y   |   Z   |  RHW  |     u     |
//  +-------+-------+-------+-------+-----------+
    { POS4X1, POS4Y1, POS4Z1,   1.0f,       0.0f},
    { POS4X2, POS4Y2, POS4Z2,   1.0f,       1.0f},
    { POS4X3, POS4Y3, POS4Z3,   1.0f,       0.0f},
    { POS4X4, POS4Y4, POS4Z4,   1.0f,       1.0f}
};


//
// Primitive Orientation - Case 05
//
#define D3DMFVFTEST_ORNT05_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE1(0))

typedef struct _D3DMFVFTEST_ORNT05 {
	float x, y, z, rhw;
	float u;
} D3DMFVFTEST_ORNT05;

static D3DMFVFTEST_ORNT05 FVFOrnt05[D3DQA_NUMVERTS] = {
//  |       |       |       |       |TEX SET #1 |
//  |   X   |   Y   |   Z   |  RHW  |     u     |
//  +-------+-------+-------+-------+-----------+
    { POS5X1, POS5Y1, POS5Z1,   1.0f,       0.0f},
    { POS5X2, POS5Y2, POS5Z2,   1.0f,       1.0f},
    { POS5X3, POS5Y3, POS5Z3,   1.0f,       0.0f},
    { POS5X4, POS5Y4, POS5Z4,   1.0f,       1.0f}
};






//
// Primitive Orientation - Case 06
//
#define D3DMFVFTEST_ORNT06_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE2(0))

typedef struct _D3DMFVFTEST_ORNT06 {
	float x, y, z, rhw;
	float u,v;
} D3DMFVFTEST_ORNT06;

static D3DMFVFTEST_ORNT06 FVFOrnt06[D3DQA_NUMVERTS] = {
//  |       |       |       |       |       TEX SET #1      |
//  |   X   |   Y   |   Z   |  RHW  |     u     |     v     |
//  +-------+-------+-------+-------+-----------+-----------+
    { POS1X1, POS1Y1, POS1Z1,   1.0f,       0.0f,       0.0f},
    { POS1X2, POS1Y2, POS1Z2,   1.0f,       1.0f,       0.0f},
    { POS1X3, POS1Y3, POS1Z3,   1.0f,       0.0f,       1.0f},
    { POS1X4, POS1Y4, POS1Z4,   1.0f,       1.0f,       1.0f}
};


//
// Primitive Orientation - Case 07
//
#define D3DMFVFTEST_ORNT07_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE2(0))

typedef struct _D3DMFVFTEST_ORNT07 {
	float x, y, z, rhw;
	float u,v;
} D3DMFVFTEST_ORNT07;

static D3DMFVFTEST_ORNT07 FVFOrnt07[D3DQA_NUMVERTS] = {
//  |       |       |       |       |       TEX SET #1      |
//  |   X   |   Y   |   Z   |  RHW  |     u     |     v     |
//  +-------+-------+-------+-------+-----------+-----------+
    { POS2X1, POS2Y1, POS2Z1,   1.0f,       0.0f,       0.0f},
    { POS2X2, POS2Y2, POS2Z2,   1.0f,       1.0f,       0.0f},
    { POS2X3, POS2Y3, POS2Z3,   1.0f,       0.0f,       1.0f},
    { POS2X4, POS2Y4, POS2Z4,   1.0f,       1.0f,       1.0f}
};


//
// Primitive Orientation - Case 08
//
#define D3DMFVFTEST_ORNT08_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE2(0))

typedef struct _D3DMFVFTEST_ORNT08 {
	float x, y, z, rhw;
	float u,v;
} D3DMFVFTEST_ORNT08;

static D3DMFVFTEST_ORNT08 FVFOrnt08[D3DQA_NUMVERTS] = {
//  |       |       |       |       |       TEX SET #1      |
//  |   X   |   Y   |   Z   |  RHW  |     u     |     v     |
//  +-------+-------+-------+-------+-----------+-----------+
    { POS3X1, POS3Y1, POS3Z1,   1.0f,       0.0f,       0.0f},
    { POS3X2, POS3Y2, POS3Z2,   1.0f,       1.0f,       0.0f},
    { POS3X3, POS3Y3, POS3Z3,   1.0f,       0.0f,       1.0f},
    { POS3X4, POS3Y4, POS3Z4,   1.0f,       1.0f,       1.0f}
};


//
// Primitive Orientation - Case 09
//
#define D3DMFVFTEST_ORNT09_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE2(0))

typedef struct _D3DMFVFTEST_ORNT09 {
	float x, y, z, rhw;
	float u,v;
} D3DMFVFTEST_ORNT09;

static D3DMFVFTEST_ORNT09 FVFOrnt09[D3DQA_NUMVERTS] = {
//  |       |       |       |       |       TEX SET #1      |
//  |   X   |   Y   |   Z   |  RHW  |     u     |     v     |
//  +-------+-------+-------+-------+-----------+-----------+
    { POS4X1, POS4Y1, POS4Z1,   1.0f,       0.0f,       0.0f},
    { POS4X2, POS4Y2, POS4Z2,   1.0f,       1.0f,       0.0f},
    { POS4X3, POS4Y3, POS4Z3,   1.0f,       0.0f,       1.0f},
    { POS4X4, POS4Y4, POS4Z4,   1.0f,       1.0f,       1.0f}
};


//
// Primitive Orientation - Case 10
//
#define D3DMFVFTEST_ORNT10_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE2(0))

typedef struct _D3DMFVFTEST_ORNT10 {
	float x, y, z, rhw;
	float u,v;
} D3DMFVFTEST_ORNT10;

static D3DMFVFTEST_ORNT10 FVFOrnt10[D3DQA_NUMVERTS] = {
//  |       |       |       |       |       TEX SET #1      |
//  |   X   |   Y   |   Z   |  RHW  |     u     |     v     |
//  +-------+-------+-------+-------+-----------+-----------+
    { POS5X1, POS5Y1, POS5Z1,   1.0f,       0.0f,       0.0f},
    { POS5X2, POS5Y2, POS5Z2,   1.0f,       1.0f,       0.0f},
    { POS5X3, POS5Y3, POS5Z3,   1.0f,       0.0f,       1.0f},
    { POS5X4, POS5Y4, POS5Z4,   1.0f,       1.0f,       1.0f}
};

