/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1985-1998  Microsoft Corporation.  All Rights Reserved.

Module Name:

     touch.cpp  

Abstract:

   Driver BVTs: Test Touch Driver

***************************************************************************/

#include "global.h"


/***********************************************************************************
***
***   Support Functions
***
************************************************************************************/
int   tNum;
int tClick;
RECT  rects[3][4];
int   filled[4];

int Zleft,Zright,Ztop,Zbottom;
extern	int  zx,zy;

//***********************************************************************************
void initRects(void) {
   RECT rcx;
	SystemParametersInfo(SPI_GETWORKAREA,0,&rcx,0);
	Zleft = rcx.left;
	Zright = rcx.right;
	Ztop = rcx.top;
	Zbottom = rcx.bottom;
	Zright = Zright-Zleft;
    Zleft =0;
	Zbottom = Zbottom- Ztop;
	Ztop=0;

	LONG lLeftColumn;
	LONG lRightColumn;
	LONG lUpperRow;
	LONG lLowerRow;

    	RETAILMSG(1, (TEXT("%d %d %d %d\r\n"), Zleft, Ztop, Zright, Zbottom));

	lLeftColumn = Zright / 4 - 10;
	lRightColumn = Zright * 3 / 4 - 10;
	lUpperRow = (Zbottom - 90) / 4 - 10 + 90;
	lLowerRow = (Zbottom - 90) * 3 / 4 - 10 + 90;

		RECT	temp[2][4] =	{
		{	{Zleft,Ztop,Zleft+20,Ztop+20},
								{Zright-20,Ztop,   Zright,   Ztop+20},
								{Zleft,   Zbottom-20,   Zleft+20,Zbottom},
								{Zright-20,Zbottom-20, Zright,Zbottom}	},
			
				
							{	{lLeftColumn,lUpperRow,lLeftColumn+20,lUpperRow+20},
								{lRightColumn,lUpperRow,lRightColumn+20,lUpperRow+20},
								{lLeftColumn,lLowerRow,lLeftColumn+20,lLowerRow+20},
								{lRightColumn,lLowerRow,lRightColumn+20,lLowerRow+20}}};
   for(int i = 0; i < 4; i++) {
      filled[i] = 0;
		rects[1][i] = temp[0][i];							// corners
		rects[0][i] = rects[2][i] = temp[1][i];		// centered
	}
}

//***********************************************************************************
void fillRect(int x, int y) {
   int   inside = 0;
   
   for(int i = 0; i < 4; i++) 
      if (y >= rects[tNum][i].top && y <= rects[tNum][i].bottom && 
          x >= rects[tNum][i].left && x <= rects[tNum][i].right) {
         filled[i] = 1;
         inside = 1;
         doUpdate(&rects[tNum][i]);
         g_pKato->Log(LOG_DETAIL,TEXT("WM_RBUTTONUP at (%d,%d) inside rect[%d]."),x,y,i);
      }
   if (!inside) 
      g_pKato->Log(LOG_DETAIL,TEXT("WM_RBUTTONUP at (%d,%d) outside of rects."),x,y);
}


//***********************************************************************************
void showRect(HDC hdc) {
   for(int i = 0; i < 4; i++) {
      if (filled[i])
         FillRect(hdc,&rects[tNum][i],(HBRUSH)GetStockObject(BLACK_BRUSH));
      else
         Rectangle(hdc,rects[tNum][i].left,rects[tNum][i].top,rects[tNum][i].right,rects[tNum][i].bottom);
   } 
   UpdateWindow( globalHwnd );
}


//***********************************************************************************
BOOL allFilled(void) {
   int sum = 0;
   for(int i = 0; i < 4; i++) 
      sum += filled[i];
   return (sum == 4);
}






