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
/*


*/
/*********************************Class************************************\
*
* class PATHWIDE
*
\**************************************************************************/

#include <pathobj.h>

class PATHWIDE : public XPATHOBJ 
{
public:
    
    PATHWIDE(XPATHOBJ *pPathToWiden) { ppoSpine = pPathToWiden; }
    
    BOOL        bWiden(UINT32 cWidth);

private:
    XPATHOBJ   *ppoSpine;       // path to be widened
    FIX         fxPenRadius;    // Half the pen width, in 28.4
    
    // The three relevant points on the spine
    PPOINTFIX   pptfxPrev;
    PPOINTFIX   pptfxCur;
    PPOINTFIX   pptfxNext;
    PATHRECORD *pprNext;        // ptr to path record containing pptfxNext
    UINT32      iNext;          // Index in pathrec of pptfxNext
    
    // To walk forwards or backwards along the spine
    BOOL        bStepFwd();
    BOOL        bStepBack();
    
    BOOL        bIsLeftTurn(POINTFIX *pa, POINTFIX *pb);     // TRUE if the vectors describe a left turn
    void        vLeftSideJoin();       // Creates the join points on the left side of the current spine pt
    POINTFIX    ptfxPenWidthLeft(POINTFIX);     // returns a vector pointing 90 deg left of the given vector,
                                                //   with length of pen radius.
                                            
    // Rotates the given vector 90 degrees left. Remember, y is down!
    POINTFIX    ptfxRotate90Left(POINTFIX ptfxVec) { POINTFIX ptfxRot = {ptfxVec.y, -ptfxVec.x};
                                                     return ptfxRot; }
    
};