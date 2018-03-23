
#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>
#include "PacketQueue.h"
#include "FrameQueue.h"
#include "Media.h"

class MediaState;
/**
 * ������Ƶ��������ݷ�װ
 */


class VideoState
{
public:
	PacketQueue* videoq;        // �����video packet�Ķ��л���

	int stream_index;           // index of video stream
	AVCodecContext *video_ctx;  // have already be opened by avcodec_open2
	AVStream *stream;           // video stream

	FrameQueue frameq;          // ���������ԭʼ֡����
	AVFrame *frame;
	AVFrame *displayFrame;

	double frame_timer;         // Sync fields
	double frame_last_pts;
	double frame_last_delay;
	double video_clock;

	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Texture *bmp;
	SDL_Rect rect;

	void video_play(MediaState *media, SDL_Play *sdl_play);

	double synchronize(AVFrame *srcFrame, double pts);

	bool live_stream;

	double speed;
	
	VideoState(bool live);

	~VideoState();
};

int decode_video(void *arg); // ��packet���룬����������Frame����FrameQueue������


#endif