
#include "py_defines.h"
#include <stdio.h>
#include <stdbool.h>

#include "setup.h"

#define DBG if(0)

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



#define CHAR8_LONG(a, b, c, d, e, f, g, h)       \
   (((long)h << 56) | ((long)g << 48) | ((long)f << 40)   \
    | ((long)e << 32) | (d << 24) | (c << 16) | (b << 8) | a)

#define CHAR4_INT(a, b, c, d)         \
   (unsigned int)((d << 24) | (c << 16) | (b << 8) | a)

static inline bool _isdigit(char c) { return c >= '0' && c <= '9'; }

static __inline__ uint64_t rdtsc(void)
{
  unsigned long long int x;
     __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
     return x;
}


typedef struct _encoder
{
  char *start, *end, *o;
  char *last; 
} Encoder;

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

  e->start = (char *) realloc (e->start, newSize);
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
    if ( (size_t) ((__enc)->end - (__enc)->start) < (size_t) (__len))  { resizeBuffer((__enc), (__len)); }

static inline void reverse(char* begin, char* end)
{
  char t;
  while (end > begin) {
    t = *end;
    *end-- = *begin;
    *begin++ = t;
  }
}

// Search for a range of characters and return a pointer to the location or buf_end if none are found
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
      if (unlikely(r != 16) && r < (int)left) {
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


static uint64_t s_end_para = 0x0000000a3e702f3c; // </p>\x0a

static char stripbuf[16*1024];
static char escbuf[16*1024];

int striphtml( char *st, int l, Encoder *e ) {
  
  char *p = st;
  char *end = st+l;
  char *last = st;
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
  }

  memcpy( o, last, end-last ); o += end-last;
  return (int)(o-stripbuf);
}

