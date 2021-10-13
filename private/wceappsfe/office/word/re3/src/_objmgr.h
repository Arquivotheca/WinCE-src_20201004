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
 *  @doc    INTERNAL
 *
 *  @module _objmgr.h   Class declaration for the object manager class |
 *
 *  Author: alexgo 11/4/95
 *
 */
#ifndef __OBJMGR_H__
#define __OBJMGR_H__

#include "_coleobj.h"
#include "_array.h"
#include "_m_undo.h"

class CTxtEdit;

/*
 *	@enum	return values for HandleClick
 */	
enum ClickStatus
{
	CLICK_IGNORED			= 0,
	CLICK_OBJDEACTIVATED	= 1,
	CLICK_SHOULDDRAG		= 2,
	CLICK_OBJSELECTED		= 3
};

typedef CArray<COleObject *> ObjectArray;

/*
 *	CObjectMgr
 *
 *	@class	keeps track of a collection of OLE embedded objects as well as
 *			various state tidbits
 */
class CObjectMgr
{
public:
	LONG			GetObjectCount(); 			//@cmember count # of objects 
	LONG			GetLinkCount();				//@cmember count # of links
	COleObject *	GetObjectFromCp(DWORD cp);	//@cmember fetch object ptr
	COleObject *	GetObjectFromIndex(DWORD index); //@cmember fetch obj ptr
												//@cmember insert object
	HRESULT			InsertObject(DWORD cp, REOBJECT *preobj, 
						IUndoBuilder *publdr);
												//@cmember re-inserts the given
												// object
	HRESULT			RestoreObject(COleObject *pobj);

	IRichEditOleCallback *GetRECallback()		//@cmember return the callback
					{return _precall;}
												//@cmember set the OLE callback
	void			SetRECallback(IRichEditOleCallback *precall);			
												//@cmember sets a temporary flag
												// indicating whether or not
												// a UI update is pending.
	void			SetShowUIPending(BOOL fPending)
												{_fShowUIPending = fPending;}

	BOOL			GetShowUIPending()			//@cmember get _fShowUIPending
												{return _fShowUIPending;}
										   		//@cmember sets the inplace
												// active object
	void			SetInPlaceActiveObject(COleObject *pobj)
												{ _pobjactive = pobj; }
	COleObject *	GetInPlaceActiveObject()	//@cmember get the active obj
												{ return _pobjactive; }
	BOOL			GetHelpMode()				//@cmember in help mode?
												{ return _fInHelpMode; }
	void			SetHelpMode(BOOL fHelp)		//@cmember set the help mode
												{ _fInHelpMode = fHelp; }
												//@cmember Set the host names
	HRESULT			SetHostNames(LPWSTR pszApp, LPWSTR pszDoc);
	LPWSTR			GetAppName()				//@cmember get the app name
												{ return _pszApp; }
	LPWSTR			GetDocName()				//@cmember get the doc name
												{ return _pszDoc; }
												//@cmember activate an object
												//if appropriate
	BOOL			HandleDoubleClick(CTxtEdit *ped, const POINT &pt, 
							DWORD flags);
												//@cmember an object may be
												// selected or de-activated.
	ClickStatus		HandleClick(CTxtEdit *ped, const POINT &pt);
												//@cmember an object may be
												// selected or deselected.
	void			HandleSingleSelect(CTxtEdit *ped, DWORD cp, BOOL fHiLite);
												//@cmember an object is
												// being selected by itself.
	COleObject *	GetSingleSelect(void)		{return _pobjselect;}
												//@cmember Count cObject
	LONG			CountObjects(LONG& rcObject,// objects up to cchMax
						LONG cp);				// chars away

												//@cmember Handles the deletion
												// of objects.
	void			ReplaceRange(DWORD cp, DWORD cchDel,
						IUndoBuilder *publdr);
												//@cmember Count the number
												//of objects in a range.
	DWORD			CountObjectsInRange(DWORD cpMin, DWORD cpMost);
												//@cmember Get the first
												//object in a range.
	COleObject *	GetFirstObjectInRange(DWORD cpMin, DWORD cpMost);
								//@cmember activate objects of one class as
								//as another
	HRESULT ActivateObjectsAs(REFCLSID rclsid, REFCLSID rclsidAs);

												//@cmember inform objects
												// that scrolling has 
												// occured.
	void			ScrollObjects(LONG dx, LONG dy, LPCRECT prcScroll);

	DWORD FindIndexForCp(DWORD cp);	//@cmember does a binary search for cp
									//@cmember find an object near a point

#ifdef DEBUG
	void			DbgDump(void);
#endif

	CObjectMgr();								//@cmember constructor
	~CObjectMgr();								//@cmember destructor

private:
	COleObject *FindObjectAtPointNearCp(const POINT &pt, DWORD cp);
	ObjectArray		_objarray;		//@cmember	the array of embedded objects
	DWORD			_lastindex;		//@cmember	the last index used 
									// (lookup optimization)
	IRichEditOleCallback *_precall;	//@cmember	callback for various OLE 
									// operations.
	COleObject *	_pobjactive;	//@cmember	the object that is currently
									// inplace active 
	COleObject *	_pobjselect;	//@cmember	the object that is currently
									// individually selected (not active)
	LPWSTR		_pszApp;			//@cmember 	the name of the app
	LPWSTR		_pszDoc;			//@cmember 	the name of the "document"

	unsigned int	_fShowUIPending:1;//@cmember a UI update is pending
	unsigned int	_fInHelpMode:1;	//@cmember in context sensitive help mode?
};

#endif  //__OBJMGR_H__

