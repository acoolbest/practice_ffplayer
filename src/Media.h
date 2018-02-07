
#ifndef MEDIA_H
#define MEDIA_H

#include <string>
#include "Audio.h"
#include "Video.h"

using namespace std;

extern "C" {

#include <libavformat/avformat.h>

}

struct VideoState;

struct MediaState
{
	AudioState *audio;
	VideoState *video;
	AVFormatContext *pFormatCtx;

	string filename;
	//bool quit;

	MediaState(string filename);

	~MediaState();

	bool openInput();
};

int decode_thread(void *data);

#endif
