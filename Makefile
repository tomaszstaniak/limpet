AROS_GCC ?= x86_64-aros-gcc
AROS_MAINLINE ?= ../AROS
AROS_SDK ?= ../AROS/build/bin/pc-x86_64/AROS/Developer
OUT ?= build/aros-xterm
HOST_CC ?= cc
HOST_CXX ?= c++
AROS_CXX ?= $(patsubst %gcc,%g++,$(AROS_GCC))
HOST_CXX_LIBS ?= -lc++
HB_FLAGS = -std=c++11 -O2 -DHB_NO_MT -DHB_NO_MMAP -DHB_NO_SETLOCALE
HB_SRC = vendor/harfbuzz/src/harfbuzz.cc
HB_HEADERS = $(wildcard vendor/harfbuzz/src/*.h vendor/harfbuzz/src/*.hh)
UNICODE_SRC = vendor/utf8proc/utf8proc.c
UNICODE_FLAGS = -Ivendor/utf8proc -DUTF8PROC_STATIC
INPUT_SRC = src/terminal_input.c src/echo_session.c
EDITOR_SRC = src/line_editor.c src/line_view.c src/window_title.c src/file_drop.c src/output_search.c
COMPLETION_SRC = src/completion.c src/aros_completion.c
SESSION_SRC = src/aros_session.c src/session_queue.c src/session_command.c src/console_input.c src/aros_break.c
SESSION_HEADERS = src/aros_session.h src/session_queue.h src/session_command.h src/console_input.h src/aros_break.h src/xterm_status.h
CLIP_GUI_SRC = src/selection.c src/paste.c src/input_order.c src/clipboard_text.c src/aros_clipboard.c src/aros_clip_job.c src/paint_damage.c src/key_queue.c src/reply_flow.c
CLIP_GUI_HEADERS = src/selection.h src/paste.h src/clipboard_text.h src/aros_clipboard.h src/aros_clip_job.h
CLUSTER_SRC = src/font_cache.c src/cluster_font.c
THEME_SRC = src/theme.c src/settings.c
KITTY_SRC = src/kitty_graphics.c
HYPERLINK_GUI_SRC = src/aros_hyperlink.c src/link_gesture.c

PYTHON ?= python3
HOST_TEST = build/test_screen_model
AROS_TEST = build/test_screen_model_aros
REPLAY_HEADER = build/replay_fixtures.h
FIXTURES = fixtures/manifest.json $(wildcard fixtures/*.expected)
WIDTH_TABLE = src/unicode_width_table.h
FONT_CFLAGS ?= $(shell pkg-config --cflags freetype2)
FONT_LIBS ?= $(shell pkg-config --libs freetype2)
FONT_PRIMARY ?= $(AROS_SDK)/../Fonts/TrueType/VeraMono.ttf
FONT_SECONDARY ?= $(AROS_SDK)/../Fonts/TrueType/OpenSans-Regular.ttf
FONT_ASSETS = assets/fonts
FONT_SOURCES = tests/test_font_fallback.c src/font_cache.c src/aros_font.h

OPTFLAGS ?= -O2
LIMPET_VERSION ?= 0.0.0-dev
LIMPET_DATE ?= $(shell date +%d.%m.%Y)
VERSION_FLAGS = -DLIMPET_VERSION='"$(LIMPET_VERSION)"' -DLIMPET_DATE='"$(LIMPET_DATE)"'
CFLAGS = $(OPTFLAGS) $(VERSION_FLAGS) -std=gnu99 -Wall -Wextra -Werror --sysroot=$(AROS_SDK) -I$(AROS_SDK)/include $(UNICODE_FLAGS) -Ivendor/harfbuzz/src
LDFLAGS = --sysroot=$(AROS_SDK) -L$(AROS_SDK)/lib

# Isolated ABIv11 frontend preview. Pass an explicit compiler and SDK from the
# same ABI family; never reuse mainline's HarfBuzz object or build directory.
ABIV11_GCC ?=
ABIV11_SDK ?=
ABIV11_CXX = $(patsubst %gcc,%g++,$(ABIV11_GCC))
.PHONY: abiv11-frontend
abiv11-frontend:
	@test -n "$(ABIV11_GCC)" || { echo "Set ABIV11_GCC to the x86_64 ABIv11 compiler"; exit 1; }
	@test -d "$(ABIV11_SDK)/include" || { echo "Set ABIV11_SDK to the ABIv11 Developer SDK"; exit 1; }
	mkdir -p build/abiv11
	$(ABIV11_CXX) $(HB_FLAGS) --sysroot=$(ABIV11_SDK) -c $(HB_SRC) -o build/abiv11/harfbuzz.o
	$(ABIV11_GCC) $(OPTFLAGS) $(VERSION_FLAGS) -std=gnu99 -Wall -Wextra -Werror --sysroot=$(ABIV11_SDK) -I$(ABIV11_SDK)/include $(UNICODE_FLAGS) -Ivendor/harfbuzz/src -DXTERM_NO_TASK_NOTIFY -o build/abiv11/aros-xterm src/aros_xterm.c $(HYPERLINK_GUI_SRC) $(THEME_SRC) $(KITTY_SRC) src/aros_font.c $(CLUSTER_SRC) src/screen_model.c src/hyperlink.c $(UNICODE_SRC) $(INPUT_SRC) $(EDITOR_SRC) src/aros_drop.c $(COMPLETION_SRC) src/aros_input.c $(CLIP_GUI_SRC) $(SESSION_SRC) build/abiv11/harfbuzz.o --sysroot=$(ABIV11_SDK) -L$(ABIV11_SDK)/lib -lfreetype2 -lstdc++ -lpthread -lcybergraphics -lm

# Guest tools for the file drop check (docs/file-drop.md): what a program
# receives (argv and ReadArgs), and a drawer of awkward names with icons.
.PHONY: abiv11-drop-tools
abiv11-drop-tools:
	@test -n "$(ABIV11_GCC)" || { echo "Set ABIV11_GCC to the x86_64 ABIv11 compiler"; exit 1; }
	mkdir -p build/abiv11
	$(ABIV11_GCC) -O2 -std=gnu99 -Wall -Wextra -Werror --sysroot=$(ABIV11_SDK) -I$(ABIV11_SDK)/include -o build/abiv11/drop-args tests/drop_args.c -L$(ABIV11_SDK)/lib
	$(ABIV11_GCC) -O2 -std=gnu99 -Wall -Wextra -Werror --sysroot=$(ABIV11_SDK) -I$(ABIV11_SDK)/include -o build/abiv11/drop-fixture tests/drop_fixture.c -L$(ABIV11_SDK)/lib
	$(ABIV11_GCC) -O2 -std=gnu99 -Wall -Wextra -Werror --sysroot=$(ABIV11_SDK) -I$(ABIV11_SDK)/include -Isrc -o build/abiv11/drop-close tests/drop_close.c src/aros_drop.c -L$(ABIV11_SDK)/lib

# A console read taken back with XTY_ACTION_CANCEL (tests/read_cancel.c).
.PHONY: abiv11-read-cancel
abiv11-read-cancel:
	@test -n "$(ABIV11_GCC)" || { echo "Set ABIV11_GCC to the x86_64 ABIv11 compiler"; exit 1; }
	mkdir -p build/abiv11
	$(ABIV11_GCC) -O2 -std=gnu99 -Wall -Wextra -Werror --sysroot=$(ABIV11_SDK) -I$(ABIV11_SDK)/include -o build/abiv11/read-cancel tests/read_cancel.c -L$(ABIV11_SDK)/lib

# The same check against the 0.4.0 candidate's close, which it must fail
# (needs this git checkout).
.PHONY: abiv11-drop-close-old
abiv11-drop-close-old:
	@test -n "$(ABIV11_GCC)" || { echo "Set ABIV11_GCC to the x86_64 ABIv11 compiler"; exit 1; }
	rm -rf build/abiv11/old-drop && mkdir -p build/abiv11/old-drop
	git show 07268aa:src/aros_drop.c >build/abiv11/old-drop/aros_drop.c
	git show 07268aa:src/aros_drop.h >build/abiv11/old-drop/aros_drop.h
	$(ABIV11_GCC) -O2 -std=gnu99 -Wall -Wextra -Werror --sysroot=$(ABIV11_SDK) -I$(ABIV11_SDK)/include -DOLD_DROP -Ibuild/abiv11/old-drop -Isrc -o build/abiv11/drop-close-old tests/drop_close.c build/abiv11/old-drop/aros_drop.c -L$(ABIV11_SDK)/lib

# Shaping benchmark (tools/bench_shaping.c), with the HarfBuzz object of
# Guest probes for checks that need AROS (doc/verification-0.3.0.md): an
# inline image at a chosen row with C=0 or C=1, and Ctrl-C/raw Ctrl-R input.
.PHONY: abiv11-probes
abiv11-probes:
	mkdir -p build/abiv11/probes
	$(ABIV11_GCC) $(OPTFLAGS) -std=gnu99 -Wall -Wextra -Werror --sysroot=$(ABIV11_SDK) -I$(ABIV11_SDK)/include -Isdk/include -o build/abiv11/probes/kitty_edge_probe tests/kitty_edge_probe.c sdk/src/limpet_query.c sdk/src/limpet_format.c -L$(ABIV11_SDK)/lib
	$(ABIV11_GCC) $(OPTFLAGS) -std=gnu99 -Wall -Wextra -Werror --sysroot=$(ABIV11_SDK) -I$(ABIV11_SDK)/include -o build/abiv11/probes/break_probe tests/break_probe.c -L$(ABIV11_SDK)/lib

# abiv11-frontend. Run it from the Limpet drawer: it reads PROGDIR:Fonts.
.PHONY: abiv11-bench-shaping
abiv11-bench-shaping:
	@test -f build/abiv11/harfbuzz.o || { echo "Run make abiv11-frontend first (ABIv11 HarfBuzz object)"; exit 1; }
	$(ABIV11_GCC) $(OPTFLAGS) -std=gnu99 -Wall -Wextra -Werror --sysroot=$(ABIV11_SDK) -I$(ABIV11_SDK)/include $(UNICODE_FLAGS) -Ivendor/harfbuzz/src -o build/abiv11/bench-shaping tools/bench_shaping.c $(CLUSTER_SRC) src/screen_model.c src/hyperlink.c $(UNICODE_SRC) build/abiv11/harfbuzz.o --sysroot=$(ABIV11_SDK) -L$(ABIV11_SDK)/lib -lfreetype2 -lstdc++ -lpthread -lm

# ABIv11 mounted-handler build: the GUI worker and CRT-free launcher that
# `Mount XTERM:` uses, plus the headless launcher harness that isolates the
# DOS startup protocol from the GUI. It reuses only the ABIv11 HarfBuzz object
# made by abiv11-frontend and leaves the frontend executable untouched.
ABIV11_CFLAGS = $(OPTFLAGS) -std=gnu99 -Wall -Wextra -Werror --sysroot=$(ABIV11_SDK) -I$(ABIV11_SDK)/include $(UNICODE_FLAGS) -Ivendor/harfbuzz/src -DXTERM_NO_TASK_NOTIFY
ABIV11_LDFLAGS = --sysroot=$(ABIV11_SDK) -L$(ABIV11_SDK)/lib
# The ABIv11 DOS overwrites fh_Type with the port a FIND went to, so clients
# reach the worker's own process port. The worker serves that port and must
# make no synchronous DOS call after accept: fonts are read into memory
# first, TAB enumeration runs in a child, child processes get explicit dirs,
# and HarfBuzz skips its lazy getenv (GetVar opens ENV:/ENVARC:). See
# docs/abiv11-worker-dos-audit.md. Mainline keeps its private session port.
ABIV11_WORKER_FLAGS = -DXTERM_DOS_HANDLER -DXTERM_PROCESS_PORT_CLIENTS -DXTERM_FONTS_IN_MEMORY
ABIV11_WORKER_SRC = src/aros_xterm.c $(HYPERLINK_GUI_SRC) $(THEME_SRC) $(KITTY_SRC) src/aros_handler.c src/aros_font.c $(CLUSTER_SRC) src/screen_model.c src/hyperlink.c $(UNICODE_SRC) $(INPUT_SRC) $(EDITOR_SRC) src/aros_drop.c $(COMPLETION_SRC) src/aros_completion_job.c src/aros_input.c $(CLIP_GUI_SRC) $(SESSION_SRC) build/abiv11/harfbuzz-handler.o
.PHONY: abiv11-handler
abiv11-handler:
	@test -n "$(ABIV11_GCC)" || { echo "Set ABIV11_GCC to the x86_64 ABIv11 compiler"; exit 1; }
	@test -d "$(ABIV11_SDK)/include" || { echo "Set ABIV11_SDK to the ABIv11 Developer SDK"; exit 1; }
	@test -f build/abiv11/harfbuzz.o || { echo "Run make abiv11-frontend first (ABIv11 HarfBuzz object)"; exit 1; }
	mkdir -p build/abiv11/harness
	$(ABIV11_CXX) $(HB_FLAGS) -DHB_NO_GETENV --sysroot=$(ABIV11_SDK) -c $(HB_SRC) -o build/abiv11/harfbuzz-handler.o
	$(ABIV11_GCC) $(ABIV11_CFLAGS) $(ABIV11_WORKER_FLAGS) -o build/abiv11/aros-xterm-worker $(ABIV11_WORKER_SRC) $(ABIV11_LDFLAGS) -lfreetype2 -lstdc++ -lpthread -lcybergraphics -lm
	$(ABIV11_GCC) $(ABIV11_CFLAGS) -nostartfiles tools/xterm_handler.c $(ABIV11_LDFLAGS) -o build/abiv11/xterm-handler
	cp build/abiv11/xterm-handler build/abiv11/harness/xterm-handler
	$(ABIV11_GCC) $(ABIV11_CFLAGS) -DXTERM_DOS_HANDLER tests/test_handler_worker.c src/aros_handler.c $(SESSION_SRC) $(ABIV11_LDFLAGS) -o build/abiv11/harness/aros-xterm-worker
	$(ABIV11_GCC) $(ABIV11_CFLAGS) tests/test_handler_launch.c $(ABIV11_LDFLAGS) -o build/abiv11/harness/test_handler_launch

# TEST worker only (XTERM_PKTWAIT_PROBE): the product worker plus a pr_PktWait
# hook that counts synchronous DOS waits after accept and shows the count in
# the title. The hook returns NULL, so DOS still does its own wait. The TAB
# child also sleeps 5 s first, so F5/close during a job can be exercised.
# Never ship.
.PHONY: abiv11-handler-probe
abiv11-handler-probe:
	@test -f build/abiv11/harfbuzz-handler.o || { echo "Run make abiv11-handler first"; exit 1; }
	mkdir -p build/abiv11/probe
	$(ABIV11_GCC) $(ABIV11_CFLAGS) $(ABIV11_WORKER_FLAGS) -DXTERM_PKTWAIT_PROBE -DXTERM_TEST_SLOW_COMPLETION -o build/abiv11/probe/aros-xterm-worker $(ABIV11_WORKER_SRC) $(ABIV11_LDFLAGS) -lfreetype2 -lstdc++ -lpthread -lcybergraphics -lm

# DIAGNOSTIC worker only (XTERM_DIAG_FORWARD): measures whether client packets
# reach the handler's process port on an ABIv11 DOS. Never package or ship it.
.PHONY: abiv11-handler-diag
abiv11-handler-diag:
	@test -f build/abiv11/harfbuzz.o || { echo "Run make abiv11-frontend first (ABIv11 HarfBuzz object)"; exit 1; }
	mkdir -p build/abiv11/diag
	$(ABIV11_GCC) $(ABIV11_CFLAGS) -DXTERM_DOS_HANDLER -DXTERM_DIAG_FORWARD -o build/abiv11/diag/aros-xterm-worker src/aros_xterm.c $(HYPERLINK_GUI_SRC) $(THEME_SRC) $(KITTY_SRC) src/aros_handler.c src/aros_font.c $(CLUSTER_SRC) src/screen_model.c src/hyperlink.c $(UNICODE_SRC) $(INPUT_SRC) $(EDITOR_SRC) src/aros_drop.c $(COMPLETION_SRC) src/aros_input.c $(CLIP_GUI_SRC) $(SESSION_SRC) build/abiv11/harfbuzz.o $(ABIV11_LDFLAGS) -lfreetype2 -lstdc++ -lpthread -lcybergraphics -lm

$(OUT): src/aros_xterm.c $(HYPERLINK_GUI_SRC) src/aros_font.c $(CLUSTER_SRC) src/aros_font.h src/cluster_font.h src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h $(INPUT_SRC) src/aros_input.c src/aros_input.h $(CLIP_GUI_SRC) $(CLIP_GUI_HEADERS) $(UNICODE_SRC) build/harfbuzz-aros.o
	mkdir -p $(dir $(OUT))
	$(AROS_GCC) $(CFLAGS) -o $(OUT) src/aros_xterm.c $(HYPERLINK_GUI_SRC) $(THEME_SRC) $(KITTY_SRC) src/aros_font.c $(CLUSTER_SRC) src/screen_model.c src/hyperlink.c $(UNICODE_SRC) $(INPUT_SRC) $(EDITOR_SRC) src/aros_drop.c $(COMPLETION_SRC) src/aros_input.c $(CLIP_GUI_SRC) $(SESSION_SRC) build/harfbuzz-aros.o $(LDFLAGS) -lfreetype2 -lstdc++ -lcybergraphics -lm

$(OUT): $(SESSION_SRC) $(SESSION_HEADERS) $(THEME_SRC) src/theme.h
$(OUT): $(EDITOR_SRC) src/line_editor.h src/line_view.h src/window_title.h src/file_drop.h src/output_search.h src/aros_drop.c src/aros_drop.h
$(OUT): $(KITTY_SRC) src/kitty_graphics.h
$(OUT): $(COMPLETION_SRC) src/completion.h src/aros_completion.h
$(OUT): src/aros_handler.h

# A fresh segment is loaded per DOS instance; never mount the CRT/GUI directly.
PTY_FRONT_SRC = src/aros_pty_frontend.c src/aros_tty_packets.c src/aros_tty_signal.c src/tty_service.c src/tty_pair.c src/tty_control.c
build/aros-xterm-pty: $(OUT) $(PTY_FRONT_SRC) src/aros_pty_frontend.h Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) -DXTERM_PTY_FRONTEND -o $@ src/aros_xterm.c $(HYPERLINK_GUI_SRC) $(THEME_SRC) $(KITTY_SRC) src/aros_font.c $(CLUSTER_SRC) src/screen_model.c src/hyperlink.c $(UNICODE_SRC) $(INPUT_SRC) $(EDITOR_SRC) src/aros_drop.c $(COMPLETION_SRC) src/aros_input.c $(CLIP_GUI_SRC) $(SESSION_SRC) $(PTY_FRONT_SRC) build/harfbuzz-aros.o $(LDFLAGS) -lfreetype2 -lstdc++ -lcybergraphics -lm
# Diagnostic PTY frontend: RAM:pty-diag.txt and forced SetVBuf failure
# (docs/pty-output-completeness-20260927.md). Never a release binary.
build/aros-xterm-pty-diag: $(OUT) $(PTY_FRONT_SRC) src/aros_pty_frontend.h src/pty_fh_diag.h Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) -DXTERM_PTY_FRONTEND -DXTERM_PTY_DIAG -o $@ src/aros_xterm.c $(HYPERLINK_GUI_SRC) $(THEME_SRC) $(KITTY_SRC) src/aros_font.c $(CLUSTER_SRC) src/screen_model.c src/hyperlink.c $(UNICODE_SRC) $(INPUT_SRC) $(EDITOR_SRC) src/aros_drop.c $(COMPLETION_SRC) src/aros_input.c $(CLIP_GUI_SRC) $(SESSION_SRC) $(PTY_FRONT_SRC) build/harfbuzz-aros.o $(LDFLAGS) -lfreetype2 -lstdc++ -lcybergraphics -lm
build/test_aros_pty_frontend: tests/test_aros_pty_frontend.c $(PTY_FRONT_SRC) src/session_queue.c src/aros_break.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) $(filter %.c,$^) $(LDFLAGS) -o $@
build/tty_pty_frontend_peer: tests/tty_fd_session.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) -DTEST_LIBC_NONBLOCK -DTEST_LIBC_GEOMETRY -DTEST_GEOMETRY_CELLS_ONLY $< $(LDFLAGS) -o $@
build/tty_pty_gui_peer: tests/tty_pty_gui_peer.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) $< $(LDFLAGS) -o $@
# Native helpers for the Crossterm/PTY resize acceptance
# (docs/crossterm-pty-resize-20260927.md).
build/tty-winsize-probe: tests/tty_winsize_probe.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) $< $(LDFLAGS) -o $@
build/pty-system-run: tests/pty_system_run.c src/pty_fh_diag.h Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) $< $(LDFLAGS) -o $@
build/pty-early-output: tests/pty_early_output.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) $< $(LDFLAGS) -o $@
build/pty-early-libc: tests/pty_early_output.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) -DEARLY_LIBC $< $(LDFLAGS) -o $@
build/pty-early-stdio: tests/pty_early_output.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) -DEARLY_LIBC -DEARLY_STDIO $< $(LDFLAGS) -o $@
build/pty-early-query: tests/pty_early_output.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) -DEARLY_LIBC -DEARLY_QUERY_FIRST $< $(LDFLAGS) -o $@
build/pty-early-stdio-query: tests/pty_early_output.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) -DEARLY_LIBC -DEARLY_STDIO -DEARLY_QUERY_FIRST $< $(LDFLAGS) -o $@
build/pty-early-dos-quick: tests/pty_early_output.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) -DEARLY_QUICK $< $(LDFLAGS) -o $@
build/pty-early-libc-quick: tests/pty_early_output.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) -DEARLY_LIBC -DEARLY_QUICK $< $(LDFLAGS) -o $@
build/pty-early-exitflush: tests/pty_early_output.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) -DEARLY_LIBC -DEARLY_STDIO -DEARLY_QUICK -DEARLY_EXIT_FLUSH $< $(LDFLAGS) -o $@
build/pty-early-exitflush-query: tests/pty_early_output.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) -DEARLY_LIBC -DEARLY_STDIO -DEARLY_QUICK -DEARLY_EXIT_FLUSH -DEARLY_QUERY_FIRST $< $(LDFLAGS) -o $@
build/dos-buffer-exit-repro: tests/dos_buffer_exit_repro.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) $< $(LDFLAGS) -o $@
build/dos-buffer-exit-repro-stdcio: tests/dos_buffer_exit_repro.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) -DREPRO_STDCIO $< $(LDFLAGS) -o $@
build/pty-matrix-dos-nl-flush: tests/pty_output_matrix.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) -DMATRIX_NEWLINE -DMATRIX_FLUSH $< $(LDFLAGS) -o $@
build/pty-matrix-dos-nl-exit: tests/pty_output_matrix.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) -DMATRIX_NEWLINE $< $(LDFLAGS) -o $@
build/pty-matrix-dos-nonl-flush: tests/pty_output_matrix.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) -DMATRIX_FLUSH $< $(LDFLAGS) -o $@
build/pty-matrix-dos-nonl-exit: tests/pty_output_matrix.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS)  $< $(LDFLAGS) -o $@
build/pty-matrix-libc-nl-flush: tests/pty_output_matrix.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) -DMATRIX_LIBC -DMATRIX_NEWLINE -DMATRIX_FLUSH $< $(LDFLAGS) -o $@
build/pty-matrix-libc-nl-exit: tests/pty_output_matrix.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) -DMATRIX_LIBC -DMATRIX_NEWLINE $< $(LDFLAGS) -o $@
build/pty-matrix-libc-nonl-flush: tests/pty_output_matrix.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) -DMATRIX_LIBC -DMATRIX_FLUSH $< $(LDFLAGS) -o $@
build/pty-matrix-libc-nonl-exit: tests/pty_output_matrix.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) -DMATRIX_LIBC $< $(LDFLAGS) -o $@
pty-output-matrix: build/pty-matrix-dos-nl-flush build/pty-matrix-dos-nl-exit build/pty-matrix-dos-nonl-flush build/pty-matrix-dos-nonl-exit build/pty-matrix-libc-nl-flush build/pty-matrix-libc-nl-exit build/pty-matrix-libc-nonl-flush build/pty-matrix-libc-nonl-exit
.PHONY: pty-output-matrix
build/pty-stderr-tail: tests/pty_stderr_tail.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) $< $(LDFLAGS) -o $@
build/pty-stderr-tail-libc: tests/pty_stderr_tail.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) -DSTDERR_LIBC $< $(LDFLAGS) -o $@
build/pty-tail-no-newline: tests/pty_tail_no_newline.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) $< $(LDFLAGS) -o $@
build/pty-tail-no-newline-libc: tests/pty_tail_no_newline.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) -DTAIL_LIBC $< $(LDFLAGS) -o $@

build/aros-xterm-worker: $(OUT) src/aros_handler.c src/aros_handler.h Makefile
	$(AROS_GCC) $(CFLAGS) -DXTERM_DOS_HANDLER -o $@ src/aros_xterm.c $(HYPERLINK_GUI_SRC) $(THEME_SRC) $(KITTY_SRC) src/aros_handler.c src/aros_font.c $(CLUSTER_SRC) src/screen_model.c src/hyperlink.c $(UNICODE_SRC) $(INPUT_SRC) $(EDITOR_SRC) src/aros_drop.c $(COMPLETION_SRC) src/aros_input.c $(CLIP_GUI_SRC) $(SESSION_SRC) build/harfbuzz-aros.o $(LDFLAGS) -lfreetype2 -lstdc++ -lcybergraphics -lm

package: build/xterm-handler build/aros-xterm-worker
package: build/geometry-probe
package: build/link-demo

build/link-demo: tools/link_demo.c
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) $< -o $@

build/geometry-probe: tests/geometry_probe.c
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) $< -o $@

# Optional visual smoke test; not included in the release package.
build/scroll-region-demo: tools/scroll_region_demo.c Makefile build/native-config.json
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) $< $(LDFLAGS) -o $@

# Optional mainline command fix; never installs/replaces the resident Run.
.PHONY: run-regression
run-regression: build/Run-fixed build/test_run_redirect

build/Run-fixed: $(AROS_MAINLINE)/workbench/c/shellcommands/Run.c
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) -Wno-pointer-sign -DADATE='"08.09.2026"' -nostartfiles -static $< -o $@

build/test_run_redirect: tests/test_run_redirect.c
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) -Wno-pointer-sign $< -o $@
test: build/test_completion
package: build/test_completion_aros build/test_aros_completion
package: build/test_shell_startup
package: build/test_aros_endpoint

build/test_handler_launch: tests/test_handler_launch.c Makefile
	mkdir -p $(dir $@)
	$(AROS_GCC) $(CFLAGS) $< $(LDFLAGS) -o $@

build/xterm-handler: tools/xterm_handler.c src/aros_handler.h Makefile
	mkdir -p $(dir $@)
	$(AROS_GCC) $(CFLAGS) -nostartfiles $< $(LDFLAGS) -o $@

build/headless/aros-xterm-worker: tests/test_handler_worker.c src/aros_handler.c src/aros_handler.h $(SESSION_SRC) $(SESSION_HEADERS) Makefile
	mkdir -p build/headless
	$(AROS_GCC) $(CFLAGS) -DXTERM_DOS_HANDLER $< src/aros_handler.c $(SESSION_SRC) $(LDFLAGS) -o $@

build/test_aros_endpoint: tests/test_aros_endpoint.c $(SESSION_SRC) $(SESSION_HEADERS) Makefile
	$(AROS_GCC) $(CFLAGS) $< $(SESSION_SRC) $(LDFLAGS) -o $@

build/test_shell_startup: tests/test_shell_startup.c $(SESSION_SRC) $(SESSION_HEADERS) Makefile
	$(AROS_GCC) $(CFLAGS) -DXTERM_SESSION_TESTING $< $(SESSION_SRC) $(LDFLAGS) -o $@

build/test_completion: tests/test_completion.c src/completion.c src/completion.h src/line_editor.c src/line_editor.h $(UNICODE_SRC) Makefile
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) $< src/completion.c src/line_editor.c $(UNICODE_SRC) -o $@

build/test_completion_aros: tests/test_completion.c src/completion.c src/completion.h src/line_editor.c src/line_editor.h $(UNICODE_SRC) Makefile
	$(AROS_GCC) $(CFLAGS) $< src/completion.c src/line_editor.c $(UNICODE_SRC) $(LDFLAGS) -o $@

build/test_aros_completion: tests/test_aros_completion.c $(COMPLETION_SRC) src/completion.h src/aros_completion.h src/line_editor.c src/line_editor.h $(UNICODE_SRC) Makefile
	$(AROS_GCC) $(CFLAGS) $< $(COMPLETION_SRC) src/line_editor.c $(UNICODE_SRC) $(LDFLAGS) -o $@
package: build/session-peer build/test_aros_session build/test_alternate_aros build/test_aros_tui build/test_line_editor_aros build/test_line_view_aros
test: build/test_session_queue build/test_session_command
test: build/test_session_test_options

build/test_session_test_options: tests/test_session_test_options.c tests/session_test_options.h Makefile
	@mkdir -p build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $< -o $@
test: build/test_file_drop
package: build/test_file_drop_aros

build/test_file_drop: tests/test_file_drop.c src/file_drop.c src/file_drop.h src/line_editor.c src/line_editor.h $(UNICODE_SRC) Makefile
	@mkdir -p build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) $< src/file_drop.c src/line_editor.c $(UNICODE_SRC) -o $@

build/test_file_drop_aros: tests/test_file_drop.c src/file_drop.c src/file_drop.h src/line_editor.c src/line_editor.h $(UNICODE_SRC) Makefile
	$(AROS_GCC) $(CFLAGS) $< src/file_drop.c src/line_editor.c $(UNICODE_SRC) $(LDFLAGS) -o $@
test: build/test_console_input
test: build/test_line_editor
test: build/test_line_view

build/test_line_view: tests/test_line_view.c src/line_view.c src/line_view.h src/line_editor.c src/line_editor.h src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h $(UNICODE_SRC) Makefile
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) $< src/line_view.c src/line_editor.c src/screen_model.c src/hyperlink.c $(UNICODE_SRC) -o $@

build/test_line_view_aros: tests/test_line_view.c src/line_view.c src/line_view.h src/line_editor.c src/line_editor.h src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h $(UNICODE_SRC) Makefile
	$(AROS_GCC) $(CFLAGS) $< src/line_view.c src/line_editor.c src/screen_model.c src/hyperlink.c $(UNICODE_SRC) $(LDFLAGS) -o $@

build/test_line_editor: tests/test_line_editor.c src/line_editor.c src/line_editor.h $(UNICODE_SRC) Makefile
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) $< src/line_editor.c $(UNICODE_SRC) -o $@

build/test_line_editor_aros: tests/test_line_editor.c src/line_editor.c src/line_editor.h $(UNICODE_SRC) Makefile
	$(AROS_GCC) $(CFLAGS) $< src/line_editor.c $(UNICODE_SRC) $(LDFLAGS) -o $@
test: build/test_alternate
test: build/test_tui_menu
test: build/test_status_area
test: build/test_title_buffer
test: build/test_osc_title
test: build/test_window_title
test: build/test_profile_prompt
test: build/test_theme
test: build/test_geometry_reply
test: build/test_mouse
test: test-bracketed-paste
test: test-resize-report
.PHONY: test-resize-report
test-resize-report: build/test_resize_report
	./build/test_resize_report
.PHONY: test-bracketed-paste
test-bracketed-paste: build/test_bracketed_paste
	./build/test_bracketed_paste
test: build/test_boxdraw
test: build/test_mouse_release
test: build/test_ssh_pump build/test_hyperlink build/test_link_gesture

$(OUT): src/hyperlink.h src/aros_hyperlink.h src/link_gesture.h

build/test_window_title: tests/test_window_title.c src/window_title.c src/window_title.h Makefile
	@mkdir -p build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $< src/window_title.c -o $@

build/test_osc_title: tests/test_osc_title.c src/hyperlink.c src/hyperlink.h src/screen_model.c src/screen_model.h $(UNICODE_SRC) Makefile
	@mkdir -p build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) $< src/hyperlink.c src/screen_model.c $(UNICODE_SRC) -o $@

build/test_hyperlink: tests/test_hyperlink.c src/hyperlink.c src/hyperlink.h src/screen_model.c src/screen_model.h $(UNICODE_SRC) Makefile
	@mkdir -p build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) $< src/hyperlink.c src/screen_model.c $(UNICODE_SRC) -o $@

build/test_link_gesture: tests/test_link_gesture.c src/link_gesture.c src/link_gesture.h
	@mkdir -p build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $< src/link_gesture.c -o $@

build/test_ssh_pump: tests/test_ssh_pump.c examples/ssh/pump.c examples/ssh/pump.h
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror tests/test_ssh_pump.c examples/ssh/pump.c -o $@
$(OUT): src/mouse_release.h

build/test_mouse_release: tests/test_mouse_release.c src/mouse_release.h src/session_queue.c src/session_queue.h
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $< src/session_queue.c -o $@
$(OUT): src/boxdraw.h

build/test_boxdraw: tests/test_boxdraw.c src/boxdraw.h
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $< -o $@

build/test_bracketed_paste: tests/test_bracketed_paste.c src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h src/session_reset.h $(UNICODE_SRC)
	@mkdir -p build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) $< src/screen_model.c src/hyperlink.c $(UNICODE_SRC) -o $@

build/test_resize_report: tests/test_resize_report.c src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h src/resize_report.h $(UNICODE_SRC)
	@mkdir -p build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) $< src/screen_model.c src/hyperlink.c $(UNICODE_SRC) -o $@

build/test_mouse: tests/test_mouse.c src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h $(UNICODE_SRC)
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) $< src/screen_model.c src/hyperlink.c $(UNICODE_SRC) -o $@

build/test_geometry_reply: tests/test_geometry_reply.c src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h $(UNICODE_SRC)
	@mkdir -p build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) $< src/screen_model.c src/hyperlink.c $(UNICODE_SRC) -o $@

build/test_theme: tests/test_theme.c src/theme.c src/theme.h src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h $(UNICODE_SRC)
	@mkdir -p build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) $< src/theme.c src/screen_model.c src/hyperlink.c $(UNICODE_SRC) -o $@

build/test_theme_aros: tests/test_theme.c src/theme.c src/theme.h src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h $(UNICODE_SRC)
	mkdir -p build
	$(AROS_GCC) $(CFLAGS) $< src/theme.c src/screen_model.c src/hyperlink.c $(UNICODE_SRC) $(LDFLAGS) -o $@

build/test_profile_prompt: tests/test_profile_prompt.c src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h $(UNICODE_SRC) Makefile
	@mkdir -p build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) $< src/screen_model.c src/hyperlink.c $(UNICODE_SRC) -o $@
package: build/test_status_area_aros

build/test_title_buffer: tests/test_title_buffer.c src/title_buffer.h Makefile
	mkdir -p $(dir $@)
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $< -o $@

build/test_status_area: tests/test_status_area.c src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h $(UNICODE_SRC) Makefile
	mkdir -p $(dir $@)
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) $< src/screen_model.c src/hyperlink.c $(UNICODE_SRC) -o $@

build/test_status_area_aros: tests/test_status_area.c src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h $(UNICODE_SRC) Makefile
	mkdir -p $(dir $@)
	$(AROS_GCC) $(CFLAGS) $< src/screen_model.c src/hyperlink.c $(UNICODE_SRC) $(LDFLAGS) -o $@

build/test_tui_menu: tests/test_tui_menu.c tools/tui_menu.c tools/tui_menu.h src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h $(UNICODE_SRC)
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) $< tools/tui_menu.c src/screen_model.c src/hyperlink.c $(UNICODE_SRC) -o $@

build/test_alternate: tests/test_alternate.c src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h $(UNICODE_SRC)
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) $< src/screen_model.c src/hyperlink.c $(UNICODE_SRC) -o $@

build/test_alternate_aros: tests/test_alternate.c src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h $(UNICODE_SRC)
	$(AROS_GCC) $(CFLAGS) $< src/screen_model.c src/hyperlink.c $(UNICODE_SRC) $(LDFLAGS) -o $@

build/test_aros_tui: tests/test_aros_tui.c src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h $(UNICODE_SRC) $(SESSION_SRC) $(SESSION_HEADERS)
	$(AROS_GCC) $(CFLAGS) $< src/screen_model.c src/hyperlink.c $(UNICODE_SRC) $(SESSION_SRC) $(LDFLAGS) -o $@

build/test_console_input: tests/test_console_input.c src/console_input.c src/session_queue.c $(SESSION_HEADERS)
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $< src/console_input.c src/session_queue.c -o $@

build/test_session_command: tests/test_session_command.c src/session_command.c src/session_command.h Makefile
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $< src/session_command.c -o $@

build/test_session_queue: tests/test_session_queue.c src/session_queue.c src/session_queue.h Makefile
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $< src/session_queue.c -o $@

build/session-peer: tools/session_peer.c tools/tui_menu.c tools/tui_menu.h tools/flood_peer.c tools/flood_peer.h tools/flood_clock.h tools/flood_protocol.c tools/flood_protocol.h Makefile
	$(AROS_GCC) $(CFLAGS) $< tools/tui_menu.c tools/flood_peer.c tools/flood_protocol.c $(LDFLAGS) -o $@

build/test_aros_session: tests/test_aros_session.c tests/session_test_options.h $(SESSION_SRC) $(SESSION_HEADERS) Makefile
	$(AROS_GCC) $(CFLAGS) $< $(SESSION_SRC) $(LDFLAGS) -o $@

build/console-contract-probe: tests/console_contract_probe.c Makefile
	mkdir -p build
	$(AROS_GCC) $(CFLAGS) $< $(LDFLAGS) -o $@

# Build-policy changes must not leave native tests at a different optimization
# level from the preview. OPTFLAGS=-O0 remains available for explicit diagnostics.
$(OUT) $(AROS_TEST) build/test_replay_aros build/test_graphemes_aros build/test_input_echo_aros build/test_aros_input build/test_selection_aros build/test_paste_aros build/test_aros_clip_job build/test_clipboard_text_aros build/test_cluster_font_aros build/test_font_aros build/font-probe-aros build/clip-probe: Makefile

build/bench-render-o0: tools/bench_render.c $(CLUSTER_SRC) src/cluster_font.h src/aros_font.h build/harfbuzz-aros.o Makefile
	$(AROS_GCC) $(CFLAGS) -O0 $< $(CLUSTER_SRC) build/harfbuzz-aros.o $(LDFLAGS) -lfreetype2 -lstdc++ -lm -o $@

build/bench-render-o2: tools/bench_render.c $(CLUSTER_SRC) src/cluster_font.h src/aros_font.h build/harfbuzz-aros.o Makefile
	$(AROS_GCC) $(CFLAGS) -O2 $< $(CLUSTER_SRC) build/harfbuzz-aros.o $(LDFLAGS) -lfreetype2 -lstdc++ -lm -o $@

build/test_dos_stream: tests/test_dos_stream.c Makefile
	$(AROS_GCC) $(CFLAGS) $< $(LDFLAGS) -o $@

build/test_selection: tests/test_selection.c src/selection.c src/selection.h src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h $(UNICODE_SRC)
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) $< src/selection.c src/screen_model.c src/hyperlink.c $(UNICODE_SRC) -o $@

build/test_selection_aros: tests/test_selection.c src/selection.c src/selection.h src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h $(UNICODE_SRC)
	$(AROS_GCC) $(CFLAGS) $< src/selection.c src/screen_model.c src/hyperlink.c $(UNICODE_SRC) $(LDFLAGS) -o $@

build/test_paste: tests/test_paste.c src/paste.c src/paste.h src/echo_session.c src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h $(UNICODE_SRC)
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) $< src/paste.c src/echo_session.c src/screen_model.c src/hyperlink.c $(UNICODE_SRC) -o $@

build/test_paste_aros: tests/test_paste.c src/paste.c src/paste.h src/echo_session.c src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h $(UNICODE_SRC)
	$(AROS_GCC) $(CFLAGS) $< src/paste.c src/echo_session.c src/screen_model.c src/hyperlink.c $(UNICODE_SRC) $(LDFLAGS) -o $@

build/test_aros_clip_job: tests/test_aros_clip_job.c src/aros_clip_job.c src/aros_clip_job.h src/aros_clipboard.c src/aros_clipboard.h
	$(AROS_GCC) $(CFLAGS) $< src/aros_clip_job.c src/aros_clipboard.c $(LDFLAGS) -o $@

build/test_clipboard_text_aros: tests/test_clipboard_text.c src/clipboard_text.c src/clipboard_text.h $(UNICODE_SRC)
	$(AROS_GCC) $(CFLAGS) $< src/clipboard_text.c $(UNICODE_SRC) $(LDFLAGS) -o $@

build/harfbuzz-host.o: $(HB_SRC) $(HB_HEADERS)
	$(HOST_CXX) $(HB_FLAGS) -c $(HB_SRC) -o $@

build/harfbuzz-aros.o: $(HB_SRC) $(HB_HEADERS)
	$(AROS_CXX) $(HB_FLAGS) --sysroot=$(AROS_SDK) -c $(HB_SRC) -o $@

build/test_cluster_font: tests/test_cluster_font.c $(CLUSTER_SRC) src/cluster_font.h src/aros_font.h build/harfbuzz-host.o
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(FONT_CFLAGS) -Ivendor/harfbuzz/src tests/test_cluster_font.c $(CLUSTER_SRC) build/harfbuzz-host.o $(FONT_LIBS) $(HOST_CXX_LIBS) -lm -o $@

build/test_cluster_font_aros: tests/test_cluster_font.c $(CLUSTER_SRC) src/cluster_font.h src/aros_font.h build/harfbuzz-aros.o
	$(AROS_GCC) $(CFLAGS) tests/test_cluster_font.c $(CLUSTER_SRC) build/harfbuzz-aros.o $(LDFLAGS) -lfreetype2 -lstdc++ -lm -o $@

build/test_cluster_bitmap: tests/test_cluster_bitmap.c $(CLUSTER_SRC) src/cluster_font.h src/aros_font.h build/harfbuzz-host.o
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(FONT_CFLAGS) -Ivendor/harfbuzz/src $< src/font_cache.c build/harfbuzz-host.o $(FONT_LIBS) $(HOST_CXX_LIBS) -lm -o $@

build/test_graphemes: tests/test_graphemes.c src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h $(UNICODE_SRC)
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) $< src/screen_model.c src/hyperlink.c $(UNICODE_SRC) -o $@

build/test_graphemes_aros: tests/test_graphemes.c src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h $(UNICODE_SRC)
	$(AROS_GCC) $(CFLAGS) $< src/screen_model.c src/hyperlink.c $(UNICODE_SRC) $(LDFLAGS) -o $@

build/test_input_echo: tests/test_input_echo.c $(INPUT_SRC) src/terminal_input.h src/echo_session.h
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $< $(INPUT_SRC) -o $@

build/test_clipboard_text: tests/test_clipboard_text.c src/clipboard_text.c src/clipboard_text.h $(UNICODE_SRC)
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) $< src/clipboard_text.c $(UNICODE_SRC) -o $@

build/test_clip_probe_args: tests/test_clip_probe_args.c tools/clip_probe.c src/clipboard_text.c src/clipboard_text.h $(UNICODE_SRC)
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) $< src/clipboard_text.c $(UNICODE_SRC) -o $@

build/test_aros_clipboard: tests/test_aros_clipboard.c src/aros_clipboard.c src/aros_clipboard.h
	$(AROS_GCC) $(CFLAGS) $< src/aros_clipboard.c $(LDFLAGS) -o $@

build/clip-probe: tools/clip_probe.c src/aros_clipboard.c src/clipboard_text.c src/aros_clipboard.h src/clipboard_text.h $(UNICODE_SRC)
	$(AROS_GCC) $(CFLAGS) $< src/aros_clipboard.c src/clipboard_text.c $(UNICODE_SRC) $(LDFLAGS) -o $@

build/test_input_echo_aros: tests/test_input_echo.c $(INPUT_SRC) src/terminal_input.h src/echo_session.h
	$(AROS_GCC) $(CFLAGS) $< $(INPUT_SRC) $(LDFLAGS) -o $@

build/test_aros_input: tests/test_aros_input.c src/aros_input.c src/aros_input.h $(INPUT_SRC)
	$(AROS_GCC) $(CFLAGS) $< src/aros_input.c $(INPUT_SRC) $(LDFLAGS) -o $@

build/font-probe: tests/font_probe.c
	mkdir -p build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(FONT_CFLAGS) $< $(FONT_LIBS) -o $@

build/font-probe-aros: tests/font_probe.c
	mkdir -p build
	$(AROS_GCC) $(CFLAGS) $< $(LDFLAGS) -lfreetype2 -o $@

build/test_font: $(FONT_SOURCES)
	mkdir -p build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(FONT_CFLAGS) tests/test_font_fallback.c src/font_cache.c $(FONT_LIBS) -o $@

build/test_font_aros: $(FONT_SOURCES)
	mkdir -p build
	$(AROS_GCC) $(CFLAGS) tests/test_font_fallback.c src/font_cache.c $(LDFLAGS) -lfreetype2 -o $@

.PHONY: test-font package
# The same font tests with memory faces (XTERM_FONTS_IN_MEMORY, used by the
# ABIv11 DOS handler worker): whole files read first, then FT_New_Memory_Face.
test-font-memory: $(FONT_SOURCES) src/cluster_font.c build/harfbuzz-host.o
	mkdir -p build/memfont
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror -DXTERM_FONTS_IN_MEMORY $(FONT_CFLAGS) tests/test_font_fallback.c src/font_cache.c $(FONT_LIBS) -o build/memfont/test_font
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror -DXTERM_FONTS_IN_MEMORY $(FONT_CFLAGS) -Ivendor/harfbuzz/src tests/test_cluster_font.c src/cluster_font.c src/font_cache.c build/harfbuzz-host.o $(FONT_LIBS) $(HOST_CXX_LIBS) -lm -o build/memfont/test_cluster_font
	./build/memfont/test_font "$(FONT_PRIMARY)" "$(FONT_SECONDARY)" $(FONT_ASSETS)/NotoEmoji.ttf $(FONT_ASSETS)/Unifont.otf
	./build/memfont/test_cluster_font "$(FONT_PRIMARY)" "$(FONT_SECONDARY)" $(FONT_ASSETS)/NotoEmoji.ttf $(FONT_ASSETS)/Unifont.otf $(FONT_ASSETS)/NotoColorEmoji.ttf

test-font: build/test_font build/test_cluster_font build/test_cluster_bitmap
	./build/test_font "$(FONT_PRIMARY)" "$(FONT_SECONDARY)" $(FONT_ASSETS)/NotoEmoji.ttf $(FONT_ASSETS)/Unifont.otf
	./build/test_cluster_font "$(FONT_PRIMARY)" "$(FONT_SECONDARY)" $(FONT_ASSETS)/NotoEmoji.ttf $(FONT_ASSETS)/Unifont.otf $(FONT_ASSETS)/NotoColorEmoji.ttf
	./build/test_cluster_bitmap

# Every cluster of a fixed corpus as the renderer draws it, compared with the
# accepted output. Run before and after updating HarfBuzz or the fonts.
build/test_shaping_corpus: tests/test_shaping_corpus.c tests/shaping_corpus.h $(CLUSTER_SRC) src/cluster_font.h src/aros_font.h src/screen_model.c src/hyperlink.c src/screen_model.h build/harfbuzz-host.o
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(FONT_CFLAGS) $(UNICODE_FLAGS) -Ivendor/harfbuzz/src $< $(CLUSTER_SRC) src/screen_model.c src/hyperlink.c $(UNICODE_SRC) build/harfbuzz-host.o $(FONT_LIBS) $(HOST_CXX_LIBS) -lm -o $@

test-shaping-corpus: build/test_shaping_corpus
	./build/test_shaping_corpus "$(FONT_PRIMARY)" "$(FONT_SECONDARY)" $(FONT_ASSETS)/NotoEmoji.ttf $(FONT_ASSETS)/Unifont.otf $(FONT_ASSETS)/NotoColorEmoji.ttf > build/shaping-corpus.out
	diff -u tests/shaping-corpus.expected build/shaping-corpus.out

package: $(OUT) build/font-probe-aros build/test_font_aros build/test_cluster_font_aros build/test_graphemes_aros build/test_input_echo_aros build/test_aros_input $(AROS_TEST) build/test_replay_aros build/test_selection_aros build/test_paste_aros build/test_aros_clip_job build/test_clipboard_text_aros build/clip-probe
	mkdir -p build/aros-xterm-demo/Fonts
	cp build/session-peer build/test_aros_session build/aros-xterm-demo/
	cp build/geometry-probe build/aros-xterm-demo/
	cp build/link-demo build/aros-xterm-demo/
	cp build/test_alternate_aros build/test_aros_tui build/aros-xterm-demo/
	cp build/test_status_area_aros build/aros-xterm-demo/
	cp build/test_line_editor_aros build/test_line_view_aros build/aros-xterm-demo/
	cp build/test_completion_aros build/test_aros_completion build/aros-xterm-demo/
	cp build/test_shell_startup build/aros-xterm-demo/
	cp build/test_aros_endpoint build/aros-xterm-demo/
	cp build/xterm-handler build/aros-xterm-worker build/aros-xterm-demo/
	mkdir -p build/aros-xterm-demo/examples
	cp examples/Shell-Startup build/aros-xterm-demo/examples/
	cp examples/XTerm-Startup build/aros-xterm-demo/examples/
	cp examples/Background-Writer build/aros-xterm-demo/examples/
	cp examples/XTERM.mountlist build/aros-xterm-demo/examples/
	mkdir -p build/aros-xterm-demo/themes
	cp themes/*.theme build/aros-xterm-demo/themes/
	cp $(OUT) build/aros-xterm-demo/aros-xterm
	cp build/font-probe-aros build/aros-xterm-demo/font-probe
	cp build/test_font_aros build/aros-xterm-demo/test-font
	cp build/test_cluster_font_aros build/aros-xterm-demo/test-cluster-font
	cp build/test_graphemes_aros build/aros-xterm-demo/test-graphemes
	cp build/test_input_echo_aros build/aros-xterm-demo/test-input-echo
	cp build/test_aros_input build/aros-xterm-demo/test-aros-input
	cp $(AROS_TEST) build/aros-xterm-demo/test-model
	cp build/test_replay_aros build/aros-xterm-demo/test-replay
	cp build/test_selection_aros build/aros-xterm-demo/test-selection
	cp build/test_paste_aros build/aros-xterm-demo/test-paste
	cp build/test_aros_clip_job build/aros-xterm-demo/test-clip-job
	cp build/test_clipboard_text_aros build/aros-xterm-demo/test-clip-codec
	cp build/clip-probe build/aros-xterm-demo/clip-probe
	cp $(FONT_ASSETS)/Unifont.otf $(FONT_ASSETS)/NotoEmoji.ttf $(FONT_ASSETS)/NotoEmoji-OFL.txt $(FONT_ASSETS)/COPYING $(FONT_ASSETS)/OFL-1.1.txt $(FONT_ASSETS)/README.md build/aros-xterm-demo/Fonts/
	cp data/unicode-17.0/LICENSE.txt build/aros-xterm-demo/UNICODE-LICENSE.txt
	cp README.md build/aros-xterm-demo/README.md
	cp $(FONT_ASSETS)/NotoColorEmoji.ttf $(FONT_ASSETS)/NotoColorEmoji-LICENSE.txt build/aros-xterm-demo/Fonts/
	cp vendor/harfbuzz/COPYING build/aros-xterm-demo/HARFBUZZ-LICENSE.txt
	cp vendor/utf8proc/LICENSE.md build/aros-xterm-demo/UTF8PROC-LICENSE.txt
	cp build/native-config.json build/aros-xterm-demo/BUILD-CONFIG.json

.PHONY: test clean FORCE
# The generated table is checked in; ordinary builds never fetch Unicode data.
$(OUT) $(HOST_TEST) $(AROS_TEST) build/test_replay build/test_replay_aros: $(WIDTH_TABLE) $(UNICODE_SRC)
FORCE:

# Revalidate file-backed inputs too, including removed files or changed paths.
# The generator preserves the header timestamp when its contents are unchanged.
$(REPLAY_HEADER): tools/check_fixtures.py $(FIXTURES) tests/replay_case.h FORCE
	mkdir -p $(dir $@)
	$(PYTHON) tools/check_fixtures.py fixtures/manifest.json --generate $@

build/test_replay: tests/test_replay.c tests/replay_case.h src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h $(REPLAY_HEADER)
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) -Itests -Ibuild tests/test_replay.c src/screen_model.c src/hyperlink.c $(UNICODE_SRC) -o $@

build/test_replay_aros: tests/test_replay.c tests/replay_case.h src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h $(REPLAY_HEADER)
	$(AROS_GCC) $(CFLAGS) -Itests -Ibuild tests/test_replay.c src/screen_model.c src/hyperlink.c $(UNICODE_SRC) $(LDFLAGS) -o $@

$(AROS_TEST): tests/test_screen_model.c src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h
	mkdir -p $(dir $@)
	$(AROS_GCC) $(CFLAGS) -o $@ tests/test_screen_model.c src/screen_model.c src/hyperlink.c $(UNICODE_SRC) $(LDFLAGS)

test: build/test_kitty_graphics
build/test_kitty_graphics: tests/test_kitty_graphics.c src/kitty_graphics.c src/kitty_graphics.h src/screen_model.c src/screen_model.h src/hyperlink.c $(UNICODE_SRC) Makefile
	@mkdir -p build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) $< src/kitty_graphics.c src/screen_model.c src/hyperlink.c $(UNICODE_SRC) -o $@

$(HOST_TEST): tests/test_screen_model.c src/screen_model.c src/hyperlink.c src/screen_model.h src/hyperlink.h
	mkdir -p $(dir $(HOST_TEST))
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) -Isrc tests/test_screen_model.c src/screen_model.c src/hyperlink.c $(UNICODE_SRC) -o $@

test: $(HOST_TEST) build/test_replay build/test_graphemes build/test_input_echo build/test_clipboard_text build/test_clip_probe_args build/test_selection build/test_paste
	./$(HOST_TEST)
	./build/test_replay
	./build/test_graphemes
	./build/test_input_echo
	./build/test_clipboard_text
	./build/test_selection
	./build/test_paste
	./build/test_session_queue
	./build/test_console_input
	./build/test_line_editor
	./build/test_completion
	./build/test_file_drop
	./build/test_line_view
	./build/test_alternate
	./build/test_tui_menu
	./build/test_status_area
	./build/test_title_buffer
	./build/test_osc_title
	./build/test_window_title
	./build/test_profile_prompt
	./build/test_theme
	./build/test_geometry_reply
	./build/test_mouse
	./build/test_boxdraw
	./build/test_mouse_release
	./build/test_ssh_pump
	./build/test_hyperlink
	./build/test_link_gesture
	./build/test_paint_damage
	./build/test_session_command
	./build/test_session_test_options
	./build/test_control_strings
	./build/test_fuzz_vt_smoke
	./build/test_key_queue
	./build/test_cursor_queries
	./build/test_reply_flow
	./build/test_tty_pair
	./build/test_tty_service
	./build/test_tty_control
	./build/test_clip_probe_args
	./build/test_kitty_graphics
	PYTHONDONTWRITEBYTECODE=1 $(PYTHON) -m unittest discover -s tests -v
	$(PYTHON) tools/check_fixtures.py fixtures/manifest.json

clean:
	rm -f $(OUT) $(HOST_TEST) $(AROS_TEST) build/test_replay build/test_replay_aros $(REPLAY_HEADER)
	rm -f build/font-probe build/font-probe-aros build/test_font build/test_font_aros

$(OUT): src/paint_damage.h
test: build/test_paint_damage
build/test_paint_damage: tests/test_paint_damage.c src/paint_damage.c src/paint_damage.h src/screen_model.h
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $< src/paint_damage.c -o $@

build/test_flood_protocol: tests/test_flood_protocol.c tools/flood_protocol.c tools/flood_protocol.h
	@mkdir -p build
	$(HOST_CC) -O2 -std=c99 -Wall -Wextra -Werror tests/test_flood_protocol.c tools/flood_protocol.c -o $@
.PHONY: test-flood
test-flood: build/test_flood_protocol
	./build/test_flood_protocol
test: test-flood

build/bench-flood: tools/bench_flood.c tools/flood_protocol.c tools/flood_protocol.h tools/flood_clock.h tools/flood_diag.h $(OUT) Makefile
	$(AROS_GCC) $(CFLAGS) -o $@ tools/bench_flood.c tools/flood_protocol.c $(HYPERLINK_GUI_SRC) $(THEME_SRC) src/aros_font.c $(CLUSTER_SRC) src/screen_model.c src/hyperlink.c $(UNICODE_SRC) $(INPUT_SRC) $(EDITOR_SRC) src/aros_drop.c $(COMPLETION_SRC) src/aros_input.c $(CLIP_GUI_SRC) $(SESSION_SRC) build/harfbuzz-aros.o $(LDFLAGS) -lfreetype2 -lstdc++ -lcybergraphics -lm

$(OUT): tools/flood_diag.h
.PHONY: package-flood
package-flood: build/bench-flood build/session-peer
	mkdir -p build/flood-diagnostic/Fonts
	cp build/bench-flood build/session-peer build/flood-diagnostic/
	cp $(FONT_ASSETS)/Unifont.otf $(FONT_ASSETS)/NotoEmoji.ttf $(FONT_ASSETS)/NotoColorEmoji.ttf build/flood-diagnostic/Fonts/
	cp $(FONT_ASSETS)/NotoEmoji-OFL.txt $(FONT_ASSETS)/COPYING $(FONT_ASSETS)/OFL-1.1.txt $(FONT_ASSETS)/NotoColorEmoji-LICENSE.txt build/flood-diagnostic/Fonts/
	cp build/native-config.json build/flood-diagnostic/BUILD-CONFIG.json

build/bench-window: tools/bench_window.c $(OUT) Makefile
	$(AROS_GCC) $(CFLAGS) -o $@ tools/bench_window.c $(HYPERLINK_GUI_SRC) $(THEME_SRC) src/aros_font.c $(CLUSTER_SRC) src/screen_model.c src/hyperlink.c $(UNICODE_SRC) $(INPUT_SRC) $(EDITOR_SRC) src/aros_drop.c $(COMPLETION_SRC) src/aros_input.c $(CLIP_GUI_SRC) $(SESSION_SRC) build/harfbuzz-aros.o $(LDFLAGS) -lfreetype2 -lstdc++ -lcybergraphics -lm

build/test-keyboard-aros: tests/test_keyboard_aros.c $(OUT) Makefile
	$(AROS_GCC) $(CFLAGS) -o $@ $< $(HYPERLINK_GUI_SRC) $(THEME_SRC) src/aros_font.c $(CLUSTER_SRC) src/screen_model.c src/hyperlink.c $(UNICODE_SRC) $(INPUT_SRC) $(EDITOR_SRC) src/aros_drop.c $(COMPLETION_SRC) src/aros_input.c $(CLIP_GUI_SRC) $(SESSION_SRC) build/harfbuzz-aros.o $(LDFLAGS) -lfreetype2 -lstdc++ -lcybergraphics -lm

build/test-reply-flow-aros: tests/test_reply_flow_aros.c $(OUT) Makefile
	$(AROS_GCC) $(CFLAGS) -o $@ $< $(HYPERLINK_GUI_SRC) $(THEME_SRC) src/aros_font.c $(CLUSTER_SRC) src/screen_model.c src/hyperlink.c $(UNICODE_SRC) $(INPUT_SRC) $(EDITOR_SRC) src/aros_drop.c $(COMPLETION_SRC) src/aros_input.c $(CLIP_GUI_SRC) $(SESSION_SRC) build/harfbuzz-aros.o $(LDFLAGS) -lfreetype2 -lstdc++ -lcybergraphics -lm
package: package-reply-flow
.PHONY: package-reply-flow
package-reply-flow: build/test-reply-flow-aros
	@mkdir -p build/aros-xterm-demo
	cp build/test-reply-flow-aros build/aros-xterm-demo/

package: package-keyboard
.PHONY: package-keyboard
package-keyboard: build/test-keyboard-aros
	@mkdir -p build/aros-xterm-demo
	cp build/test-keyboard-aros build/aros-xterm-demo/

package: package-render-probe
.PHONY: package-render-probe
package-render-probe: build/bench-window
	@mkdir -p build/aros-xterm-demo
	cp build/bench-window build/aros-xterm-demo/

# Optional read-only diagnostic; no URL launch or preference mutation.
build/openurl-probe: tools/openurl_probe.c Makefile
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) $< $(LDFLAGS) -o $@

package: package-openurl-probe
.PHONY: package-openurl-probe
package-openurl-probe: build/openurl-probe
	@mkdir -p build/aros-xterm-demo
	cp build/openurl-probe build/aros-xterm-demo/

test: build/test_control_strings

FUZZ_CC ?= clang
.PHONY: fuzz-vt
fuzz-vt: build/fuzz_vt

build/fuzz_vt: tests/fuzz_vt.c src/screen_model.c src/screen_model.h src/hyperlink.c src/hyperlink.h $(UNICODE_SRC) Makefile
	@mkdir -p build
	$(FUZZ_CC) -std=c99 -O1 -g -Wall -Wextra -Werror -UNDEBUG -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer $(UNICODE_FLAGS) tests/fuzz_vt.c src/screen_model.c src/hyperlink.c $(UNICODE_SRC) -o $@

test: build/test_fuzz_vt_smoke
build/test_fuzz_vt_smoke: tests/fuzz_vt.c src/screen_model.c src/screen_model.h src/hyperlink.c src/hyperlink.h $(UNICODE_SRC) Makefile
	@mkdir -p build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror -DVT_FUZZ_SMOKE $(UNICODE_FLAGS) tests/fuzz_vt.c src/screen_model.c src/hyperlink.c $(UNICODE_SRC) -o $@

.PHONY: test-control-strings-sanitize
test-control-strings-sanitize: build/test_control_strings_sanitize
	./build/test_control_strings_sanitize

build/test_control_strings_sanitize: tests/test_control_strings.c src/screen_model.c src/screen_model.h src/hyperlink.c src/hyperlink.h $(UNICODE_SRC) Makefile
	@mkdir -p build
	$(HOST_CC) -std=c99 -O1 -g -Wall -Wextra -Werror -UNDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer $(UNICODE_FLAGS) tests/test_control_strings.c src/screen_model.c src/hyperlink.c $(UNICODE_SRC) -o $@
test: build/test_key_queue
$(OUT): src/key_queue.h
build/test_key_queue: tests/test_key_queue.c src/key_queue.c src/key_queue.h src/line_editor.c src/line_editor.h $(UNICODE_SRC)
	@mkdir -p build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) $< src/key_queue.c src/line_editor.c $(UNICODE_SRC) -o $@
build/test_control_strings: tests/test_control_strings.c src/screen_model.c src/screen_model.h src/hyperlink.c src/hyperlink.h $(UNICODE_SRC) Makefile
	@mkdir -p build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) $< src/screen_model.c src/hyperlink.c $(UNICODE_SRC) -o $@

build/test_control_strings_aros: tests/test_control_strings.c src/screen_model.c src/screen_model.h src/hyperlink.c src/hyperlink.h $(UNICODE_SRC) Makefile
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) $< src/screen_model.c src/hyperlink.c $(UNICODE_SRC) $(LDFLAGS) -o $@

package: package-control-strings
.PHONY: package-control-strings
package-control-strings: build/test_control_strings_aros
	@mkdir -p build/aros-xterm-demo
	cp build/test_control_strings_aros build/aros-xterm-demo/test-control-strings

test: build/test_cursor_queries
build/test_cursor_queries: tests/test_cursor_queries.c src/screen_model.c src/screen_model.h src/hyperlink.c src/hyperlink.h $(UNICODE_SRC) Makefile
	@mkdir -p build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) $< src/screen_model.c src/hyperlink.c $(UNICODE_SRC) -o $@
build/test_cursor_queries_aros: tests/test_cursor_queries.c src/screen_model.c src/screen_model.h src/hyperlink.c src/hyperlink.h $(UNICODE_SRC) Makefile
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) $< src/screen_model.c src/hyperlink.c $(UNICODE_SRC) $(LDFLAGS) -o $@
package: package-cursor-queries
.PHONY: package-cursor-queries
package-cursor-queries: build/test_cursor_queries_aros build/test-vt-reply
	@mkdir -p build/aros-xterm-demo
	cp build/test_cursor_queries_aros build/aros-xterm-demo/test-cursor-queries
	cp build/test-vt-reply build/aros-xterm-demo/
build/test-vt-reply: tests/test_vt_reply_aros.c Makefile
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) $< $(LDFLAGS) -o $@

$(OUT): src/reply_flow.h
test: build/test_reply_flow
build/test_reply_flow: tests/test_reply_flow.c src/reply_flow.c src/reply_flow.h src/session_queue.c src/screen_model.c src/screen_model.h src/hyperlink.c $(UNICODE_SRC)
	@mkdir -p build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $(UNICODE_FLAGS) $< src/reply_flow.c src/session_queue.c src/screen_model.c src/hyperlink.c $(UNICODE_SRC) -o $@

TTY_PAIR_TEST_SRC = tests/test_tty_pair.c src/tty_pair.c
build/tty_fd_consumer_stock: tests/tty_fd_consumer.c Makefile
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) $< $(LDFLAGS) -o $@
TTY_FD_BRIDGE_SRC = src/aros_tty_fd_bridge.c src/aros_tty_termios.c src/aros_tty_client.c src/tty_termios.c
TTY_FD_BRIDGE_FLAGS = -Wl,--wrap=tcgetattr -Wl,--wrap=tcsetattr
TTY_FD_IO_FLAGS = $(TTY_FD_BRIDGE_FLAGS) -Wl,--wrap=read -Wl,--wrap=write
TTY_FD_IO_SRC = $(TTY_FD_BRIDGE_SRC) src/aros_tty_fd_io.c
TTY_FD_IO_HEADERS = src/aros_tty_fd_bridge.h src/aros_tty_client.h src/aros_tty_termios.h src/tty_termios.h src/tty_control.h
.PHONY: test-tty-fd-io test-tty-fd-io-sanitize
test: test-tty-fd-io
test-tty-fd-io: build/test_tty_fd_io build/test_tty_fd_try_flow
	./build/test_tty_fd_io
	./build/test_tty_fd_try_flow
test-tty-fd-io-sanitize: build/test_tty_fd_io_sanitize build/test_tty_fd_try_flow_sanitize
	./build/test_tty_fd_io_sanitize
	./build/test_tty_fd_try_flow_sanitize
TTY_FD_TRY_FLOW_SRC = tests/test_tty_fd_try_flow.c src/aros_tty_fd_io.c src/tty_service.c src/tty_pair.c
build/test_tty_fd_try_flow: $(TTY_FD_TRY_FLOW_SRC) $(TTY_FD_IO_HEADERS) tests/fd_io/dos/dos.h src/tty_service.h src/tty_pair.h Makefile
	@mkdir -p build
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Werror -UNDEBUG -Itests/fd_io -Dfcntl=fd_test_fcntl $(TTY_FD_TRY_FLOW_SRC) -o $@
build/test_tty_fd_try_flow_sanitize: $(TTY_FD_TRY_FLOW_SRC) $(TTY_FD_IO_HEADERS) tests/fd_io/dos/dos.h src/tty_service.h src/tty_pair.h Makefile
	@mkdir -p build
	$(HOST_CC) -std=c99 -O1 -g -Wall -Wextra -Werror -UNDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer -Itests/fd_io -Dfcntl=fd_test_fcntl $(TTY_FD_TRY_FLOW_SRC) -o $@
TTY_FD_IO_TEST_SRC = tests/test_tty_fd_io.c src/aros_tty_fd_io.c
TTY_FD_IO_TEST_HEADERS = $(TTY_FD_IO_HEADERS) tests/fd_io/dos/dos.h tests/fd_io/proto/dos.h
build/test_tty_fd_io: $(TTY_FD_IO_TEST_SRC) $(TTY_FD_IO_TEST_HEADERS) Makefile
	@mkdir -p build
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Werror -UNDEBUG -Itests/fd_io -Dfcntl=fd_test_fcntl $(TTY_FD_IO_TEST_SRC) -o $@
build/test_tty_fd_io_sanitize: $(TTY_FD_IO_TEST_SRC) $(TTY_FD_IO_TEST_HEADERS) Makefile
	@mkdir -p build
	$(HOST_CC) -std=c99 -O1 -g -Wall -Wextra -Werror -UNDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer -Itests/fd_io -Dfcntl=fd_test_fcntl $(TTY_FD_IO_TEST_SRC) -o $@
build/tty_fd_read_break: tests/tty_fd_break_peer.c $(TTY_FD_IO_SRC) $(TTY_FD_IO_HEADERS) Makefile
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) $(filter %.c,$^) $(LDFLAGS) $(TTY_FD_IO_FLAGS) -o $@
build/tty_fd_write_break: tests/tty_fd_break_peer.c $(TTY_FD_IO_SRC) $(TTY_FD_IO_HEADERS) Makefile
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) -DXTY_WRITE_PROBE $(filter %.c,$^) $(LDFLAGS) $(TTY_FD_IO_FLAGS) -o $@
build/tty_fd_io_session: tests/tty_fd_session.c $(TTY_FD_IO_SRC) $(TTY_FD_IO_HEADERS) Makefile
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) $(filter %.c,$^) $(LDFLAGS) $(TTY_FD_IO_FLAGS) -o $@
build/test_aros_fd_flags: tests/test_aros_fd_flags.c Makefile
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) $< $(LDFLAGS) -o $@
build/tty_fd_read_stock: tests/tty_fd_break_peer.c Makefile
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) $< $(LDFLAGS) -o $@
build/tty_fd_write_stock: tests/tty_fd_break_peer.c Makefile
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) -DXTY_WRITE_PROBE $< $(LDFLAGS) -o $@
build/test_aros_tty_fd_break: tests/test_aros_tty_fd_break.c src/aros_tty_packets.c src/aros_tty_signal.c src/aros_break.c src/tty_service.c src/tty_pair.c src/tty_control.c Makefile
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) $(filter %.c,$^) $(LDFLAGS) -o $@
build/tty_fd_session: tests/tty_fd_session.c $(TTY_FD_BRIDGE_SRC) src/tty_control.h Makefile
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) $(filter %.c,$^) $(LDFLAGS) $(TTY_FD_BRIDGE_FLAGS) -o $@
# Experimental runtime only: real libc fcntl/read/write, termios bridge retained.
build/tty_fd_libc_session: tests/tty_fd_session.c $(TTY_FD_BRIDGE_SRC) src/tty_control.h Makefile build/native-config.json
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) -DTEST_LIBC_NONBLOCK $(filter %.c,$^) $(LDFLAGS) $(TTY_FD_BRIDGE_FLAGS) -o $@
build/tty_fd_pressure_peer: tests/tty_fd_pressure_peer.c Makefile build/native-config.json
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) $< $(LDFLAGS) -o $@
build/tty_libc_geometry_session: tests/tty_fd_session.c $(TTY_FD_BRIDGE_SRC) src/tty_control.h Makefile build/native-config.json
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) -DTEST_LIBC_NONBLOCK -DTEST_LIBC_GEOMETRY $(filter %.c,$^) $(LDFLAGS) $(TTY_FD_BRIDGE_FLAGS) -o $@
build/test_aros_tty_libc_geometry: tests/test_aros_tty_fd_session.c src/aros_tty_worker.c src/aros_tty_packets.c src/aros_tty_signal.c src/aros_break.c src/tty_service.c src/tty_pair.c src/tty_control.c Makefile build/native-config.json
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) -DTEST_LIBC_GEOMETRY $(filter %.c,$^) $(LDFLAGS) -o $@
# Experimental runtime only; no wrapper or private client linked into the child.
build/tty_libc_native_session: tests/tty_fd_session.c Makefile build/native-config.json
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) -DTEST_LIBC_NONBLOCK -DTEST_LIBC_GEOMETRY $< $(LDFLAGS) -o $@
build/test_aros_tty_control_reply: tests/test_aros_tty_control_reply.c src/aros_tty_client.c src/aros_tty_client.h src/tty_control.h Makefile build/native-config.json
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) $(filter %.c,$^) $(LDFLAGS) -o $@
build/test_aros_tty_fd_pressure: tests/test_aros_tty_fd_pressure.c src/aros_tty_packets.c src/aros_tty_signal.c src/aros_break.c src/tty_service.c src/tty_pair.c src/tty_control.c Makefile build/native-config.json
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) $(filter %.c,$^) $(LDFLAGS) -o $@
build/test_aros_tty_libc_break: tests/test_aros_tty_fd_break.c src/aros_tty_packets.c src/aros_tty_signal.c src/aros_break.c src/tty_service.c src/tty_pair.c src/tty_control.c Makefile build/native-config.json
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) -DTEST_LIBC_NONBLOCK $(filter %.c,$^) $(LDFLAGS) -o $@
build/tty_libc_read_break: tests/tty_fd_break_peer.c Makefile build/native-config.json
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) -DTEST_LIBC_NONBLOCK $< $(LDFLAGS) -o $@
build/tty_libc_write_break: tests/tty_fd_break_peer.c Makefile build/native-config.json
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) -DTEST_LIBC_NONBLOCK -DXTY_WRITE_PROBE $< $(LDFLAGS) -o $@
build/test_aros_tty_fd_session: tests/test_aros_tty_fd_session.c src/aros_tty_worker.c src/aros_tty_packets.c src/aros_tty_signal.c src/aros_break.c src/tty_service.c src/tty_pair.c src/tty_control.c Makefile
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) $(filter %.c,$^) $(LDFLAGS) -o $@
build/tty_fd_consumer: tests/tty_fd_consumer.c $(TTY_FD_BRIDGE_SRC) src/tty_control.h Makefile
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) $(filter %.c,$^) $(LDFLAGS) $(TTY_FD_BRIDGE_FLAGS) -o $@
build/test_fd_bridge_legacy: tests/test_libc_terminal_aros.c $(TTY_FD_BRIDGE_SRC) src/tty_control.h Makefile
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) $(filter %.c,$^) $(LDFLAGS) $(TTY_FD_BRIDGE_FLAGS) -o $@
build/test_aros_tty_fd: tests/test_aros_tty_fd.c src/aros_tty_worker.c src/aros_tty_packets.c src/aros_tty_signal.c src/aros_break.c src/tty_service.c src/tty_pair.c src/tty_control.c Makefile
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) $(filter %.c,$^) $(LDFLAGS) -o $@
TTY_SIGNAL_WORKER_TARGETS = build/test_aros_tty_worker build/test_aros_tty_termios build/test_aros_tty_read_timing build/test_aros_tty_control_child build/test_aros_tty_child build/test_aros_tty_wait build/test_aros_tty_controls build/test_aros_tty_signal_worker
$(TTY_SIGNAL_WORKER_TARGETS): src/aros_tty_signal.c src/aros_tty_signal.h src/aros_break.c src/aros_break.h
$(TTY_SIGNAL_WORKER_TARGETS): LDFLAGS += src/aros_tty_signal.c src/aros_break.c
TTY_SIGNAL_TEST_SRC = tests/test_aros_tty_signal.c src/aros_tty_signal.c src/aros_break.c src/aros_tty_packets.c src/aros_tty_client.c src/aros_tty_termios.c src/tty_termios.c src/tty_control.c src/tty_service.c src/tty_pair.c
TTY_SIGNAL_WORKER_SRC = tests/test_aros_tty_signal_worker.c src/aros_tty_worker.c src/aros_tty_packets.c src/aros_tty_client.c src/aros_tty_termios.c src/tty_termios.c src/tty_control.c src/tty_service.c src/tty_pair.c
build/test_aros_tty_signal_worker: $(TTY_SIGNAL_WORKER_SRC) src/aros_tty_worker.h src/aros_tty_packets.h src/tty_service.h src/tty_pair.h src/tty_control.h Makefile
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) $(TTY_SIGNAL_WORKER_SRC) $(LDFLAGS) -o $@
build/test_aros_tty_signal: $(TTY_SIGNAL_TEST_SRC) src/aros_tty_signal.h src/aros_break.h src/aros_tty_packets.h src/tty_service.h src/tty_pair.h Makefile
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) $(TTY_SIGNAL_TEST_SRC) $(LDFLAGS) -o $@
.PHONY: test-tty-pair test-tty-pair-sanitize
test: build/test_tty_pair
test-tty-pair: build/test_tty_pair
	./build/test_tty_pair
test-tty-pair-sanitize: build/test_tty_pair_sanitize
	./build/test_tty_pair_sanitize
build/test_tty_pair: $(TTY_PAIR_TEST_SRC) src/tty_pair.h Makefile
	@mkdir -p build
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Werror -UNDEBUG $(TTY_PAIR_TEST_SRC) -o $@
build/test_tty_pair_sanitize: $(TTY_PAIR_TEST_SRC) src/tty_pair.h Makefile
	@mkdir -p build
	$(HOST_CC) -std=c99 -O1 -g -Wall -Wextra -Werror -UNDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer $(TTY_PAIR_TEST_SRC) -o $@
build/test_tty_pair_aros: $(TTY_PAIR_TEST_SRC) src/tty_pair.h Makefile
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) -UNDEBUG $(TTY_PAIR_TEST_SRC) $(LDFLAGS) -o $@

TTY_SERVICE_TEST_SRC = tests/test_tty_service.c src/tty_service.c src/tty_pair.c
TTY_TRY_TEST_SRC = tests/test_tty_try_io.c src/tty_service.c src/tty_pair.c
TTY_TRY_WIRE_SRC = tests/test_aros_tty_try.c src/aros_tty_client.c src/aros_tty_worker.c src/aros_tty_packets.c src/aros_tty_signal.c src/aros_break.c src/tty_service.c src/tty_pair.c src/tty_control.c
build/test_aros_tty_try: $(TTY_TRY_WIRE_SRC) src/aros_tty_client.h src/aros_tty_worker.h src/aros_tty_packets.h src/tty_control.h src/tty_service.h Makefile
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) $(TTY_TRY_WIRE_SRC) $(LDFLAGS) -o $@
.PHONY: test-tty-try-io test-tty-try-io-sanitize
test: test-tty-try-io
test-tty-try-io: build/test_tty_try_io
	./build/test_tty_try_io
test-tty-try-io-sanitize: build/test_tty_try_io_sanitize
	./build/test_tty_try_io_sanitize
build/test_tty_try_io: $(TTY_TRY_TEST_SRC) src/tty_service.h src/tty_pair.h Makefile
	@mkdir -p build
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Werror -UNDEBUG $(TTY_TRY_TEST_SRC) -o $@
build/test_tty_try_io_sanitize: $(TTY_TRY_TEST_SRC) src/tty_service.h src/tty_pair.h Makefile
	@mkdir -p build
	$(HOST_CC) -std=c99 -O1 -g -Wall -Wextra -Werror -UNDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer $(TTY_TRY_TEST_SRC) -o $@
build/test_tty_try_io_aros: $(TTY_TRY_TEST_SRC) src/tty_service.h src/tty_pair.h Makefile
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) -UNDEBUG $(TTY_TRY_TEST_SRC) $(LDFLAGS) -o $@
.PHONY: test-tty-service test-tty-service-sanitize
test: build/test_tty_service
test-tty-service: build/test_tty_service
	./build/test_tty_service
test-tty-service-sanitize: build/test_tty_service_sanitize
	./build/test_tty_service_sanitize
build/test_tty_service: $(TTY_SERVICE_TEST_SRC) src/tty_service.h src/tty_pair.h Makefile
	@mkdir -p build
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Werror -UNDEBUG $(TTY_SERVICE_TEST_SRC) -o $@
build/test_tty_service_sanitize: $(TTY_SERVICE_TEST_SRC) src/tty_service.h src/tty_pair.h Makefile
	@mkdir -p build
	$(HOST_CC) -std=c99 -O1 -g -Wall -Wextra -Werror -UNDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer $(TTY_SERVICE_TEST_SRC) -o $@
build/test_tty_service_aros: $(TTY_SERVICE_TEST_SRC) src/tty_service.h src/tty_pair.h Makefile
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) -UNDEBUG $(TTY_SERVICE_TEST_SRC) $(LDFLAGS) -o $@

# Kitty graphics producers (docs/kitty-graphics-20260928.md): terminal I/O only.
# Memory-measurement control (docs/release-resources-20260929.md).
build/leak-control-min build/leak-control-stdio build/leak-control-posix build/leak-control-printf: build/leak-control-%: tools/leak_control.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) -DLEAK_CONTROL_$(shell echo $* | tr a-z A-Z) $< $(LDFLAGS) -o $@
build/leak-open: tools/leak_open.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) $< $(LDFLAGS) -o $@
build/leak-ft: tools/leak_ft.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) $< $(LDFLAGS) -lfreetype2 -o $@
build/gui-regress: tools/gui_regress.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) $< $(LDFLAGS) -o $@
# Release regression helpers (docs/release-checklist.md, R3).
build/r3-helpers: tools/release/r3_helpers.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) $< $(LDFLAGS) -o $@
build/kitty-image-demo: tools/kitty_image_demo.c tools/term_write.h Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) $< $(LDFLAGS) -lm -o $@
build/kitty-plasma: tools/kitty_plasma.c tools/term_write.h Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) $< $(LDFLAGS) -lm -o $@
# Doom as a Kitty producer (docs/doom-kitty-20260928.md). doomgeneric is
# vendored third-party code (GPL-2.0), built without -Werror; the backend is
# ours and keeps the project's warnings.
DOOM_DIR = vendor/doomgeneric/doomgeneric
DOOM_SRC = $(addprefix $(DOOM_DIR)/,$(filter-out doomgeneric_xlib.c,$(patsubst %.o,%.c,$(shell [ -f $(DOOM_DIR)/Makefile ] && sed -n 's/^SRC_DOOM = //p' $(DOOM_DIR)/Makefile))))
DOOM_FLAGS = -DNORMALUNIX -D_DEFAULT_SOURCE -DDOOMGENERIC_RESX=320 -DDOOMGENERIC_RESY=200 -I$(DOOM_DIR) -Itools
build/doom-kitty: tools/doom_kitty.c tools/term_write.h $(DOOM_SRC) Makefile build/native-config.json
	@rm -rf build/doom-obj && mkdir -p build/doom-obj
	cd build/doom-obj && $(AROS_GCC) $(OPTFLAGS) -std=gnu99 -w --sysroot=$(AROS_SDK) -I$(AROS_SDK)/include \
	    $(DOOM_FLAGS) -I$(CURDIR)/$(DOOM_DIR) -c $(addprefix $(CURDIR)/,$(DOOM_SRC))
	$(AROS_GCC) $(CFLAGS) $(DOOM_FLAGS) tools/doom_kitty.c build/doom-obj/*.o $(LDFLAGS) -lm -o $@
build/doom-kitty-host: tools/doom_kitty.c tools/term_write.h tests/host_dos_shim.h $(DOOM_SRC) Makefile
	@mkdir -p build
	$(HOST_CC) -O2 -w -DHOST_DOS_SHIM $(DOOM_FLAGS) $(DOOM_SRC) tools/doom_kitty.c -lm -o $@
build/test_posix_partial_write: tests/test_posix_partial_write.c src/aros_tty_client.c src/aros_tty_packets.c src/aros_tty_packets.h src/tty_service.c src/tty_pair.c src/tty_control.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) tests/test_posix_partial_write.c src/aros_tty_packets.c src/tty_service.c src/tty_pair.c src/tty_control.c src/aros_tty_client.c $(LDFLAGS) -o $@
build/test_aros_tty_break_locks: tests/test_aros_tty_break_locks.c src/aros_tty_packets.c src/aros_tty_packets.h src/tty_service.c src/tty_pair.c src/tty_control.c Makefile build/native-config.json
	$(AROS_GCC) $(CFLAGS) tests/test_aros_tty_break_locks.c src/aros_tty_packets.c src/tty_service.c src/tty_pair.c src/tty_control.c $(LDFLAGS) -o $@
build/test_aros_tty_packets: tests/test_aros_tty_packets.c src/aros_tty_packets.c src/aros_tty_packets.h src/tty_service.c src/tty_service.h src/tty_pair.c src/tty_pair.h Makefile src/tty_control.c src/tty_control.h src/tty_control_server.h src/aros_tty_client.c src/aros_tty_client.h
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) tests/test_aros_tty_packets.c src/aros_tty_packets.c src/tty_service.c src/tty_pair.c $(LDFLAGS) -o $@ src/tty_control.c src/aros_tty_client.c

build/test_aros_tty_worker: tests/test_aros_tty_worker.c src/aros_tty_worker.c src/aros_tty_worker.h src/aros_tty_packets.c src/aros_tty_packets.h src/tty_service.c src/tty_service.h src/tty_pair.c src/tty_pair.h Makefile src/tty_control.c src/tty_control.h src/tty_control_server.h src/aros_tty_client.c src/aros_tty_client.h
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) tests/test_aros_tty_worker.c src/aros_tty_worker.c src/aros_tty_packets.c src/tty_service.c src/tty_pair.c $(LDFLAGS) -o $@ src/tty_control.c src/aros_tty_client.c

.PHONY: test-tty-control test-tty-control-sanitize
.PHONY: test-tty-termios test-tty-termios-sanitize
test: test-tty-termios
test-tty-termios: build/test_tty_termios
	./build/test_tty_termios
test-tty-termios-sanitize: build/test_tty_termios_sanitize
	./build/test_tty_termios_sanitize
build/test_tty_termios: tests/test_tty_termios.c src/tty_termios.c src/tty_termios.h src/tty_control.h Makefile
	mkdir -p build
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Werror -UNDEBUG tests/test_tty_termios.c src/tty_termios.c -o $@
build/test_tty_termios_sanitize: tests/test_tty_termios.c src/tty_termios.c src/tty_termios.h src/tty_control.h Makefile
	mkdir -p build
	$(HOST_CC) -std=c99 -O1 -g -Wall -Wextra -Werror -UNDEBUG -fsanitize=address,undefined tests/test_tty_termios.c src/tty_termios.c -o $@
build/test_tty_termios_aros: tests/test_tty_termios.c src/tty_termios.c src/tty_termios.h src/tty_control.h Makefile
	mkdir -p build
	$(AROS_GCC) $(CFLAGS) -UNDEBUG tests/test_tty_termios.c src/tty_termios.c $(LDFLAGS) -o $@
build/test_aros_tty_termios: tests/test_aros_tty_termios.c src/aros_tty_termios.c src/aros_tty_termios.h src/tty_termios.c src/tty_termios.h src/aros_tty_client.c src/aros_tty_client.h src/aros_tty_worker.c src/aros_tty_worker.h src/aros_tty_packets.c src/aros_tty_packets.h src/tty_service.c src/tty_service.h src/tty_pair.c src/tty_pair.h src/tty_control.c src/tty_control.h src/tty_control_server.h Makefile
	mkdir -p build
	$(AROS_GCC) $(CFLAGS) tests/test_aros_tty_termios.c src/aros_tty_termios.c src/tty_termios.c src/aros_tty_client.c src/aros_tty_worker.c src/aros_tty_packets.c src/tty_service.c src/tty_pair.c src/tty_control.c $(LDFLAGS) -o $@
build/test_aros_tty_read_timing: tests/test_aros_tty_read_timing.c src/aros_tty_termios.c src/aros_tty_termios.h src/tty_termios.c src/tty_termios.h src/aros_tty_client.c src/aros_tty_client.h src/aros_tty_worker.c src/aros_tty_worker.h src/aros_tty_packets.c src/aros_tty_packets.h src/tty_service.c src/tty_service.h src/tty_pair.c src/tty_pair.h src/tty_control.c src/tty_control.h src/tty_control_server.h Makefile
	mkdir -p build
	$(AROS_GCC) $(CFLAGS) tests/test_aros_tty_read_timing.c src/aros_tty_termios.c src/tty_termios.c src/aros_tty_client.c src/aros_tty_worker.c src/aros_tty_packets.c src/tty_service.c src/tty_pair.c src/tty_control.c $(LDFLAGS) -o $@
build/test_aros_tty_interrupt: tests/test_aros_tty_interrupt.c src/aros_tty_termios.c src/aros_tty_termios.h src/tty_termios.c src/tty_termios.h src/aros_tty_client.c src/aros_tty_client.h src/aros_tty_packets.c src/aros_tty_packets.h src/tty_service.c src/tty_service.h src/tty_pair.c src/tty_pair.h src/tty_control.c src/tty_control.h src/tty_control_server.h Makefile
	mkdir -p build
	$(AROS_GCC) $(CFLAGS) tests/test_aros_tty_interrupt.c src/aros_tty_termios.c src/tty_termios.c src/aros_tty_client.c src/aros_tty_packets.c src/tty_service.c src/tty_pair.c src/tty_control.c $(LDFLAGS) -o $@
build/test_aros_tty_io_interrupt: tests/test_aros_tty_io_interrupt.c src/aros_tty_client.c src/aros_tty_client.h src/aros_tty_packets.c src/aros_tty_packets.h src/tty_service.c src/tty_service.h src/tty_pair.c src/tty_pair.h src/tty_control.c src/tty_control.h src/tty_control_server.h Makefile
	mkdir -p build
	$(AROS_GCC) $(CFLAGS) tests/test_aros_tty_io_interrupt.c src/aros_tty_client.c src/aros_tty_packets.c src/tty_service.c src/tty_pair.c src/tty_control.c $(LDFLAGS) -o $@
test: build/test_tty_control
test-tty-control: build/test_tty_control
	./build/test_tty_control
test-tty-control-sanitize: build/test_tty_control_sanitize
	./build/test_tty_control_sanitize
build/test_tty_control: tests/test_tty_control.c src/tty_control.c src/tty_control.h src/tty_control_server.h src/tty_service.h src/tty_pair.c src/tty_pair.h Makefile
	mkdir -p build
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Werror -UNDEBUG tests/test_tty_control.c src/tty_control.c src/tty_pair.c -o $@
build/test_tty_control_sanitize: tests/test_tty_control.c src/tty_control.c src/tty_control.h src/tty_control_server.h src/tty_service.h src/tty_pair.c src/tty_pair.h Makefile
	mkdir -p build
	$(HOST_CC) -std=c99 -O1 -g -Wall -Wextra -Werror -UNDEBUG -fsanitize=address,undefined tests/test_tty_control.c src/tty_control.c src/tty_pair.c -o $@
build/tty_control_peer: tests/tty_dos_peer.c src/aros_tty_client.c src/aros_tty_client.h src/tty_control.h Makefile
	mkdir -p build
	$(AROS_GCC) $(CFLAGS) -DXTY_CONTROL_PROBE tests/tty_dos_peer.c src/aros_tty_client.c $(LDFLAGS) -o $@
build/test_aros_tty_control_child: tests/test_aros_tty_child.c src/aros_tty_worker.c src/aros_tty_packets.c src/tty_control.c src/tty_service.c src/tty_pair.c src/aros_tty_worker.h src/aros_tty_packets.h src/tty_control.h src/tty_control_server.h src/tty_service.h src/tty_pair.h Makefile
	mkdir -p build
	$(AROS_GCC) $(CFLAGS) -DXTY_CONTROL_PROBE tests/test_aros_tty_child.c src/aros_tty_worker.c src/aros_tty_packets.c src/tty_control.c src/tty_service.c src/tty_pair.c $(LDFLAGS) -o $@

build/tty_dos_peer: tests/tty_dos_peer.c Makefile
	mkdir -p build
	$(AROS_GCC) $(CFLAGS) tests/tty_dos_peer.c $(LDFLAGS) -o $@

build/test_aros_tty_child: tests/test_aros_tty_child.c src/aros_tty_worker.c src/aros_tty_worker.h src/aros_tty_packets.c src/aros_tty_packets.h src/tty_service.c src/tty_service.h src/tty_pair.c src/tty_pair.h Makefile src/tty_control.c src/tty_control.h src/tty_control_server.h src/aros_tty_client.c src/aros_tty_client.h
	mkdir -p build
	$(AROS_GCC) $(CFLAGS) tests/test_aros_tty_child.c src/aros_tty_worker.c src/aros_tty_packets.c src/tty_service.c src/tty_pair.c $(LDFLAGS) -o $@ src/tty_control.c src/aros_tty_client.c

build/test_aros_tty_wait: tests/test_aros_tty_wait.c src/aros_tty_worker.c src/aros_tty_worker.h src/aros_tty_packets.c src/aros_tty_packets.h src/tty_service.c src/tty_service.h src/tty_pair.c src/tty_pair.h Makefile src/tty_control.c src/tty_control.h src/tty_control_server.h src/aros_tty_client.c src/aros_tty_client.h
	mkdir -p build
	$(AROS_GCC) $(CFLAGS) tests/test_aros_tty_wait.c src/aros_tty_worker.c src/aros_tty_packets.c src/tty_service.c src/tty_pair.c $(LDFLAGS) -o $@ src/tty_control.c src/aros_tty_client.c

build/test_aros_tty_controls: tests/test_aros_tty_controls.c src/aros_tty_worker.c src/aros_tty_worker.h src/aros_tty_packets.c src/aros_tty_packets.h src/tty_service.c src/tty_service.h src/tty_pair.c src/tty_pair.h Makefile src/tty_control.c src/tty_control.h src/tty_control_server.h src/aros_tty_client.c src/aros_tty_client.h
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) tests/test_aros_tty_controls.c src/aros_tty_worker.c src/aros_tty_packets.c src/tty_service.c src/tty_pair.c $(LDFLAGS) -o $@ src/tty_control.c src/aros_tty_client.c

# Declared target, not automatic ABI detection. One configuration at a time.
AROS_BUILD_TARGET ?= mainline-x86_64
# Freeze the global configuration: target-specific flags must not leak into a
# shared prerequisite depending on which parent Make visits first.
export XTERM_NATIVE_TARGET := $(AROS_BUILD_TARGET)
export XTERM_NATIVE_CC := $(AROS_GCC)
export XTERM_NATIVE_CXX := $(AROS_CXX)
export XTERM_NATIVE_SDK := $(AROS_SDK)
export XTERM_NATIVE_CFLAGS := $(CFLAGS)
export XTERM_NATIVE_LDFLAGS := $(LDFLAGS)
export XTERM_NATIVE_HB_FLAGS := $(HB_FLAGS)

.PHONY: native-config-force
native-config-force:
build/native-config.json: native-config-force tools/write_native_config.py
	$(PYTHON) tools/write_native_config.py $@

# Keep this dependency on every cross-compiled target, including shared objects.
$(OUT): build/native-config.json
build/aros-xterm-worker: build/native-config.json
build/link-demo: build/native-config.json
build/geometry-probe: build/native-config.json
build/Run-fixed: build/native-config.json
build/test_run_redirect: build/native-config.json
build/test_handler_launch: build/native-config.json
build/xterm-handler: build/native-config.json
build/headless/aros-xterm-worker: build/native-config.json
build/test_aros_endpoint: build/native-config.json
build/test_shell_startup: build/native-config.json
build/test_completion_aros: build/native-config.json
build/test_aros_completion: build/native-config.json
build/test_line_view_aros: build/native-config.json
build/test_line_editor_aros: build/native-config.json
build/test_status_area_aros: build/native-config.json
build/test_alternate_aros: build/native-config.json
build/test_aros_tui: build/native-config.json
build/session-peer: build/native-config.json
build/test_aros_session: build/native-config.json
build/console-contract-probe: build/native-config.json
build/bench-render-o0: build/native-config.json
build/bench-render-o2: build/native-config.json
build/test_dos_stream: build/native-config.json
build/test_selection_aros: build/native-config.json
build/test_paste_aros: build/native-config.json
build/test_aros_clip_job: build/native-config.json
build/test_clipboard_text_aros: build/native-config.json
build/harfbuzz-aros.o: build/native-config.json
build/test_cluster_font_aros: build/native-config.json
build/test_graphemes_aros: build/native-config.json
build/test_aros_clipboard: build/native-config.json
build/clip-probe: build/native-config.json
build/test_input_echo_aros: build/native-config.json
build/test_aros_input: build/native-config.json
build/font-probe-aros: build/native-config.json
build/test_font_aros: build/native-config.json
build/test_replay_aros: build/native-config.json
$(AROS_TEST): build/native-config.json
build/bench-window: build/native-config.json
build/test-keyboard-aros: build/native-config.json
build/test-reply-flow-aros: build/native-config.json
build/openurl-probe: build/native-config.json
build/test_control_strings_aros: build/native-config.json
build/test_cursor_queries_aros: build/native-config.json
build/test-vt-reply: build/native-config.json
build/tty_fd_consumer_stock: build/native-config.json
build/tty_fd_read_break: build/native-config.json
build/tty_fd_write_break: build/native-config.json
build/tty_fd_io_session: build/native-config.json
build/test_aros_fd_flags: build/native-config.json
build/tty_fd_read_stock: build/native-config.json
build/tty_fd_write_stock: build/native-config.json
build/test_aros_tty_fd_break: build/native-config.json
build/tty_fd_session: build/native-config.json
build/test_aros_tty_fd_session: build/native-config.json
build/tty_fd_consumer: build/native-config.json
build/test_fd_bridge_legacy: build/native-config.json
build/test_aros_tty_fd: build/native-config.json
build/test_aros_tty_signal_worker: build/native-config.json
build/test_aros_tty_signal: build/native-config.json
build/test_tty_pair_aros: build/native-config.json
build/test_aros_tty_try: build/native-config.json
build/test_tty_try_io_aros: build/native-config.json
build/test_tty_service_aros: build/native-config.json
build/test_aros_tty_packets: build/native-config.json
build/test_aros_tty_worker: build/native-config.json
build/test_tty_termios_aros: build/native-config.json
build/test_aros_tty_termios: build/native-config.json
build/test_aros_tty_read_timing: build/native-config.json
build/test_aros_tty_interrupt: build/native-config.json
build/test_aros_tty_io_interrupt: build/native-config.json
build/tty_control_peer: build/native-config.json
build/test_aros_tty_control_child: build/native-config.json
build/tty_dos_peer: build/native-config.json
build/test_aros_tty_child: build/native-config.json
build/test_aros_tty_wait: build/native-config.json
build/test_aros_tty_controls: build/native-config.json

# The unattended Doom benchmark's own checks (tools/doom-bench/drive.py).
.PHONY: test-doom-bench-driver
# test-doom-bench-driver needs the local doom bench tools; run it by name.
test-doom-bench-driver:
	$(PYTHON) tests/test_doom_bench_driver.py

test: test-presentation
.PHONY: test-presentation
test-presentation: build/test_presentation
	./build/test_presentation
build/test_presentation: tests/test_presentation.c src/xterm_presentation.h src/tty_control.h
	@mkdir -p build
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Werror $< -o $@
build/presentation-probe: tools/presentation_probe.c src/aros_tty_client.c src/aros_tty_client.h src/tty_control.h
	@mkdir -p build
	$(AROS_GCC) $(CFLAGS) $< src/aros_tty_client.c $(LDFLAGS) -o $@
$(OUT) build/aros-xterm-pty build/aros-xterm-worker: src/xterm_presentation.h src/tty_control.h

build/test_ssh_term_query: tests/test_ssh_term_query.c examples/ssh/term_query.c examples/ssh/term_query.h src/xterm_status.h src/tty_control.h
	@mkdir -p build
	cc -std=c99 -Wall -Wextra -Werror tests/test_ssh_term_query.c examples/ssh/term_query.c -o $@
test: test-ssh-term-query
.PHONY: test-ssh-term-query
test-ssh-term-query: build/test_ssh_term_query
	./build/test_ssh_term_query

build/test_reflow: tests/test_reflow.c src/screen_model.c src/screen_model.h src/hyperlink.c
	@mkdir -p build
	cc -std=c99 -Wall -Wextra -Werror -Ivendor/utf8proc -DUTF8PROC_STATIC tests/test_reflow.c src/screen_model.c src/hyperlink.c vendor/utf8proc/utf8proc.c -o $@
test: test-reflow
.PHONY: test-reflow
test-reflow: build/test_reflow
	./build/test_reflow

# Scrollback search (src/output_search.h), with the sanitizers on.
build/test_output_search: tests/test_output_search.c src/output_search.c src/output_search.h src/screen_model.c src/screen_model.h src/hyperlink.c
	@mkdir -p build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror -g -fsanitize=address,undefined $(UNICODE_FLAGS) tests/test_output_search.c src/output_search.c src/screen_model.c src/hyperlink.c $(UNICODE_SRC) -o $@
# Settings file (src/settings.h).
build/test_settings: tests/test_settings.c src/settings.c src/settings.h
	@mkdir -p build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror -g -fsanitize=address,undefined tests/test_settings.c src/settings.c -o $@
test: test-settings
.PHONY: test-settings
test-settings: build/test_settings
	./build/test_settings
test: test-output-search
.PHONY: test-output-search
test-output-search: build/test_output_search
	./build/test_output_search

# Public SDK (sdk/): its contract tests run against the terminal's own
# presentation reply code, plus the SDK's host compile check.
SDK_SRC = sdk/src/limpet_format.c sdk/src/limpet_query.c
SDK_HEADERS = sdk/include/limpet/limpet.h sdk/include/limpet/tty_control.h
test: test-sdk
.PHONY: test-sdk
test-sdk: build/test_sdk
	./build/test_sdk
	$(MAKE) -C sdk host-check
	sh sdk/tests/test_sdk_compat.sh
build/test_sdk: sdk/tests/test_sdk.c $(SDK_SRC) $(SDK_HEADERS) src/xterm_presentation.h
	@mkdir -p build
	$(HOST_CC) -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all \
		-Isdk/include -Isdk/tests/host $< $(SDK_SRC) -o $@

# Bracketed paste and resize reports against keys and replies (input_order.h).
test: test-input-order
.PHONY: test-input-order
test-input-order: build/test_input_order
	./build/test_input_order
build/test_input_order: tests/test_input_order.c src/input_order.c src/input_order.h src/paste.c src/paste.h src/resize_report.h $(UNICODE_SRC)
	@mkdir -p build
	$(HOST_CC) -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all \
		$(UNICODE_FLAGS) $< src/input_order.c src/paste.c $(UNICODE_SRC) -o $@
