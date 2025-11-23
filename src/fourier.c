#include <complex.h>
#include <err.h>
#include <math.h>
// #include <stdio.h>

// TODO: ADD FFT AND RADIX CONTROL

// Computes real resultant DFT of signal
int dft_real(short *samples_re, short *samples_im, double *result_buffer,
             unsigned int length) {

  // temp value to store x[k] during summation
  double complex x_k;
  // TODO: Stop reallocating this somehow
  // float result_buffer[length];

  for (int k = 0; k < length; k++) {
    x_k = 0.0;
    for (int n = 0; n < length; n++) {
      x_k = x_k +
            ((double)samples_re[n] + (double)samples_im[n] * (double)I) *
                cexp(-I * 2.0 * M_PI * (double)k * (double)n / (double)length);
    }
    // printf("Real: %f, Imaginary: %f, Magnitude: %f\n", creal(x_k),
    // cimag(x_k),
    //        cabs(x_k));
    result_buffer[k] = cabs(x_k);
  }
  return 0;
}
