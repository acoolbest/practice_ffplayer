
#include "Media.h"
#include <iostream>

extern "C"{
#include "libavutil/time.h"
}
extern bool quit;

MediaState::MediaState(string input_file, int index)
	:filename(input_file),media_index(index)
{
	pFormatCtx = nullptr;
	mutex = SDL_CreateMutex();
	cond = SDL_CreateCond();

	live_stream = (input_file.find("rtmp") != std::string::npos || input_file.find("udp") != std::string::npos) ? true : false;
	//live_stream = (input_file.find("rtmp") != std::string::npos || input_file.find("udp") != std::string::npos) ? false : true;

	audio = new AudioState(live_stream);

	video = new VideoState(live_stream);
	//quit = false;
}

MediaState::~MediaState()
{
	if(audio)
		delete audio;

	if (video)
		delete video;
}

static void calculate_display_rect(SDL_Rect *rect,
                               int scr_xleft, int scr_ytop, int scr_width, int scr_height,
                               int pic_width, int pic_height, AVRational pic_sar)
{
    float aspect_ratio;
    int width, height, x, y;

    if (pic_sar.num == 0)
        aspect_ratio = 0;
    else
        aspect_ratio = av_q2d(pic_sar);

    if (aspect_ratio <= 0.0)
        aspect_ratio = 1.0;
    aspect_ratio *= (float)pic_width / (float)pic_height;

    /* XXX: we suppose the screen has a 1.0 pixel ratio */
    height = scr_height;
    width = ((int)rint(height * aspect_ratio)) & ~1;
    if (width > scr_width) {
        width = scr_width;
        height = ((int)rint(width / aspect_ratio)) & ~1;
    }
    x = (scr_width - width) / 2;
    y = (scr_height - height) / 2;
    rect->x = scr_xleft + x;
    rect->y = scr_ytop  + y;
    rect->w = FFMAX(width,  1);
    rect->h = FFMAX(height, 1);
}

void MediaState::set_default_window_size(int width, int height, AVRational sar)
{
	calculate_display_rect(&video->rect, 0, 0, INT_MAX, height, width, height, sar);
}
bool MediaState::openInput()
{
	// Open input file
	if (avformat_open_input(&pFormatCtx, filename.c_str(), nullptr, nullptr) < 0)
		return false;

	if (avformat_find_stream_info(pFormatCtx, nullptr) < 0)
		return false;

	// Output the stream info to standard 
	av_dump_format(pFormatCtx, 0, filename.c_str(), 0);

	for (uint32_t i = 0; i < pFormatCtx->nb_streams; i++)
	{
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO && audio->stream_index < 0)
			audio->stream_index = i;

		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO && video->stream_index < 0)
		{
			video->stream_index = i;
			AVStream *st = pFormatCtx->streams[i];
	        AVCodecContext *avctx = st->codec;
	        AVRational sar = av_guess_sample_aspect_ratio(pFormatCtx, st, NULL);
	        if (avctx->width)
	        {
				set_default_window_size(avctx->width, avctx->height, sar);
				printf("%d %d\n", avctx->width, avctx->height);
			}
		}
	}

	if (audio->stream_index < 0 || video->stream_index < 0)
		return false;

	// Fill audio state
	AVCodec *pCodec = avcodec_find_decoder(pFormatCtx->streams[audio->stream_index]->codec->codec_id);
	if (!pCodec)
		return false;

	audio->stream = pFormatCtx->streams[audio->stream_index];

	audio->audio_ctx = avcodec_alloc_context3(pCodec);
	if (avcodec_copy_context(audio->audio_ctx, pFormatCtx->streams[audio->stream_index]->codec) != 0)
		return false;

	avcodec_open2(audio->audio_ctx, pCodec, nullptr);

	// Fill video state
	AVCodec *pVCodec = avcodec_find_decoder(pFormatCtx->streams[video->stream_index]->codec->codec_id);
	if (!pVCodec)
		return false;

	video->stream = pFormatCtx->streams[video->stream_index];

	video->video_ctx = avcodec_alloc_context3(pVCodec);
	if (avcodec_copy_context(video->video_ctx, pFormatCtx->streams[video->stream_index]->codec) != 0)
		return false;

	avcodec_open2(video->video_ctx, pVCodec, nullptr);

	video->frame_timer = static_cast<double>(av_gettime()) / 1000000.0;
	video->frame_last_delay = 40e-3;

	return true;
}

int decode_thread(void *data)
{
	MediaState *media = (MediaState*)data;

	//AVPacket *packet = av_packet_alloc();
	AVPacket *packet = (AVPacket *)av_malloc(sizeof(AVPacket));
	
	printf("decode_thread filename %s\n", media->filename.c_str());
	while (true)
	{
		
		int ret = av_read_frame(media->pFormatCtx, packet);
		if (ret < 0)
		{
			if (ret == AVERROR_EOF)
				break;
			if (media->pFormatCtx->pb->error == 0) // No error,wait for user input
			{
				SDL_Delay(100);
				continue;
			}
			else
				break;
		}

		if (packet->stream_index == media->audio->stream_index) // audio stream
		{
			media->audio->audioq.enQueue(packet);
			//av_packet_unref(packet);
		}		

		else if (packet->stream_index == media->video->stream_index) // video stream
		{
			media->video->videoq->enQueue(packet);
			//av_packet_unref(packet);
		}		
		//else
		//	av_packet_unref(packet);
		//av_free_packet(packet);
	}

	//av_packet_free(&packet);
	av_free_packet(packet);

	return 0;
}