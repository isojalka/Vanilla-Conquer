// TiberianDawn.DLL and RedAlert.dll and corresponding source code is free
// software: you can redistribute it and/or modify it under the terms of
// the GNU General Public License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.

// TiberianDawn.DLL and RedAlert.dll and corresponding source code is distributed
// in the hope that it will be useful, but with permitted additional restrictions
// under Section 7 of the GPL. See the GNU General Public License in LICENSE.TXT
// distributed with this program. You should have received a copy of the
// GNU General Public License along with permitted additional restrictions
// with this program. If not, see https://github.com/electronicarts/CnC_Remastered_Collection
#include <devices/ahi.h>
#include <dos/dostags.h>

#include <proto/dos.h>
#include <proto/exec.h>

#include <string.h>
#include <stdio.h>

#include "vqaaudio.h"
#include "vqafile.h"
#include "vqaloader.h"
#include "vqatask.h"
#include "timer.h"
#include <algorithm>
#include <sys/timeb.h>

extern "C" {
void dprintf(const char*, ...);
}

#if 0
#define DEBUG(x, y...) dprintf("VQAAudio: " x, ##y)
#else
#define DEBUG(x, y...)                                                                                                 \
    do {                                                                                                               \
    } while (0)
#endif

static int VQA_CopyAudio_Locked(VQAHandle* handle);
static int VQA_StartAudio_Locked(VQAHandle* handle);

static TimerClass timer;

struct VQAThreadMessage
{
    struct Message Message;
    VQAAudio* Audio;
};

static ULONG GetAHIType(VQAAudio* audio)
{
    ULONG ret;

    if (audio->Channels == 1 && audio->BitsPerSample == 8) {
        printf("%s:%d/%s(): 8 bit sound needs to be converted\n", __FILE__, __LINE__, __func__);
        ret = AHIST_M8S;
    } else if (audio->Channels == 2 && audio->BitsPerSample == 8) {
        printf("%s:%d/%s(): 8 bit sound needs to be converted\n", __FILE__, __LINE__, __func__);
        ret = AHIST_S8S;
    } else if (audio->Channels == 1 && audio->BitsPerSample == 16) {
        ret = AHIST_M16S;
    } else if (audio->Channels == 2 && audio->BitsPerSample == 16) {
        ret = AHIST_S16S;
    } else {
        ret = AHIST_S16S;
    }

    return ret;
}

static void WriteAudio(VQAHandle* handle)
{
    VQAConfig* config = &handle->Config;
    VQAData* data = handle->VQABuf;
    VQAAudio* audio = &data->Audio;
    struct AHIRequest* ahireq;

#if 0
    printf("%s:%d/%s(): audio->nextplaybackblock %d\n", __FILE__, __LINE__, __func__, audio->nextplaybackblock);
    fflush(stdout);
#endif

    while (1) {
        ahireq = audio->reqs[audio->nextreq].ahireq;

        if (audio->reqs[audio->nextreq].ahireqsent) {
#if 0
				printf("%s:%d/%s(): Request %d is not done yet\n", __FILE__, __LINE__, __func__, audio->nextreq);
				fflush(stdout);
#endif
            return;
        }

#if 0
		printf("%s:%d/%s(): audio->IsLoaded[%d]: %d\n", __FILE__, __LINE__, __func__, audio->nextplaybackblock, audio->IsLoaded[audio->nextplaybackblock]);
		fflush(stdout);
#endif

        if (!audio->IsLoaded[audio->nextplaybackblock]) {
            return;
        }

#if 0
		printf("%s:%d/%s(): Sending request %d, block %d\n", __FILE__, __LINE__, __func__, audio->nextreq, audio->nextplaybackblock);
		fflush(stdout);
#endif

        ahireq->ahir_Std.io_Command = CMD_WRITE;
        ahireq->ahir_Std.io_Data = &audio->Buffer[audio->nextplaybackblock * config->HMIBufSize];
        ahireq->ahir_Std.io_Length = config->HMIBufSize;
        ahireq->ahir_Std.io_Actual = 0;
        ahireq->ahir_Std.io_Offset = 0;
        ahireq->ahir_Frequency = audio->SampleRate;
        ahireq->ahir_Type = GetAHIType(audio);
        ahireq->ahir_Volume = 0x10000;
        ahireq->ahir_Position = 0x8000;
        ahireq->ahir_Link = audio->reqs[audio->nextreq ^ 1].ahireqsent ? audio->reqs[audio->nextreq ^ 1].ahireq : 0;

        SendIO((struct IORequest*)ahireq);

        audio->reqs[audio->nextreq].ahireqsent = true;
        audio->nextreq ^= 1;

        audio->IsLoaded[audio->nextplaybackblock] = 0;
        audio->nextplaybackblock++;

        if (audio->nextplaybackblock == audio->NumAudBlocks) {
            audio->nextplaybackblock = 0;
        }
    }
}

