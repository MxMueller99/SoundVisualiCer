#ifndef PULSE_H
#define PULSE_H

#include <stdint.h>

#define FFT_BUF_SIZE 512
#define SAMPLE_RATE 44100

int pa_init(void);
void get_pa_fft_buf(double *fft_buf);
int is_fft_ready(void);

#endif
