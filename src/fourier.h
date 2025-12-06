#ifndef FOURIER_H
#define FOURIER_H

int *dft_real(short *samples_re, short *samples_im, double *result_buffer,
              unsigned int length);

#endif
