!if 0
Copyright (c) Microsoft Corporation.  All rights reserved.
!endif
!if 0
Use of this source code is subject to the terms of your Microsoft Windows CE
Source Alliance Program license form.  If you did not accept the terms of
such a license, you are not authorized to use this source code.
!endif
SDK_DSHOW_INC_PATH=$(_PUBLICROOT)\$(WINCEPROJ)\sdk\inc

mytarget: copy

copy: \
    $(SDK_DSHOW_INC_PATH)\amaudio.h  \
    $(SDK_DSHOW_INC_PATH)\amvideo.h  \
    $(SDK_DSHOW_INC_PATH)\aviriff.h  \
    $(SDK_DSHOW_INC_PATH)\comlite.h  \
    $(SDK_DSHOW_INC_PATH)\cutlist.h  \
    $(SDK_DSHOW_INC_PATH)\dvdmedia.h \
    $(SDK_DSHOW_INC_PATH)\edevdefs.h \
    $(SDK_DSHOW_INC_PATH)\errors.h   \
    $(SDK_DSHOW_INC_PATH)\evcode.h   \
    $(SDK_DSHOW_INC_PATH)\ksuuids.h  \
    $(SDK_DSHOW_INC_PATH)\mpconfig.h \
    $(SDK_DSHOW_INC_PATH)\mpegtype.h \
    $(SDK_DSHOW_INC_PATH)\uuids.h    \
    $(SDK_DSHOW_INC_PATH)\vpconfig.h \
    $(SDK_DSHOW_INC_PATH)\vpnotify.h \
    $(SDK_DSHOW_INC_PATH)\vptype.h   \
    $(SDK_DSHOW_INC_PATH)\dvdevcod.h \
    $(SDK_DSHOW_INC_PATH)\il21dec.h

#
# Header files
#
$(SDK_DSHOW_INC_PATH)\amvideo.h  :$(@F)
$(SDK_DSHOW_INC_PATH)\aviriff.h  :$(@F)
$(SDK_DSHOW_INC_PATH)\comlite.h  :$(@F)
$(SDK_DSHOW_INC_PATH)\cutlist.h  :$(@F)
$(SDK_DSHOW_INC_PATH)\dvdmedia.h :$(@F)
$(SDK_DSHOW_INC_PATH)\edevdefs.h :$(@F)
$(SDK_DSHOW_INC_PATH)\errors.h   :$(@F)
$(SDK_DSHOW_INC_PATH)\evcode.h   :$(@F)
$(SDK_DSHOW_INC_PATH)\ksuuids.h  :$(@F)
$(SDK_DSHOW_INC_PATH)\mpconfig.h :$(@F)
$(SDK_DSHOW_INC_PATH)\mpegtype.h :$(@F)
$(SDK_DSHOW_INC_PATH)\uuids.h    :$(@F)
$(SDK_DSHOW_INC_PATH)\vpconfig.h :$(@F)
$(SDK_DSHOW_INC_PATH)\vpnotify.h :$(@F)
$(SDK_DSHOW_INC_PATH)\vptype.h   :$(@F)
$(SDK_DSHOW_INC_PATH)\dvdevcod.h :$(@F)
$(SDK_DSHOW_INC_PATH)\il21dec.h  :$(@F)

#
# Inference Rules
#
.SUFFIXES:.h 

.h{$(SDK_DSHOW_INC_PATH)}.h:
    copy $** $@
