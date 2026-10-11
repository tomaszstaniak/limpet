# Limpet SDK build.
#
#   make CC=/path/to/x86_64-aros-gcc AROS_SDK=/path/to/SDK/Development ABI=abiv11
#       builds every example into build/<ABI>/, linked against
#       lib/<ABI>/libLimpet.a (the prebuilt one, while the sources are
#       unchanged)
#   make ... rebuild-lib
#       compiles lib/<ABI>/libLimpet.a from src/ with your toolchain
#   make host-check
#       compiles the library with the host compiler; proves nothing about AROS
#
# AROS_SDK is optional: a toolchain that already knows its sysroot needs
# only CC. ABI only names the output directories, so builds for different
# ABIs never overwrite each other; it does not select a toolchain.

# make predefines CC=cc and AR=ar, so ?= would never apply to them.
ifeq ($(origin CC),default)
CC = x86_64-aros-gcc
endif
ifeq ($(origin AR),default)
AR = $(patsubst %gcc,%ar,$(CC))
endif
ARFLAGS = rcsD
ABI ?= abiv11
AROS_SDK ?=
CFLAGS ?= -O2
WARN = -std=gnu99 -Wall -Wextra -Werror
ifneq ($(AROS_SDK),)
SYSROOT = --sysroot=$(AROS_SDK) -I$(AROS_SDK)/include
LIBDIRS = -L$(AROS_SDK)/lib
endif
EXAMPLES = hello colors progress geometry hyperlink image keys title
LIB = lib/$(ABI)/libLimpet.a
LIB_SRC = src/limpet_query.c src/limpet_format.c
LIB_HEADERS = include/limpet/limpet.h include/limpet/tty_control.h
OBJDIR = build/$(ABI)/obj

.PHONY: all lib rebuild-lib examples host-check clean
all: examples
lib: $(LIB)
examples: $(addprefix build/$(ABI)/,$(EXAMPLES))

# The library depends on its sources, not on object files: the archive
# ships it without objects, and a library as new as its sources is used
# as it is.
$(LIB): $(LIB_SRC) $(LIB_HEADERS)
	@mkdir -p $(OBJDIR) $(dir $@)
	$(CC) $(CFLAGS) $(WARN) $(SYSROOT) -Iinclude -c src/limpet_query.c -o $(OBJDIR)/limpet_query.o
	$(CC) $(CFLAGS) $(WARN) $(SYSROOT) -Iinclude -c src/limpet_format.c -o $(OBJDIR)/limpet_format.o
	rm -f $@
	$(AR) $(ARFLAGS) $@ $(OBJDIR)/limpet_query.o $(OBJDIR)/limpet_format.o

rebuild-lib:
	rm -f $(LIB)
	$(MAKE) lib

# -lm for image's arithmetic; harmless for the others.
build/$(ABI)/%: examples/%.c $(LIB)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(WARN) $(SYSROOT) -Iinclude $< -o $@ $(LIBDIRS) -Llib/$(ABI) -lLimpet -lm

# Host compilers have no <dos/dos.h>; tests/host has stand-ins with the
# x86_64 AROS type widths. The library is compiled, never run.
host-check:
	@mkdir -p build/host
	cc $(WARN) -Iinclude -Itests/host -c src/limpet_format.c -o build/host/limpet_format.o
	cc $(WARN) -Iinclude -Itests/host -c src/limpet_query.c -o build/host/limpet_query.o
	c++ -std=c++11 -Wall -Wextra -Werror -fsyntax-only -Iinclude -Itests/host -x c++ include/limpet/limpet.h

# Removes what this Makefile built. The shipped library stays.
clean:
	rm -rf build
