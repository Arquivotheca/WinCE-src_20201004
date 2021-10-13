//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#include <windows.h>
#include <tchar.h>

#define countof(a) (sizeof(a)/sizeof(*(a)))

#ifdef UNDER_CE

HINSTANCE g_hInstance = NULL;

extern "C" INT __cdecl main(INT argc, TCHAR *argv[]);

//******************************************************************************
// Windows CE does not support console applications.  As a hack, we use WimMain 
// as our entry point, convert the command line string to argc/argv format, and 
// then call our main() routine.

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow) {
   
   // Store our instance for later use.
   g_hInstance = hInstance;

   // Parse command line into individual arguments - arg/argv style.
   LPTSTR argv[16] = { TEXT("TUX.EXE") };
   INT    i, argc = 1;
   BOOL   fInQuotes;
   
   // Loop for up to 16 arguments.
   for (i = 1; i < countof(argv); i++) {

      // Clear our quote flag.
      fInQuotes = FALSE;

      // Move past and zero out any leading whitespace.
      while (*lpCmdLine && _istspace(*lpCmdLine)) {
         *(lpCmdLine++) = TEXT('\0');
      }
      
      // If the next character is a quote, move over it and remember that we saw it.
      if (*lpCmdLine == TEXT('\"')) {
         *(lpCmdLine++) = TEXT('\0');
         fInQuotes = TRUE;
      }

      // Point the current argument to our current location.
      argv[i] = lpCmdLine;

      // If this argument contains some text, then update our argument count.
      if (*argv[i]) {
         argc = i + 1;
      }

      // Move over valid text of this argument.
      while (*lpCmdLine) {

         if (fInQuotes) {

            // If in quotes, we only break out when we see another quote.
            if (*lpCmdLine == TEXT('\"')) {
               *(lpCmdLine++) = TEXT('\0');
               break;
            }

         } else {

            // If we are not in quotes and we see a quote, then we break out and
            // consider the quote as the start of the next argument.
            if (*lpCmdLine == TEXT('\"')) {
               break;

            // If we are not in quotes and we see whitespace, then we break out.
            } else if (_istspace(*lpCmdLine)) {
               *(lpCmdLine++) = TEXT('\0');
               break;
            }
         }

         // Move to the next character
         lpCmdLine++;
      }
   }

   // Call the standard main with our parsed arguments.
   return main(argc, argv);
}
#endif