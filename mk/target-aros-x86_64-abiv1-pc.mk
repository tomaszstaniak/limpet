# Mainline AROS (ABIv1), x86_64: the mainline GCC 10.5 toolchain and the
# Developer SDK of a mainline build (docs/platform/toolchains.md in AROS-dev).
# Set T_CC to .../x86_64-aros-gcc and T_SDK to .../AROS/Developer.
T_CC ?=
T_CXX ?= $(patsubst %gcc,%g++,$(T_CC))
T_SDK ?=
T_ARCH_FLAGS =
T_ARCH_LDFLAGS =
# Mainline DOS keeps the handler's own session port (no process-port
# clients) and has task notification for the break signal.
T_FRONT_FLAGS =
T_WORKER_FLAGS =
T_WORKER_SRC =
T_LIBS = -lfreetype2 -lstdc++ -lcybergraphics -lm
T_CXX_FLAGS =
# The debug sections name the toolchain's host paths; --strip-debug is safe
# (a full strip breaks relocation, see the workspace notes).
T_STRIP = $(patsubst %gcc,%strip,$(T_CC))
