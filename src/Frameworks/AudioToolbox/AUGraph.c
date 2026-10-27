// AUGraph.c - graphs of audio units, a node pulling its input from another node's render
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include <AudioToolbox/AudioToolbox.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NODES 32

struct node {
    AUNode id;
    AudioComponentDescription desc;
    AudioComponent component;
    AudioUnit unit;
    // the node feeding input 0, 0 when nothing is connected
    AUNode source;
    UInt32 sourceOutput;
};

struct OpaqueAUGraph {
    pthread_mutex_t lock;
    struct node nodes[MAX_NODES];
    UInt32 count;
    AUNode nextID;
    Boolean open, initialized, running;
};

static struct node *findNode(AUGraph g, AUNode id)
{
    for (UInt32 i = 0; i < g->count; i++) {
        if (g->nodes[i].id == id)
            return &g->nodes[i];
    }
    return NULL;
}

static Boolean isOutputNode(const struct node *n)
{
    return n->desc.componentType == kAudioUnitType_Output;
}

OSStatus NewAUGraph(AUGraph *outGraph)
{
    if (outGraph == NULL)
        return kAudio_ParamError;
    AUGraph g = calloc(1, sizeof(*g));
    if (g == NULL)
        return kAudio_MemFullError;
    pthread_mutex_init(&g->lock, NULL);
    g->nextID = 1;
    *outGraph = g;
    return noErr;
}

// the source node renders into the buffer the destination unit asks its input for
static OSStatus renderSource(void *refCon, AudioUnitRenderActionFlags *flags, const AudioTimeStamp *timeStamp, UInt32 bus,
                             UInt32 frames, AudioBufferList *ioData)
{
    struct node *src = refCon;

    (void)bus;
    return AudioUnitRender(src->unit, flags, timeStamp, src->sourceOutput, frames, ioData);
}

static OSStatus applyConnection(AUGraph g, struct node *dest)
{
    struct node *src = dest->source ? findNode(g, dest->source) : NULL;
    AURenderCallbackStruct cb = { src ? renderSource : NULL, src };

    if (!dest->unit || (src && !src->unit))
        return noErr;
    return AudioUnitSetProperty(dest->unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &cb, sizeof(cb));
}

OSStatus AUGraphAddNode(AUGraph g, const AudioComponentDescription *desc, AUNode *outNode)
{
    if (g == NULL || desc == NULL || outNode == NULL)
        return kAudio_ParamError;
    AudioComponent c = AudioComponentFindNext(NULL, desc);
    // a node needs a component behind it, the DLS synth for one is not installed
    if (c == NULL)
        return kAUGraphErr_InvalidAudioUnit;
    pthread_mutex_lock(&g->lock);
    if (g->count == MAX_NODES) {
        pthread_mutex_unlock(&g->lock);
        return kAudio_MemFullError;
    }
    struct node *n = &g->nodes[g->count++];
    memset(n, 0, sizeof(*n));
    n->id = g->nextID++;
    n->desc = *desc;
    n->component = c;
    OSStatus err = g->open ? AudioComponentInstanceNew(c, &n->unit) : noErr;
    if (err != noErr)
        g->count--;
    else
        *outNode = n->id;
    pthread_mutex_unlock(&g->lock);
    return err;
}

OSStatus AUGraphRemoveNode(AUGraph g, AUNode id)
{
    if (g == NULL)
        return kAudio_ParamError;
    pthread_mutex_lock(&g->lock);
    struct node *n = findNode(g, id);
    if (n == NULL) {
        pthread_mutex_unlock(&g->lock);
        return kAUGraphErr_NodeNotFound;
    }
    if (n->unit) {
        if (isOutputNode(n))
            AudioOutputUnitStop(n->unit);
        AudioComponentInstanceDispose(n->unit);
    }
    for (UInt32 i = 0; i < g->count; i++) {
        if (g->nodes[i].source == id) {
            g->nodes[i].source = 0;
            applyConnection(g, &g->nodes[i]);
        }
    }
    *n = g->nodes[--g->count];
    pthread_mutex_unlock(&g->lock);
    return noErr;
}

