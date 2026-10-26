# AROS ABIv1 on AArch64 (Raspberry Pi): the AROS-patched Clang 20.1 wrapper
# and the Developer SDK of the official raspi-aarch64 system archive
# (AROS/toolchains/raspi-aarch64/README.md in the workspace).
# Set T_ROOT to that toolchain (bin/, sdk/), or T_CC, T_CXX and T_SDK.
T_ROOT ?=
T_CC ?= $(if $(T_ROOT),$(T_ROOT)/bin/aarch64-aros-clang)
T_CXX ?= $(if $(T_ROOT),$(T_ROOT)/bin/aarch64-aros-clang++)
T_SDK ?= $(if $(T_ROOT),$(T_ROOT)/sdk)
# Clang (not GCC) warns about "Limpet " LIMPET_VERSION in an initializer.
T_ARCH_FLAGS = -Wno-string-concatenation
T_ARCH_LDFLAGS =
# The SDK has no C++ standard library. HarfBuzz needs only the libc++
# headers (templates); built without exceptions and RTTI it calls nothing
# but C functions, so nothing C++ is linked. Headers: the AROS libc++ of
# the Macaros cross tools (.../crosstools/include/c++/v1); set T_CXX_INCLUDE.
T_CXX_INCLUDE ?=
T_NEED_VARS = T_CXX_INCLUDE
T_CXX_FLAGS = -fno-exceptions -fno-rtti -nostdinc++ -isystem $(T_CXX_INCLUDE)
# The Pi SDK has no task-removal notification (task.resource
# AddTaskNotifyHook), as ABIv11: the break goes only to a listed task.
T_FRONT_FLAGS = -DXTERM_NO_TASK_NOTIFY
T_WORKER_FLAGS = -DXTERM_NO_TASK_NOTIFY
T_WORKER_SRC =
T_LIBS = -lfreetype2 -lcybergraphics -lm
# No host paths in its debug sections; left as built.
T_STRIP =
