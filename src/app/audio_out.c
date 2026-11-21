#include "audio_out.h"

#include <stddef.h>
#include <string.h>

void audio_out_init(AudioOut *out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof *out);

    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        TraceLog(LOG_WARNING, "INKSTORM: audio device unavailable; running muted");
        return;
    }

    SetAudioStreamBufferSizeDefault(AUDIO_STREAM_FRAMES);
    out->stream = LoadAudioStream(AUDIO_SAMPLE_RATE, 16, 1);
    if (!IsAudioStreamValid(out->stream)) {
        CloseAudioDevice();
        TraceLog(LOG_WARNING, "INKSTORM: audio stream unavailable; running muted");
        return;
    }
    PlayAudioStream(out->stream);
    out->ready = true;
}

void audio_out_pump(AudioOut *out, AudioState *audio)
{
    if (out == NULL || !out->ready || audio == NULL) {
        return;
    }
    for (int i = 0; i < 2; ++i) {
        if (!IsAudioStreamProcessed(out->stream)) {
            break;
        }
        audio_render(audio, out->pcm, AUDIO_STREAM_FRAMES);
        UpdateAudioStream(out->stream, out->pcm, AUDIO_STREAM_FRAMES);
    }
}

void audio_out_close(AudioOut *out)
{
    if (out == NULL || !out->ready) {
        return;
    }
    StopAudioStream(out->stream);
    UnloadAudioStream(out->stream);
    CloseAudioDevice();
    memset(out, 0, sizeof *out);
}
