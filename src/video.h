#ifndef _VIDEO_H
#define _VIDEO_H

#include <glib/gtypes.h>

#define VIDEO_YSCALE       2
#define VIDEO_WIDTH        640
#define VIDEO_SCREEN_PITCH VIDEO_WIDTH
#define VIDEO_HEIGHT       (256 * VIDEO_YSCALE)
#define VIDEO_MEMORY_SIZE  (VIDEO_SCREEN_PITCH * VIDEO_HEIGHT)

#define VIDEO_LOGICAL_COLOR_COUNT 16

typedef struct _Video Video;

struct _Video
{
  const guint8 *memory;
  guint8 screen_memory[VIDEO_MEMORY_SIZE];
  guint16 start_address;
  guint8 logical_colors[VIDEO_LOGICAL_COLOR_COUNT];
  guint8 mode;
};

void video_init (Video *video, const guint8 *memory);
void video_draw_scanline (Video *video, int line);
void video_set_start_address (Video *video, guint16 start);
void video_set_mode (Video *video, guint8 mode);
void video_set_color (Video *video, guint8 logical, guint8 physical);

#endif /* _VIDEO_H */
