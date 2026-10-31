#ifndef XTERM_BACKDROP_BLUR_H
#define XTERM_BACKDROP_BLUR_H
/* Blur of what lies behind a translucent window, done by the compositor:
 * Limpet only asks for it. This is the one place that knows the contract,
 * a proposal for mainline from the Dockbar project (AROS issue #1981) that
 * no AROS release has. Its numbers are working numbers and may change or
 * go. Their being in one file does not protect a user: an attribute given
 * the screen tag's number one day would answer too. So only the exact
 * answer below counts, and any other, 1 included, is a system without blur:
 * no tag is sent and everything works as if this did not exist. */
struct Screen;
struct TagItem;
/* "BLR" and a version, the same as Dockbar's (present_compose.c). */
#define BACKDROP_BLUR_CONTRACT 0x424c5201UL
static inline int backdrop_blur_contract_known(unsigned long answer)
{ return answer == BACKDROP_BLUR_CONTRACT; }
int backdrop_blur_supported(struct Screen *);
/* Fills a tag for OpenWindow: the request, or TAG_IGNORE. */
void backdrop_blur_tag(struct TagItem *, int wanted);
#endif
