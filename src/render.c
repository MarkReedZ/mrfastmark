
#include "py_defines.h"
#include <stdio.h>
#include <stdbool.h>

#ifdef __AVX2__
#include <immintrin.h>
#elif defined __SSE4_2__
#ifdef _MSC_VER
#include <nmmintrin.h>
#else
#include <x86intrin.h>
#endif
#endif

#if __GNUC__ >= 3
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif



#define CHAR4_INT(a, b, c, d)         \
   (unsigned int)((d << 24) | (c << 16) | (b << 8) | a)

static inline bool _isdigit(char c) { return c >= '0' && c <= '9'; }


typedef struct _encoder
{
  char *start, *end, *o;
  char *last; 
} Encoder;

//static void print_buffer( char* b, int len ) {
  //for ( int z = 0; z < len; z++ ) {
    //printf( "%02x ",(int)b[z]);
  //}
  //printf("\n");
//}

static int SetError(const char *message)
{
  PyErr_Format (PyExc_ValueError, "%s", message);
  return 0;
}
static int resizeBuffer(Encoder *e, size_t len)
{
  size_t curSize = e->end - e->start;
  size_t newSize = curSize * 2;
  size_t offset = e->o - e->start;

  while (newSize < curSize + len) newSize *= 2;

  e->start = (unsigned char *) realloc (e->start, newSize);
  if (e->start == NULL)
  {
    SetError ("Could not reserve memory block");
    printf("resizeBuffer failed\n");
    return 0;
  }

  e->o = e->start + offset;
  e->end = e->start + newSize;
  return 1;
}

#define resizeBufferIfNeeded(__enc, __len) \
    if ( (size_t) ((__enc)->end - (__enc)->o) < (size_t) (__len))  { resizeBuffer((__enc), (__len)); }

static inline void reverse(char* begin, char* end)
{
  char t;
  while (end > begin) {
    t = *end;
    *end-- = *begin;
    *begin++ = t;
  }
}

static char *findchar_fast(char *buf, char *buf_end, char *ranges, size_t ranges_size, int *found)
{
    *found = 0;
    __m128i ranges16 = _mm_loadu_si128((const __m128i *)ranges);
    if (likely(buf_end - buf >= 16)) {

        size_t left = (buf_end - buf) & ~15;
        do {
            __m128i b16 = _mm_loadu_si128((const __m128i *)buf);
            int r = _mm_cmpestri(ranges16, ranges_size, b16, 16, _SIDD_LEAST_SIGNIFICANT | _SIDD_CMP_RANGES | _SIDD_UBYTE_OPS);
            if (unlikely(r != 16)) {
                buf += r;
                *found = 1;
                return buf;
            }
            buf += 16;
            left -= 16;
        } while (likely(left != 0));

    }

    size_t left = buf_end - buf;
    if ( left != 0 ) {
      static char sbuf[16] = {0};
      memcpy( sbuf, buf, left );
      __m128i b16 = _mm_loadu_si128((const __m128i *)sbuf);
      int r = _mm_cmpestri(ranges16, ranges_size, b16, 16, _SIDD_LEAST_SIGNIFICANT | _SIDD_CMP_RANGES | _SIDD_UBYTE_OPS);
      if (unlikely(r != 16) && r < left) {
        buf += r;
        *found = 1;
        return buf;
      } else {
        buf = buf_end;
      }
    }

    *found = 0;
    return buf;
}

//static const char *get_token_to_eol(const char *buf, const char *buf_end, const char **token, size_t *token_len, int *ret)
//{
  //char *end = path + length;
  //pat = findchar_fast(pat, end, ranges1, sizeof(ranges1) - 1, &found);
//
    //return buf;
//}
//

static uint32_t s_httpcode = 0x70747468;
static uint32_t s_h1 = 0x3e31683c;
static uint64_t s_end_h1 = 0x0a0a3e31682f3c;
static uint32_t s_para = 0x203e703c; // <p>
static uint64_t s_end_para = 0x0000000a3e702f3c; // </p>\x0a

static char s_code[16]     = "<code>";
static char s_end_code[16] = "</code>";
static char s_strong[16]     = "<strong>";
static char s_end_strong[16] = "</strong>";
static char s_bq[16] = "<blockquote>\n";
static char s_end_bq[16] = "\n</blockquote>";

// <a href="http://you.czm">http://you.czm</a>
static char s_http[16] = "<a href=\"http://"; // 16
static char s_mid_http[16] = "\">http://"; // 9
static char s_end_http[16] = "</a>"; // 4

static char stripbuf[16*1024];
static char escbuf[16*1024];