//********************************************************************************************
LRESULT CALLBACK WheelWindProc(HWND hwnd, UINT message,WPARAM wParam, LPARAM lParam) {
   
   PAINTSTRUCT ps;
   static int scrolNum =0;
   static  BOOL midButton =FALSE;
   int move =0;
  HDC hdc;
 

   
   switch(message)
    {
    case WM_PAINT:
      BeginPaint(hwnd, &ps);
         switch(tNum)
             {
                 case 0:
                     initWindow(ps.hdc,TEXT("Driver Tests: Wheel: Mouse Wheel Test"),TEXT("Scroll up 5 times"));
                     break;
                 case 1: 
                     initWindow(ps.hdc,TEXT("Driver Tests: Wheel: Mouse Wheel Test"),TEXT("Scroll Down 5 times"));
                     break;
       
             }
               
      EndPaint(hwnd, &ps);
      return 0;
            
         case WM_INIT:
             tNum=(int)wParam;
             scrolNum=0;
             midButton=FALSE;
             break;

         case WM_MOUSEWHEEL:
             move= (SHORT)HIWORD(wParam);
             hdc = GetDC(hwnd);
             //scrolling up instead of down 
             if(tNum==0 && move<0)
                {
                 scrollBody(hdc, TEXT("Scroll in the other direction \0"));
                 ReleaseDC(hwnd,hdc);
                 break;
                 }
                //check if scrolling down instead of up
             else if (tNum==1 && move>0)
                 {
                 scrollBody(hdc, TEXT("Scroll in the other direction \0"));
                 ReleaseDC(hwnd,hdc);
                 break;
                 }
             else
                 {
                      scrolNum=scrolNum +(move/WHEEL_DELTA);  
                      if ((move%WHEEL_DELTA) ==0)
                          {
                               
                          TCHAR szBuffer[80];
                          wsprintf(szBuffer, TEXT(" scrolling %d                  "),scrolNum);                        
                          scrollBody(hdc,szBuffer);
                           ReleaseDC(hwnd,hdc);
                     
                              }
                         return 0;
                 }
             break;

            case WM_GOAL:
             return (scrolNum>=5 ||scrolNum <=-5 ||midButton) ;
            
             
 }


 return DefWindowProc(hwnd, message, wParam, lParam);
   
}


//********************************************************************************** *
LRESULT CALLBACK TapWindProc(HWND hwnd, UINT message,WPARAM wParam, LPARAM lParam) {
   POINTS      p;
   PAINTSTRUCT ps;
   BOOL flag=FALSE;
   TCHAR button[100];
   
   switch(message) {
   case WM_PAINT:
      if (tClick  ==0)
        wsprintf(button, TEXT("Driver Tests: Touch: Left Mouse Button Click Tests"));
      else if (tClick  ==1)
        wsprintf(button, TEXT("Driver Tests: Touch: Right  Mouse Button Click Tests"));
      else if (tClick  ==2)
        wsprintf(button, TEXT("Driver Tests: Touch: Middle Mouse Button Click Tests"));
        
      BeginPaint(hwnd, &ps);
      switch(tNum)
        {
          case 0:
             initWindow(ps.hdc,button ,TEXT("Click and release inside each of the four rectangles."));
             break;
          case 1:
             initWindow(ps.hdc,button ,TEXT("Click and release inside each of the four rectangles."));
             break;
          case 2:
             initWindow(ps.hdc,button ,TEXT("Double Click inside each of the four rectangles."));
             break;
        }
      showRect(ps.hdc);
      EndPaint(hwnd, &ps);
      return 0;

   case WM_INIT:
      tNum = (int)wParam;
      tClick = (int)lParam;
      if (tClick  ==40 || tClick  ==41 || tClick  ==42 ) 
        tClick=0 ;
      if (tClick  ==45 || tClick  ==46 || tClick  ==47 ) 
        tClick=1 ;
      if (tClick  ==49 || tClick  ==50 || tClick  ==51) 
        tClick =2 ;
      initRects();
      return 0;
       
   case WM_LBUTTONUP:
      if (tClick==0 && tNum!=2) 
      {
        p = (*((POINTS FAR *) & (lParam)));
         fillRect(p.x,p.y);
        }
      return 0;


   case WM_MBUTTONUP:
      if (tClick==2 && tNum!=2) 
        {
         p = (*((POINTS FAR *) & (lParam)));
         fillRect(p.x,p.y);  
        }
      return 0;
    
   case WM_RBUTTONUP:
     if (tNum != 2 && tClick==1) 
        {
         p = (*((POINTS FAR *) & (lParam)));
         fillRect(p.x,p.y);
        }
     return 0;

   case WM_LBUTTONDBLCLK:
      if (tNum == 2 ) {
         p = (*((POINTS FAR *) & (lParam)));
         fillRect(p.x,p.y);
      }
      return 0;

   case WM_GOAL:
      return allFilled();
   }
   return DefWindowProc(hwnd, message, wParam, lParam);
}






