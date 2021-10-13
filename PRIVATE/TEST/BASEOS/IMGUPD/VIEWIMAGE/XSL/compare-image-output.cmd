@echo off
rem compare-image-output.cmd

REM
REM Copyright (c) Microsoft Corporation.  All rights reserved.
REM
REM
REM This source code is licensed under Microsoft Shared Source License
REM Version 1.0 for Windows CE.
REM For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
REM

REM 
REM compare-image-output.cmd <name-a> <file-a> <name-b> <file-b> <output-dir>
REM 
REM   or
REM 
REM compare-image-output.cmd clean <name-a> <name-b> <output-dir>
REM 

setlocal

set COMPARE_XML_LISTS_DIR=%_PRIVATEROOT%\test\imgupd\ViewImage\CompareXmlLists\bin\Debug
set COMPARE_XML_LISTS=%COMPARE_XML_LISTS_DIR%\CompareXmlLists.exe

if "%1" == "clean" goto CLEAN

set NAME_A=%1
set FILE_A=%2
set NAME_B=%3
set FILE_B=%4
set OD=%5

echo NAME_A=     [%NAME_A%]
echo FILE_A=     [%FILE_A%]
echo NAME_B=     [%NAME_B%]
echo FILE_B=     [%FILE_B%]
echo OUTPUT_DIR= [%OD%]

goto PROCESS_DATA

:CLEAN

set NAME_A=%2
set NAME_B=%3
set OD=%4

echo Cleaning %OD%...

set DATA_FILES=%NAME_A%-filtered.xml %NAME_B%-filtered.xml 
set DATA_FILES=%DATA_FILES% %NAME_A%-nk-modules-extracted.xml %NAME_A%-nk-files-extracted.xml %NAME_A%-imgfs-modules-extracted.xml %NAME_A%-reserves-extracted.xml
set DATA_FILES=%DATA_FILES% %NAME_B%-nk-modules-extracted.xml  %NAME_B%-nk-files-extracted.xml  %NAME_B%-imgfs-modules-extracted.xml  %NAME_B%-reserves-extracted.xml
set DATA_FILES=%DATA_FILES% %NAME_A%-imgfs-files-extracted.xml %NAME_B%-imgfs-files-extracted.xml
set DATA_FILES=%DATA_FILES% diff-nk-modules-text.txt diff-imgfs-modules-text.txt diff-nk-files-text.txt diff-imgfs-files-text.txt diff-reserves-text.txt
set DATA_FILES=%DATA_FILES% compare-nk-file-lists.txt compare-nk-module-lists.txt compare-imgfs-module-lists.txt compare-imgfs-file-lists.txt

for %%f in (%DATA_FILES%) do if exist %OD%\%%f del %OD%\%%f

goto END

:PROCESS_DATA

    rem *
    rem * Check for input files
    rem *

if "%NAME_A%" == "" goto USAGE
if "%FILE_A%" == "" goto USAGE
if "%NAME_B%" == "" goto USAGE
if "%FILE_B%" == "" goto USAGE
if "%OD%" == "" goto USAGE

set AFILE=%FILE_A%
if not exist %AFILE% goto ERRMISSINGFILE

set AFILE=%FILE_B%
if not exist %AFILE% goto ERRMISSINGFILE

if not exist %OD% mkdir %OD%

    rem *
    rem * Remove troublesome data
    rem *

echo Filtering %NAME_A%.xml...
msxsl %FILE_A% filter-data.xslt > %OD%\%NAME_A%-filtered.xml

echo Filtering %NAME_B%.xml...
msxsl %FILE_B% filter-data.xslt > %OD%\%NAME_B%-filtered.xml

    rem *
    rem * Extract the modules
    rem *

echo Extracting %NAME_A% Nk modules...
msxsl %OD%\%NAME_A%-filtered.xml extract-nk-modules.xslt > %OD%\%NAME_A%-nk-modules-extracted.xml

echo Extracting %NAME_A% IMGFS modules...
msxsl %OD%\%NAME_A%-filtered.xml extract-imgfs-modules.xslt > %OD%\%NAME_A%-imgfs-modules-extracted.xml

echo Extracting %NAME_A% reserves...
msxsl %OD%\%NAME_A%-filtered.xml extract-reserves.xslt > %OD%\%NAME_A%-reserves-extracted.xml

