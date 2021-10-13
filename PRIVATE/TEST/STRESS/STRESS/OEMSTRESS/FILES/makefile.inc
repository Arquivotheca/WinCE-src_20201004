!if "$(NOPASS0)"=="1"
dummy.txt:

!else

!if "$(WINCEREL)"=="1"

dummy.txt:
    copy oemstress.ini $(_FLATRELEASEDIR)
    copy results.xslt $(_FLATRELEASEDIR)
    copy oemstress.ini $(_RELEASEDIR)
    copy results.xslt $(_RELEASEDIR)
!else

dummy.txt:
    copy oemstress.ini $(_RELEASEDIR)
    copy results.xslt $(_RELEASEDIR)
!endif

!endif
