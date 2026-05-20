/*
 * serial_vst.c
 *
 * VST2 instrument plugin — reads audio from ESP32 via Serial (COM3)
 * and feeds it directly into Guitar Rig's audio callback.
 *
 * Build:
 *   gcc serial_vst.c -o serial_vst.dll -shared -O2
 *       -I./sdk -lole32 -lwinmm -lwinpthread
 *       -Wl,--kill-at
 *
 * Install:
 *   Copy serial_vst.dll to your VST2 plugins folder, e.g.:
 *   C:\Program Files\VSTPlugins\
 *   or wherever Guitar Rig scans for VST2 plugins.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "sdk/aeffect.h"

/* ------------------------------------------------------------------ */
/*  Configuration                                                       */
/* ------------------------------------------------------------------ */
#define PORT "COM3"
#define BAUD 1200000
#define SAMPLES_PER_PKT 64
#define PACKET_SIZE (SAMPLES_PER_PKT * 2 + 2) /* 130 bytes     */

/* Ring buffer: holds enough for ~100 ms at 48kHz */
#define RING_SLOTS 128

/* ------------------------------------------------------------------ */
/*  Lock-free ring buffer                                               */
/* ------------------------------------------------------------------ */
typedef struct
{
    float data[RING_SLOTS][SAMPLES_PER_PKT];
    volatile LONG head;
    volatile LONG tail;
} RingBuf;

static int rb_push(RingBuf *rb, const float *src)
{
    LONG h = rb->head;
    LONG next = (h + 1) % RING_SLOTS;
    if (next == rb->tail)
        return 0; /* full — drop */
    memcpy(rb->data[h], src, SAMPLES_PER_PKT * sizeof(float));
    InterlockedExchange(&rb->head, next);
    return 1;
}

