#ifndef _VIDEO_H
#define _VIDEO_H

#include "stypes.h"

#define VIDEO_YSCALE       2
#define VIDEO_WIDTH        640
#define VIDEO_SCREEN_PITCH VIDEO_WIDTH
#define VIDEO_HEIGHT       (256 * VIDEO_YSCALE)
#define VIDEO_MEMORY_SIZE  (VIDEO_SCREEN_PITCH * VIDEO_HEIGHT)

#define VIDEO_LOGICAL_COLOR_COUNT 16

typedef struct _Video Video;

struct _Video
{
  const UBYTE *memory;
  UBYTE screen_memory[VIDEO_MEMORY_SIZE];
  UWORD start_address;
  UBYTE logical_colors[VIDEO_LOGICAL_COLOR_COUNT];
  UBYTE mode;
};

void video_init (Video *video, const UBYTE *memory);
void video_draw_scanline (Video *video, int line);
void video_set_start_address (Video *video, UWORD start);
void video_set_mode (Video *video, UBYTE mode);
void video_set_color (Video *video, UBYTE logical, UBYTE physical);

#endif /* _VIDEO_H */