//***********************************************************************************
LRESULT CALLBACK DragWindProc(HWND hwnd, UINT message,WPARAM wParam, LPARAM lParam) {
   static int     draw, keyPress, goal;
   BOOL flag = FALSE;
   static POINT   p[2], *pp = p;
   POINTS         tempP;
   PAINTSTRUCT    ps;
  // zx = Zleft-Zright;
  // zy = Zbottom-Ztop;
  g_pKato->Log(LOG_DETAIL, TEXT("Zx and Zy %d,%d"),zx,zy);
   RECT           rect = {0+5,90+5,zx-5,zy-5};           
   HRGN           hRgn;
   HDC            hdc;

   switch (message) {

	case WM_LBUTTONDOWN:    
       case WM_RBUTTONDOWN:
       case WM_MBUTTONDOWN:
        if((message==WM_LBUTTONDOWN && tClick==43) ||(message==WM_RBUTTONDOWN && tClick==48) || (message ==WM_MBUTTONDOWN && tClick==52))
        {
              tempP = (*((POINTS FAR *) & (lParam )));
              hRgn = CreateRectRgnIndirect(&rect);
              if (PtInRegion(hRgn,tempP.x, tempP.y)) 
                {
                  p[0].x = tempP.x;
                  p[0].y = tempP.y;
                  draw = 1;
                  }
               DeleteObject(hRgn);
           }
        	return 0;
      
   	case WM_LBUTTONUP:	     
       case WM_RBUTTONUP:        
       case WM_MBUTTONUP:        
        if((message==WM_LBUTTONUP && tClick==43) ||(message==WM_RBUTTONUP && tClick==48) || (message ==WM_MBUTTONUP && tClick==52))
            {
           draw = 0;
           doUpdate(NULL);
            }
         return 0;

      
	case WM_MOUSEMOVE:
		if (draw) {
         hdc = GetDC(hwnd);
         hRgn = CreateRectRgnIndirect(&rect);
         SelectClipRgn(hdc,hRgn);
         SelectObject(hdc,(HPEN)GetStockObject(BLACK_PEN));
         tempP = (*((POINTS FAR *) & (lParam)));
         p[1].x = tempP.x;
         p[1].y = tempP.y;
		   Polyline(hdc,p,2);
         p[0] = p[1];
         SelectClipRgn(hdc,NULL);
         DeleteObject(hRgn);
         ReleaseDC(hwnd,hdc);
      }
		return 0;
	case WM_PAINT:
		BeginPaint(hwnd,&ps);
      initWindow(ps.hdc,TEXT("Driver Tests: Touch: Drag Test"),TEXT("Click and drag in bottom box. Type <SPACE> to continue."));
		EndPaint(hwnd,&ps);	
      return 0;
   case WM_INIT:
      draw     = 0;
      keyPress = 0;
      goal     = (int)wParam;
      tClick=(int)lParam;
      return 0;
   case WM_KEYDOWN:
      keyPress = (int)wParam;
      return 0;
   case WM_GOAL:
      return keyPress == goal;     
   }
   return DefWindowProc(hwnd, message, wParam, lParam);
}





