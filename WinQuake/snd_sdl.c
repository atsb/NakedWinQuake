#include <stdio.h>
#include <SDL3/SDL.h>
#include "quakedef.h"

static dma_t the_shm;
static int snd_inited;
static SDL_AudioStream *audio_stream = NULL;

extern int desired_speed;
extern int desired_bits;

qboolean SNDDMA_Init(void)
{
	SDL_AudioSpec desired, obtained;
	snd_inited = 0;
	SDL_zero(desired);
	desired.freq = desired_speed;
	
	switch (desired_bits) {
	case 8:
		desired.format = SDL_AUDIO_U8;
		break;
	case 16:
		desired.format = SDL_AUDIO_S16;
		break;
	default:
		Con_Printf("Unknown number of audio bits: %d\n", desired_bits);
		return 0;
	}
	
	desired.channels = 2;
	audio_stream = SDL_OpenAudioDeviceStream(
		SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
		&desired,
		NULL,
		NULL
	);
	
	if (!audio_stream) {
		Con_Printf("Couldn't open SDL audio: %s\n", SDL_GetError());
		return 0;
	}

	if (!SDL_GetAudioStreamFormat(audio_stream, &obtained, NULL)) {
		Con_Printf("Couldn't get audio stream format: %s\n", SDL_GetError());
		SDL_DestroyAudioStream(audio_stream);
		audio_stream = NULL;
		return 0;
	}

	SDL_ResumeAudioStreamDevice(audio_stream);

	shm = &the_shm;
	shm->splitbuffer = 0;
	
	switch (obtained.format) {
	case SDL_AUDIO_U8:
		shm->samplebits = 8;
		break;
	case SDL_AUDIO_S16:
		shm->samplebits = 16;
		break;
	default:
		shm->samplebits = 16;
		break;
	}
	
	shm->speed = obtained.freq;
	shm->channels = obtained.channels;
	shm->samples = 512 * shm->channels;
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	
	int buffer_size = shm->samples * (shm->samplebits / 8);
	shm->buffer = (unsigned char*)malloc(buffer_size);
	
	if (!shm->buffer) {
		Con_Printf("Couldn't allocate audio buffer\n");
		SDL_DestroyAudioStream(audio_stream);
		audio_stream = NULL;
		return 0;
	}
	
	if (shm->samplebits == 8)
		memset(shm->buffer, 0x80, buffer_size);
	else
		memset(shm->buffer, 0, buffer_size);

	snd_inited = 1;
	
	Con_Printf("Sound initialized: %d Hz, %d bits, %d channels, %d samples\n",
	           shm->speed, shm->samplebits, shm->channels, shm->samples);
	
	return 1;
}

int SNDDMA_GetDMAPos(void)
{
	return shm->samplepos;
}

void SNDDMA_Shutdown(void)
{
	if (snd_inited)
	{
		if (audio_stream) {
			SDL_PauseAudioStreamDevice(audio_stream);
			SDL_DestroyAudioStream(audio_stream);
			audio_stream = NULL;
		}
		
		if (shm && shm->buffer) {
			free(shm->buffer);
			shm->buffer = NULL;
		}
		
		snd_inited = 0;
	}
}

void SNDDMA_Submit(void)
{
	if (!snd_inited || !audio_stream || !shm || !shm->buffer)
		return;

	int bytes_per_sample = (shm->samplebits / 8) * shm->channels;
	int queued_bytes = SDL_GetAudioStreamQueued(audio_stream);
	int target_bytes = (shm->speed / 10) * bytes_per_sample;
	
	if (queued_bytes < target_bytes / 2) {
		int chunk_samples = 512;
		int chunk_bytes = chunk_samples * bytes_per_sample;
		shm->samplepos += chunk_samples;
		S_PaintChannels(shm->samplepos);
		SDL_PutAudioStreamData(audio_stream, shm->buffer, chunk_bytes);
	}
}