int striphtml( const char *st, int l, Encoder *e ) {
  
  const char *p = st;
  const char *end = st+l;
  const char *last = st;
  char *o = stripbuf;
  int found;
  static char ranges1[] = "<<" ">>";

  char *opentag = NULL;

  while ( p < end ) {
    p = findchar_fast(p, end, ranges1, sizeof(ranges1) - 1, &found);
    if ( found ) {
      if ( p[0] == '<' ) {
        if ( !opentag ) {
          p += 1;
          opentag = p;
        }
      }
      else if ( p[0] == '>' ) {
        if ( opentag ) {
          memcpy( o, last, opentag-last-1 ); o += opentag-last-1;
          opentag = NULL;
          p += 1;
          last = p;
        } else {
          p += 1;
        }
      }
    } 
    //else {
      //while ( p++ < end ) {
      //}
    //}
  }

  memcpy( o, last, end-last ); o += end-last;
  return (int)(o-stripbuf);
}

int escape( const char *st, int l, Encoder *e ) {
  
  const char *p = st;
  const char *end = st+l;
  const char *last = st;
  char *o = escbuf;
  int found;
  static char ranges1[] = "<<" ">>" "\x22\x22" "&&";

  while ( p < end ) {
    //printf("escepe loop %c\n",*p);
    p = findchar_fast(p, end, ranges1, sizeof(ranges1) - 1, &found);
    if ( found ) {
      //printf("found\n");
      if ( p[0] == '<' ) {
        memcpy( o, last, p-last ); o += p-last;
        memcpy( o, "&lt;", 4); o += 4;
        p++;
        last = p;
      }
      if ( p[0] == '&' ) {
        memcpy( o, last, p-last ); o += p-last;
        memcpy( o, "&amp;", 5); o += 5;
        p++;
        last = p;
      }
      if ( p[0] == '>' ) {
        memcpy( o, last, p-last ); o += p-last;
        memcpy( o, "&gt;", 4); o += 4;
        p++;
        last = p;
      }
      if ( p[0] == 0x22 ) {
        memcpy( o, last, p-last ); o += p-last;
        memcpy( o, "&quot;", 6); o += 6;
        p++;
        last = p;
      }
    } 
    //else {
      //printf(" end - p %d\n", end-p);
      //p++;
    //}

  }

  memcpy( o, last, end-last ); o += end-last;
  return (int)(o-escbuf);
}

void line( const char *st, int l, Encoder *e ) {

  //l = striphtml( st, l , e );
  //st = stripbuf;
  l = escape( st, l , e );
  st = escbuf;
  //printf("After escape l is %d >%.*s<\n",l,l, st);

  char *em = NULL;
  char *bold = NULL;
  char *code = NULL;
  int found;
  //printf(" l %d\n",l);

  const char *p = st;
  const char *end = st+l;
  const char *last = st;
  //printf("A\n");

  // TODO?
  // Keep a stack of struct ( open/close, type, ptr )
  // At end of line we construct by memcpy insert tag memcpy insert tag
  // Unclosed tags are left in
  //  **  `  `  **  **

  // Right now we don't allow nesting
  while ( p < end ) {
    //printf("line loop %c\n",*p);
    static char ranges1[] = "**" "::" "\x00\x00" "<<" ">>" "``";
    p = findchar_fast(p, end, ranges1, sizeof(ranges1) - 1, &found);
    //printf("YAY\n");
    if ( found ) {
      //printf( " found >%.*s<\n", 2, p );
      if ( p[0] == ':' ) {

        // http://
        if ( p[1] == '/' && (p-e->start) > 4 && *((uint32_t*)(p-4)) == s_httpcode && p[2] == '/' ) {
          p += 3;
          char *linkstart = p; 
          while ( p < end && !(*p == '.' || *p == ' ' || *p == '\n') ) p++;

          if ( p[0] == '.' ) {
            while ( p < end && *p != ' ' && *p != '\n' ) p++;
            //printf(" link >%.*s<\n", p-linkstart, linkstart );            
            
            memcpy( e->o, last, linkstart-last-7 ); e->o += linkstart-last-7;
            memcpy( e->o, s_http, 16); e->o += 16;
              memcpy( e->o, linkstart, p-linkstart); e->o += p-linkstart;
            memcpy( e->o, s_mid_http, 9); e->o += 9;
              memcpy( e->o, linkstart, p-linkstart); e->o += p-linkstart;
            memcpy( e->o, s_end_http, 4); e->o += 4;
            last = p;
          } else {
            p++;
          }
             
        } else {
          p++;
        }
      } 
      else if ( p[0] == '*' && p[1] == '*' && !code) {
        if ( bold ) {
          memcpy( e->o, last, bold-last-2 ); e->o += bold-last-2;
          memcpy( e->o, s_strong, 8); e->o += 8;
          memcpy( e->o, bold, p-bold ); e->o += p-bold;
          memcpy( e->o, s_end_strong, 9); e->o += 9;
          bold = NULL;
          p += 2;
          last = p;
        } else {
          p += 2;
          bold = p;
          //printf( " before >%.*s<\n", (int)(bold-last), last );
        }
      }
      else if ( p[0] == 0x60 ) { //'`' ) {
        //printf("code tick\n");
        if ( bold ) bold = NULL;
        if ( code ) {
          memcpy( e->o, last, code-last-1 ); e->o += code-last-1;
          memcpy( e->o, s_code, 6); e->o += 6;
          memcpy( e->o, code, p-code ); e->o += p-code;
          memcpy( e->o, s_end_code, 7); e->o += 7;
          code = NULL;
          p += 1;
          last = p;
        } else {
          p += 1;
          code = p;
        } 
      }
      else if ( p[0] == '<' ) {
        //opentag = p;
        //memcpy( e->o, last, bold-last-2 ); e->o += bold-last-2;
        p++;
      } else {
        p++;
      }
      //memcpy( e->o, st, p-st ); e->o += p-st;
    } else {
      //printf(" not found end-p is %d\n", (int)(end-p));
      while ( p++ < end ) {
        if ( p[0] == '*' ) {
        }
      }
      
      //memcpy( e->o, st, p-st ); e->o += p-st;
    }
  
  } 
  //printf("last to end? %d >%.*s<\n", (int)(end-last), end-last, last);
  memcpy( e->o, last, end-last ); e->o += end-last;
  //printf("done?\n");
}

