# Release build for one ABI and CPU: make target TARGET=<name>
# (mk/target-<name>.mk sets the compiler, SDK and flags). Everything goes to
# build/<name>/, HarfBuzz included, so a build for one target never links an
# object made for another. The ABIv11 release keeps its own abiv11-* rules.
TD := build/$(TARGET)
T_CFLAGS = $(OPTFLAGS) $(VERSION_FLAGS) -std=gnu99 -Wall -Wextra -Werror --sysroot=$(T_SDK) -I$(T_SDK)/include $(UNICODE_FLAGS) -Ivendor/harfbuzz/src $(T_ARCH_FLAGS)
T_PROBE_CFLAGS = $(OPTFLAGS) -std=gnu99 -Wall -Wextra -Werror --sysroot=$(T_SDK) -I$(T_SDK)/include $(T_ARCH_FLAGS)
T_LDFLAGS = --sysroot=$(T_SDK) -L$(T_SDK)/lib $(T_ARCH_LDFLAGS)
T_GUI_SRC = src/aros_xterm.c $(HYPERLINK_GUI_SRC) $(THEME_SRC) $(KITTY_SRC) src/aros_font.c $(CLUSTER_SRC) src/screen_model.c src/hyperlink.c $(UNICODE_SRC) $(INPUT_SRC) $(EDITOR_SRC) src/aros_drop.c $(COMPLETION_SRC) src/aros_input.c $(CLIP_GUI_SRC) $(SESSION_SRC)

.PHONY: target target-check
target-check:
	@test -n "$(T_CC)" && command -v $(firstword $(T_CC)) >/dev/null || { echo "$(TARGET): compiler '$(T_CC)' not found (set T_CC)"; exit 1; }
	@test -d "$(T_SDK)/include" || { echo "$(TARGET): no SDK at '$(T_SDK)' (set T_SDK)"; exit 1; }
	@$(foreach v,$(T_NEED_VARS),test -d "$($(v))" || { echo "$(TARGET): set $(v) (now '$($(v))')"; exit 1; };)

target: target-check
	mkdir -p $(TD)/probes
	$(T_CXX) $(HB_FLAGS) $(T_CXX_FLAGS) --sysroot=$(T_SDK) $(T_ARCH_FLAGS) -c $(HB_SRC) -o $(TD)/harfbuzz.o
	$(T_CXX) $(HB_FLAGS) $(T_CXX_FLAGS) -DHB_NO_GETENV --sysroot=$(T_SDK) $(T_ARCH_FLAGS) -c $(HB_SRC) -o $(TD)/harfbuzz-handler.o
	$(T_CC) $(T_CFLAGS) $(T_FRONT_FLAGS) -o $(TD)/Limpet $(T_GUI_SRC) $(TD)/harfbuzz.o $(T_LDFLAGS) $(T_LIBS)
	$(T_CC) $(T_CFLAGS) -DXTERM_DOS_HANDLER $(T_WORKER_FLAGS) -o $(TD)/limpet-console $(T_GUI_SRC) src/aros_handler.c $(T_WORKER_SRC) $(TD)/harfbuzz-handler.o $(T_LDFLAGS) $(T_LIBS)
	$(T_CC) $(T_PROBE_CFLAGS) $(UNICODE_FLAGS) -nostartfiles -DXHANDLER_WORKER='"limpet-console"' tools/xterm_handler.c $(T_LDFLAGS) -o $(TD)/limpet-handler
	$(T_CC) $(T_PROBE_CFLAGS) -o $(TD)/limpet-handler-remove tools/xterm-console/xterm_remove.c $(T_LDFLAGS)
	$(T_CC) $(T_PROBE_CFLAGS) -o $(TD)/probes/con-contract tests/con_contract.c $(T_LDFLAGS)
	$(T_CC) $(T_PROBE_CFLAGS) -o $(TD)/probes/read-cancel tests/read_cancel.c $(T_LDFLAGS)
	$(T_CC) $(T_PROBE_CFLAGS) -Wno-error -o $(TD)/probes/tty-winsize tests/tty_winsize_probe.c $(T_LDFLAGS)
	$(T_CC) $(T_PROBE_CFLAGS) -o $(TD)/probes/console-geometry-probe tests/console_geometry_probe.c $(T_LDFLAGS)
	$(T_CC) $(T_PROBE_CFLAGS) -o $(TD)/probes/key-inject tests/key_inject.c $(T_LDFLAGS)
	$(T_CC) $(T_CFLAGS) $(T_FRONT_FLAGS) -o $(TD)/probes/test_aros_endpoint tests/test_aros_endpoint.c $(SESSION_SRC) $(T_LDFLAGS)
	$(if $(T_STRIP),$(T_STRIP) --strip-debug $(TD)/Limpet $(TD)/limpet-console $(TD)/limpet-handler $(TD)/limpet-handler-remove)
	@echo "$(TARGET): built in $(TD)"
