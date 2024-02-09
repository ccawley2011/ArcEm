#include "win.h"
#include "../armdefs.h"
#include "../arch/sound.h"
#include "../arch/ControlPane.h"
#include "../arch/dbugsys.h"
#include "../arch/displaydev.h"

#include <stdio.h>
#include <assert.h>
#include <windows.h>
#include <mmsystem.h>

#include "../armemu.h"

#define SSD_Name(x) ssd_ ## x
#define SSD_SoundData int16_t

HWAVEOUT hWaveOut;

SSD_SoundData sound_buffer[256*2]; /* Must be >= 2*Sound_BatchSize! */

static SSD_SoundData *SSD_Name(GetHostBuffer)(ARMul_State *state,int32_t *destavail);
static void SSD_Name(HostBuffered)(ARMul_State *state,SSD_SoundData *buffer, int32_t numSamples);
static int SSD_Name(InitHost)(ARMul_State *state);
static void SSD_Name(QuitHost)(ARMul_State *state);

#include "stdsounddev.c"

/*
 * some good values for block size and count
 */
#define BLOCK_SIZE 8192
#define BLOCK_COUNT 20
/*
 * module level variables
 */
static CRITICAL_SECTION waveCriticalSection;
static WAVEHDR* waveBlocks;
static volatile int waveFreeBlockCount;
static int waveCurrentBlock;

static void CALLBACK sound_callback(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	int* freeBlockCounter = (int*)dwInstance;

	if(uMsg != WOM_DONE)
		return;

	EnterCriticalSection(&waveCriticalSection);
	(*freeBlockCounter)++;
	LeaveCriticalSection(&waveCriticalSection);
}

static SSD_SoundData *SSD_Name(GetHostBuffer)(ARMul_State *state,int32_t *destavail)
{
	/* Just assume we always have enough space for the max batch size */
	*destavail = sizeof(sound_buffer)/(sizeof(SSD_SoundData)*2);
	return sound_buffer;
}

static void SSD_Name(HostBuffered)(ARMul_State *state,SSD_SoundData *buffer,int32_t numSamples)
{
	LPSTR lpbuffer = (LPSTR)buffer;
	DWORD_PTR size = numSamples * 2 * sizeof(SSD_SoundData);

	WAVEHDR* current;
	DWORD_PTR remain;
	current = &waveBlocks[waveCurrentBlock];
	while(size > 0) {
		/*
		 * first make sure the header we're going to use is unprepared
		 */
		if(current->dwFlags & WHDR_PREPARED)
			waveOutUnprepareHeader(hWaveOut, current, sizeof(WAVEHDR));

		if(size < (int)(BLOCK_SIZE - current->dwUser)) {
			memcpy(current->lpData + current->dwUser, lpbuffer, size);
			current->dwUser += size;
			break;
		}
		remain = BLOCK_SIZE - current->dwUser;
		memcpy(current->lpData + current->dwUser, lpbuffer, remain);
		size -= remain;
		lpbuffer += remain;
		current->dwBufferLength = BLOCK_SIZE;
		waveOutPrepareHeader(hWaveOut, current, sizeof(WAVEHDR));
		waveOutWrite(hWaveOut, current, sizeof(WAVEHDR));
		EnterCriticalSection(&waveCriticalSection);
		waveFreeBlockCount--;
		LeaveCriticalSection(&waveCriticalSection);
		/*
		 * wait for a block to become free
		 */
		while(!waveFreeBlockCount) {
			fprintf(stderr, "Waiting for a free block");
			Sleep(10);
		}
		/*
		 * point to the next block
		 */
		waveCurrentBlock++;
		waveCurrentBlock%= BLOCK_COUNT;
		current = &waveBlocks[waveCurrentBlock];
		current->dwUser = 0;
	}
}

static int SSD_Name(InitHost)(ARMul_State *state)
{
	WAVEFORMATEX format;
	char* buffer;
	int i;

	DWORD totalBufferSize = (BLOCK_SIZE + sizeof(WAVEHDR)) * BLOCK_COUNT;
	if((buffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, totalBufferSize)) == NULL) {
		fprintf(stderr, "Memory allocation error\n");
		ExitProcess(1);
	}

	waveBlocks = (WAVEHDR*)buffer;
	buffer += sizeof(WAVEHDR) * BLOCK_COUNT;
	for(i = 0; i < BLOCK_COUNT; i++) {
		waveBlocks[i].dwBufferLength = BLOCK_SIZE;
		waveBlocks[i].lpData = buffer;
		buffer += BLOCK_SIZE;
	}

	waveFreeBlockCount = BLOCK_COUNT;
	waveCurrentBlock= 0;
	InitializeCriticalSection(&waveCriticalSection);

	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = 2;
	format.wBitsPerSample = 16;
	format.nSamplesPerSec = 44100;
	format.nBlockAlign = format.nChannels * (format.wBitsPerSample / 8);
	format.nAvgBytesPerSec =  format.nSamplesPerSec * format.nBlockAlign;

	if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &format, (DWORD_PTR)sound_callback, (DWORD_PTR)&waveFreeBlockCount, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
		fprintf(stderr, "Failed to initialise the sound system\n");
		HeapFree(GetProcessHeap(), 0, waveBlocks);
		return -1;
	}

	/* TODO - Tweak these as necessary */
	SI.StereoSense = Stereo_LeftRight;

	SI.BatchSize = 256;

	SI.HostRate = format.nSamplesPerSec<<10;

	return 0;
}

static void SSD_Name(QuitHost)(ARMul_State *state)
{
	int i;

	waveOutReset(hWaveOut);
	assert(waveFreeBlockCount >= BLOCK_COUNT);

	for(i = 0; i < waveFreeBlockCount; i++)
		if(waveBlocks[i].dwFlags & WHDR_PREPARED)
			waveOutUnprepareHeader(hWaveOut, &waveBlocks[i], sizeof(WAVEHDR));

	DeleteCriticalSection(&waveCriticalSection);
	HeapFree(GetProcessHeap(), 0, waveBlocks);
	waveOutClose(hWaveOut);
}

/*-----------------------------------------------------------------------------*/
int
SoundDev_Init(ARMul_State *state)
{
	/* Setup sound device */
	return SoundDev_Set(state, &SSD_SoundDev);
}
