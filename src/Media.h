
#ifndef MEDIA_H
#define MEDIA_H

#include <string>
#include "Audio.h"
#include "Video.h"

using namespace std;

extern "C" {

#include <libavformat/avformat.h>

}

class VideoState;


class MediaState
{
public:

	AudioState *audio;
	VideoState *video;
	AVFormatContext *pFormatCtx;

	int media_index;
	string filename;
	//bool quit;
	SDL_mutex *mutex;
	SDL_cond  *cond;

	bool live_stream;
	
	MediaState(string filename, int index);

	~MediaState();

	bool openInput();

	void set_default_window_size(int width, int height, AVRational sar);
	
	
};

int decode_thread(void *data);


#endif
