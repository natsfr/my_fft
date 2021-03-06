#include <stdio.h>
#include <math.h>
#include <malloc.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <stdint.h>

uint32_t FFT_SIZE=(4*1024);

double *sines;
double *cosines;

double *din_r;
double *din_i;

double *dout_r;
double *dout_i;

double *dout_r_ref;
double *dout_i_ref;

static void init_buf(uint32_t fftsize) {
	sines = (double *)aligned_alloc(32, fftsize*sizeof(double));
	cosines = (double *)aligned_alloc(32, fftsize*sizeof(double));

	din_r = (double *)aligned_alloc(32, fftsize*sizeof(double));
	din_i = (double *)aligned_alloc(32, fftsize*sizeof(double));

	dout_r = (double *)aligned_alloc(32, fftsize*sizeof(double));
	dout_i = (double *)aligned_alloc(32, fftsize*sizeof(double));

	dout_r_ref = (double *)aligned_alloc(32, fftsize*sizeof(double));
	dout_i_ref = (double *)aligned_alloc(32, fftsize*sizeof(double));
}

//=================================================================
// Save calling sin() and cos() all the time
//=================================================================
static void init_tables(void) {
   for(int i = 0; i < FFT_SIZE; i++) {
     double phase = 2.0*M_PI*i/FFT_SIZE;
     cosines[i] = cos(phase);
     sines[i]   = sin(phase);
  }
}

static void print_out(double *d_r, double *d_i, int size, char *message) {
   printf("-----------------------------------------\n");
   if(message) {
      printf("%s:\n", message);
   }
   for(int i = 0; i < size; i++) {
      printf("%3i, %10f, %10f\n", i, d_r[i], d_i[i]);
   }
   getchar();
}

static void dft4(double *r_i, double *i_i, double *r_o, double *i_o, int stride) {
   int total_size = stride*4;
   // Bin 0
   r_o[0] = (r_i[0*stride] + r_i[1*stride] + r_i[2*stride] + r_i[3*stride])/total_size;
   i_o[0] = (i_i[0*stride] + i_i[1*stride] + i_i[2*stride] + i_i[3*stride])/total_size;
   // Bin 1
   r_o[1] = (r_i[0*stride] - i_i[1*stride] - r_i[2*stride] + i_i[3*stride])/total_size;
   i_o[1] = (i_i[0*stride] + r_i[1*stride] - i_i[2*stride] - r_i[3*stride])/total_size;
   // Bin 2
   r_o[2] = (r_i[0*stride] - r_i[1*stride] + r_i[2*stride] - r_i[3*stride])/total_size;
   i_o[2] = (i_i[0*stride] - i_i[1*stride] + i_i[2*stride] - i_i[3*stride])/total_size;
   // Bin 3
   r_o[3] = (r_i[0*stride] + i_i[1*stride] - r_i[2*stride] - i_i[3*stride])/total_size;
   i_o[3] = (i_i[0*stride] - r_i[1*stride] - i_i[2*stride] + r_i[3*stride])/total_size;
}

static void dft(double *r_i, double *i_i, double *r_o, double *i_o, int size, int stride) {
   for(int bin = 0; bin < size; bin++) {
      int i = 0;
      double total_i = 0.0;
      double total_q = 0.0;
      for(int s = 0; s < size; s++) {
         total_i +=  r_i[s*stride] * cosines[i] - i_i[s*stride] *   sines[i];
         total_q +=  r_i[s*stride] *   sines[i] + i_i[s*stride] * cosines[i];
         i += bin*(FFT_SIZE/size);
         if(i >= FFT_SIZE) i -= FFT_SIZE;
      }
      r_o[bin] = total_i/FFT_SIZE;
      i_o[bin] = total_q/FFT_SIZE;
   }
}

static int reverse_bits(int i, int max) {
   int o = 0;
   assert(i < max || i >= 0);
   i |= max;
   while(i != 1) {
      o <<= 1;
      if(i&1) 
        o |= 1; 
      i >>= 1;
   }
   return o;
}

static void fft_v2(double *r_i, double *i_i, double *r_o, double *i_o, int size) {
    int i, stride, step;

    stride = size/4;
    for(i = 0; i < stride ; i++) {
        int out_offset = reverse_bits(i,stride) * 4;
        dft4(r_i+i, i_i+i,  r_o+out_offset, i_o+out_offset, stride);
    }

    stride = 8;
    step = size/stride;
    while(stride <= size) {
       for(i = 0; i < size; i+= stride) {
          double *real = r_o+i;
          double *imag = i_o+i;
          int half_size = stride/2;
          for(int j = 0; j < half_size; j++) {
             double c = cosines[j*step], s = sines[j*step];
             double rotated_r =  real[half_size] * c - imag[half_size] * s;
             double rotated_i =  real[half_size] * s + imag[half_size] * c;
             real[half_size] = real[0] - rotated_r;
             imag[half_size] = imag[0] - rotated_i;
             real[0]         = real[0] + rotated_r;
             imag[0]         = imag[0] + rotated_i;
             real++;
             imag++;
          }
       }
       stride *= 2;
       step   /= 2;
    }
}

