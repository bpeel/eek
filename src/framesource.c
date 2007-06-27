#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#include "framesource.h"

typedef struct _FrameSource FrameSource;

struct _FrameSource
{
  GSource source;

  GTimeVal start_time;
  guint last_time, frame_time;
};

static gboolean frame_source_prepare (GSource *source, gint *timeout);
static gboolean frame_source_check (GSource *source);
static gboolean frame_source_dispatch (GSource *source, GSourceFunc callback,
				       gpointer user_data);

static GSourceFuncs frame_source_funcs = 
  {
    frame_source_prepare,
    frame_source_check,
    frame_source_dispatch,
    NULL
  };

guint
frame_source_add (guint frame_time, GSourceFunc function, gpointer data,
		  GDestroyNotify notify)
{
  guint ret;
  GSource *source = g_source_new (&frame_source_funcs, sizeof (FrameSource));
  FrameSource *frame_source = (FrameSource *) source;

  frame_source->last_time = 0;
  frame_source->frame_time = frame_time;
  g_get_current_time (&frame_source->start_time);

  g_source_set_callback (source, function, data, notify);

  ret = g_source_attach (source, NULL);

  g_source_unref (source);

  return ret;
}

static guint
frame_source_get_ticks (FrameSource *frame_source)
{
  GTimeVal time_now;

  g_source_get_current_time ((GSource *) frame_source, &time_now);
  
  return (time_now.tv_sec - frame_source->start_time.tv_sec) * 1000
    + (time_now.tv_usec - frame_source->start_time.tv_usec) / 1000;
}

static gboolean
frame_source_prepare (GSource *source, gint *timeout)
{
  FrameSource *frame_source = (FrameSource *) source;

  guint now = frame_source_get_ticks (frame_source);

  /* If time has gone backwards or the time since the last frame is
     greater than the two frames worth then reset the time and do a
     frame now */
  if (frame_source->last_time > now || now - frame_source->last_time
      > frame_source->frame_time * 2)
  {
    frame_source->last_time = now - frame_source->frame_time;
    if (timeout)
      *timeout = 0;
    return TRUE;
  }
  else if (now - frame_source->last_time >= frame_source->frame_time)
  {
    if (timeout)
      *timeout = 0;
    return TRUE;
  }
  else
  {
    if (timeout)
      *timeout = now - frame_source->last_time;
    return FALSE;
  }
}

static gboolean
frame_source_check (GSource *source)
{
  return frame_source_prepare (source, NULL);
}

static gboolean
frame_source_dispatch (GSource *source, GSourceFunc callback, gpointer user_data)
{
  FrameSource *frame_source = (FrameSource *) source;

  if ((* callback) (user_data))
  {
    frame_source->last_time += frame_source->frame_time;
    return TRUE;
  }
  else
    return FALSE;
}
