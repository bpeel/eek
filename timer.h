#ifndef _TIMER_H
#define _TIMER_H

void timer_init (void);
unsigned int timer_ticks (void);
void timer_sleep (int sleep_time);

#endif /* _TIMER_H */