echo Extracting %NAME_B% Nk modules...
msxsl %OD%\%NAME_B%-filtered.xml extract-nk-modules.xslt > %OD%\%NAME_B%-nk-modules-extracted.xml

echo Extracting %NAME_B% IMGFS modules...
msxsl %OD%\%NAME_B%-filtered.xml extract-imgfs-modules.xslt > %OD%\%NAME_B%-imgfs-modules-extracted.xml

echo Extracting %NAME_B% reserves...
msxsl %OD%\%NAME_B%-filtered.xml extract-reserves.xslt > %OD%\%NAME_B%-reserves-extracted.xml

    rem *
    rem * Extract the files
    rem *

echo Extracting %NAME_A% files...
msxsl %OD%\%NAME_A%-filtered.xml extract-nk-files.xslt > %OD%\%NAME_A%-nk-files-extracted.xml
msxsl %OD%\%NAME_A%-filtered.xml extract-imgfs-files.xslt > %OD%\%NAME_A%-imgfs-files-extracted.xml

echo Extracting %NAME_B% files...
msxsl %OD%\%NAME_B%-filtered.xml extract-nk-files.xslt > %OD%\%NAME_B%-nk-files-extracted.xml
msxsl %OD%\%NAME_B%-filtered.xml extract-imgfs-files.xslt > %OD%\%NAME_B%-imgfs-files-extracted.xml

    rem *
    rem * Make diffs
    rem *

echo Text diff of Nk modules...
diff %OD%\%NAME_A%-nk-modules-extracted.xml %OD%\%NAME_B%-nk-modules-extracted.xml > %OD%\diff-nk-modules-text.txt

echo Text diff of Nk files...
diff %OD%\%NAME_A%-nk-files-extracted.xml %OD%\%NAME_B%-nk-files-extracted.xml > %OD%\diff-nk-files-text.txt

echo Text diff of reserves...
diff %OD%\%NAME_A%-nk-files-extracted.xml %OD%\%NAME_B%-nk-files-extracted.xml > %OD%\diff-nk-files-text.txt

echo Text diff of IMGFS modules...
diff %OD%\%NAME_A%-imgfs-modules-extracted.xml %OD%\%NAME_B%-imgfs-modules-extracted.xml > %OD%\diff-imgfs-modules-text.txt

echo Text diff of IMGFS files...
diff %OD%\%NAME_A%-imgfs-files-extracted.xml %OD%\%NAME_B%-imgfs-files-extracted.xml > %OD%\diff-imgfs-files-text.txt

echo Text diff of reserves...
diff %OD%\%NAME_A%-reserves-extracted.xml %OD%\%NAME_B%-reserves-extracted.xml > %OD%\diff-reserves-text.txt

    rem *
    rem * Make lists
    rem *

echo Comparing Nk module lists...
%COMPARE_XML_LISTS% -list -f1 %OD%\%NAME_A%-filtered.xml -p1 //nk//modules/module -f2 %OD%\%NAME_B%-filtered.xml -sort name > %OD%\compare-nk-module-lists.txt

echo Comparing Nk file lists...
%COMPARE_XML_LISTS% -list -f1 %OD%\%NAME_A%-filtered.xml -p1 //nk//files/file -f2 %OD%\%NAME_B%-filtered.xml -sort name > %OD%\compare-nk-file-lists.txt

echo Comparing IMGFS module lists...
%COMPARE_XML_LISTS% -list -f1 %OD%\%NAME_A%-filtered.xml -p1 //imgfs//modules/module -f2 %OD%\%NAME_B%-filtered.xml -sort name > %OD%\compare-imgfs-module-lists.txt

echo Comparing IMGFS file lists...
%COMPARE_XML_LISTS% -list -f1 %OD%\%NAME_A%-filtered.xml -p1 //imgfs//files/file -f2 %OD%\%NAME_B%-filtered.xml -sort name > %OD%\compare-imgfs-file-lists.txt

dir %OD%

    rem *
    rem * Done, goto END
    rem *

goto END

:ERRMISSINGFILE

echo *** Couldn't find file: %AFILE%
goto END

:USAGE
echo --------
echo USAGE:
echo "compare-image-output.cmd <name-a> <file-a> <name-b> <file-b> <output-dir>"
echo     or
echo "compare-image-output.cmd clean <name-a> <name-b> <output-dir>"
goto END


:END

endlocal