static int rb_pop(RingBuf *rb, float *dst)
{
    LONG t = rb->tail;
    if (t == rb->head)
        return 0; /* empty */
    memcpy(dst, rb->data[t], SAMPLES_PER_PKT * sizeof(float));
    InterlockedExchange(&rb->tail, (t + 1) % RING_SLOTS);
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Plugin state                                                        */
/* ------------------------------------------------------------------ */
typedef struct
{
    AEffect effect; /* MUST be first member                 */
    RingBuf ring;

    HANDLE hSerial;
    HANDLE hThread;
    volatile int running;

    /* Cursor inside current decoded packet */
    float pkt_buf[SAMPLES_PER_PKT];
    int pkt_pos; /* next sample index within pkt_buf     */
    int have_pkt;

    double sampleRate;
    int blockSize;
    volatile int com_ok;      /* 1 = COM opened OK        */
    volatile float sig_level; /* peak level last block    */
    float dc_x1;              /* DC blocking filter state */
    float dc_y1;
} SerialPlugin;

/* ------------------------------------------------------------------ */
/*  Serial helpers                                                      */
/* ------------------------------------------------------------------ */
static HANDLE serial_open(void)
{
    HANDLE h = CreateFileA("\\\\.\\" PORT,
                           GENERIC_READ | GENERIC_WRITE,
                           0, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return INVALID_HANDLE_VALUE;

    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    GetCommState(h, &dcb);
    dcb.BaudRate = BAUD;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    SetCommState(h, &dcb);
    SetupComm(h, 96000, 4096);

    COMMTIMEOUTS ct = {0};
    ct.ReadIntervalTimeout = MAXDWORD;
    ct.ReadTotalTimeoutMultiplier = MAXDWORD;
    ct.ReadTotalTimeoutConstant = 50;
    SetCommTimeouts(h, &ct);
    return h;
}

static int read_exact(HANDLE h, uint8_t *buf, DWORD n)
{
    DWORD total = 0;
    while (total < n)
    {
        DWORD got = 0;
        if (!ReadFile(h, buf + total, n - total, &got, NULL) || got == 0)
            return 0;
        total += got;
    }
    return 1;
}

static void sync_to_marker(HANDLE h)
{
    uint8_t prev = 0, cur = 0;
    DWORD got;
    while (1)
    {
        if (!ReadFile(h, &cur, 1, &got, NULL) || got == 0)
            continue;
        if (prev == 0xff && cur == 0xff)
            return;
        prev = cur;
    }
}

/* ------------------------------------------------------------------ */
/*  Serial reader thread                                                */
/* ------------------------------------------------------------------ */
static DWORD WINAPI serial_thread(LPVOID arg)
{
    SerialPlugin *p = (SerialPlugin *)arg;
    uint8_t buf[PACKET_SIZE];

    PurgeComm(p->hSerial, PURGE_RXCLEAR | PURGE_TXCLEAR);
    sync_to_marker(p->hSerial);

    while (p->running)
    {
        if (!read_exact(p->hSerial, buf, PACKET_SIZE))
            continue;

        /* Verify stop bytes */
        if (buf[PACKET_SIZE - 2] != 0xff || buf[PACKET_SIZE - 1] != 0xff)
        {
            PurgeComm(p->hSerial, PURGE_RXCLEAR);
            sync_to_marker(p->hSerial);
            continue;
        }

        /* Convert uint16 LE → float32 */
        float floats[SAMPLES_PER_PKT];
        for (int i = 0; i < SAMPLES_PER_PKT; i++)
        {
            uint16_t u;
            memcpy(&u, buf + i * 2, 2);
            float f = ((float)u - 1449.0f) / 2048.0f;
            if (f > 1.0f)
                f = 1.0f;
            else if (f < -1.0f)
                f = -1.0f;
            floats[i] = f;
        }
        rb_push(&p->ring, floats);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  VST dispatcher                                                      */
/* ------------------------------------------------------------------ */
static VstIntPtr dispatcher(AEffect *e, VstInt32 opcode,
                            VstInt32 index, VstIntPtr value,
                            void *ptr, float opt)
{
    SerialPlugin *p = (SerialPlugin *)e->object;
    (void)index;
    (void)value;
    (void)ptr;
    (void)opt;

    switch (opcode)
    {
    case effOpen:
    {
        FILE *f = fopen("C:\\Users\\user\\Desktop\\serial_vst_log.txt", "a");
        if (f)
        {
            fprintf(f, "effOpen called\n");
            fclose(f);
        }
    }
        p->hSerial = serial_open();
        if (p->hSerial != INVALID_HANDLE_VALUE)
        {
            p->com_ok = 1;
            p->running = 1;
            p->hThread = CreateThread(NULL, 0, serial_thread, p, 0, NULL);
            FILE *f = fopen("C:\\Users\\user\\Desktop\\serial_vst_log.txt", "a");
            if (f)
            {
                fprintf(f, "COM3 opened OK\n");
                fclose(f);
            }
        }
        else
        {
            p->com_ok = 0;
            FILE *f = fopen("C:\\Users\\user\\Desktop\\serial_vst_log.txt", "a");
            if (f)
            {
                fprintf(f, "COM3 FAILED: %lu\n", GetLastError());
                fclose(f);
            }
        }
        return 0;

    case effClose:
        p->running = 0;
        if (p->hThread)
        {
            WaitForSingleObject(p->hThread, 2000);
            CloseHandle(p->hThread);
            p->hThread = NULL;
        }
        if (p->hSerial != INVALID_HANDLE_VALUE)
        {
            CloseHandle(p->hSerial);
            p->hSerial = INVALID_HANDLE_VALUE;
        }
        free(p);
        return 0;

    case effSetSampleRate:
        p->sampleRate = (double)opt;
        {
            FILE *f = fopen("C:\\Users\\user\\Desktop\\serial_vst_log.txt", "a");
            if (f)
            {
                fprintf(f, "sampleRate=%.0f\n", p->sampleRate);
                fclose(f);
            }
        }
        return 0;

    case effSetBlockSize:
        p->blockSize = (int)value;
        {
            FILE *f = fopen("C:\\Users\\user\\Desktop\\serial_vst_log.txt", "a");
            if (f)
            {
                fprintf(f, "blockSize=%d\n", p->blockSize);
                fclose(f);
            }
        }
        return 0;

    case effMainsChanged:
        /* value=1: activate, value=0: deactivate */
        return 0;

    case effGetParamName:
        if (ptr)
        {
            if (index == 0)
                strncpy((char *)ptr, "COM Status", 16);
            else if (index == 1)
                strncpy((char *)ptr, "Sig Level", 16);
        }
        return 0;
    case effGetParamLabel:
        if (ptr)
            ((char *)ptr)[0] = '\0';
        return 0;
    case effGetParamDisplay:
        if (ptr)
        {
            SerialPlugin *pp = (SerialPlugin *)e->object;
            if (index == 0)
                snprintf((char *)ptr, 8, pp->com_ok ? "OK" : "FAIL");
            else if (index == 1)
                snprintf((char *)ptr, 8, "%.3f", pp->sig_level);
        }
        return 0;

    case effGetProgramName:
        if (ptr)
            strncpy((char *)ptr, "Default", 24);
        return 0;

    case effIdentify:
        return kEffectMagic;

    default:
        return 0;
    }
}

/* ------------------------------------------------------------------ */
/*  VST processReplacing — called by Guitar Rig on its audio thread    */
/* ------------------------------------------------------------------ */
static void process_replacing(AEffect *e, float **inputs,
                              float **outputs, VstInt32 frames)
{
    SerialPlugin *p = (SerialPlugin *)e->object;
    float *outL = outputs[0];
    float *outR = outputs[1];
    (void)inputs;

    float peak = 0.0f;
    for (VstInt32 i = 0; i < frames; i++)
    {
        /* Fetch next packet from ring buffer when current is exhausted */
        if (p->pkt_pos >= SAMPLES_PER_PKT)
        {
            p->have_pkt = rb_pop(&p->ring, p->pkt_buf);
            p->pkt_pos = 0;
        }

        float val = p->have_pkt ? p->pkt_buf[p->pkt_pos++] : 0.0f;
        /* DC blocking filter: y = x - x1 + 0.995*y1 */
        float y = val - p->dc_x1 + 0.995f * p->dc_y1;
        p->dc_x1 = val;
        p->dc_y1 = y;
        val = y;
        if ((val < 0.0f ? -val : val) > peak)
            peak = (val < 0.0f ? -val : val);
        outL[i] = val;
        outR[i] = val; /* mono source → both channels */
    }
    p->sig_level = peak;
}

/* process() — legacy, not used by modern hosts but required */
static void process(AEffect *e, float **inputs,
                    float **outputs, VstInt32 frames)
{
    process_replacing(e, inputs, outputs, frames);
}

static void set_parameter(AEffect *e, VstInt32 index, float value)
{
    (void)e;
    (void)index;
    (void)value;
} /* read-only params */

static float get_parameter(AEffect *e, VstInt32 index)
{
    SerialPlugin *p = (SerialPlugin *)e->object;
    if (index == 0)
        return p->com_ok ? 1.0f : 0.0f; /* COM status  */
    if (index == 1)
        return p->sig_level; /* signal level */
    return 0.f;
}

/* ------------------------------------------------------------------ */
/*  DLL entry point — VSTPluginMain                                     */
/* ------------------------------------------------------------------ */
__attribute__((visibility("default")))
AEffect *
VSTPluginMain(audioMasterCallback audioMaster)
{
    (void)audioMaster;

    SerialPlugin *p = (SerialPlugin *)calloc(1, sizeof(SerialPlugin));
    if (!p)
        return NULL;

    p->hSerial = INVALID_HANDLE_VALUE;
    p->pkt_pos = SAMPLES_PER_PKT; /* force fetch on first block     */

    AEffect *e = &p->effect;
    e->magic = kEffectMagic;
    e->dispatcher = dispatcher;
    e->process = process;
    e->processReplacing = process_replacing;
    e->processDoubleReplacing = NULL;
    e->setParameter = set_parameter;
    e->getParameter = get_parameter;
    e->object = p;

    e->numInputs = 0;  /* instrument: no audio input  */
    e->numOutputs = 2; /* stereo output               */
    e->numParams = 2;
    e->numPrograms = 1;
    e->flags = effFlagsCanReplacing | effFlagsIsSynth;
    e->uniqueID = ('S' << 24) | ('E' << 16) | ('R' << 8) | '1';
    e->version = 1000;

    return e;
}

/* Some hosts call "main" instead of VSTPluginMain.
   We export it via the .def file to avoid -Wmain warnings. */

/* ------------------------------------------------------------------ */
/*  DLL boilerplate                                                     */
/* ------------------------------------------------------------------ */
BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
    (void)hInst;
    (void)reason;
    (void)reserved;
    return TRUE;
}