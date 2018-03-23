
#ifndef AUDIO_H
#define AUDIO_H

#include "PacketQueue.h"
extern "C"{

#include <libavformat/avformat.h>

}

#include <soundtouch/SoundTouch.h>

/**
 * ����audioʱ����Ҫ�����ݷ�װ
 */

class AudioSample{
public:
	int raw_size;
	int channels;
	int samples_number;
	int pcm_size;
	uint8_t *audio_buf;
	AudioSample(){ raw_size = 0; channels = 0; samples_number = 0;}
	AudioSample(int raw, int ch, int nb){  raw_size = raw; channels = ch; samples_number = nb, pcm_size = 0; audio_buf = new uint8_t[192000]; }
	~AudioSample(){ if (audio_buf) delete[] audio_buf; }
};

class SampleQueue
{
public:

	static const int capacity = 30;// ʵ��ÿ��20�����30���������ܵ���ǰ�������ݵ�

	std::queue<AudioSample*> queue;

	uint32_t nb_frames;

	SDL_mutex* mutex;
	SDL_cond * cond;

	SampleQueue()
	{
		nb_frames = 0;
		mutex     = SDL_CreateMutex();
		cond      = SDL_CreateCond();
	}
	
	bool enQueue(AudioSample* sample)
	{
		SDL_LockMutex(mutex);
		queue.push(sample);
		nb_frames++;
		SDL_CondSignal(cond);
		SDL_UnlockMutex(mutex);
		return true;
	}

	bool deQueue(AudioSample **sample)
	{
		bool ret = true;

		SDL_LockMutex(mutex);
		while (true)
		{
			if (!queue.empty())
			{
				*sample = queue.front();
				queue.pop();
				nb_frames--;
				ret = true;
				break;
			}
			else
			{
				SDL_CondWait(cond, mutex);
			}
		}

		SDL_UnlockMutex(mutex);
		return ret;
	}
};

class AudioState
{
public:
	static const AVSampleFormat dst_format = AV_SAMPLE_FMT_S16;
	const uint32_t BUFFER_SIZE;// �������Ĵ�С

	PacketQueue audioq;

	double audio_clock; // audio clock
	AVStream *stream; // audio stream

	SampleQueue sampleq;          // ������������Ƶ�����Ƶ����
	
	uint8_t *audio_buff;       // ��������ݵĻ���ռ�
	uint32_t audio_buff_size;  // buffer�е��ֽ���
	uint32_t audio_buff_index; // buffer��δ�������ݵ�index
	
	int stream_index;          // audio��index
	AVCodecContext *audio_ctx; // �Ѿ�����avcodec_open2��

	bool live_stream;
	double speed;
	soundtouch::SoundTouch s_touch;
	soundtouch::SAMPLETYPE touch_buffer_put[96000];
	soundtouch::SAMPLETYPE touch_buffer_recv[96000];
	SDL_mutex* mutex;
	
	AudioState(bool live);              //Ĭ�Ϲ��캯��
	AudioState(AVCodecContext *audio_ctx, int audio_stream);
	
	~AudioState();

	/**
	* audio play
	*/
	bool audio_play();

	// get audio clock
	double get_audio_clock();

	int sound_touch_recv(uint8_t *audio_buf, int len, int channels);
	void sound_touch_put(uint8_t *audio_buf, int data_size, int nb);
};



/**
 * ���豸����audio���ݵĻص�����
 */
void audio_callback(void* userdata, Uint8 *stream, int len);

/**
 * ����Avpacket�е�������䵽����ռ�
 */
int audio_decode_frame(AudioState *audio_state, uint8_t *audio_buf, int buf_size);

int deQueue_sample(AudioState *audio, uint8_t *audio_buf, int buf_size);

int  decode_audio(void *arg);
#endif