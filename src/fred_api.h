#ifndef FRED_API
#define FRED_API
#include <stdint.h>

typedef struct {
  char* str;
  uint64_t size;
} String8;


#define str8_lit(cstr) {.str=cstr,.size=(sizeof(cstr))-1}


#define push_array_no_zero_aligned(a, T, c, align) arena_push_array_no_zero_aligned(a, T, c, align)
#define push_array_aligned(a, T, c, align) arena_push_array_aligned(a, T, c, align)
#define push_array(a, T, c) arena_push_array_aligned(a, T, c, Max(8, AlignOf(T)))

typedef struct Arena Arena;

void feed_queue_info(String8 msg);
void feed_queue_error(String8 msg);


String8 str8_fmt(Arena* a, const char* fmt, ...);
uint32_t try_f64_from_str8(String8 str, double* num);

#endif // FRED_API
