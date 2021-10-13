###############################################################################
#
#   This makefile copies files to the %_PROJECTOAKROOT%\files directory, and
#   to the %_FLATRELEASEDIR% directory if WINCEREL is set to "1".
#
###############################################################################

.SUFFIXES: .bat .txt .ddsuite .cfg

{}.bat{$(_RELEASEDIR)\}.bat:
	xcopy /d $? $(_RELEASEDIR)
    
{}.txt{$(_RELEASEDIR)\}.txt:
	xcopy /d $? $(_RELEASEDIR)
    
{}.ddsuite{$(_RELEASEDIR)\}.ddsuite:
	xcopy /d $? $(_RELEASEDIR)

{}.cfg{$(_RELEASEDIR)\}.cfg:
	xcopy /d $? $(_RELEASEDIR)

{}.bat{$(_FLATRELEASEDIR)\}.bat:
	xcopy /d $? $(_FLATRELEASEDIR)
    
{}.txt{$(_FLATRELEASEDIR)\}.txt:
	xcopy /d $? $(_FLATRELEASEDIR)
    
{}.ddsuite{$(_FLATRELEASEDIR)\}.ddsuite:
	xcopy /d $? $(_FLATRELEASEDIR)

{}.cfg{$(_FLATRELEASEDIR)\}.cfg:
	xcopy /d $? $(_FLATRELEASEDIR)

