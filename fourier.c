#include <complex.h>
#include <err.h>
#include <math.h>
#include <stdio.h>

// TODO: ADD FFT AND RADIX CONTROL

// Computes real resultant DFT of signal
int dft_real(short *samples_re, short *samples_im, float *result_buffer,
             unsigned int length) {

  // temp value to store x[k] during summation
  float _Complex x_k;
  // TODO: Stop reallocating this somehow
  // float result_buffer[length];

  for (int k = 0; k < length; k++) {
    x_k = 0.0f;
    for (int n = 0; n < length; n++) {
      x_k = x_k +
            ((float)samples_re[n] + (float)samples_im[n] * (float)_Complex_I) *
                expf(-(float)_Complex_I * 2.0f * (float)M_PI * (float)k *
                     (float)n / (float)length);
    }
    printf("Real: %f, Imaginary: %f, Magnitude: %f\n", creal(x_k), cimag(x_k),
           cabsf(x_k));
    // result_buffer[k] = cabsf(x_k);
  }
  return 0;
}
