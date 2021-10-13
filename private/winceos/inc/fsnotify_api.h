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
#ifndef _FSNOTIFY_API_H_
#define _FSNOTIFY_API_H_

//
// Function specifications for exports of file system notification helper library (fsdnot.lib).
//

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

//
// Helper APIs for requesting and monitoring file change notifications:
//

// Create a notification handle to use for receiving file change notificaitons. Helper function
// for implementing FindFirstChangeNotification.
HANDLE  NotifyCreateEvent(HANDLE hVolume, HANDLE hProc, const WCHAR* pszPath, BOOL bWatchSubTree, DWORD dwFlags);

// Reset notification watch event and optionally retrieve data describing existing changes. Helper
// function for implementing FindNextChangeNotification and CeGetFileNotificationInfo APIs.
BOOL    NotifyGetNextChange(HANDLE h, DWORD dwFlags, LPVOID lpBuffer, DWORD nBufferLength, LPDWORD lpBytesReturned, LPDWORD lpBytesAvailable);

// Same as NotifyGetNextChange, but should be used instead when the notification owner is the kernel
// and not a user process.
BOOL    INT_NotifyGetNextChange(HANDLE h, DWORD dwFlags, LPVOID lpBuffer, DWORD nBufferLength, LPDWORD lpBytesReturned, LPDWORD lpBytesAvailable);

// Close a file change notification handle. Helper function for implementing FindCloseChangeNotification.
BOOL    NotifyCloseChangeHandle(HANDLE h);

// Same as NotifyCloseChangeHandle, but should be used instead when the notification owner is the kernel
// and not a user process.
BOOL    INT_NotifyCloseChangeHandle(HANDLE h);

//
// Helper APIs for generating file change notifications:
//

// Create a new root notification volume instance to associate with a mounted volume.
// The HANDLE returned is a psuedo-handle.
HANDLE  NotifyCreateVolume(WCHAR* pszNotifyPoint);

// Cleanup an existing notificaiton volume.
void    NotifyDeleteVolume(HANDLE hVolume);

// Create a file notification object to associate with an open file handle.
// The HANDLE returned is a psuedo-handle.
HANDLE  NotifyCreateFile(HANDLE hVolume, const WCHAR* pszFileName);

// Notify that an open file on a volume has been modified.
void    NotifyHandleChange(HANDLE hVolume, HANDLE hFile, DWORD dwFlags);

// Notify that a file handle is being closed. Generates final notifications (if any)
// and cleans up the handle returned by NotifyCreateFile.
void    NotifyCloseHandle(HANDLE hVolume, HANDLE hFile);

// Notify that a file or directory has been changed. Assumes that the final "\" character
// in lpszPath separates the path from the file/directory being changed.
void    NotifyPathChange(HANDLE hVolume, LPCWSTR lpszPath, BOOL bPath, DWORD dwAction);

// Notify that a file or direcotry has been changed. This API splits the path and changed
// file/directory name into separate parameters, wheras NotifyPathChange does not.
void    NotifyPathChangeEx(HANDLE hVolume, LPCWSTR lpszPath, LPCWSTR lpszFile, BOOL bPath, DWORD dwAction);

// Notify that a file has been moved/renamed. NOTE: Use NotifyMoveFileEx instead.
void    NotifyMoveFile(HANDLE hVolume, LPCWSTR lpszExisting, LPCWSTR lpszNew);

// Notify that a file or directory has been moved/renamed.
void    NotifyMoveFileEx(HANDLE hVolume, LPCWSTR lpszExisting, LPCWSTR lpszNew, BOOL bPath);

#ifdef __cplusplus
}
#endif

#endif // _FSNOTIFY_API_H_
