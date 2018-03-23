
#include "Audio.h"

#include <iostream>
#include <fstream>
extern "C" {

#include <libswresample/swresample.h>
#include <libavutil/time.h>

}

extern bool quit;

#define SOUND_TOUCH

AudioState::AudioState(bool live)
	:BUFFER_SIZE(192000*16)
{
	audio_ctx = nullptr;
	stream_index = -1;
	stream = nullptr;
	audio_clock = 0;

	audio_buff = new uint8_t[BUFFER_SIZE];
	audio_buff_size = 0;
	audio_buff_index = 0;
	live_stream = live;
	speed = 1.9;

	s_touch.setSampleRate(44100); // ���ò�����
	s_touch.setChannels(2); // ����ͨ����

	////////////////////////////////////////////
	// ���� rate����pitch�ĸı����
	s_touch.setRate(speed); // �����ٶ�Ϊ0.5��ԭʼ��Ϊ1.0
	//s_touch.setRateChange(-50.0);
	//s_touch.setPitch(0.1);
	s_touch.setSetting(SETTING_USE_QUICKSEEK, 1); 
	s_touch.setSetting(SETTING_USE_AA_FILTER, 1);
	mutex     = SDL_CreateMutex();
}

AudioState::AudioState(AVCodecContext *audioCtx, int index)
	:BUFFER_SIZE(192000)
{
	audio_ctx = audioCtx;
	stream_index = index;
	

	audio_buff = new uint8_t[BUFFER_SIZE];
	audio_buff_size = 0;
	audio_buff_index = 0;
	mutex     = SDL_CreateMutex();
}

AudioState::~AudioState()
{
	if (audio_buff)
		delete[] audio_buff;
}

bool AudioState::audio_play()
{
	SDL_AudioSpec desired;
	desired.freq = audio_ctx->sample_rate;
	desired.channels = audio_ctx->channels;
	desired.format = AUDIO_S16SYS;
	desired.samples = 1024;
	desired.silence = 0;
	desired.userdata = this;
	desired.callback = audio_callback;

	if (SDL_OpenAudio(&desired, nullptr) < 0)
	{
		return false;
	}
	#ifdef SOUND_TOUCH
	SDL_CreateThread(decode_audio, "", this);
	#endif
	SDL_PauseAudio(0); // playing
	return true;
}

double AudioState::get_audio_clock()
{
	int hw_buf_size = audio_buff_size - audio_buff_index;
	int bytes_per_sec = stream->codec->sample_rate * audio_ctx->channels * 2;

	double pts = audio_clock - static_cast<double>(hw_buf_size) / bytes_per_sec;

	
	return pts;
}

/**
* ���豸����audio���ݵĻص�����
*/
void audio_callback(void* userdata, Uint8 *stream, int len)
{
	AudioState *audio_state = (AudioState*)userdata;

	SDL_memset(stream, 0, len);

	int audio_size = 0;
	int len1 = 0;
	while (len > 0)// ���豸���ͳ���Ϊlen������
	{
		if (audio_state->audio_buff_index >= audio_state->audio_buff_size) // ��������������
		{
			// �Ӷ����ж�ȡ�ѽ��������
			#ifdef SOUND_TOUCH
			audio_size = deQueue_sample(audio_state, audio_state->audio_buff, sizeof(audio_state->audio_buff));
			#else
			audio_size = audio_decode_frame(audio_state, audio_state->audio_buff, sizeof(audio_state->audio_buff));
			#endif
			if (audio_size < 0) // û�н��뵽���ݻ�������0
			{
				audio_state->audio_buff_size = 0;
				memset(audio_state->audio_buff, 0, audio_state->BUFFER_SIZE);
			}
			else
				audio_state->audio_buff_size = audio_size;

			audio_state->audio_buff_index = 0;
		}
		len1 = audio_state->audio_buff_size - audio_state->audio_buff_index; // ��������ʣ�µ����ݳ���
		if (len1 > len) // ���豸���͵����ݳ���Ϊlen
			len1 = len;

		SDL_MixAudio(stream, audio_state->audio_buff + audio_state->audio_buff_index, len, SDL_MIX_MAXVOLUME);

		len -= len1;
		stream += len1;
		audio_state->audio_buff_index += len1;
	}
}

