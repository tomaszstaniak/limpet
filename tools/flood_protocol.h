#ifndef XTERM_FLOOD_PROTOCOL_H
#define XTERM_FLOOD_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define FLOOD_INPUT_SIZE 16
#define FLOOD_HEADER_SIZE 42
#define FLOOD_FRAME_MAX 2048
#define FLOOD_SAMPLES 65536
enum { FLOOD_KEY=1, FLOOD_PAUSE=2, FLOOD_STOP=3 };
typedef int (*FloodRecord)(void *, unsigned, uint64_t, uint64_t);
typedef struct {
    unsigned char bytes[FLOOD_FRAME_MAX];
    unsigned char expected[FLOOD_FRAME_MAX];
    size_t length, need;
    uint64_t inputs, frames, acks, bytes_seen;
    int error, stopped;
} FloodParser;
typedef struct {
    uint64_t due, offer, dispatch, admit, peer, parsed, uploaded, readback;
} FloodSample;
void flood_input(unsigned char *, unsigned, uint64_t);
int flood_input_feed(FloodParser *, const unsigned char *, size_t, FloodRecord, void *);
size_t flood_frame(unsigned char *, size_t, unsigned, uint64_t, uint64_t);
int flood_output_feed(FloodParser *, const unsigned char *, size_t, FloodRecord, void *);
int flood_finish(const FloodParser *);
int flood_complete(FloodSample *, size_t, uint64_t *, uint64_t, uint64_t, uint64_t, int);
uint64_t flood_quantile(uint64_t *, size_t, unsigned);
#endif
