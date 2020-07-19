#include "py_defines.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct _decoder
{
  unsigned char *start, *end, *s;
} Decoder;


//static char    *sbuf;
//static uint32_t sbuf_len;

//static void print_buffer( unsigned char* b, int len ) {
  //for ( int z = 0; z < len; z++ ) {
    //printf( "%02x ",(int)b[z]);
  //}
  //printf("\n");
//}


//PyObject *decode( PyObject *htm, Decoder *d ) { 
  //return htm;
//}

PyObject* html(PyObject* self, PyObject *htm) {

  //Decoder d = { p,p+l,p,0 };
  //return decode( &d );

  return htm;

}

