#include <stdio.h>
void dump_spectrum(double *x_buffer, double *y_buffer, int buffer_length,
                   char *file_name) {
  FILE *flptr = fopen(file_name, "w");
  for (int i = 0; i < buffer_length; i++) {
    fprintf(flptr, "%lf %lf\n", x_buffer[i], y_buffer[i]);
  }
  fclose(flptr);
}