/*
������
setChannels(int) ����������1 = mono������, 2 = stereo������
setSampleRate(uint) ���ò�����

���ʣ�
setRate(double) ָ���������ʣ�ԭʼֵΪ1.0�����С��
setTempo(double) ָ�����ģ�ԭʼֵΪ1.0�����С��
setRateChange(double)��setTempoChange(double) ��ԭ��1.0�����ϣ����ٷֱ���������ȡֵ(-50 .. +100 %)

������
setPitch(double) ָ������ֵ��ԭʼֵΪ1.0
setPitchOctaves(double) ��ԭ�����������԰˶���Ϊ��λ���е�����ȡֵΪ[-1.00,+1.00]
setPitchSemiTones(int) ��ԭ�����������԰���Ϊ��λ���е�����ȡֵΪ[-12,+12]

���ϵ�����������������е�λ���㣬��������ͬ�Ĵ�������calcEffectiveRateAndTempo()��
���������Բ���û�����½������ƣ�ֻ�ǲ�������ʧ��Խ��
SemiToneָ������ͨ��˵�ġ���1��key�����ǽ���1��������
��������Ϊʹ��SemiToneΪ��λ�����������󣬲���������⡣

����
putSamples(const SAMPLETYPE *samples, uint nSamples) �����������
receiveSamples(SAMPLETYPE *output, uint maxSamples) ������������ݣ���Ҫѭ��ִ��
flush() �������ܵ��е����һ�顰�����������ݣ�Ӧ�����ִ��
*/

int AudioState::sound_touch_recv(uint8_t *audio_buf, int samples_number, int channels)
{
	static int count = 0;
	auto len = channels * samples_number * av_get_bytes_per_sample(dst_format);
	
	SDL_LockMutex(mutex);
	auto nb = s_touch.receiveSamples(touch_buffer_recv, samples_number);
	SDL_UnlockMutex(mutex);
	
	auto length = nb * channels * av_get_bytes_per_sample(dst_format);
	
	for (auto i = 0; i < length/2; i++)
	{
		audio_buf[i*2] = uint8_t(touch_buffer_recv[i]);
		audio_buf[i*2+1] = uint8_t(touch_buffer_recv[i]>>8);
	}
	
	//printf("external_clock: %f, pcm_size: %d, count: %d\n", av_gettime() / 1000000.0, length, count++);
	return length;
}


void AudioState::sound_touch_put(uint8_t *audio_buf, int data_size, int nb)
{
	for (auto i = 0; i < data_size; i++)
	{
		touch_buffer_put[i] = (audio_buf[i * 2] | (audio_buf[i * 2 + 1] << 8));
	}
	
	SDL_LockMutex(mutex);
	s_touch.putSamples(touch_buffer_put, nb);
	SDL_UnlockMutex(mutex);
}

int audio_decode_frame(AudioState *audio_state, uint8_t *audio_buf, int buf_size)
{
	int ret = -1;
	static uint8_t audio_raw_buffer[192000] = {0};
	static uint8_t *p_audio_raw_buff = audio_raw_buffer;
	AVFrame *frame = av_frame_alloc();
	int data_size = 0;
	AVPacket pkt;
	SwrContext *swr_ctx = nullptr;
	static double clock = 0;
	int got_frame = 0;
	
	if (quit)
		goto _AUDIO_DECODE_END_;
	if (!audio_state->audioq.deQueue(&pkt, true))
		goto _AUDIO_DECODE_END_;

	if (pkt.pts != AV_NOPTS_VALUE)
	{
		audio_state->audio_clock = av_q2d(audio_state->stream->time_base) * pkt.pts;
	}
	
	#if 0
	int ret = avcodec_send_packet(audio_state->audio_ctx, &pkt);
	if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
		return -1;

	ret = avcodec_receive_frame(audio_state->audio_ctx, frame);
	if (ret < 0 && ret != AVERROR_EOF)
		return -1;
	#endif
	
	ret = avcodec_decode_audio4(audio_state->audio_ctx, frame, &got_frame, &pkt);
	if(ret < 0 || got_frame <= 0)
		goto _AUDIO_DECODE_END_;

	// ����ͨ������channel_layout
	if (frame->channels > 0 && frame->channel_layout == 0)
		frame->channel_layout = av_get_default_channel_layout(frame->channels);
	else if (frame->channels == 0 && frame->channel_layout > 0)
		frame->channels = av_get_channel_layout_nb_channels(frame->channel_layout);

	AVSampleFormat dst_format = AV_SAMPLE_FMT_S16;//av_get_packed_sample_fmt((AVSampleFormat)frame->format);
	Uint64 dst_layout = av_get_default_channel_layout(frame->channels);
	// ����ת������
	swr_ctx = swr_alloc_set_opts(nullptr, dst_layout, dst_format, frame->sample_rate,
		frame->channel_layout, (AVSampleFormat)frame->format, frame->sample_rate, 0, nullptr);
	if (!swr_ctx || swr_init(swr_ctx) < 0)
	{
		ret = -1;
		goto _AUDIO_DECODE_END_;
	}

	// ����ת�����sample���� a * b / c
	uint64_t dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, frame->sample_rate) + frame->nb_samples, frame->sample_rate, frame->sample_rate, AVRounding(1));
	// ת��������ֵΪת�����sample����

	int nb = swr_convert(swr_ctx, &audio_buf, static_cast<int>(dst_nb_samples), (const uint8_t**)frame->data, frame->nb_samples);

	data_size = frame->channels * nb * av_get_bytes_per_sample(dst_format);
	ret = data_size;
	printf("data_size %d\n", data_size);
	// ÿ������Ƶ���ŵ��ֽ��� sample_rate * channels * sample_format(һ��sampleռ�õ��ֽ���)
	audio_state->audio_clock += static_cast<double>(data_size) / (2 * audio_state->stream->codec->channels * audio_state->stream->codec->sample_rate);
	
