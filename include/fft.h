#ifndef FFT_H
#define FFT_H

#include <stddef.h>
#include <fftw3.h>

#define FFT_BUF_SIZE 512

void fft_init();
void compute_fft(double *input, double *output_bins);
void smooth_fft(double *input, double *smoothed_bins, double alpha);
void destroy_fft();

#endif


