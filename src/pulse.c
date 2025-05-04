#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pulse/pulseaudio.h>
#include <math.h>
#include "pulse.h"
#include <pthread.h>

// PulseAudio objects
static pa_mainloop *pa_ml = NULL;
static pa_context *context = NULL;
static pa_stream *stream = NULL;

// FFT buffer
// ToDo: Implement ringbuffer
static double fft_buf[FFT_BUF_SIZE];
static int fft_buf_index = 0;
static pthread_mutex_t fft_lock = PTHREAD_MUTEX_INITIALIZER;
static int fft_ready = 0;
static double latest_fft_buf[FFT_BUF_SIZE];

// Stereo 16-bit buffer for audio
static int16_t audio_buffer[FFT_BUF_SIZE * 2];

// Source Name
// ToDo: Make it automatically select right source
static const char *device = "alsa_output.pci-0000_04_00.6.5.analog-stereo.monitor";

// Sample Spec
static const pa_sample_spec ss = {
    .format = PA_SAMPLE_S16LE,
    .rate = SAMPLE_RATE,
    .channels = 2
};

void stream_read_cb(pa_stream *s, size_t length, void *userdata) {

    const void *data;
    if (pa_stream_peek(s, &data, &length) < 0 || !data || length == 0) {
        printf("pa peek failed.\n");
        return;
    }

    size_t frames = length / (2 * sizeof(int16_t)); // L and R
    const int16_t *samples = (const int16_t *)data;

    for (size_t i = 0; i < frames; i++) {
        int16_t left = samples[2 * i];
        int16_t right = samples[2 * i + 1];
        double mono = (left + right) / 2.0;

        fft_buf[fft_buf_index++] = mono;

        if (fft_buf_index >= FFT_BUF_SIZE) {
            pthread_mutex_lock(&fft_lock);
            memcpy(latest_fft_buf, fft_buf, sizeof(latest_fft_buf));
            fft_ready = 1;
            pthread_mutex_unlock(&fft_lock);
            fft_buf_index = 0;
        }
    }

    // drop current fragment after peek
    pa_stream_drop(s);
}


void context_state_cb(pa_context *c, void *userdata) {
    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_READY:
            pa_buffer_attr attr = {
                .maxlength = (uint32_t)-1,
                .tlength = 0,
                .prebuf = 0,
                .minreq = 0,
                .fragsize = sizeof(audio_buffer)
            };

            stream = pa_stream_new(c, "SoundVisualiCer", &ss, NULL);
            if (!stream) {
                printf("Failed to create stream.\n");
                pa_mainloop_quit(pa_ml, 1);
                return;
            }

            pa_stream_set_read_callback(stream, stream_read_cb, NULL);

            if (pa_stream_connect_record(stream, device, &attr, PA_STREAM_ADJUST_LATENCY) < 0) {
                printf("Failed to connect record stream.\n");
                pa_mainloop_quit(pa_ml, 1);
            }
            break;

        case PA_CONTEXT_FAILED:
            printf("Context failed.\n");
            pa_mainloop_quit(pa_ml, 1);
            break;

        case PA_CONTEXT_TERMINATED:
            printf("Context terminated.\n");
            pa_mainloop_quit(pa_ml, 1);
            break;
    }
}

void get_pa_fft_buf(double *out_buf) {
    pthread_mutex_lock(&fft_lock);

    if (fft_ready) {
        memcpy(out_buf, latest_fft_buf, sizeof(latest_fft_buf));
        fft_ready = 0;
    } else {
        // no data to fill
        memset(out_buf, 0, sizeof(latest_fft_buf));
    }

    pthread_mutex_unlock(&fft_lock);
}

void *pa_thread_routine() {
    int ret;
    pa_mainloop_run(pa_ml, &ret);
    return NULL;
}

int is_fft_ready(void) {
    pthread_mutex_lock(&fft_lock);
    int ready = fft_ready;
    pthread_mutex_unlock(&fft_lock);
    return ready;
}


int pa_init() {
    pa_ml = pa_mainloop_new();
    pa_mainloop_api *pa_mlapi = pa_mainloop_get_api(pa_ml);

    context = pa_context_new(pa_mlapi, "SoundVisualiCer");
    pa_context_set_state_callback(context, context_state_cb, NULL);

    if (pa_context_connect(context, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0) {
        printf("Failed to connect pa context.\n");
        return 1;
    }

    pthread_t thread;
    if (pthread_create(&thread, NULL, pa_thread_routine, NULL) != 0) {
        return 1;
    }

    pthread_detach(thread);
    return 0;
}


