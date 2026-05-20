#pragma once
#include <stdint.h>

#define kEffectMagic 0x56737450 /* 'VstP' */

typedef int32_t VstInt32;
typedef intptr_t VstIntPtr;

/* Forward declaration so function pointer typedefs can reference AEffect */
typedef struct AEffect AEffect;

typedef VstIntPtr (*audioMasterCallback)(AEffect *, VstInt32, VstInt32, VstIntPtr, void *, float);
typedef VstIntPtr (*AEffectDispatcherProc)(AEffect *, VstInt32, VstInt32, VstIntPtr, void *, float);
typedef void (*AEffectProcessProc)(AEffect *, float **, float **, VstInt32);
typedef void (*AEffectProcessDoubleProc)(AEffect *, double **, double **, VstInt32);
typedef void (*AEffectSetParameterProc)(AEffect *, VstInt32, float);
typedef float (*AEffectGetParameterProc)(AEffect *, VstInt32);

enum AEffectOpcodes
{
    effOpen = 0,
    effClose,
    effSetProgram,
    effGetProgram,
    effSetProgramName,
    effGetProgramName,
    effGetParamLabel,
    effGetParamDisplay,
    effGetParamName,
    effGetVu,
    effSetSampleRate,
    effSetBlockSize,
    effMainsChanged,
    effEditGetRect,
    effEditOpen,
    effEditClose,
    effEditDraw,
    effEditMouse,
    effEditKey,
    effEditIdle,
    effEditTop,
    effEditSleep,
    effIdentify,
    effGetChunk,
    effSetChunk,
    effNumOpcodes
};

enum VstAEffectFlags
{
    effFlagsHasEditor = 1 << 0,
    effFlagsCanReplacing = 1 << 4,
    effFlagsIsSynth = 1 << 8,
    effFlagsNoSoundInStop = 1 << 9
};

struct AEffect
{
    VstInt32 magic;
    AEffectDispatcherProc dispatcher;
    AEffectProcessProc process;
    AEffectSetParameterProc setParameter;
    AEffectGetParameterProc getParameter;
    VstInt32 numPrograms;
    VstInt32 numParams;
    VstInt32 numInputs;
    VstInt32 numOutputs;
    VstInt32 flags;
    VstIntPtr resvd1;
    VstIntPtr resvd2;
    VstInt32 initialDelay;
    VstInt32 realQualities;
    VstInt32 offQualities;
    float ioRatio;
    void *object;
    void *user;
    VstInt32 uniqueID;
    VstInt32 version;
    AEffectProcessProc processReplacing;
    AEffectProcessDoubleProc processDoubleReplacing;
    char future[56];
};