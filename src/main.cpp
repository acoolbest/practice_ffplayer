extern "C" {

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

}

#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>

#include <iostream>
#include <string>
#include <vector>


#include "PacketQueue.h"
#include "Audio.h"
#include "Media.h"
#include "VideoDisplay.h"
using namespace std;

bool quit = false;

vector<string> file;

static int lockmgr(void **mtx, enum AVLockOp op)
{
   switch(op) {
      case AV_LOCK_CREATE:
          *mtx = SDL_CreateMutex();
          if(!*mtx)
              return 1;
          return 0;
      case AV_LOCK_OBTAIN:
          return !!SDL_LockMutex(*mtx);
      case AV_LOCK_RELEASE:
          return !!SDL_UnlockMutex(*mtx);
      case AV_LOCK_DESTROY:
          SDL_DestroyMutex(*mtx);
          return 0;
   }
   return 1;
}

int main(int argc, char* argv[])
{
	file.push_back("1.mp4");
	file.push_back("rtmp://192.168.0.163/hls/test");
	av_register_all();
	avformat_network_init();
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
	
	SDL_Play sdl_play = {0};
	sdl_play.window = SDL_CreateWindow("FFmpeg Decode", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		1800, 600, SDL_WINDOW_OPENGL);
	sdl_play.renderer = SDL_CreateRenderer(sdl_play.window, -1, 0);

	if (av_lockmgr_register(lockmgr)) {
        av_log(NULL, AV_LOG_FATAL, "Could not initialize lock manager!\n");
        exit(0);
    }


	
	MediaState * media[2] = {nullptr};
	for(int i=0;i<2;i++)
	{
		media[i] = new MediaState(string(file[i]), i);
		//MediaState media(filename);

		if (media[i]->openInput())
			SDL_CreateThread(decode_thread, "", media[i]); // 创建解码线程，读取packet到队列中缓存

		media[i]->audio->audio_play(); // create audio thread
		sdl_play.rect.x = i*800;
		sdl_play.rect.y = i*100;
		media[i]->video->video_play(media[i], &sdl_play); // create video thread
	}
	

	#if 0
	AVStream *audio_stream = media.pFormatCtx->streams[media.audio->stream_index];
	AVStream *video_stream = media.pFormatCtx->streams[media.video->stream_index];

	double audio_duration = audio_stream->duration * av_q2d(audio_stream->time_base);
	double video_duration = video_stream->duration * av_q2d(video_stream->time_base);

	cout << "audio时长：" << audio_duration << endl;
	cout << "video时长：" << video_duration << endl;
	#endif
	
	SDL_Event event;
	while (true) // SDL event loop
	{
		SDL_WaitEvent(&event);
		switch (event.type)
		{
		case FF_QUIT_EVENT:
			printf("FF_QUIT_EVENT\n");
		case SDL_QUIT:
			printf("SDL_QUIT\n");
			quit = 1;
			SDL_Quit();

			return 0;
			break;

		case FF_REFRESH_EVENT:
			video_refresh_timer(media[0]);
			//printf("1\n");
			break;
		case FF_REFRESH_EVENT1:
			video_refresh_timer(media[1]);
			//printf("2\n");
			break;
		default:
			break;
		}
	}

	getchar();
	return 0;
}