static void VQA_AudioThread(VQAAudio* audio)
{
    unsigned int i;
    bool initok;

    initok = false;

    DEBUG("Thread starting up\n");

    audio->ThreadSignal = AllocSignal(-1);
    if (audio->ThreadSignal != -1) {
        audio->ahimp = CreateMsgPort();
        if (audio->ahimp) {
            audio->reqs[0].ahireq = (struct AHIRequest*)CreateIORequest(audio->ahimp, sizeof(*audio->reqs[0].ahireq));
            if (audio->reqs[0].ahireq) {
                audio->reqs[1].ahireq = (struct AHIRequest*)AllocVec(sizeof(*audio->reqs[1].ahireq), MEMF_ANY);
                if (audio->reqs[1].ahireq) {
                    if (OpenDevice(AHINAME, AHI_DEFAULT_UNIT, (struct IORequest*)audio->reqs[0].ahireq, 0) == 0) {
                        memcpy(audio->reqs[1].ahireq, audio->reqs[0].ahireq, sizeof(*audio->reqs[1].ahireq));

                        audio->LastCompletedRequestTimePoint = std::chrono::steady_clock::now();

                        initok = true;
                        audio->ThreadInitOK = true;

                        DEBUG("Thread start up successful\n");
                        Signal(audio->MainTask, SIGF_SINGLE);

                        while (1) {
                            Wait((1 << audio->ThreadSignal) | (1 << audio->ahimp->mp_SigBit));

                            if (audio->DoQuit) {
                                break;
                            }

                            ObtainSemaphore(audio->Semaphore);

                            for (i = 0; i < 2; i++) {
                                if (audio->reqs[i].ahireqsent) {
                                    if (CheckIO((struct IORequest*)audio->reqs[i].ahireq)) {
                                        WaitIO((struct IORequest*)audio->reqs[i].ahireq);
                                        audio->reqs[i].ahireqsent = false;
                                        audio->completedreqcount++;
                                        audio->LastCompletedRequestTimePoint = std::chrono::steady_clock::now();
                                    }
                                }
                            }

                            ReleaseSemaphore(audio->Semaphore);
                        }

                        DEBUG("Thread shutting down\n");

                        for (i = 0; i < 2; i++) {
                            if (audio->reqs[i].ahireqsent) {
                                AbortIO((struct IORequest*)audio->reqs[i].ahireq);
                                WaitIO((struct IORequest*)audio->reqs[i].ahireq);
                                audio->reqs[i].ahireqsent = false;
                            }
                        }

                        CloseDevice((struct IORequest*)audio->reqs[0].ahireq);
                        audio->reqs[0].ahireq = 0;
                    }

                    FreeVec(audio->reqs[1].ahireq);
                    audio->reqs[1].ahireq = 0;
                }

                DeleteIORequest((struct IORequest*)audio->reqs[0].ahireq);
                audio->reqs[0].ahireq = 0;
            }

            DeleteMsgPort(audio->ahimp);
            audio->ahimp = 0;
        }
        FreeSignal(audio->ThreadSignal);
    }

    if (!initok) {
        DEBUG("Thread start up failed\n");
        audio->ThreadInitOK = false;
        Signal(audio->MainTask, SIGF_SINGLE);
    }

    DEBUG("Thread ending\n");
}

int VQA_StartTimerInt(VQAHandle* handle, int a2)
{
    return 0;
}

void VQA_StopTimerInt(VQAHandle* handle)
{
}

