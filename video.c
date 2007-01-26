#include "config.h"

#include <stdio.h>
#include <SDL/SDL.h>
#include <string.h>

#include "eek.h"
#include "video.h"
#include "util.h"

#define VIDEO_YSCALE 2
#define VIDEO_WIDTH  640
#define VIDEO_HEIGHT (256 * VIDEO_YSCALE)

#define VIDEO_LOGICAL_COLOR_COUNT 16

static struct
{
  SDL_Surface *screen;
  
  int scan_line;
  UBYTE *memory;

  UBYTE mode;

  UWORD start;
} video;

static SDL_Color video_physical_colors[] =
  {
    { 0xff, 0xff, 0xff, 0x00 }, /* white */
    { 0x00, 0xff, 0xff, 0x00 }, /* cyan */
    { 0xff, 0x00, 0xff, 0x00 }, /* magenta */
    { 0x00, 0x00, 0xff, 0x00 }, /* blue */
    { 0xff, 0xff, 0x00, 0x00 }, /* yellow */
    { 0x00, 0xff, 0x00, 0x00 }, /* green */
    { 0xff, 0x00, 0x00, 0x00 }, /* red */
    { 0x00, 0x00, 0x00, 0x00 }  /* black */
  };

static UBYTE video_logical_colors[VIDEO_LOGICAL_COLOR_COUNT] =
  {
    0x07, 0x00
  };

int
video_init (UBYTE *memory, int flags)
{
  int sdl_flags = SDL_HWSURFACE;

  if ((flags & VIDEO_FULLSCREEN))
    sdl_flags |= SDL_FULLSCREEN;

  video.scan_line = 0;
  video.memory = memory;
  video.mode = 0;
  video.start = 0x4000;

  if (SDL_Init (SDL_INIT_VIDEO) == -1)
  {
    eprintf ("SDL_Init: %s\n", SDL_GetError ());
    return -1;
  }
  
  if ((video.screen = SDL_SetVideoMode (VIDEO_WIDTH, VIDEO_HEIGHT, 
					8, sdl_flags)) == NULL)
  {
    SDL_Quit ();
    eprintf ("SDL_SetVideoMode: %s\n", SDL_GetError ());
    return -1;
  }

  SDL_WM_SetCaption ("Electron Emulator Kit", "Electron Emulator Kit");

  /* Initialise the palette */
  SDL_SetColors (video.screen, video_physical_colors, 0, 
		 sizeof (video_physical_colors) / sizeof (SDL_Color));

  return 0;
}

void
video_update (void)
{
  SDL_Flip (video.screen);
}

void
video_set_mode (UBYTE mode)
{
  video.mode = mode & 7;
}

void
video_set_start_address (UWORD start)
{
  video.start = start;
}

void
video_set_color (UBYTE logical, UBYTE physical)
{
  video_logical_colors[logical] = physical;
}

void
video_draw_scanline (int line)
{
  int i, j, c, v;
  unsigned char *p;
  UWORD a;

  SDL_LockSurface (video.screen);

  p = video.screen->pixels + video.screen->pitch * line * VIDEO_YSCALE;

  switch (video.mode)
  {
    case 0:
      a = (line / 8) * 0x280 + (line % 8) + (video.start ? video.start : 0x3000);
      if (a >= 0x8000)
	a = (a + 0x3000) & 0x7fff;
      for (i = 0; i < 80; i++)
      {
	c = video.memory[a];
	for (j = 0; j < 8; j++)
	  *(p++) = video_logical_colors[((c <<= 1) & 0x100) ? 1 : 0];
	if ((a += 8) >= 0x8000)
	  a = (a + 0x3000) & 0x7fff;
      }
      break;
    case 1:
      a = (line / 8) * 0x280 + (line % 8) + (video.start ? video.start : 0x3000);
      if (a >= 0x8000)
	a = (a + 0x3000) & 0x7fff;
      for (i = 0; i < 80; i++)
      {
	c = video.memory[a];
	for (j = 0; j < 4; j++)
	{
	  *(p++) = v = video_logical_colors[((c >> 6) & 2) | ((c >> 3) & 1)];
	  *(p++) = v;
	  c <<= 1;
	}
	if ((a += 8) >= 0x8000)
	  a = (a + 0x3000) & 0x7fff;
      }
      break;
    case 2:
      a = (line / 8) * 0x280 + (line % 8) + (video.start ? video.start : 0x3000);
      if (a >= 0x8000)
	a = (a + 0x3000) & 0x7fff;
      for (i = 0; i < 80; i++)
      {
	c = video.memory[a];
	for (j = 0; j < 2; j++)
	{
	  *(p++) = v = video_logical_colors[((c >> 4) & 8)
					    | ((c >> 3) & 4)
					    | ((c >> 2) & 2)
					    | ((c >> 1) & 1)];
	  *(p++) = v;
	  *(p++) = v;
	  *(p++) = v;
	  c <<= 1;
	}
	if ((a += 8) >= 0x8000)
	  a = (a + 0x3000) & 0x7fff;
      }
      break;
    case 3:
      /* the last two out of every 10 lines are blank */
      if (line % 10 >= 8)
	memset (p, 0x7, video.screen->w);
      else
      {
	/* 0x240 bytes per 10 lines */
	a = (line / 10) * 0x240 + (line % 10) + video.start;
	if (a >= 0x7e40)
	  a = (a % 0x3e40) + 0x4000;
	for (i = 0; i < 80; i++)
	{
	  c = video.memory[a];
	  for (j = 0; j < 8; j++)
	    *(p++) = video_logical_colors[((c <<= 1) & 0x100) ? 1 : 0];
	  if ((a += 8) >= 0x7e40)
	    a = (a % 0x3e40) + 0x7fff;
	}
      }
      break;
    case 5:
      break;
    case 6:
      /* the last two out of every 10 lines and the last six lines of
	 the screen are blank */
      if (line % 10 >= 8 || line >= 250)
	memset (p, 0x7, video.screen->w);
      else
      {
	/* 0x140 bytes per 10 lines */
	a = (line / 10) * 0x140 + (line % 10) + video.start;
	if (a >= 0x7f40)
	  a = 0x6000;
	for (i = 0; i < 40; i++)
	{
	  c = video.memory[a];
	  for (j = 0; j < 8; j++)
	  {
	    /* Double up the pixels along the x-axis */
	    *(p++) = v = video_logical_colors[((c <<= 1) & 0x100) ? 1 : 0];;
	    *(p++) = v;
	  }
	  if ((a += 8) >= 0x7f40)
	    a -= 0x7f40 - 0x6000;
	}
      }
      break;
    default:
      /* mode 7 becomes mode 4 */
    case 4:
      break;
  }

  /* Copy the scanline a few times */
  for (i = 1; i < VIDEO_YSCALE; i++)
    memcpy (video.screen->pixels + video.screen->pitch * line * VIDEO_YSCALE
	    + video.screen->pitch * i,
	    video.screen->pixels + video.screen->pitch * line * VIDEO_YSCALE,
	    video.screen->w);

  SDL_UnlockSurface (video.screen);
}

void
video_quit (void)
{
  SDL_Quit ();
}