PyObject *_render( PyObject *md, Encoder *e ) {

  //resizeBufferIfNeeded(e,2048);

  Py_ssize_t l;
  const char *p = PyUnicode_AsUTF8AndSize(md, &l);
  const char *end = p+l;
  int found;

  int bq = 0;
  int para = 0;
  int sameline = 0;

// > dd
// >
// > zz



  while ( p < end ) {
    //printf("txt >%.*s<\n", 4, p);
    

    if ( p[0] == '\n' ) {
      //printf("blank line\n");
      if ( para ) {
        *((uint64_t *)e->o) = s_end_para;
        e->o += 5;
      }
      if ( bq && !sameline) {
        if (para) e->o -= 1;
        memcpy( e->o, s_end_bq, 14 ); e->o += 14;
        *(e->o) = '\n'; e->o += 1;
        bq = 0;
      }
      para = 0;
    }
    else if ( p[0] == '#' ) {

      if ( para ) { 
        *((uint64_t *)e->o) = s_end_para;
        e->o += 5;
        para = 0;
      }

      *((unsigned int*)e->o) = s_h1;
      e->o += 4;

      const char *st = p;
      static char ranges1[] = "\x0a\x0a";
      p = findchar_fast(p, end, ranges1, sizeof(ranges1) - 1, &found);
      if ( found ) {
        memcpy( e->o, st, p-st ); e->o += p-st;
      } else {
        while ( p < end && *p++ != '\n' );
        memcpy( e->o, st, p-st ); e->o += p-st;
      }

      *((uint64_t *)e->o) = s_end_h1;
      e->o += 7;
                 
    }
    else if ( p[0] == '>' ) {
      p++;
      sameline = 1;
 
      if ( bq == 0 ) { 
        if ( para ) {
          *((uint64_t *)e->o) = s_end_para;
          e->o += 5;
          para = 0;
        }
        bq = 1;
        memcpy( e->o, s_bq, 13 ); e->o += 13;
      } 
      continue;
    }
    else if ( (p[1] == '.' || p[1] == ')') && _isdigit(p[0]) && p[2] == ' ' ) {
      p++;
      //ol = 1
      //p += 2
// 1. xx 
// 2. yy
// 3. zz

// <ol>
// <li>One</li>
// <li>Two</li>
// <li>Three</li>
// </ol>

      
       
    
    }
    else {

        //printf("else line\n");
      //if ( !bq ) {
      if ( para == 0 ) {
        *((unsigned int*)e->o) = s_para;
        e->o += 3;
        para = 1;
      } else {
        *(e->o++) = '\n'; 
      }

      //}
      const char *st = p;
      static char ranges1[] = "\x0a\x0a";
      p = findchar_fast(p, end, ranges1, sizeof(ranges1) - 1, &found);
      if ( found ) {
        //printf("fnd line\n");
        line( st, p-st, e );
        //memcpy( e->o, st, p-st ); e->o += p-st;
      } else {
        //printf("not fnd line\n");
        while ( p < end && *p++ != '\n' );
        line( st, p-st, e );
        //printf("after line\n");
        //memcpy( e->o, st, p-st ); e->o += p-st;
        if ( p>=end) {

          if ( para ) {
            *((uint64_t *)e->o) = s_end_para;
            e->o += 5;
          }
          if ( bq ) {
            e->o -= 1;
            memcpy( e->o, s_end_bq, 14 ); e->o += 14;
          }
        }
        //return PyUnicode_FromStringAndSize( e->start, e->o-e->start );  
      }

    }

    p += 1;
    sameline = 0;
    //while ( p < end && *p++ == '\n' );
    
  }
  return PyUnicode_FromStringAndSize( e->start, e->o-e->start );  
}

PyObject* render(PyObject* self, PyObject *md) {

  Encoder enc = { NULL,NULL,NULL };

  int len = 65536;
  unsigned char *s = (unsigned char *) malloc (len);
  if (!s) {
    SetError("Could not reserve memory block");
    return 0;
  }

  enc.start = s;
  enc.end   = s + len;
  enc.o = s;

  return _render(md, &enc);
 
}