OSStatus AUGraphGetNodeCount(AUGraph g, UInt32 *outCount)
{
    if (g == NULL || outCount == NULL)
        return kAudio_ParamError;
    *outCount = g->count;
    return noErr;
}

OSStatus AUGraphGetIndNode(AUGraph g, UInt32 index, AUNode *outNode)
{
    if (g == NULL || outNode == NULL)
        return kAudio_ParamError;
    if (index >= g->count)
        return kAUGraphErr_NodeNotFound;
    *outNode = g->nodes[index].id;
    return noErr;
}

OSStatus AUGraphNodeInfo(AUGraph g, AUNode id, AudioComponentDescription *outDesc, AudioUnit *outUnit)
{
    if (g == NULL)
        return kAudio_ParamError;
    struct node *n = findNode(g, id);
    if (n == NULL)
        return kAUGraphErr_NodeNotFound;
    if (outDesc)
        *outDesc = n->desc;
    if (outUnit)
        *outUnit = n->unit;
    return noErr;
}

OSStatus AUGraphConnectNodeInput(AUGraph g, AUNode source, UInt32 sourceOutput, AUNode dest, UInt32 destInput)
{
    if (g == NULL)
        return kAudio_ParamError;
    pthread_mutex_lock(&g->lock);
    struct node *s = findNode(g, source), *d = findNode(g, dest);
    OSStatus err = noErr;
    // every unit here has one input bus
    if (s == NULL || d == NULL)
        err = kAUGraphErr_NodeNotFound;
    else if (s == d || destInput != 0)
        err = kAUGraphErr_InvalidConnection;
    else {
        d->source = source;
        d->sourceOutput = sourceOutput;
        err = applyConnection(g, d);
    }
    pthread_mutex_unlock(&g->lock);
    return err;
}

OSStatus AUGraphDisconnectNodeInput(AUGraph g, AUNode dest, UInt32 destInput)
{
    if (g == NULL)
        return kAudio_ParamError;
    pthread_mutex_lock(&g->lock);
    struct node *d = findNode(g, dest);
    OSStatus err = d == NULL ? kAUGraphErr_NodeNotFound : destInput != 0 ? kAUGraphErr_InvalidConnection : noErr;
    if (err == noErr) {
        d->source = 0;
        err = applyConnection(g, d);
    }
    pthread_mutex_unlock(&g->lock);
    return err;
}

OSStatus AUGraphOpen(AUGraph g)
{
    if (g == NULL)
        return kAudio_ParamError;
    pthread_mutex_lock(&g->lock);
    OSStatus err = noErr;
    for (UInt32 i = 0; i < g->count && err == noErr; i++) {
        if (!g->nodes[i].unit)
            err = AudioComponentInstanceNew(g->nodes[i].component, &g->nodes[i].unit);
    }
    for (UInt32 i = 0; i < g->count && err == noErr; i++)
        err = applyConnection(g, &g->nodes[i]);
    g->open = err == noErr;
    pthread_mutex_unlock(&g->lock);
    return err;
}

OSStatus AUGraphInitialize(AUGraph g)
{
    if (g == NULL)
        return kAudio_ParamError;
    if (!g->open && AUGraphOpen(g) != noErr)
        return kAUGraphErr_InvalidAudioUnit;
    pthread_mutex_lock(&g->lock);
    OSStatus err = noErr;
    for (UInt32 i = 0; i < g->count && err == noErr; i++)
        err = AudioUnitInitialize(g->nodes[i].unit);
    g->initialized = err == noErr;
    pthread_mutex_unlock(&g->lock);
    return err;
}

OSStatus AUGraphUninitialize(AUGraph g)
{
    if (g == NULL)
        return kAudio_ParamError;
    AUGraphStop(g);
    pthread_mutex_lock(&g->lock);
    for (UInt32 i = 0; i < g->count; i++) {
        if (g->nodes[i].unit)
            AudioUnitUninitialize(g->nodes[i].unit);
    }
    g->initialized = false;
    pthread_mutex_unlock(&g->lock);
    return noErr;
}