/***********************************************************************************
***
***   SimpleTapTest
***
************************************************************************************/

//***********************************************************************************
void SimpleTapTest(DWORD test) {
   int done = 0;

   SendMessage(globalHwnd,WM_SWITCHPROC,TAP,0);
     
   do {
      SendMessage(globalHwnd,WM_INIT,0,test);
      doUpdate(NULL);
      if (pump() == 2)
         done = 1;
      else if (askMessage(TEXT("Did all four rectangles turn black?"),0)) 
         done = 1;
      else if (!askMessage(TEXT("Do you want to try again?"),1))
         done = 1;
   } while (!done);
}


/***********************************************************************************
***
***   CornerTapTest
***
************************************************************************************/

//***********************************************************************************
void CornerTapTest(DWORD test) {
   int done = 0;

   SendMessage(globalHwnd,WM_SWITCHPROC,TAP,0);
     
   do {
      SendMessage(globalHwnd,WM_INIT,1,test);
      doUpdate(NULL);
      if (pump() == 2)
         done = 1;
      else if (askMessage(TEXT("Did all four rectangles turn black?"),0)) 
         done = 1;
      else if (!askMessage(TEXT("Do you want to try again?"),1))
         done = 1;
   } while (!done);
}


/***********************************************************************************
***
***   DoubleClickTest
***
************************************************************************************/

//***********************************************************************************
void DoubleClickTest(void ) {
   int done = 0;

   SendMessage(globalHwnd,WM_SWITCHPROC,TAP,0);
     
   do {
      SendMessage(globalHwnd,WM_INIT,2,0);
      doUpdate(NULL);
      if (pump() == 2)
         done = 1;
      else if (askMessage(TEXT("Did all four rectangles turn black?"),0)) 
         done = 1;
      else if (!askMessage(TEXT("Do you want to try again?"),1))
         done = 1;
   } while (!done);
}


/***********************************************************************************
***
***   DragTest
***
************************************************************************************/

//***********************************************************************************
void DragTest(DWORD test) {
   int done = 0;

   SendMessage(globalHwnd,WM_SWITCHPROC,DRAG,0);
   do {
      SendMessage(globalHwnd,WM_INIT,goKey,test);
      doUpdate(NULL);
      if (pump() == 2)
         done = 1;
      else if (askMessage(TEXT("Were you able to draw correctly?"),0)) 
         done = 1;
      else if (!askMessage(TEXT("Do you want to try again?"),1))
         done = 1;
   } while (!done);
}

/***********************************************************************************
***
***   SimpleWheelTest
***
************************************************************************************/

//***********************************************************************************
void WheelTest (void) {
   int done = 0;
   tNum=0;


           SendMessage(globalHwnd,WM_SWITCHPROC,WHEEL,0);
   for (tNum=0;tNum<2;tNum++)
    {
             
           do 
            {
                       SendMessage(globalHwnd,WM_INIT,tNum,0);
                       doUpdate(NULL);
                       if (pump() == 2)
                             done = 1;
                       else if (askMessage(TEXT("Did All the numbers Appear in Sequence"),0) && (tNum==0 ||tNum==1)) 
                             done = 1;
                        else if (!askMessage(TEXT("Do you want to try again?"),1))
                             done = 1;
           } while (!done);
    }
}








/***********************************************************************************
***
***   Tux Entry Points
***
************************************************************************************/
#define CLEN 30
#define ASPEN _T("S1 HITACHI SH4 PLATFORM") 
#define KEYWEST _T("HITACHI SH3 PLATFORM") 
//***********************************************************************************
TESTPROCAPI Tap_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE){
   NO_MESSAGES;
    RECT rcx;
	SystemParametersInfo(SPI_GETWORKAREA,0,&rcx,0);
	 TCHAR PlatformName[CLEN]={0};
	Zleft = rcx.left;
	Zright = rcx.right;
	Ztop = rcx.top;
	Zbottom = rcx.bottom; 
    zx = Zright-Zleft;
	zy = Zbottom - Ztop;
if(GetPlatformName(PlatformName,CLEN))
    {
         if((_tcscmp(PlatformName,ASPEN) && _tcscmp(PlatformName,KEYWEST)))
               SimpleTapTest( lpFTE->dwUniqueID);
         else if(lpFTE->dwUniqueID!=49 ) 
            SimpleTapTest( lpFTE->dwUniqueID);
         else 
             g_pKato->Log( LOG_SKIP, TEXT("Mouse is not supported on %s Platform"), PlatformName);
         
    }

return getCode();

}

