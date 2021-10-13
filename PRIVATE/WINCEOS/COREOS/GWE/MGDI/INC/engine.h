//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*


*/

#ifndef __ENGINE_H__
#define __ENGINE_H__

/******************************Class***************************************\
* EPOINTFIX
*
* This is an "energized" version of the common POINTFIX.
*
* History:
*
*  Tue 06-Nov-1990 -by- Paul Butzi [paulb]
* Shamelessly stole code from POINTL class
*
\**************************************************************************/

class EPOINTFIX : public _POINTFIX   /* eptfx */
{
public:
// Constructor -- This one is to allow EPOINTFIX inside other classes.

    EPOINTFIX()                      {}

// Constructor -- Fills the EPOINTFIX. If want to produce EPOINTFIX from
// LONG's x1 and y1, those ought to be converted to FIX using
// LTOFX macro before being passed to this constructor

    EPOINTFIX(FIX x1,FIX y1)
    {
        x = x1;
        y = y1;
    }

// Constructor -- Fills the EPOINTFIX.

    EPOINTFIX(POINTFIX& ptfx)
    {
        x = ptfx.x;
        y = ptfx.y;
    }

// Destructor -- Does nothing.

   ~EPOINTFIX()                      {}

// Operator= -- Create from a POINTL

    VOID operator=(POINTL ptl)
    {
        x = LTOFX(ptl.x);
        y = LTOFX(ptl.y);
    }

// Operator= -- Create from a POINTFIX

    VOID operator=(POINTFIX ptfx)
    {
        x = ptfx.x;
        y = ptfx.y;
    }

// Operator+= -- Add to another POINTFIX.

    VOID operator+=(POINTFIX& ptfx)
    {
        x += ptfx.x;
        y += ptfx.y;
    }

// Operator += -- Add in a POINTL

        VOID operator+=(POINTL& ptl)
        {
                x += LTOFX(ptl.x);
                y += LTOFX(ptl.y);
        }

// Operator-= -- subtract another POINTFIX.

    VOID operator-=(POINTFIX& ptfx)
    {
        x -= ptfx.x;
        y -= ptfx.y;
    }
};

/******************************Class***************************************\
* ERECTFX
*
* Parallel of ERECTL but for POINTFIX coordinates
*
* History:
*  Thu 25-Oct-1990 13:10:00 -by- Paul Butzi [paulb]
* Shamelessly stole Chuck Whitmer's code from ERECTL.HXX.
*
*  Thu 25-Oct-1990 13:40:00 -by- Paul Butzi [paulb]
* Added vInclude to make bounds erectfx computation simple
\**************************************************************************/

class ERECTFX : public _RECTFX
{
public:

// Constructor -- Allows ERECTFXs inside other classes.

    ERECTFX()                        {}

// Constructor -- Initialize the rectangle.

    ERECTFX(FIX x1,FIX y1,FIX x2,FIX y2)
    {
        xLeft   = x1;
        yTop    = y1;
        xRight  = x2;
        yBottom = y2;
    }

// Destructor -- Does nothing.

   ~ERECTFX()                        {}

// bEmpty -- Check if the RECTFX is empty.

    BOOL bEmpty()
    {
        return((xLeft == xRight) || (yTop == yBottom));
    }


// Operator+= -- Offset the RECTFX.

    VOID operator+=(POINTFIX& ptfx)
    {
        xLeft   += ptfx.x;
        xRight  += ptfx.x;
        yTop    += ptfx.y;
        yBottom += ptfx.y;
    }

// Operator+= -- Offset the RECTFX by a POINTL

        VOID operator+=(POINTL& ptl)
        {
                xLeft   += LTOFX(ptl.x);
                xRight  += LTOFX(ptl.x);
                yTop    += LTOFX(ptl.y);
                yBottom += LTOFX(ptl.y);
        }

// Operator-= -- Offset the RECTFX by a POINTL

        VOID operator-=(POINTL& ptl)
        {
                xLeft   -= LTOFX(ptl.x);
                xRight  -= LTOFX(ptl.x);
                yTop    -= LTOFX(ptl.y);
                yBottom -= LTOFX(ptl.y);
        }

// Operator+= -- Add another RECTFX.  Only the destination may be empty.