int VQA_OpenAudio(VQAHandle* handle, void* hwnd)
{
    VQAData* data = handle->VQABuf;
    VQAAudio* audio = &data->Audio;
    int ret;

    DEBUG("Initialising\n");

    ret = -1;

    audio->reqs[0].ahireqsent = false;
    audio->reqs[1].ahireqsent = false;
    audio->nextreq = 0;
    audio->nextplaybackblock = 0;
    audio->nextcopyblock = 0;
    audio->completedreqcount = 0;

    audio->MainTask = FindTask(0);

    audio->Semaphore = (struct SignalSemaphore*)AllocVec(sizeof(*audio->Semaphore), MEMF_CLEAR);
    if (audio->Semaphore) {
        InitSemaphore(audio->Semaphore);
        audio->threadmp = CreateMsgPort();
        if (audio->threadmp) {
            audio->threadmessage = (struct VQAThreadMessage*)AllocVec(sizeof(*audio->threadmessage), MEMF_CLEAR);
            if (audio->threadmessage) {
                audio->threadmessage->Message.mn_Node.ln_Type = NT_MESSAGE;
                audio->threadmessage->Message.mn_ReplyPort = audio->threadmp;
                audio->threadmessage->Message.mn_Length = sizeof(*audio->threadmessage);

                audio->process = CreateNewProcTags(NP_Entry,
                                                   VQA_AudioThread,
                                                   NP_CodeType,
                                                   CODETYPE_PPC,
                                                   NP_PPC_Arg1,
                                                   audio,
                                                   NP_Name,
                                                   "Vanilla Conquer Audio Thread",
                                                   NP_StartupMsg,
                                                   audio->threadmessage,
                                                   NP_Priority,
                                                   1,
                                                   TAG_DONE);
                if (audio->process) {

                    DEBUG("Waiting for thread to start up\n");
                    Wait(SIGF_SINGLE);

                    if (audio->ThreadInitOK) {
                        DEBUG("Init complete\n");
                        fflush(stdout);

                        ret = 0;
                    }

                    if (ret) {
                        DEBUG("Waiting for thread to end\n");
                        while (!GetMsg(audio->threadmp)) {
                            WaitPort(audio->threadmp);
                        }
                    }
                }

                if (ret) {
                    FreeVec(audio->threadmessage);
                    audio->threadmessage = 0;
                }
            }

            if (ret) {
                DeleteMsgPort(audio->threadmp);
                audio->threadmp = 0;
            }
        }

        if (ret) {
            FreeVec(audio->Semaphore);
            audio->Semaphore = 0;
        }
    }

    if (ret) {
        DEBUG("Init failed\n");
    }

    return ret;
}

void VQA_CloseAudio(VQAHandle* handle)
{
    VQAData* data = handle->VQABuf;
    VQAAudio* audio = &data->Audio;

    DEBUG("Shutting down\n");

    audio->DoQuit = true;

    if (audio->process) {
        DEBUG("Waiting for thread to end\n");

        Signal((struct Task*)audio->process, (1 << audio->ThreadSignal));

        while (!GetMsg(audio->threadmp)) {
            WaitPort(audio->threadmp);
        }
    }

    if (audio->threadmessage) {
        FreeVec(audio->threadmessage);
        audio->threadmessage = 0;
    }

    if (audio->threadmp) {
        DeleteMsgPort(audio->threadmp);
        audio->threadmp = 0;
    }

    if (audio->Semaphore) {
        FreeVec(audio->Semaphore);
        audio->Semaphore = 0;
    }
}

int VQA_StartAudio(VQAHandle* handle)
{
    VQAData* data = handle->VQABuf;
    VQAAudio* audio = &data->Audio;
    int ret;

    ObtainSemaphore(audio->Semaphore);

    ret = VQA_StartAudio_Locked(handle);

    ReleaseSemaphore(audio->Semaphore);

    return ret;
}

static int VQA_StartAudio_Locked(VQAHandle* handle)
{
    WriteAudio(handle);

    return 0;
}

void VQA_StopAudio(VQAHandle* handle)
{
}

void VQA_PauseAudio()
{
    timer.Stop();
}

void VQA_ResumeAudio()
{
    timer.Start();
}

int VQA_CopyAudio(VQAHandle* handle)
{
    VQAData* data = handle->VQABuf;
    VQAAudio* audio = &data->Audio;
    int ret;

    ObtainSemaphore(audio->Semaphore);

    ret = VQA_CopyAudio_Locked(handle);

    ReleaseSemaphore(audio->Semaphore);

    return ret;
}

