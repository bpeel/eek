#include "config.h"

#include <SDL/SDL.h>

#include "eek.h"
#include "events.h"
#include "electron.h"
#include "monitor.h"

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
	switch (event.key.keysym.sym)
	{
	  case SDLK_F12:
	    if (monitor (electron) == -1)
	      return -1;
	    break;
	  default:
	    break;
	}
    }

  return 0;
}
