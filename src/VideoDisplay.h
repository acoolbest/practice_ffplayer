
#ifndef VIDEO_DISPLAY_H
#define VIDEO_DISPLAY_H

#include "Video.h"

#define FF_QUIT_EVENT	(SDL_USEREVENT)
#define FF_REFRESH_EVENT (SDL_USEREVENT + 1)
#define FF_REFRESH_EVENT1 (SDL_USEREVENT + 2)
#define FF_REFRESH_EVENT2 (SDL_USEREVENT + 3)

// ÑÓ³Ùdelay msºóË¢ÐÂvideoÖ¡
void schedule_refresh(MediaState *media, int delay);

uint32_t sdl_refresh_timer_cb(uint32_t interval, void *opaque);

void *video_refresh_timer(void *userdata);

//void video_display(VideoState *video);


#endif