#include "config.h"

#include <SDL/SDL.h>

#include "eek.h"
#include "events.h"
#include "electron.h"
#include "monitor.h"

static int events_keymap[14 * 4] =
  {
    SDLK_RIGHT, SDLK_TAB, 0, SDLK_SPACE,
    SDLK_LEFT, SDLK_DOWN, SDLK_RETURN, SDLK_BACKSPACE,
    SDLK_MINUS, SDLK_UP, SDLK_COLON, 0,
    SDLK_0, SDLK_p, SDLK_SEMICOLON, SDLK_SLASH,
    SDLK_9, SDLK_o, SDLK_l, SDLK_PERIOD,
    SDLK_8, SDLK_i, SDLK_k, SDLK_COMMA,
    SDLK_7, SDLK_u, SDLK_j, SDLK_m,
    SDLK_6, SDLK_y, SDLK_h, SDLK_n,
    SDLK_5, SDLK_t, SDLK_g, SDLK_b,
    SDLK_4, SDLK_r, SDLK_f, SDLK_v,
    SDLK_3, SDLK_e, SDLK_d, SDLK_c,
    SDLK_2, SDLK_w, SDLK_s, SDLK_x,
    SDLK_1, SDLK_q, SDLK_a, SDLK_z,
    SDLK_ESCAPE, SDLK_LALT, SDLK_LCTRL, SDLK_LSHIFT
  };

int
events_check (Electron *electron)
{
  SDL_Event event;

  while (SDL_PollEvent (&event))
    switch (event.type)
    {
      case SDL_QUIT:
	return -1;
	break;
      case SDL_KEYDOWN:
	if (event.key.keysym.sym == SDLK_F10)
	{
	  if (monitor (electron) == -1)
	    return -1;
	}
	else
	{
	  int i;

	  for (i = 0; i < 14 * 4; i++)
	    if (events_keymap[i] == event.key.keysym.sym)
	    {
	      electron_press_key (electron, i >> 2, i & 3);
	      break; 
	    }
	}
	break;
      case SDL_KEYUP:
	{
	  int i;

	  for (i = 0; i < 14 * 4; i++)
	    if (events_keymap[i] == event.key.keysym.sym)
	    {
	      electron_release_key (electron, i >> 2, i & 3);
	      break; 
	    }
	}
	break;
    }

  return 0;
}
