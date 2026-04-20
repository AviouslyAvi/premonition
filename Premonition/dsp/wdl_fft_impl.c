/* Compile WDL FFT as C (not C++) — fft.c uses the register keyword which is
   invalid in C++17 but legal in C99/C11. */
#include "../../iPlug2/WDL/fft.c"
