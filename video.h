#ifndef _VIDEO_H
#define _VIDEO_H

#include "stypes.h"

int video_init (UBYTE *memory, int flags);
void video_update (void);
void video_quit (void);
void video_draw_scanline (int line);
void video_set_start_address (UWORD start);
void video_set_mode (UBYTE mode);
void video_set_color (UBYTE logical, UBYTE physical);

#define VIDEO_FULLSCREEN 1

#endif /* _VIDEO_H */