_AUDIO_DECODE_END_:
	av_frame_free(&frame);
	if(swr_ctx)
		swr_free(&swr_ctx);

	return ret;
}


int deQueue_sample(AudioState *audio, uint8_t *audio_buf, int buf_size)
{
	int ret = -1;
	AudioSample *sample = nullptr;
	
	if (audio->stream_index >= 0)
	{
		if (audio->sampleq.queue.empty())
		{
			return -1;
		}
		else
		{
			audio->sampleq.deQueue(&sample);
			if(sample)
			{
				if(sample->raw_size && sample->channels && sample->samples_number)
				{
					//printf("sample->raw_size: %d\n", sample->raw_size);
					int data_size = audio->sound_touch_recv(audio_buf, int((double)sample->samples_number/audio->speed), sample->channels);
					audio->audio_clock += static_cast<double>(data_size) / (2 * audio->stream->codec->channels * audio->stream->codec->sample_rate);
					ret = data_size;
				}
				//printf("raw_size %d\n", sample->raw_size);
				delete sample;
				sample = nullptr;
			}
		}
	}
	return ret;
}

int  decode_audio(void *arg)
{
	AudioState *audio = (AudioState*)arg;

	AVFrame *frame = nullptr;

	AVPacket packet;

	int got_frame = 0;

	AVSampleFormat dst_format = AV_SAMPLE_FMT_S16;//av_get_packed_sample_fmt((AVSampleFormat)frame->format);
	Uint64 dst_layout = 0;
	SwrContext *swr_ctx = nullptr;

	AudioSample *sample = nullptr;
	uint8_t *audio_buf = new uint8_t[192000];
	int raw_size = 0;

	static int count = 0;
	while (!quit)
	{
		frame = av_frame_alloc();
		audio->audioq.deQueue(&packet, true);
		if (packet.pts != AV_NOPTS_VALUE)
		{
			audio->audio_clock = av_q2d(audio->stream->time_base) * packet.pts;
		}

		int ret = avcodec_decode_audio4(audio->audio_ctx, frame, &got_frame, &packet);
		if(ret < 0 || got_frame <= 0)
		{
			av_frame_free(&frame);
			sample = new AudioSample();
			audio->sampleq.enQueue(sample);
			continue;
		}

		// ����ͨ������channel_layout
		if (frame->channels > 0 && frame->channel_layout == 0)
			frame->channel_layout = av_get_default_channel_layout(frame->channels);
		else if (frame->channels == 0 && frame->channel_layout > 0)
			frame->channels = av_get_channel_layout_nb_channels(frame->channel_layout);
		
		dst_layout = av_get_default_channel_layout(frame->channels);

		// ����ת������
		swr_ctx = swr_alloc_set_opts(nullptr, dst_layout, dst_format, frame->sample_rate,
			frame->channel_layout, (AVSampleFormat)frame->format, frame->sample_rate, 0, nullptr);
		if (!swr_ctx || swr_init(swr_ctx) < 0)
		{
			swr_free(&swr_ctx);
			av_frame_free(&frame);
			sample = new AudioSample();
			audio->sampleq.enQueue(sample);
			continue;
		}

		// ����ת�����sample���� a * b / c
		uint64_t dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, frame->sample_rate) + frame->nb_samples, frame->sample_rate, frame->sample_rate, AVRounding(1));
		// ת��������ֵΪת�����sample����
		
		int nb = swr_convert(swr_ctx, &audio_buf, static_cast<int>(dst_nb_samples), (const uint8_t**)frame->data, frame->nb_samples);

		int raw_size = frame->channels * nb * av_get_bytes_per_sample(dst_format);
		//printf("input size %d, count: %d\n", raw_size, count++);
		audio->sound_touch_put(audio_buf, raw_size, nb);
		#if 0
		if (audio->sampleq.nb_frames >= SampleQueue::capacity) SDL_Delay(500);
		else if(audio->sampleq.nb_frames >= SampleQueue::capacity/2) SDL_Delay(250);
		else SDL_Delay(5);
		#endif
		if(audio->sampleq.nb_frames >= 3) SDL_Delay(10);
		
		sample = new AudioSample(raw_size, frame->channels, nb);
		audio->sampleq.enQueue(sample);
		
		av_frame_free(&frame);
	}
	
	return 0;
}

