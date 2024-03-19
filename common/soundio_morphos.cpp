#include <devices/ahi.h>

#include <proto/exec.h>

#include <string.h>
#include <stdlib.h>

#include "soundio_imp.h"

struct SampleTrackerTypeImp
{
    struct
    {
        struct AHIRequest ahireq;
        bool ahireqsent;
        bool ahireqused;
        void* buffer;
        size_t buffersize;
    } reqs[2];
    unsigned int nextreq;

    bool IsPlaying;
    int Frequency;
    ULONG AHIType;
    unsigned int AHIVolume;
};

static struct MsgPort* ahimp;
static struct AHIRequest* globalahireq;

static ULONG GetAHIType(unsigned int bits, bool stereo);
static void SoundImp_Update_Sample(SampleTrackerTypeImp* st);
static unsigned int VolumeToAHI(unsigned int volume);

static ULONG GetAHIType(unsigned int bits, bool stereo)
{
    ULONG ret;

    if (!stereo && bits == 8) {
        ret = AHIST_M8S;
    } else if (stereo && bits == 8) {
        ret = AHIST_S8S;
    } else if (!stereo && bits == 16) {
        ret = AHIST_M16S;
    } else if (stereo && bits == 16) {
        ret = AHIST_S16S;
    } else {
        ret = AHIST_S16S;
    }

    return ret;
}

void SoundImp_Buffer_Sample_Data(SampleTrackerTypeImp* st, const void* data, size_t datalen)
{
    struct AHIRequest* ahireq;

    SoundImp_Update_Sample(st);

    if (st->reqs[st->nextreq].ahireqused) {
        return;
    }

    if (st->reqs[st->nextreq].buffersize < datalen) {
        FreeVec(st->reqs[st->nextreq].buffer);
        st->reqs[st->nextreq].buffer = AllocVec(datalen, MEMF_ANY);
        if (st->reqs[st->nextreq].buffer) {
            st->reqs[st->nextreq].buffersize = datalen;
        }
    }

    if (!st->reqs[st->nextreq].buffer) {
        return;
    }

    memcpy(st->reqs[st->nextreq].buffer, data, datalen);

    if (st->AHIType == AHIST_M8S || st->AHIType == AHIST_S8S) {
        /* Converted unsigned 8-bit samples to signed 8-bit samples */
        unsigned char* buf;
        size_t i;

        buf = (unsigned char*)st->reqs[st->nextreq].buffer;

        for (i = 0; i < datalen; i++) {
            buf[i] ^= 0x80;
        }
    }

    ahireq = &st->reqs[st->nextreq].ahireq;

    ahireq->ahir_Std.io_Command = CMD_WRITE;
    ahireq->ahir_Std.io_Data = st->reqs[st->nextreq].buffer;
    ahireq->ahir_Std.io_Length = datalen;
    ahireq->ahir_Std.io_Actual = 0;
    ahireq->ahir_Std.io_Offset = 0;
    ahireq->ahir_Frequency = st->Frequency;
    ahireq->ahir_Type = st->AHIType;
    ahireq->ahir_Volume = st->AHIVolume;
    ahireq->ahir_Position = 0x8000;
    ahireq->ahir_Link = st->reqs[st->nextreq ^ 1].ahireqused ? &st->reqs[st->nextreq ^ 1].ahireq : 0;

    st->reqs[st->nextreq].ahireqused = true;

    if (st->IsPlaying) {
        SendIO((struct IORequest*)ahireq);
        st->reqs[st->nextreq].ahireqsent = true;
    }

    st->nextreq ^= 1;
}

int SoundImp_Get_Sample_Free_Buffer_Count(SampleTrackerTypeImp* st)
{
    SoundImp_Update_Sample(st);

    return (!st->reqs[0].ahireqused) + (!st->reqs[1].ahireqused);
}

bool SoundImp_Init(int bits_per_sample, bool stereo, int rate, bool reverse_channels)
{
    bool ret;

    ret = false;

    ahimp = CreateMsgPort();
    if (ahimp) {
        globalahireq = (struct AHIRequest*)CreateIORequest(ahimp, sizeof(*globalahireq));
        if (globalahireq) {
            if (OpenDevice(AHINAME, AHI_DEFAULT_UNIT, (struct IORequest*)globalahireq, 0) == 0) {
                ret = true;
            }

            if (!ret) {
                DeleteIORequest((struct IORequest*)globalahireq);
                globalahireq = 0;
            }
        }

        if (!ret) {
            DeleteMsgPort(ahimp);
            ahimp = 0;
        }
    }

    return ret;
}