int escape( char *st, int l, Encoder *e ) {
  if ( l > 16000 ) return -1;  
  char *p = st;
  char *end = st+l;
  char *last = st;
  char *o = escbuf;
  int found;
  static char ranges1[] = "<<" ">>" "\x22\x22" "&&";

  while ( p < end ) {
    p = findchar_fast(p, end, ranges1, sizeof(ranges1) - 1, &found);
    if ( found ) {
      if ( p[0] == '<' ) {
        memcpy( o, last, p-last ); o += p-last;
        memcpy( o, "&lt;", 4); o += 4;
        p++;
        last = p;
      }
      // TODO The browser renders the & stuff appropriately.  What do we need here?
      if ( p[0] == '&' ) {
        memcpy( o, last, p-last ); 
        //printf("last: >%.*s<\n", p-last, o );
        o += p-last;

        if ( p[1] == '#' ) { // &#X22;  &#34;
          last = p;
          p+=2;
          while ( p < end && *p++ != ';' );
          DBG printf("last: >%.*s<\n", (int)(p-last), last );
/*
          uint32_t i = 0;
          if ( p[0] == 'X' ) { // HEX
            p++;
            while ( p < end && *p != ';' ) { 
              // 65-48 97-48 --  17-22 49-54 - 7 42-47
              int byte = (*p++ - '0'); 
              printf("DELME byte %d\n",byte);
              if ( byte > 10 ) byte -= 7;
              if ( byte > 15 ) byte -= 32;
              i = (i << 4) + byte;
            }
            //if ( p == end ) { } // TODO error no ;
            printf("DELME i %d\n",i);
            if ( i == 34 ) {
              memcpy( o, "&quot;", 6); o += 6;
            } else {
              while ( i ) { *o++ = i & 0xFF; i >>= 8; }
            }
            p++;
          } else { // DEC
            while ( p < end && *p != ';' ) { i = (i * 10) + (*p++ - '0'); }
            *o++ = i;
            while ( i ) { *o++ = i & 0xFF; i >>= 8; }
            //if ( p == end ) { } // TODO error no ;
          }
*/
        } else {
          memcpy( o, "&amp;", 5); o += 5;
          p++;
          last = p;
        }
/* I have this somewhere
void GetUnicodeChar(unsigned int code, char chars[5]) {
    if (code <= 0x7F) {
        chars[0] = (code & 0x7F); chars[1] = '\0';
    } else if (code <= 0x7FF) {
        // one continuation byte
        chars[1] = 0x80 | (code & 0x3F); code = (code >> 6);
        chars[0] = 0xC0 | (code & 0x1F); chars[2] = '\0';
    } else if (code <= 0xFFFF) {
        // two continuation bytes
        chars[2] = 0x80 | (code & 0x3F); code = (code >> 6);
        chars[1] = 0x80 | (code & 0x3F); code = (code >> 6);
        chars[0] = 0xE0 | (code & 0xF); chars[3] = '\0';
    } else if (code <= 0x10FFFF) {
        // three continuation bytes
        chars[3] = 0x80 | (code & 0x3F); code = (code >> 6);
        chars[2] = 0x80 | (code & 0x3F); code = (code >> 6);
        chars[1] = 0x80 | (code & 0x3F); code = (code >> 6);
        chars[0] = 0xF0 | (code & 0x7); chars[4] = '\0';
    } else {
        // unicode replacement character
        chars[2] = 0xEF; chars[1] = 0xBF; chars[0] = 0xBD;
        chars[3] = '\0';
    }
}
*/

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
  }

  memcpy( o, last, end-last ); o += end-last;
  return (int)(o-escbuf);
}


void line( char *st, int l, Encoder *e ) {

  char *bold = NULL;
  char *emph = NULL;
  char *strike = NULL;
  char *code = NULL;
  bool escapechar = false;

  DBG printf("Befor escape l is %d >%.*s<\n",l,l, st);

  if ( strip_html_tags ) {
    l = striphtml( st, l , e );
    st = stripbuf;
  }
  int rc = escape( st, l , e );
  if ( rc != -1 ) {
    l = rc;
    st = escbuf;
  }
  DBG printf("After escape l is %d >          %.*s<  \n",l,l, st);
  DBG printf("After escape l is %d\n",l);

  int found;

  char *p = st;
  char *end = st+l;
  char *last = st;

  // TODO Do this properly? right now we don't allow nested tags
  // Keep a stack of objects
  // At end of line we walk the stack writing to the output
  // Unclosed tags are left in  **  `  `  **  **

  while ( p < end ) {
    DBG printf("line loop %p\n",p);
    static char ranges1[] = "**" "::" "\x00\x00" "``" "~~" "]]" "\\\\";
    p = findchar_fast(p, end, ranges1, sizeof(ranges1) - 1, &found);
    if ( found ) {
      DBG printf( " found >%.*s< p %p\n", 2, p,p );

      if ( p[0] == ':' && !code ) {
        // http://
        if ( p[1] == '/' && p[2] == '/' && (p-st) > 4 ) { 
          char *linkstart = NULL;
          if ( *((uint32_t*)(p-4)) == CHAR4_INT('h','t','t','p') ) {
            linkstart = p - 4; 
          } else if ( *((uint32_t*)(p-4)) == CHAR4_INT('t','t','p','s') ) {
            linkstart = p - 5; 
          }
          if ( linkstart ) {
            p += 3;
            while ( p < end && !(*p == '.' || *p == ' ' || *p == '\n') ) p++;
  
            if ( p[0] == '.' ) {
              while ( p < end && *p != ' ' && *p != '\n' ) p++;
              
              memcpy( e->o, last, linkstart-last ); e->o += linkstart-last;
              memcpy( e->o, "<a href=\"", 9); e->o += 9;
                memcpy( e->o, linkstart, p-linkstart); e->o += p-linkstart;
              *(e->o++) = '"';
              *(e->o++) = '>';
                memcpy( e->o, linkstart, p-linkstart); e->o += p-linkstart;
              memcpy( e->o, "</a>", 4); e->o += 4;
              last = p;
              
            } else {
              p++;
            }

          } else {
            p++;
          }
        } else {
          p++;
        }
      } 
      else if ( p[0] == '*' && !code) {
        if ( p[1] == '*' ) {
          if ( bold ) {
            DBG printf("Writing bold out bold %p last %p\n",bold,last);
            DBG printf(" bold-last %d\n", (int)(bold-last));
            if ( bold-last > 2 ) {
              memcpy( e->o, last, bold-last-2 ); e->o += bold-last-2;
            }
            memcpy( e->o, "<strong>", 8); e->o += 8;
            if ( !escapechar ) {
              memcpy( e->o, bold, p-bold ); e->o += p-bold;
            } else {
              escapechar = false;
              char *tmp = bold; 
              while (tmp < p) {
                if ( *tmp != '\\' ) *(e->o++) = *tmp;
                tmp += 1;
              }
            }
            memcpy( e->o, "</strong>", 9); e->o += 9;
            bold = NULL;
            p += 2;
            last = p;
          } else {
            DBG printf("bold start\n");
            //if ( strike ) strike = NULL;
            //if ( emph ) emph = NULL;

            p += 2;
            if ( !strike && !emph ) bold = p;
            //printf( " before >%.*s<\n", (int)(bold-last), last );
          }
        } else {
          if ( emph ) {
            DBG printf("Writing emph out emph %p last %p\n",emph,last);
            if ( emph-last > 2 ) {
              memcpy( e->o, last, emph-last-2 ); e->o += emph-last-2;
            }

            memcpy( e->o, "<em>", 4); e->o += 4;
            if ( !escapechar ) {
              memcpy( e->o, emph, p-emph ); e->o += p-emph;
            } else {
              escapechar = false;
              char *tmp = emph; 
              while (tmp < p) {
                if ( *tmp != '\\' ) *(e->o++) = *tmp;
                tmp += 1;
              }
            }
            memcpy( e->o, "</em>", 5); e->o += 5;
            emph = NULL;
            p += 1;
            last = p;
            DBG printf("Wrote emph out\n");
          } else {
            DBG printf("emph start\n");
            //if ( bold ) bold = NULL;
            //if ( strike ) strike = NULL;
            p += 1;
            if ( !strike && !bold ) emph = p;
            //printf( " before >%.*s<\n", (int)(bold-last), last );
          }
        }
      }

      else if ( p[0] == '~' && p[1] == '~' && !code && !bold && !emph) {
        if ( strike ) {
          memcpy( e->o, last, strike-last-2 ); e->o += strike-last-2;
          memcpy( e->o, "<s>", 3); e->o += 3;
            if ( !escapechar ) {
              memcpy( e->o, strike, p-strike ); e->o += p-strike;
            } else {
              escapechar = false;
              char *tmp = strike; 
              while (tmp < p) {
                if ( *tmp != '\\' ) *(e->o++) = *tmp;
                tmp += 1;
              }
            }
          memcpy( e->o, "</s>", 4); e->o += 4;
          strike = NULL;
          p += 2;
          last = p;
        } else {
          p += 2;
          strike = p;
        }
      }
      else if ( p[0] == '\\' && (p[1] == '*' || p[1] == '_' || p[1] == '`') ) { // && !code ) {
        if ( emph || bold || strike ) {
          escapechar = true;
          p += 2; 
        } else {
          memcpy( e->o, last, p-last ); e->o += p-last;
          *(e->o++) = p[1];
          p += 2; 
          last = p;
        }
      }
      else if ( p[0] == 0x60 ) { //'`' ) {
        if ( code ) {
          memcpy( e->o, last, code-last-1 ); e->o += code-last-1;
          memcpy( e->o, "<code>", 6); e->o += 6;
          memcpy( e->o, code, p-code ); e->o += p-code;
          memcpy( e->o, "</code>", 7); e->o += 7;
          code = NULL;
          p += 1;
          last = p;
        } else {
          if ( emph ) emph = NULL;
          if ( bold ) bold = NULL;
          if ( strike ) strike = NULL;
          p += 1;
          code = p;
        } 
      }
//  [test](http://alink.com)
//  <a href="http://alink.com">test</a>

      else if ( p[0] == ']' && !code ) {
        if ( CHAR8_LONG(']', '(', 'h','t','t','p','s',':') == *((unsigned long *)(p)) ||
             CHAR8_LONG(']', '(', 'h','t','t','p',':','/') == *((unsigned long *)(p)) ) {
          char *label = p;
          while ( --label > last &&  *label != '[' );
          if ( label != last ) { 
            label += 1;
            //printf(" label >%.*s<\n", p-label, label );
            char *link_end = p;
            while ( ++link_end < end && *link_end != ')' );
            if ( link_end != end ) { 
              link_end -= 1;
              //printf(" link >%.*s<\n", link_end-p, p );

              memcpy( e->o, last, label-last-1 ); e->o += label-last-1; 
              memcpy( e->o, "<a href=\"", 9 ); e->o += 9;
              memcpy( e->o, p+2, link_end-p-1); e->o += link_end-p-1;
              *(e->o++) = '"';
              *(e->o++) = '>';
              memcpy( e->o, label, p-label ); e->o += p-label;
              *((unsigned int*)e->o) = CHAR4_INT('<','/','a','>'); e->o += 4;
              p = link_end+2;
              last = p;
  
            }
          
          }
        }
        p++;
  
      }

      else {
        p++;
      }
      //memcpy( e->o, st, p-st ); e->o += p-st;
    } 
  } 
  DBG printf( "end-last %d\n", (int)(end-last) );
  if ( end-last > 0 ) {
    memcpy( e->o, last, end-last ); e->o += end-last;
  }
}

PyObject *_render( PyObject *md, Encoder *e ) {

  Py_ssize_t l;
  char *p = PyUnicode_AsUTF8AndSize(md, &l);
  char *end = p+l;
  int found;

  resizeBufferIfNeeded( e, l );

  int bq = 0;
  int para = 0;
  int sameline = 0;
  int list = 0;

  while ( p < end ) {
    //printf("txt >%.*s<\n", 4, p);

    if ( p[0] == '\n' ) {
      if ( para ) {
        *((uint64_t *)e->o) = CHAR8_LONG('<','/','p','>',0x0a,0,0,0); e->o += 5;
      }
      if ( bq && !sameline) {
        if (para) e->o -= 1;
        memcpy( e->o, "\n</blockquote>", 14 ); e->o += 14;
        *(e->o) = '\n'; e->o += 1;
        bq = 0;
      }
      para = 0;
    }
    else if ( p[0] == '#' ) {

      int num = 1;
      char *zz = p+1;
      while( *(zz++) == '#' ) num += 1;

      if ( para ) { 
        *((uint64_t *)e->o) = CHAR8_LONG('<','/','p','>',0x0a,0,0,0); e->o += 5;
        //memcpy( e->o, "</p>\n", 5 ); e->o += 5;
        para = 0;
      }

      //*((unsigned int*)e->o) = s_h1; e->o += 4;

      switch (num) {
        case 1: memcpy( e->o, "<h1>", 4 ); e->o += 4; break;
        case 2: memcpy( e->o, "<h2>", 4 ); e->o += 4; break;
        case 3: memcpy( e->o, "<h3>", 4 ); e->o += 4; break;
        case 4: memcpy( e->o, "<h4>", 4 ); e->o += 4; break;
        case 5: memcpy( e->o, "<h5>", 4 ); e->o += 4; break;
      }

      p += 1+num;
      char *st = p;
      static char ranges1[] = "\x0a\x0a";
      p = findchar_fast(p, end, ranges1, sizeof(ranges1) - 1, &found);
      if ( found ) {
        memcpy( e->o, st, p-st ); e->o += p-st;
      } else {
        while ( p < end && *p++ != '\n' );
        memcpy( e->o, st, p-st ); e->o += p-st;
      }

      //*((uint64_t *)e->o) = s_end_h1;
      //e->o += 6;
      //memcpy( e->o, "</h1>\n", 6 ); e->o += 6;
      switch (num) {
        case 1: memcpy( e->o, "</h1>\n", 6 ); e->o += 6; break;
        case 2: memcpy( e->o, "</h2>\n", 6 ); e->o += 6; break;
        case 3: memcpy( e->o, "</h3>\n", 6 ); e->o += 6; break;
        case 4: memcpy( e->o, "</h4>\n", 6 ); e->o += 6; break;
        case 5: memcpy( e->o, "</h5>\n", 6 ); e->o += 6; break;
      }
                 
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
        memcpy( e->o, "<blockquote>\n", 13 ); e->o += 13;
      } 
      continue;
    }
    else if ( (p[0] == '-' || p[0] == '*') && p[1] == ' ' ) {  
      if ( para ) { 
        *((uint64_t *)e->o) = s_end_para;
        e->o += 5;
        para = 0;
      }
      p += 1;
      if ( list == 0 ) {
        list = 2;
        *((unsigned long*)e->o) = CHAR8_LONG('<','u','l','>',0x0a,'<','l','i'); e->o += 8;
        //*(e->o++) = '<'; *(e->o++) = 'u'; *(e->o++) = 'l'; *(e->o++) = '>'; *(e->o++) = 0x0A; *(e->o++) = '<'; *(e->o++) = 'l'; *(e->o++) = 'i'; 
        *(e->o++) = '>';
      } else {
        if ( bq ) {
          bq = 0;
          memcpy( e->o, "\n</blockquote>", 14 ); e->o += 14;
          *(e->o) = '\n'; e->o += 1;
        }
        *((unsigned long*)e->o) = CHAR8_LONG('<','/','l','i','>', 0x0a,'<','l'); e->o += 8;
        //*(e->o++) = '<'; *(e->o++) = '/'; *(e->o++) = 'l'; *(e->o++) = 'i'; *(e->o++) = '>'; *(e->o++) = 0x0A; *(e->o++) = '<'; *(e->o++) = 'l'; 
        *(e->o++) = 'i'; 
        *(e->o++) = '>';
      }
    }
    else if ( (p[1] == '.' || p[1] == ')') && _isdigit(p[0]) && p[2] == ' ' ) {  // '1. '

      if ( para ) { 
        *((uint64_t *)e->o) = s_end_para;
        e->o += 5;
        para = 0;
      }

      p += 2;
      if ( list == 0 ) {
        list = 1;
        *((unsigned long*)e->o) = CHAR8_LONG('<','o','l','>',0x0a,'<','l','i'); e->o += 8;
        *(e->o++) = '>';
        //*(e->o++) = '<'; *(e->o++) = 'o'; *(e->o++) = 'l'; *(e->o++) = '>'; *(e->o++) = 0x0A; *(e->o++) = '<'; *(e->o++) = 'l'; *(e->o++) = 'i'; *(e->o++) = '>';
      } else {
        if ( bq ) {
          bq = 0;
          memcpy( e->o, "\n</blockquote>", 14 ); e->o += 14;
          *(e->o) = '\n'; e->o += 1;
        }
        *((unsigned long*)e->o) = CHAR8_LONG('<','/','l','i','>', 0x0a,'<','l'); e->o += 8;
        *(e->o++) = 'i'; *(e->o++) = '>';
        //*(e->o++) = '<'; *(e->o++) = '/'; *(e->o++) = 'l'; *(e->o++) = 'i'; *(e->o++) = '>'; *(e->o++) = 0x0A;
        //*(e->o++) = '<'; *(e->o++) = 'l'; *(e->o++) = 'i'; *(e->o++) = '>';
      }
         
    }
    else if ( p[0] == '`' && p[1] == '`' && p[2] == '`' ) { // && p[3] == '\x0a'  ) {
      //if ( precode == NULL ) {

      if ( para ) { 
        *((uint64_t *)e->o) = s_end_para;
        e->o += 5;
        para = 0;
      }

      // Scan to closing ``` and if not found we ignore

      p += 4; 
      char *st = p;
      static char ranges1[] = "``";
      p = findchar_fast(p, end, ranges1, sizeof(ranges1) - 1, &found);
      if ( found ) {
        if ( p[-1] == '\n' && p[1] == '`' && p[2] == '`' ) {
          memcpy( e->o, "<pre><code>", 11 ); e->o += 11;
          memcpy( e->o, st, p-st ); e->o += p-st;
          memcpy( e->o, "</code></pre>\n", 14 ); e->o += 14;  
          p += 2;
        }  
      } else {
        p = st-1;
      }

    }
    else if ( p[0] == '~' && p[1] == '~' && p[2] == '~'  ) {
      //if ( precode == NULL ) {

      if ( para ) { 
        *((uint64_t *)e->o) = s_end_para;
        e->o += 5;
        para = 0;
      }

      // Scan to closing ``` and if not found we ignore

      p += 4; 
      char *st = p;
      static char ranges1[] = "~~";
      p = findchar_fast(p, end, ranges1, sizeof(ranges1) - 1, &found);
      if ( found ) {
        if ( p[-1] == '\n' && p[1] == '~' && p[2] == '~' ) {
          memcpy( e->o, "<pre><code>", 11 ); e->o += 11;
          memcpy( e->o, st, p-st ); e->o += p-st;
          memcpy( e->o, "</code></pre>\n", 14 ); e->o += 14;  
          p += 2;
        }  
      }
    }
// TODO Only do one hr method
    else if ( p[0] == '*' && p[1] == '*' && p[2] == '*'  ) {
      memcpy( e->o, "<hr>\x0a", 5); e->o += 5;
      static char ranges1[] = "\x0a\x0a";
      p = findchar_fast(p, end, ranges1, sizeof(ranges1) - 1, &found);
    }
    else if ( p[0] == '-' && p[1] == '-' && p[2] == '-'  ) {
      memcpy( e->o, "<hr>\x0a", 5); e->o += 5;
      static char ranges1[] = "\x0a\x0a";
      p = findchar_fast(p, end, ranges1, sizeof(ranges1) - 1, &found);
    }
    else if ( p[0] == '_' && p[1] == '_' && p[2] == '_'  ) {
      memcpy( e->o, "<hr>\x0a", 5); e->o += 5;
      static char ranges1[] = "\x0a\x0a";
      p = findchar_fast(p, end, ranges1, sizeof(ranges1) - 1, &found);
    }
    else if ( p[0] == ' ' && list ) {
    }
    else {

      //if ( !bq ) {
      if ( para == 0 ) {
        *((unsigned int*)e->o) = CHAR4_INT('<','p','>','z');
        e->o += 3;
        para = 1;
      } else {
        *(e->o++) = '\n'; 
      }

      //}
      char *st = p;
      static char ranges1[] = "\x0a\x0a";
      p = findchar_fast(p, end, ranges1, sizeof(ranges1) - 1, &found);
      if ( found ) {
        line( st, p-st, e );
      } 
      else if ( p == end ) {
        line( st, p-st, e );
      }

    }

    p += 1;
    sameline = 0;
  }

  if ( para ) {
    *((uint64_t *)e->o) = s_end_para;
    e->o += 5;
  }
  if ( bq ) {
    if ( para ) e->o -= 1;
    memcpy( e->o, "\n</blockquote>", 14 ); e->o += 14;
    //*(e->o) = '\n'; e->o += 1;
  }
  if ( list ) {
    // TODO uint64 * this
    *((unsigned long*)e->o) = CHAR8_LONG('<','/','l','i','>', 0x0a,0,0); e->o += 6;
    //*(e->o++) = '<'; *(e->o++) = '/'; *(e->o++) = 'l'; *(e->o++) = 'i'; *(e->o++) = '>'; *(e->o++) = 0x0A;
    if ( list == 2 ) {
      //*(e->o++) = '<'; *(e->o++) = '/'; *(e->o++) = 'u'; *(e->o++) = 'l'; *(e->o++) = '>'; *(e->o++) = 0x0A;
      *((unsigned long*)e->o) = CHAR8_LONG('<','/','u','l','>', 0x0a,0,0); e->o += 6;
    } else {
      //*(e->o++) = '<'; *(e->o++) = '/'; *(e->o++) = 'o'; *(e->o++) = 'l'; *(e->o++) = '>'; *(e->o++) = 0x0A;
      *((unsigned long*)e->o) = CHAR8_LONG('<','/','o','l','>', 0x0a,0,0); e->o += 6;
    }
  }

  return PyUnicode_FromStringAndSize( e->start, e->o-e->start );  
}

PyObject* render(PyObject* self, PyObject *md) {

  Encoder enc = { NULL,NULL,NULL };

  int len = 65536;
  char *s = (char *) malloc (len);
  if (!s) {
    SetError("Could not reserve memory block");
    return 0;
  }

  enc.start = s;
  enc.end   = s + len;
  enc.o = s;

  PyObject *ret = _render(md, &enc);
  //unsigned long long start = rdtsc();
  //printf("Cycles start %lld\n", start);
  //unsigned long long end = rdtsc();
  //printf("Cycles end %lld\n", end);
  //printf("Cycles spent is %lld\n", (uint64_t)end-start);
  free(s);
  return ret;
 
}

