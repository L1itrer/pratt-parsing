#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#if PRATT_DEBUG
# define ARENA_DEBUG 1
#else
# define ARENA_DEBUG 0
#endif
#include "arena.h"


#define push_array_no_zero_aligned(a, T, c, align) arena_push_array_no_zero_aligned(a, T, c, align)
#define push_array_aligned(a, T, c, align) arena_push_array_aligned(a, T, c, align)
#define push_array(a, T, c) arena_push_array_aligned(a, T, c, Max(8, AlignOf(T)))

typedef uint64_t u64;
typedef int64_t  i64;
typedef int32_t  i32;

typedef struct {
  char* str;
  uint64_t size;
} String8;


#define str8_lit(cstr) {.str=cstr,.size=(sizeof(cstr))-1}
#define Str8Fmt(str8) (int)str8.size, str8.str
#define Unused(p) (void)p

uint32_t try_f64_from_str8(String8 str, double* num)
{
  uint32_t result = 1;
  char* endptr = NULL;
  double temp = strtod(str.str, &endptr);
  if (endptr != (str.str + str.size))
  {
    result = 0;
  }
  else
  {
    *num = temp;
  }
  return result;
}

String8 str8_fmt(Arena* a, const char* fmt, ...)
{
  String8 result = {0};
  Unused(a);
  Unused(fmt);
  Assert(0);
  return result;
}

void feed_queue_error(String8 msg)
{
  printf("(err) %.*s\n", (int)msg.size, msg.str);
}


void feed_queue_info(String8 msg)
{
  printf("(info) %.*s\n", (int)msg.size, msg.str);
}


#define FRED_EMULATION

#include "quick_calc.c"

String8 input_right = str8_lit("1 + 2 + 3+4");
String8 input_left = str8_lit("4 * 3 + 2 - 1");
String8 input_mixed = str8_lit("1 + 2 + 3 + 4 + 5");
String8 input_paren = str8_lit("1 + 2 + 3 + (4 + 5)");
String8 input_paren2 = str8_lit("(1+2)*(3*4.0)");
String8 input_paren3 = str8_lit("(((1 + 2)))");
String8 input_paren4 = str8_lit("((1 + 2) * (3 + 4))*(4)-1");
String8 input_minus = str8_lit("-1 + 2");
String8 input_zero = str8_lit("1 / 0");

String8 input_start_op = str8_lit("+ + 2 - 4");


int main(void)
{
  String8 input = input_start_op;
  Arena* arena = arena_alloc();
  QCResult result = qc_core(arena, input);
  printf("[%.*s] = %lf\n", Str8Fmt(input), result.value);
  arena_release(arena);
}



#include "arena.c"