    VOID operator+=(RECTFX& rcl)
    {
        if (xLeft == xRight || yTop == yBottom)
                        *(RECTFX *)this = rcl;
        else
        {
            if (rcl.xLeft < xLeft)
                xLeft = rcl.xLeft;
            if (rcl.yTop < yTop)
                yTop = rcl.yTop;
            if (rcl.xRight > xRight)
                xRight = rcl.xRight;
            if (rcl.yBottom > yBottom)
                yBottom = rcl.yBottom;
        }
    }

// vOrder -- Make the rectangle well ordered.

    VOID vOrder()
    {
        FIX l;
        if (xLeft > xRight)
        {
            l       = xLeft;
            xLeft   = xRight;
            xRight  = l;
        }
        if (yTop > yBottom)
        {
            l       = yTop;
            yTop    = yBottom;
            yBottom = l;
        }
    }


// vInclude -- stretch the rectangle to include a given point

        VOID vInclude(POINTFIX& ptfx)
        {
                if ( xLeft > ptfx.x )
                        xLeft = ptfx.x;
                else if ( xRight < ptfx.x)
                        xRight = ptfx.x;

                if ( yBottom < ptfx.y )
                        yBottom = ptfx.y;
                else if ( yTop > ptfx.y )
                        yTop = ptfx.y;
        }
};

/******************************Class***************************************\
* EPOINTL.                                                                 *
*                                                                          *
* This is an "energized" version of the common POINTL.                     *
*                                                                          *
* History:                                                                 *
*
*  Fri 05-Oct-1990 -by- Bodin Dresevic [BodinD]
* update: added one more constructor and -=  overloaded operators
*
*  Thu 13-Sep-1990 15:11:42 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

class EPOINTL : public _POINTL     /* eptl */
{
public:
// Constructor -- This one is to allow EPOINTLs inside other classes.

    EPOINTL()                       {}

// Constructor -- Fills the EPOINTL.

    EPOINTL(LONG x1,LONG y1)
    {
        x = x1;
        y = y1;
    }

// Constructor -- Fills the EPOINTL.

    EPOINTL(POINTL& ptl)
    {
        x = ptl.x;
        y = ptl.y;
    }

// Constructor -- Fills the EPOINTL.

    EPOINTL(POINTFIX& ptfx)
    {
        x = FXTOL(ptfx.x);
        y = FXTOL(ptfx.y);
    }

// Destructor -- Does nothing.

   ~EPOINTL()                       {}

// Initializer -- Initialize the point.

    VOID vInit(LONG x1,LONG y1)
    {
    x = x1;
    y = y1;
    }

// Operator+= -- Add to another POINTL.

    VOID operator+=(POINTL& ptl)
    {
        x += ptl.x;
        y += ptl.y;
    }

// Operator-= -- Add to another POINTL.

    VOID operator-=(POINTL& ptl)
    {
        x -= ptl.x;
        y -= ptl.y;
    }

// operator== -- checks that the two coordinates are equal

    BOOL operator==(POINTL& ptl)
    {
        return(x == ptl.x && y == ptl.y);
    }
};

/******************************Class***************************************\
* ERECTL
*
* This is an "energized" version of the common RECTL.
*
* History:
*
*  Fri 05-Oct-1990 -by- Bodin Dresevic [BodinD]
* update: added one more constructor and -=  overloaded operators
*
*  Thu 13-Sep-1990 15:11:42 -by- Charles Whitmer [chuckwh]
* Wrote it.
\**************************************************************************/

class ERECTL : public _RECTL
{
public:
// Constructor -- Allows ERECTLs inside other classes.

    ERECTL()                        {}

// Constructor -- Initialize the rectangle.

    ERECTL(LONG x1,LONG y1,LONG x2,LONG y2)
    {
        left   = x1;
        top    = y1;
        right  = x2;
        bottom = y2;
    }

// Constructor -- energize given rectangle so that it can be manipulated

    ERECTL(RECTL& rcl)
    {
        left   = rcl.left   ;
        top    = rcl.top    ;
        right  = rcl.right  ;
        bottom = rcl.bottom ;
    }

// Constructor -- Create given a RECTFX

    ERECTL(RECTFX& rcfx)
    {
        left   = FXTOLFLOOR(rcfx.xLeft);
        top    = FXTOLFLOOR(rcfx.yTop);
        right  = FXTOLCEILING(rcfx.xRight);
        bottom = FXTOLCEILING(rcfx.yBottom);
    }

// Destructor -- Does nothing.

