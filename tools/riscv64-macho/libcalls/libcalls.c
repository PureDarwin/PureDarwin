// calls llvm makes up on its own, darwin has them under libSystem names
double pow(double, double);
float powf(float, float);
double sin(double), cos(double);
float sinf(float), cosf(float);

double exp10_d(double x) { return pow(10.0, x); }
float exp10_f(float x) { return powf(10.0f, x); }

// sin and cos of one value fold into a single __sincos_stret call
double sincos_d(double x, double *c) {
  *c = cos(x);
  return sin(x);
}
float sincos_f(float x, float *c) {
  *c = cosf(x);
  return sinf(x);
}

void zero(void *p, unsigned long n) { __builtin_memset(p, 0, n); }
