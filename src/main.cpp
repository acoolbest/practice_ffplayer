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
#include <thread>
#include <pthread.h>
#include <unistd.h>

#include "PacketQueue.h"
#include "Audio.h"
#include "Media.h"
#include "VideoDisplay.h"
using namespace std;

//#define PTHREAD_FRESH

bool quit = false;

vector<string> file;
SDL_Rect rect[3] = {{0,0},{900,100},{0,500}};
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
	//file.push_back("rtmp://192.168.0.163/hls/test129");
	//file.push_back("1.flv");
	av_register_all();
	avformat_network_init();
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
	
	SDL_Play sdl_play = {0};
	sdl_play.window = SDL_CreateWindow("FFmpeg Decode", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		1280, 720, SDL_WINDOW_OPENGL);
	sdl_play.renderer = SDL_CreateRenderer(sdl_play.window, -1, 0);

	if (av_lockmgr_register(lockmgr)) {
        av_log(NULL, AV_LOG_FATAL, "Could not initialize lock manager!\n");
        exit(0);
    }

	pthread_t ntid;
	
	MediaState * media[3] = {nullptr};
	for(int i=0;i<file.size();i++)
	{
		media[i] = new MediaState(string(file[i]), i);
		//MediaState media(filename);

		if (media[i]->openInput())
			SDL_CreateThread(decode_thread, "", media[i]); // 创建解码线程，读取packet到队列中缓存

		media[i]->audio->audio_play(); // create audio thread
		sdl_play.rect.x = rect[i].x;
		sdl_play.rect.y = rect[i].y;
		media[i]->video->video_play(media[i], &sdl_play); // create video thread
		//std::thread(video_refresh_timer, media[i]).detach();
		//pthread_create(&ntid, NULL, video_refresh_timer, media[i]);
		//pthread_detach(ntid);
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
		#if 0
		if(event.type == FF_QUIT_EVENT)
		{
			printf("FF_QUIT_EVENT");
			break;
		}
		else if(event.type == SDL_QUIT)
		{
			printf("SDL_QUITn");
			quit = 1;
			break;
		}
		else if(event.type == file.size() + FF_REFRESH_EVENT)
		{
			//video_refresh_timer(media);
			std::thread (video_refresh_timer,media[event.type-FF_REFRESH_EVENT]).detach();
			continue;
		}
		#else
		switch (event.type)
		{
		case FF_QUIT_EVENT:
			printf("FF_QUIT_EVENT\n");
		case SDL_QUIT:
			printf("SDL_QUIT\n");
			quit = 1;
			SDL_Quit();
			return 0;
		case FF_REFRESH_EVENT:
			#if 0
			SDL_CondSignal(media[0]->cond);
			#else
			#ifdef PTHREAD_FRESH
			pthread_create(&ntid, NULL, video_refresh_timer, media[0]);
			pthread_detach(ntid);
			//usleep(20*1000);
			#else
			//std::thread (video_refresh_timer,media[0]).detach();
			video_refresh_timer(media[0]);
			#endif
			#endif
			break;
		case FF_REFRESH_EVENT1:
			SDL_CondSignal(media[1]->cond);
			
			#ifdef PTHREAD_FRESH
			pthread_create(&ntid, NULL, video_refresh_timer, media[1]);
			pthread_detach(ntid);
			usleep(20*1000);
			#else
			//std::thread (video_refresh_timer,media[1]).detach();
			video_refresh_timer(media[1]);
			#endif
			
			break;
		case FF_REFRESH_EVENT2:
			SDL_CondSignal(media[2]->cond);
			
			#ifdef PTHREAD_FRESH
			pthread_create(&ntid, NULL, video_refresh_timer, media[2]);
			pthread_detach(ntid);
			usleep(20*1000);
			#else
			//std::thread (video_refresh_timer,media[2]).detach();
			video_refresh_timer(media[2]);
			#endif
			
			break;
		case SDL_KEYDOWN:
			switch (event.key.keysym.sym) {
				case SDLK_LEFT:
					if(media[0]->audio->speed == media[0]->audio->old_speed && media[0]->audio->speed >= 0.8)
						media[0]->audio->speed -= 0.1;
					break;
				case SDLK_RIGHT:
					if(media[0]->audio->speed == media[0]->audio->old_speed && media[0]->audio->speed <= 1.4)
						media[0]->audio->speed += 0.1;
					break;
				case SDLK_UP:
					if(media[0]->audio->speed == media[0]->audio->old_speed && media[0]->audio->speed <= 1.3)
						media[0]->audio->speed +=0.2;
					break;
				case SDLK_DOWN:
					if(media[0]->audio->speed == media[0]->audio->old_speed && media[0]->audio->speed >= 0.9)
						media[0]->audio->speed -=0.2;
					break;
			}
			printf("speed %f\n", media[0]->audio->speed);
			break;
		default:
			break;
		}
		#endif
	}
	getchar();
	return 0;
}