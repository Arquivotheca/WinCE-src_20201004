# func.mk - General !include file for sources in the functional tree
!if "$(DDRAW_TST_ROOT)"==""
DDRAW_TST_ROOT=..\..
!endif

!include $(DDRAW_TST_ROOT)\ddraw.mk

TARGETLIBS=$(TARGETLIBS)                                        \
    $(TEST_OAK_LIB_PATH)\qalib.lib             \
    $(TEST_OAK_LIB_PATH)\syslib.lib            \
    $(TEST_OAK_LIB_PATH)\hqahelp.lib           \
    $(TEST_OAK_LIB_PATH)\QATestUty.lib         \
    $(TEST_OAK_LIB_PATH)\DebugStreamUty.lib    \
    $(TEST_OAK_LIB_PATH)\WinAppUty.lib         \
    $(TEST_OAK_LIB_PATH)\TuxDLLUty.lib         \
    $(TEST_OAK_LIB_PATH)\DDrawUty.lib          \
