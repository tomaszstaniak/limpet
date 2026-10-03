# libssh's pre-project defaults force PIC, producing GOT relocations which the
# mainline AROS ELF loader cannot apply. This static client needs ordinary code.
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)