   ~ERECTL()                        {}

// Initializer -- Initialize the rectangle.

    VOID vInit(LONG x1,LONG y1,LONG x2,LONG y2)
    {
    left   = x1;
    top    = y1;
    right  = x2;
    bottom = y2;
    }

// Initializer -- energize given rectangle so that it can be manipulated

    VOID vInit(const RECTL& rcl)
    {
    left    = rcl.left   ;
    top = rcl.top    ;
    right   = rcl.right  ;
    bottom  = rcl.bottom ;
    }

// Initializer -- Initialize the rectangle to a point.

    VOID vInit(POINTL& ptl)
    {
    left = right = ptl.x;
    top = bottom = ptl.y;
    }

// bEmpty -- Check if the RECTL is empty.

    BOOL bEmpty()
    {
        return((left == right) || (top == bottom));
    }

// bWrapped -- Check if the RECTL is empty or not well-ordered

    BOOL bWrapped()
    {
        return((left >= right) || (top >= bottom));
    }

// Operator+= -- Offset the RECTL.

    VOID operator+=(POINTL& ptl)
    {
        left   += ptl.x;
        right  += ptl.x;
        top    += ptl.y;
        bottom += ptl.y;
    }

// Operator-= -- Offset the RECTL.

    VOID operator-=(POINTL& ptl)
    {
        left   -= ptl.x;
        right  -= ptl.x;
        top    -= ptl.y;
        bottom -= ptl.y;
    }

// Operator+= -- Add another RECTL.  Only the destination may be empty.

    VOID operator+=(RECTL& rcl)
    {
        if (left == right || top == bottom)
            *((RECTL *) this) = rcl;
        else
        {
            if (rcl.left < left)
                left = rcl.left;
            if (rcl.top < top)
                top = rcl.top;
            if (rcl.right > right)
                right = rcl.right;
            if (rcl.bottom > bottom)
                bottom = rcl.bottom;
        }
    }

// Operator |= -- Accumulate bounds

    VOID operator|=(RECTL& rcl)
    {
        if (rcl.left < left)
            left = rcl.left;
        if (rcl.top < top)
            top = rcl.top;
        if (rcl.right > right)
            right = rcl.right;
        if (rcl.bottom > bottom)
            bottom = rcl.bottom;
    }

// Operator*= -- Intersect another RECTL.  The result may be empty.

    ERECTL& operator*= (RECTL& rcl)
    {
        {
            if (rcl.left > left)
                left = rcl.left;
            if (rcl.top > top)
                top = rcl.top;
            if (rcl.right < right)
                right = rcl.right;
            if (rcl.bottom < bottom)
                bottom = rcl.bottom;

            if (right < left)
                left = right;          // Only need to collapse one pair
            else
                if (bottom < top)
                    top = bottom;
        }
        return(*this);
    }

// vOrder -- Make the rectangle well ordered.

    VOID vOrder()
    {
        LONG l;
        if (left > right)
        {
            l      = left;
            left   = right;
            right  = l;
        }
        if (top > bottom)
        {
            l      = top;
            top    = bottom;
            bottom = l;
        }
    }

// bContain -- Test if it contains another rectangle

    BOOL bContain(RECTL& rcl)
    {
        return ((left   <= rcl.left)    &&
                (right  >= rcl.right)   &&
                (top    <= rcl.top)     &&
                (bottom >= rcl.bottom));

    }

// bEqual -- Test if two ERECTLs are equal to each other

    BOOL bEqual(ERECTL &ercl)
    {
        return ((left   == ercl.left)    &&
                (right  == ercl.right)   &&
                (top    == ercl.top)     &&
                (bottom == ercl.bottom));
    }

// bNoIntersect -- Return TRUE if the rectangles have no intersection.
//         Both rectangles must not be empty.

    BOOL bNoIntersect(ERECTL& ercl)
    {
        ASSERT(!bEmpty() && !ercl.bEmpty());

        return (( left   > ercl.right  ) ||
                ( right  < ercl.left   ) ||
                ( top    > ercl.bottom ) ||
                ( bottom < ercl.top    ));
    }
};

typedef ERECTL *PERECTL;

#endif  // !__ENGINE_H__
