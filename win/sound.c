#if defined(SOUND_SUPPORT)

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

HWAVEOUT hWaveOut = INVALID_HANDLE_VALUE;

SoundFrame sound_buffer[256]; /* Must be >= Sound_BatchSize! */

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

	UNUSED_VAR(hwo);
	UNUSED_VAR(dwParam1);
	UNUSED_VAR(dwParam2);

	if(uMsg != WOM_DONE)
		return;

	EnterCriticalSection(&waveCriticalSection);
	(*freeBlockCounter)++;
	LeaveCriticalSection(&waveCriticalSection);
}

SoundFrame *Sound_GetHostBuffer(size_t *destavail)
{
	/* Just assume we always have enough space for the max batch size */
	*destavail = countof(sound_buffer);
	return sound_buffer;
}

void Sound_HostBuffered(SoundFrame *buffer,size_t numFrames)
{
	LPSTR lpbuffer = (LPSTR)buffer;
	DWORD_PTR size = numFrames * sizeof(SoundFrame);

	WAVEHDR* current;
	DWORD_PTR remain;
	current = &waveBlocks[waveCurrentBlock];
	while(size > 0) {
		/*
		 * first make sure the header we're going to use is unprepared
		 */
		if(current->dwFlags & WHDR_PREPARED)
			waveOutUnprepareHeader(hWaveOut, current, sizeof(WAVEHDR));

		if(size < (BLOCK_SIZE - current->dwUser)) {
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
			warn_sound("Waiting for a free block");
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

bool
Sound_InitHost(ARMul_State *state)
{
	WAVEFORMATEX format;
	LPSTR buffer;
	int i;

	DWORD totalBufferSize = (BLOCK_SIZE + sizeof(WAVEHDR)) * BLOCK_COUNT;
	if((buffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, totalBufferSize)) == NULL) {
		ControlPane_Error(false,"Memory allocation error");
		return false;
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
	format.wBitsPerSample = sizeof(SoundData) * 8;
	format.nSamplesPerSec = 44100;
	format.nBlockAlign = sizeof(SoundFrame);
	format.nAvgBytesPerSec =  format.nSamplesPerSec * sizeof(SoundFrame);

	if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &format, (DWORD_PTR)sound_callback, (DWORD_PTR)&waveFreeBlockCount, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
		ControlPane_Error(false,"Failed to initialise the sound system");
		Sound_ShutdownHost(state);
		return false;
	}

	/* TODO - Tweak these as necessary */
	eSound_StereoSense = Stereo_LeftRight;

	Sound_BatchSize = 256;

	Sound_HostRate = format.nSamplesPerSec<<10;

	return true;
}

void
Sound_ShutdownHost(ARMul_State *state)
{
	int i;

	UNUSED_VAR(state);

	if (hWaveOut == INVALID_HANDLE_VALUE)
		return;

	waveOutReset(hWaveOut);
	assert(waveFreeBlockCount >= BLOCK_COUNT);

	for(i = 0; i < waveFreeBlockCount; i++)
		if(waveBlocks[i].dwFlags & WHDR_PREPARED)
			waveOutUnprepareHeader(hWaveOut, &waveBlocks[i], sizeof(WAVEHDR));

	DeleteCriticalSection(&waveCriticalSection);
	HeapFree(GetProcessHeap(), 0, waveBlocks);
	waveBlocks = NULL;
	waveOutClose(hWaveOut);
	hWaveOut = INVALID_HANDLE_VALUE;
}

#endif
