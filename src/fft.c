#include <fftw3.h>
#include "fft.h"
#include <math.h>

static double *fft_input;
static fftw_complex *fft_output;
static fftw_plan p;

void fft_init() {
    fft_input = (double *) fftw_malloc(sizeof(double) * FFT_BUF_SIZE);
    fft_output = (fftw_complex *) fftw_malloc(sizeof(fftw_complex) * (FFT_BUF_SIZE/2 + 1)); 
    p = fftw_plan_dft_r2c_1d(FFT_BUF_SIZE, fft_input, fft_output, FFTW_MEASURE); // FFTW_MEASURE takes some seconds to setup   
}

void compute_fft(double *input, double *output_bins) {
    // processes the input data, executes fftw plan, processes the output data

    for (int i=0; i<FFT_BUF_SIZE; i++) {
        fft_input[i] = input[i];
    }

    fftw_execute(p);

    for (int i=0; i<FFT_BUF_SIZE/2; i++) {
        double re = fft_output[i][0];
        double im = fft_output[i][1];
        //output_bins[i] = sqrt(re*re + im*im) / (FFT_BUF_SIZE);
        double magnitude = sqrt(re*re + im*im) / (FFT_BUF_SIZE);
        output_bins[i] = 20.0 * log10(magnitude + 1e-6); // dB Calculation
    }
}

void smooth_fft(double *input, double *smoothed_bins, double alpha) {
    for (int i=0; i<FFT_BUF_SIZE/2; i++) {
        smoothed_bins[i] = alpha * smoothed_bins[i] + (1.0 - alpha) * input[i];
    }
}

void destroy_fft() {
    fftw_destroy_plan(p);
    fftw_free(fft_input);
    fftw_free(fft_output);
}