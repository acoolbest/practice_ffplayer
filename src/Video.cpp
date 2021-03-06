
#include "Video.h"
#include "VideoDisplay.h"

extern "C"{

#include <libswscale/swscale.h>
#include <libavutil/time.h>

}

VideoState::VideoState(bool live)
{
	video_ctx        = nullptr;
	stream_index     = -1;
	stream           = nullptr;

	window           = nullptr;
	bmp              = nullptr;
	renderer         = nullptr;

	frame            = nullptr;
	displayFrame     = nullptr;

	videoq           = new PacketQueue();

	frame_timer      = 0.0;
	frame_last_delay = 0.0;
	frame_last_pts   = 0.0;
	video_clock      = 0.0;
	live_stream 	 = live;
	speed 			 = 1.0;
	fps				 = 85;//一般而言，大脑处理视频的极限是每秒85帧
}

VideoState::~VideoState()
{
	delete videoq;

	av_frame_free(&frame);
	av_free(displayFrame->data[0]);
	av_frame_free(&displayFrame);
}

void VideoState::video_play(MediaState *media, SDL_Play *sdl_play)
{
	int width = rect.w;
	int height = rect.h;
	// 创建sdl窗口
	window = sdl_play->window;
	renderer = sdl_play->renderer;
	//renderer = SDL_CreateRenderer(window, -1, 0);
	bmp = SDL_CreateTexture(sdl_play->renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
		width, height);//SDL_PIXELFORMAT_IYUV,SDL_PIXELFORMAT_YV12
	rect.x = sdl_play->rect.x;
	rect.y = sdl_play->rect.y;
	//rect.w = media->video->video_ctx->width;
	//rect.h = media->video->video_ctx->height;
	#if 0
	window = SDL_CreateWindow("FFmpeg Decode", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		width, height, SDL_WINDOW_OPENGL);
	renderer = SDL_CreateRenderer(window, -1, 0);
	bmp = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING,
		width, height);
	
	rect.x = 0;
	rect.y = 0;
	rect.w = width;
	rect.h = height;
	#endif

	frame = av_frame_alloc();
	displayFrame = av_frame_alloc();

	displayFrame->format = AV_PIX_FMT_YUV420P;
	displayFrame->width = width;
	displayFrame->height = height;

	int numBytes = avpicture_get_size((AVPixelFormat)displayFrame->format,displayFrame->width, displayFrame->height);
	uint8_t *buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));

	avpicture_fill((AVPicture*)displayFrame, buffer, (AVPixelFormat)displayFrame->format, displayFrame->width, displayFrame->height);

	SDL_CreateThread(decode_video, "", this);

	schedule_refresh(media, 40); // start display
}

double VideoState::synchronize(AVFrame *srcFrame, double pts)
{
	double frame_delay;

	if (pts != 0)
		video_clock = pts; // Get pts,then set video clock to it
	else
		pts = video_clock; // Don't get pts,set it to video clock

	frame_delay = av_q2d(stream->codec->time_base);
	//extra_delay = repeat_pict / (2*fps)
	frame_delay += srcFrame->repeat_pict * (frame_delay * 0.5);

	video_clock += frame_delay;

	return pts;
}


int  decode_video(void *arg)
{
	VideoState *video = (VideoState*)arg;

	AVFrame *frame = av_frame_alloc();

	AVPacket packet;
	double pts;
	int got_frame = 0;
	static int count = 0;
	int delay_time = 0;
	while (true)
	{
		video->videoq->deQueue(&packet, true);
		#if 0
		int ret = avcodec_send_packet(video->video_ctx, &packet);
		if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
			continue;

		ret = avcodec_receive_frame(video->video_ctx, frame);
		if (ret < 0 && ret != AVERROR_EOF)
			continue;
		#endif
		int ret = avcodec_decode_video2(video->video_ctx, frame, &got_frame, &packet);
		if(ret < 0 || got_frame <= 0)
			continue;
		
		if ((pts = av_frame_get_best_effort_timestamp(frame)) == AV_NOPTS_VALUE)
			pts = 0;
		
		pts *= av_q2d(video->stream->time_base);

		pts = video->synchronize(frame, pts);

		frame->opaque = &pts;
		
		if(video->live_stream)
		{
			if (video->frameq.nb_frames >= video->fps/3)
			{
				delay_time = 1000/video->fps-10;
				video->speed = 1.4;
				//printf("video->speed = 1.1\n");
			}
			else if(video->frameq.nb_frames >= 1)
			{
				delay_time = 1000/video->fps-5;
				//printf("video->frameq.nb_frames >= 1\n");
			}
			else
			{
				delay_time = 0;
				video->speed = 0.8;
				//printf("video->speed = 1.0\n");
			}
		}
		else
		{
			video->speed = 1.0;
			if (video->frameq.nb_frames >= 1)
				delay_time = 1000/video->fps-5;
			else
				delay_time = 0;
		}
		
		if(delay_time)
			SDL_Delay(delay_time);
		
		//printf("external_clock: %f, put frame cout %d\n", av_gettime() / 1000000.0, count++);
		video->frameq.enQueue(frame);
		
		av_frame_unref(frame);
	}


	av_frame_free(&frame);

	return 0;
}