//***********************************************************************************
TESTPROCAPI CornerTap_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE){
   NO_MESSAGES;
    RECT rcx;
	SystemParametersInfo(SPI_GETWORKAREA,0,&rcx,0);
	 TCHAR PlatformName[CLEN]={0};
	Zleft = rcx.left;
	Zright = rcx.right;
	Ztop = rcx.top;
	Zbottom = rcx.bottom; 
    zx = Zright-Zleft;
	zy = Zbottom - Ztop;
	BOOL r=ImmDisableIME(0);

if(GetPlatformName(PlatformName,CLEN))
    {
         if((_tcscmp(PlatformName,ASPEN)&& _tcscmp(PlatformName,KEYWEST )))
            CornerTapTest(lpFTE->dwUniqueID);
         else if (lpFTE->dwUniqueID!=50 )
            CornerTapTest(lpFTE->dwUniqueID);            
         else 
           g_pKato->Log( LOG_SKIP, TEXT("Mouse is not supported on %s Platform"), PlatformName);
    }

return getCode();

         
 }


//***********************************************************************************
TESTPROCAPI DoubleClickTest_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE){
   NO_MESSAGES;
    RECT rcx;
	SystemParametersInfo(SPI_GETWORKAREA,0,&rcx,0);
	Zleft = rcx.left;
	Zright = rcx.right;
	Ztop = rcx.top;
	Zbottom = rcx.bottom; 
    zx = Zright-Zleft;
	zy = Zbottom - Ztop;

   DoubleClickTest();
   return getCode();
}


//***********************************************************************************
TESTPROCAPI Drag_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE){
   NO_MESSAGES;
    RECT rcx;
	SystemParametersInfo(SPI_GETWORKAREA,0,&rcx,0);
 TCHAR PlatformName[CLEN]={0};
 Zleft = rcx.left;
	Zright = rcx.right;
	Ztop = rcx.top;
	Zbottom = rcx.bottom; 
    zx = Zright-Zleft;
	zy = Zbottom - Ztop;

if(GetPlatformName(PlatformName,CLEN))
    {
        if((_tcscmp(PlatformName,ASPEN ) && _tcscmp(PlatformName,KEYWEST)))
           DragTest(lpFTE->dwUniqueID);
        else if (lpFTE->dwUniqueID!=52 )
            DragTest(lpFTE->dwUniqueID);
        
   else 
           g_pKato->Log( LOG_SKIP, TEXT("Mouse is not supported on %s Platform"), PlatformName);
    }
return getCode();

}




//*************************************************************************************
TESTPROCAPI WheelTest_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE){
   NO_MESSAGES;
    RECT rcx;
    SystemParametersInfo(SPI_GETWORKAREA,0,&rcx,0);

  TCHAR PlatformName[CLEN]={0};  
  Zleft = rcx.left;
    Zright = rcx.right;
    Ztop = rcx.top;
    Zbottom = rcx.bottom; 
    zx = Zright-Zleft;
    zy = Zbottom - Ztop;


    
   if(GetPlatformName(PlatformName,CLEN))
    {
        if(_tcscmp(PlatformName,ASPEN) && _tcscmp(PlatformName,KEYWEST))
          WheelTest();
        else 
           g_pKato->Log( LOG_SKIP, TEXT("Mouse is not supported on %s Platform"), PlatformName);
       }

   return getCode();

   
}