SampleTrackerTypeImp* SoundImp_Init_Sample(int bits_per_sample, bool stereo, int rate)
{
    SampleTrackerTypeImp* st;

    st = (SampleTrackerTypeImp*)AllocVec(sizeof(*st), MEMF_ANY | MEMF_CLEAR);
    if (st) {
        memcpy(&st->reqs[0].ahireq, globalahireq, sizeof(st->reqs[0].ahireq));
        memcpy(&st->reqs[1].ahireq, globalahireq, sizeof(st->reqs[1].ahireq));

        st->Frequency = rate;
        st->AHIType = GetAHIType(bits_per_sample, stereo);
        st->AHIVolume = 0x10000;

        return st;
    }

    return NULL;
}

void SoundImp_PauseSound()
{
}

bool SoundImp_ResumeSound()
{
    return true;
}

bool SoundImp_Sample_Status(SampleTrackerTypeImp* st)
{
    SoundImp_Update_Sample(st);

    return st->reqs[0].ahireqsent || st->reqs[1].ahireqsent;
}

void SoundImp_Set_Sample_Attributes(SampleTrackerTypeImp* st, int bits_per_sample, bool stereo, int rate)
{
    st->AHIType = GetAHIType(bits_per_sample, stereo);
    st->Frequency = rate;
}

void SoundImp_Set_Sample_Volume(SampleTrackerTypeImp* st, unsigned int volume)
{
    int i;

    st->AHIVolume = VolumeToAHI(volume);

    for (i = 0; i < 2; i++) {
        if (st->reqs[i].ahireqused && !st->reqs[i].ahireqsent) {
            st->reqs[i].ahireq.ahir_Volume = st->AHIVolume;
        }
    }
}

void SoundImp_Shutdown()
{
    CloseDevice((struct IORequest*)globalahireq);
    DeleteMsgPort(ahimp);
}

void SoundImp_Shutdown_Sample(SampleTrackerTypeImp* st)
{
    SoundImp_Stop_Sample(st);
    FreeVec(st->reqs[0].buffer);
    FreeVec(st->reqs[1].buffer);
    FreeVec(st);
}

void SoundImp_Start_Sample(SampleTrackerTypeImp* st)
{
    if (st->reqs[st->nextreq].ahireqused) {
        SendIO((struct IORequest*)&st->reqs[st->nextreq].ahireq);
        st->reqs[st->nextreq].ahireqsent = true;
    }

    if (st->reqs[st->nextreq ^ 1].ahireqused) {
        SendIO((struct IORequest*)&st->reqs[st->nextreq ^ 1].ahireq);
        st->reqs[st->nextreq ^ 1].ahireqsent = true;
    }

    st->IsPlaying = true;
}

void SoundImp_Stop_Sample(SampleTrackerTypeImp* st)
{
    unsigned int i;

    for (i = 0; i < 2; i++) {
        if (st->reqs[i].ahireqsent) {
            AbortIO((struct IORequest*)&st->reqs[i].ahireq);
        }
    }

    for (i = 0; i < 2; i++) {
        if (st->reqs[i].ahireqsent) {
            WaitIO((struct IORequest*)&st->reqs[i].ahireq);
            st->reqs[i].ahireqsent = 0;
            st->reqs[i].ahireqused = 0;
        }
    }

    st->IsPlaying = false;
}

static void SoundImp_Update_Sample(SampleTrackerTypeImp* st)
{
    unsigned int i;

    for (i = 0; i < 2; i++) {
        if (st->reqs[i].ahireqsent) {
            if (CheckIO((struct IORequest*)&st->reqs[i].ahireq)) {
                WaitIO((struct IORequest*)&st->reqs[i].ahireq);
                st->reqs[i].ahireqsent = false;
                st->reqs[i].ahireqused = false;
            }
        }
    }
}

static unsigned int VolumeToAHI(unsigned int volume)
{
    if (volume > 65535) {
        volume = 65535;
    }

    volume = (volume * 0x10000) / 65535;

    return volume;
}