static int VQA_CopyAudio_Locked(VQAHandle* handle)
{
    VQAConfig* config = &handle->Config;
    VQAData* data = handle->VQABuf;
    VQAAudio* audio = &data->Audio;

    WriteAudio(handle);

    if ((config->OptionFlags & VQAOPTF_AUDIO)) {
        if (audio->Buffer != nullptr) {
            if (audio->TempBufSize > 0) {
                int current_block = audio->AudBufPos / config->HMIBufSize;
                int next_block = (audio->TempBufSize + audio->AudBufPos) / config->HMIBufSize;

                if ((unsigned)next_block >= audio->NumAudBlocks) {
                    next_block -= audio->NumAudBlocks;
                }

                if (audio->IsLoaded[next_block] == 1) {
                    return VQAERR_SLEEPING;
                }

                // Need to loop back and treat like circular buffer?
                if (next_block < current_block) {
                    int end_space = config->AudioBufSize - audio->AudBufPos;
                    int remaining = audio->TempBufSize - end_space;
                    memcpy(&audio->Buffer[audio->AudBufPos], audio->TempBuf, end_space);
                    memcpy(audio->Buffer, &audio->TempBuf[end_space], remaining);
                    audio->AudBufPos = remaining;
                    audio->TempBufSize = 0;

                    for (unsigned i = current_block; i < audio->NumAudBlocks; ++i) {
                        audio->IsLoaded[i] = 1;
                    }

                    for (int i = 0; i < next_block; ++i) {
                        audio->IsLoaded[i] = 1;
                    }
                } else {
                    memcpy(&audio->Buffer[audio->AudBufPos], audio->TempBuf, audio->TempBufSize);
                    audio->AudBufPos += audio->TempBufSize;
                    audio->TempBufSize = 0;

                    for (int i = current_block; i < next_block; ++i) {
                        audio->IsLoaded[i] = 1;
                    }
                }

                WriteAudio(handle);
            }
        }
    }

    return 0;
}

void VQA_SetTimer(VQAHandle* handle, int time, int method)
{
    timer.Set(0);
}

unsigned VQA_GetTime(VQAHandle* handle)
{
    VQAConfig* config = &handle->Config;
    VQAData* data = handle->VQABuf;
    VQAAudio* audio = &data->Audio;
    std::chrono::time_point<std::chrono::steady_clock> now;
    unsigned int milliseconds;
    unsigned int samplesplayed;
    unsigned int ret;
    unsigned int ret2;

    DEBUG("%s: audio->completedreqcount %d\n", __func__, audio->completedreqcount);
    DEBUG("%s: audio->SampleRate %d\n", __func__, audio->SampleRate);
    DEBUG("%s: audio->Channels %d\n", __func__, audio->Channels);
    DEBUG("%s: audio->BitsPerSample %d\n", __func__, audio->BitsPerSample);
    DEBUG("%s: config->HMIBufSize %d\n", __func__, config->HMIBufSize);

    samplesplayed = audio->completedreqcount * config->HMIBufSize;

    DEBUG("%s: samplesplayed = %d\n", __func__, samplesplayed);

    if (audio->reqs[audio->nextreq].ahireqsent) {
        samplesplayed += audio->reqs[audio->nextreq].ahireq->ahir_Std.io_Actual;
    }

    if (audio->reqs[audio->nextreq ^ 1].ahireqsent) {
        samplesplayed += audio->reqs[audio->nextreq ^ 1].ahireq->ahir_Std.io_Actual;
    }

    DEBUG("%s: samplesplayed = %d\n", __func__, samplesplayed);

    now = std::chrono::steady_clock::now();
    milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - audio->LastCompletedRequestTimePoint).count();

    samplesplayed += milliseconds * (audio->SampleRate * audio->Channels * (audio->BitsPerSample / 8)) / 1000;

    ret = 60 * samplesplayed / (audio->SampleRate * audio->Channels * (audio->BitsPerSample / 8));
    ret2 = timer.Time();

    DEBUG("Microseconds since last completed audio request: %d\n", milliseconds);

    DEBUG("Calculated ret is %d, timer-based ret is %d\n", ret, ret2);

    return ret;
}

int VQA_TimerMethod()
{
    return 0;
}
