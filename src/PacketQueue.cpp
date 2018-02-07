
#include "PacketQueue.h"
#include <iostream>

extern bool quit;

PacketQueue::PacketQueue()
{
	nb_packets = 0;
	size       = 0;

	mutex      = SDL_CreateMutex();
	cond       = SDL_CreateCond();
}

bool PacketQueue::enQueue(const AVPacket *packet)
{
	#if 0
	AVPacket *pkt = av_packet_alloc();
	if (av_packet_ref(pkt, packet) < 0)
		return false;
	#endif
	
	if(av_dup_packet(packet) < 0)
		return false;
	AVPacket pkt = *packet;
	
	SDL_LockMutex(mutex);
	queue.push(pkt);

	size += pkt.size;
	nb_packets++;

	SDL_CondSignal(cond);
	SDL_UnlockMutex(mutex);
	return true;
}

bool PacketQueue::deQueue(AVPacket *packet, bool block)
{
	bool ret = false;

	SDL_LockMutex(mutex);
	while (true)
	{
		if (quit)
		{
			ret = false;
			break;
		}

		if (!queue.empty())
		{
			#if 0
			if (av_packet_ref(packet, &queue.front()) < 0)
			{
				ret = false;
				break;
			}
			AVPacket pkt = queue.front();

			queue.pop();
			av_packet_unref(&pkt);
			#endif
			*packet = queue.front();
			queue.pop();
			nb_packets--;
			size -= packet->size;

			ret = true;
			break;
		}
		else if (!block)
		{
			ret = false;
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