// output nodes are what pull the graph, starting them starts everything
OSStatus AUGraphStart(AUGraph g)
{
    if (g == NULL)
        return kAudio_ParamError;
    if (!g->initialized)
        return kAudioUnitErr_Uninitialized;
    pthread_mutex_lock(&g->lock);
    OSStatus err = noErr;
    Boolean any = false;
    for (UInt32 i = 0; i < g->count && err == noErr; i++) {
        if (isOutputNode(&g->nodes[i])) {
            err = AudioOutputUnitStart(g->nodes[i].unit);
            any = true;
        }
    }
    if (!any && err == noErr)
        err = kAUGraphErr_OutputNodeErr;
    g->running = err == noErr;
    pthread_mutex_unlock(&g->lock);
    return err;
}

OSStatus AUGraphStop(AUGraph g)
{
    if (g == NULL)
        return kAudio_ParamError;
    pthread_mutex_lock(&g->lock);
    for (UInt32 i = 0; i < g->count; i++) {
        if (isOutputNode(&g->nodes[i]) && g->nodes[i].unit)
            AudioOutputUnitStop(g->nodes[i].unit);
    }
    g->running = false;
    pthread_mutex_unlock(&g->lock);
    return noErr;
}

OSStatus AUGraphClose(AUGraph g)
{
    if (g == NULL)
        return kAudio_ParamError;
    AUGraphUninitialize(g);
    pthread_mutex_lock(&g->lock);
    for (UInt32 i = 0; i < g->count; i++) {
        if (g->nodes[i].unit)
            AudioComponentInstanceDispose(g->nodes[i].unit);
        g->nodes[i].unit = NULL;
    }
    g->open = false;
    pthread_mutex_unlock(&g->lock);
    return noErr;
}

OSStatus DisposeAUGraph(AUGraph g)
{
    if (g == NULL)
        return kAudio_ParamError;
    AUGraphClose(g);
    pthread_mutex_destroy(&g->lock);
    free(g);
    return noErr;
}

OSStatus AUGraphIsOpen(AUGraph g, Boolean *out)
{
    if (g == NULL || out == NULL)
        return kAudio_ParamError;
    *out = g->open;
    return noErr;
}

OSStatus AUGraphIsInitialized(AUGraph g, Boolean *out)
{
    if (g == NULL || out == NULL)
        return kAudio_ParamError;
    *out = g->initialized;
    return noErr;
}

OSStatus AUGraphIsRunning(AUGraph g, Boolean *out)
{
    if (g == NULL || out == NULL)
        return kAudio_ParamError;
    *out = g->running;
    return noErr;
}

// connections take effect when they are made, so there is never anything pending
OSStatus AUGraphUpdate(AUGraph g, Boolean *outIsUpdated)
{
    if (g == NULL)
        return kAudio_ParamError;
    if (outIsUpdated)
        *outIsUpdated = true;
    return noErr;
}

// none of the installed units is a music device, they have no MIDI to take
OSStatus MusicDeviceMIDIEvent(MusicDeviceComponent unit, UInt32 status, UInt32 data1, UInt32 data2, UInt32 offset)
{
    AudioComponentDescription d;

    (void)status;
    (void)data1;
    (void)data2;
    (void)offset;
    if (unit == NULL)
        return kAudio_ParamError;
    AudioComponentGetDescription(AudioComponentInstanceGetComponent(unit), &d);
    return d.componentType == kAudioUnitType_MusicDevice ? kAudioUnitErr_Uninitialized : kAudioUnitErr_InvalidProperty;
}

OSStatus MusicDeviceSysEx(MusicDeviceComponent unit, const UInt8 *data, UInt32 length)
{
    return MusicDeviceMIDIEvent(unit, data && length ? data[0] : 0, 0, 0, 0);
}
