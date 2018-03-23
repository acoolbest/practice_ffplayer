
#include "VideoDisplay.h"
#include <iostream>

extern "C"{

#include <libswscale/swscale.h>
#include <libavutil/time.h>

}
extern bool quit;
static const double SYNC_THRESHOLD = 0.01;
static const double NOSYNC_THRESHOLD = 10.0;

static SDL_mutex *gmutex = SDL_CreateMutex();;

// �ӳ�delay ms��ˢ��video֡
void schedule_refresh(MediaState *media, int delay)
{
	SDL_AddTimer(delay/media->video->speed, sdl_refresh_timer_cb, media);
}

uint32_t sdl_refresh_timer_cb(uint32_t interval, void *opaque)
{
	SDL_Event event;
	//event.type = FF_REFRESH_EVENT;
	event.type =  ((MediaState *)opaque)->media_index + FF_REFRESH_EVENT;
	//printf("event.type: %d\n",((MediaState *)opaque)->media_index);
	event.user.data1 = opaque;
	SDL_PushEvent(&event);
	return 0; /* 0 means stop timer */
}

void *video_refresh_timer(void *userdata)
{
	MediaState *media = (MediaState*)userdata;
	VideoState *video = media->video;
	//media->audio->speed = media->video->speed;// sync speed;
	static int count = 0;
	SDL_LockMutex(media->mutex);
	static int count1 = 0;
	while (true)
	{
		//SDL_CondWait(media->cond, media->mutex);
		if (quit)
		{
			break;
		}

		if (video->stream_index >= 0)
		{
			if (video->videoq->queue.empty() && !video->frameq.nb_frames)
			{
				//video->frameq.enQueue();
				schedule_refresh(media, 10);
				//printf("meida[%s] sleep 1 ms\n", media->filename.c_str());
			}
			else
			{
				printf("external_clock: %f, get frame cout %d\n", av_gettime() / 1000000.0, count1++);
				video->frameq.deQueue(&video->frame);
				#if 0
				// ����Ƶͬ������Ƶ�ϣ�������һ֡���ӳ�ʱ��
				double current_pts = *(double*)video->frame->opaque;
				double delay = (current_pts - video->frame_last_pts)*2/3;
				if (delay <= 0 || delay >= 1.0)
					delay = video->frame_last_delay;

				video->frame_last_delay = delay;
				video->frame_last_pts = current_pts;

				// ��ǰ��ʾ֡��PTS��������ʾ��һ֡���ӳ�
				double ref_clock = media->audio->get_audio_clock();

				double diff = current_pts - ref_clock;// diff < 0 => video slow,diff > 0 => video quick

				double threshold = (delay > SYNC_THRESHOLD) ? delay : SYNC_THRESHOLD;

				if (fabs(diff) < NOSYNC_THRESHOLD) // ��ͬ��
				{
					if (diff <= -threshold) // ���ˣ�delay��Ϊ0
						delay = 0;
					else if (diff >= threshold) // ���ˣ��ӱ�delay
						delay *= 2;
				}
				
				printf("external_clock: %f, diff[%f] threshold[%f] current_pts[%f] ref_clock[%f], last_pts[%f] delay[%f]ms, count: %d\n", av_gettime() / 1000000.0, diff*1000, threshold*1000, current_pts, ref_clock, video->frame_last_pts, delay*1000, count++);
				video->frame_timer += delay;
				double actual_delay = video->frame_timer - static_cast<double>(av_gettime()) / 1000000.0;
				if (actual_delay <= 0.010) 
					actual_delay = 0.010; 
				
				schedule_refresh(media, static_cast<int>(actual_delay * 1000 + 0.5));
				//printf("meida[%s] sleep %d ms\n", media->filename.c_str(), static_cast<int>(actual_delay * 1000 + 0.5));
				#else
				schedule_refresh(media, 12);
				#endif
				
				SwsContext *sws_ctx = sws_getContext(video->video_ctx->width, video->video_ctx->height, video->video_ctx->pix_fmt,
				video->displayFrame->width,video->displayFrame->height,(AVPixelFormat)video->displayFrame->format, SWS_BILINEAR, nullptr, nullptr, nullptr);

				sws_scale(sws_ctx, (uint8_t const * const *)video->frame->data, video->frame->linesize, 0, 
					video->video_ctx->height, video->displayFrame->data, video->displayFrame->linesize);

				// Display the image to screen
				//SDL_UpdateTexture(video->bmp, &video->rect, video->displayFrame->data[0], video->displayFrame->linesize[0]);
				SDL_UpdateTexture(video->bmp, NULL, video->displayFrame->data[0], video->displayFrame->linesize[0]);
				//SDL_RenderClear(video->renderer);
				//SDL_RenderCopy(video->renderer, video->bmp, &video->rect, &video->rect);
				//SDL_LockMutex(gmutex);
				SDL_RenderCopy(video->renderer, video->bmp, NULL, &video->rect);
				SDL_RenderPresent(video->renderer);
				//SDL_UnlockMutex(gmutex);
				sws_freeContext(sws_ctx);
				av_frame_unref(video->frame);
			}
		}
		else
		{
			printf("meida[%s] sleep 100ms\n", media->filename.c_str());
			schedule_refresh(media, 100);
		}
		break;
	}
	SDL_UnlockMutex(media->mutex);
}