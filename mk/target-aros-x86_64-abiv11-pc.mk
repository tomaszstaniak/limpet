# AROS ABIv11 (AROS One and other distributions), x86_64: the ABIv11 GCC
# 10.5 toolchain and Developer SDK (docs/platform/toolchains.md in
# AROS-dev). Set T_CC to .../x86_64-aros-gcc and T_SDK to .../Development,
# or ABIV11_GCC and ABIV11_SDK as for the abiv11-* rules.
# ABIV11_GCC/ABIV11_SDK, when given, win over a T_CC/T_SDK left in the
# environment for another target; make-packages.sh also checks the ABI.
T_CC := $(if $(ABIV11_GCC),$(ABIV11_GCC),$(T_CC))
T_CXX ?= $(patsubst %gcc,%g++,$(T_CC))
T_SDK := $(if $(ABIV11_SDK),$(ABIV11_SDK),$(T_SDK))
T_ARCH_FLAGS =
T_ARCH_LDFLAGS =
# ABIv11 has no task-removal notification (task.resource AddTaskNotifyHook):
# the break goes only to a listed task.
T_FRONT_FLAGS = -DXTERM_NO_TASK_NOTIFY
# Its DOS sets fh_Type to the port a FIND went to, so the handler serves
# clients on its process port; fonts are read once into memory.
T_WORKER_FLAGS = -DXTERM_NO_TASK_NOTIFY -DXTERM_PROCESS_PORT_CLIENTS -DXTERM_FONTS_IN_MEMORY
T_WORKER_SRC = src/aros_completion_job.c
# The link order of the 0.7.0 and earlier releases (abiv11-handler), so a
# rebuild of a released tree gives the released worker byte for byte.
T_WORKER_ALL_SRC = src/aros_xterm.c $(HYPERLINK_GUI_SRC) $(THEME_SRC) $(KITTY_SRC) src/aros_handler.c src/aros_font.c $(CLUSTER_SRC) src/screen_model.c src/hyperlink.c $(UNICODE_SRC) $(INPUT_SRC) $(EDITOR_SRC) src/aros_drop.c $(COMPLETION_SRC) src/aros_completion_job.c src/aros_input.c $(CLIP_GUI_SRC) $(SESSION_SRC)
T_LIBS = -lfreetype2 -lstdc++ -lpthread -lcybergraphics -lm
T_CXX_FLAGS =
# Released ABIv11 binaries have always been left as built.
T_STRIP =