static void fft_v1(double *r_i, double *i_i, double *r_o, double *i_o, int size, int stride) {
    int half_size = size/2;

    if((half_size & 1) != 0) {
       dft(r_i,        i_i,        r_o,            i_o,           half_size, stride*2);
       dft(r_i+stride, i_i+stride, r_o+half_size,  i_o+half_size, half_size, stride*2);
    } else if (half_size==4) {
       dft4(r_i,        i_i,        r_o,            i_o,           stride*2);
       dft4(r_i+stride, i_i+stride, r_o+half_size,  i_o+half_size, stride*2);
    } else {
       fft_v1(r_i,        i_i,        r_o,            i_o,           half_size, stride*2);
       fft_v1(r_i+stride, i_i+stride, r_o+half_size,  i_o+half_size, half_size, stride*2);
    }

    int step = stride;
    for(int i = 0; i < half_size; i++) {
       double c = cosines[i*step];
       double s =   sines[i*step];

       double even_r = r_o[i];
       double even_i = i_o[i];
       double odd_r  = r_o[i+half_size];
       double odd_i  = i_o[i+half_size];

       double rotated_r =  odd_r * c - odd_i * s;
       double rotated_i =  odd_r * s + odd_i * c;
       r_o[i]           = even_r + rotated_r;
       i_o[i]           = even_i + rotated_i;
       r_o[i+half_size] = even_r - rotated_r;
       i_o[i+half_size] = even_i - rotated_i;
    }
}

void ts_sub(struct timespec *r, struct timespec *a, struct timespec *b) {
   r->tv_sec = a->tv_sec - b->tv_sec;
   if(a->tv_nsec > b->tv_nsec) {
      r->tv_nsec = a->tv_nsec - b->tv_nsec;
   } else {
      r->tv_sec--;
      r->tv_nsec = a->tv_nsec - b->tv_nsec+1000000000;
   }
}

void check_error(double *r_ref, double *i_ref, double *r, double *j, int size) {
   // Check for error
   double error = 0.0;
   for(int j = 0; j < size; j++) {
      error += fabs(dout_r_ref[j] - dout_r[j]); 
      error += fabs(dout_i_ref[j] - dout_i[j]); 
   };
   printf("  Total error is %10e\n",error);
}

int main(int argc, char *argv[]) {
   struct timespec tv_start_dft,    tv_end_dft,    tv_dft;
   struct timespec tv_start_fft_v1, tv_end_fft_v1, tv_fft_v1;
   struct timespec tv_start_fft_v2, tv_end_fft_v2, tv_fft_v2;

   if(argc < 3) {
   		printf("Error: my_fft FFT_SIZE nbcycle");
   		return -1;
   }

	uint32_t fftsize = 0;
	fftsize = atoi(argv[1]);
	FFT_SIZE = fftsize;

  	uint32_t nbcycle = 0;
  	nbcycle = atoi(argv[2]);

  	init_buf(fftsize);

   // Setup
   for(int i = 0; i < FFT_SIZE; i++) {
#if 0
      if(i %64 < 32)  
         din_r[i] =   1.0;
      else
         din_r[i] =  -1.0;
      din_i[i] = 0;
#else
      din_r[i] =rand() %101 /100.0;
      din_i[i] =rand() %101 /100.0;
#endif
   }
   init_tables();

   printf("Transform of %5i random complex numbers\n", FFT_SIZE);
   printf("=========================================\n");

   // The standar DFT implementation
   clock_gettime(CLOCK_MONOTONIC, &tv_start_dft);
   //for(uint32_t i = 0; i < nbcycle; i++) {
   		dft(din_r,din_i, dout_r_ref, dout_i_ref, FFT_SIZE,1);
	//}
   clock_gettime(CLOCK_MONOTONIC, &tv_end_dft);  

   ts_sub(&tv_dft,    &tv_end_dft,    &tv_start_dft);
   uint64_t dft_time = (unsigned)tv_dft.tv_sec * 1e9 + (unsigned)tv_dft.tv_nsec;
   printf("DFT %u.%09u secs\n",(unsigned)tv_dft.tv_sec, (unsigned)tv_dft.tv_nsec);
   //printf("DFT %d nsecs\n",dft_time);

   // The recursive FFT implementation
   clock_gettime(CLOCK_MONOTONIC, &tv_start_fft_v1);
   for(uint32_t i = 0; i < nbcycle; i++) {
   		fft_v1(din_r,din_i, dout_r,     dout_i, FFT_SIZE, 1);
   }
   clock_gettime(CLOCK_MONOTONIC, &tv_end_fft_v1);  

   ts_sub(&tv_fft_v1, &tv_end_fft_v1, &tv_start_fft_v1);
   // Beware of the valid range
   uint64_t r_fft_time = ((unsigned)tv_fft_v1.tv_sec * 1e9 + (unsigned)tv_fft_v1.tv_nsec)/nbcycle;
   printf("FFT recursive %d nsecs\n",r_fft_time);

   check_error(dout_r_ref, dout_i_ref, dout_r,dout_i, FFT_SIZE);

   // The looping FFT implementation
   clock_gettime(CLOCK_MONOTONIC, &tv_start_fft_v2);
   for(uint32_t i = 0; i < nbcycle; i++) { 
   		fft_v2(din_r,din_i, dout_r,     dout_i, FFT_SIZE);
   }
   clock_gettime(CLOCK_MONOTONIC, &tv_end_fft_v2);  

   ts_sub(&tv_fft_v2, &tv_end_fft_v2, &tv_start_fft_v2);
   uint64_t l_fft_time = ((unsigned)tv_fft_v2.tv_sec * 1e9 + (unsigned)tv_fft_v2.tv_nsec)/nbcycle;
   printf("FFT looped %d nsecs\n",l_fft_time);

   check_error(dout_r_ref, dout_i_ref, dout_r,dout_i, FFT_SIZE);

   (void)print_out;  
}

