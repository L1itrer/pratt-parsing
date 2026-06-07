#ifndef ARENA_H
#define ARENA_H
#include <stdint.h>
#include <stdbool.h>

// i don't really care about c++ but maybe i will in the future
// so leaving C_LINKAGE here
#define C_LINKAGE

#if ASAN_ENABLED
C_LINKAGE void __asan_poison_memory_region(void const volatile *addr, size_t size);
C_LINKAGE void __asan_unpoison_memory_region(void const volatile *addr, size_t size);
# define AsanPoisonMemoryRegion(addr, size)   __asan_poison_memory_region((addr), (size))
# define AsanUnpoisonMemoryRegion(addr, size) __asan_unpoison_memory_region((addr), (size))
#else
# define AsanPoisonMemoryRegion(addr, size)   ((void)(addr), (void)(size))
# define AsanUnpoisonMemoryRegion(addr, size) ((void)(addr), (void)(size))
#endif

typedef enum {
  ArenaFlag_NoChain = (1 << 0),
  ArenaFlag_LargePages = (1 << 1),
}ArenaFlagsEnum;
typedef uint64_t ArenaFlags;

#define ARENA_HEADER_SIZE 128

typedef struct Arena Arena;
struct Arena {
  Arena* prev;
  Arena* curr;
  ArenaFlags flags;
  uint64_t commited_size;
  uint64_t reserved_size;
  uint64_t base_position;
  uint64_t position;
  uint64_t commited;
  uint64_t reserved;
  char* allocation_site_file;
  int allocation_site_line;
  char* name;
};


typedef struct ArenaParams
{
  ArenaFlags flags;
  uint64_t reserve_size;
  uint64_t commit_size;
  void *optional_backing_buffer;
  char *allocation_site_file;
  int allocation_site_line;
  char *name;
} ArenaParams;
#define ArenaGlue_(A,B) A##B
#define ArenaGlue(A,B) ArenaGlue_(A,B)
#define ArenaStaticAssert(C, ID) static uint8_t ArenaGlue(ID, __LINE__)[(C)?1:-1]

ArenaStaticAssert(sizeof(Arena) <= ARENA_HEADER_SIZE, arena_size_check);

#define KB(num) (num*1024LL)
#define MB(num) (KB(num)*1024LL)
#define GB(num) (MB(num)*1024LL)

#define arena_default_reserve_size MB(64)
#define arena_default_commit_size  KB(64)
#define arena_default_flags = 0;

Arena* arena_alloc_params(ArenaParams* params);
void arena_release(Arena* arena);

#define arena_alloc(...) arena_alloc_params(&(ArenaParams){.reserve_size = arena_default_reserve_size, .commit_size = arena_default_commit_size, .allocation_site_file = __FILE__, .allocation_site_line = __LINE__, __VA_ARGS__})

void*    arena_push(Arena *arena, uint64_t  size, uint64_t alignment, bool should_zero);
uint64_t arena_pos(Arena *arena);
void     arena_pop_to(Arena *arena, uint64_t pos);


void arena_clear(Arena *arena);
void arena_pop(Arena *arena, uint64_t amount);

typedef struct Temp {
  Arena* arena;
  uint64_t pos;
} Temp;


Temp temp_begin(Arena *arena);
void temp_end(Temp temp);

#define Min(a, b) (((a) < (b)) ? (a) : (b))
#define Max(a, b) (((a) > (b)) ? (a) : (b))

#ifdef _MSC_VER
#define COMPILER_MSVC 1
#elif __clang__
#define COMPILER_CLANG 1
#elif __GNUC__
#define COMPILER_GCC 1
#else
#error This file requires extensions from msvc, clang or gcc
#endif

#define AlignPow2(x,b) (((x) + (b) - 1)&(~((b) - 1)))

#if COMPILER_MSVC
# define AlignOf(T) __alignof(T)
#elif COMPILER_CLANG
# define AlignOf(T) __alignof(T)
#elif COMPILER_GCC
# define AlignOf(T) __alignof__(T)
#else
# error AlignOf not defined for this compiler.
#endif

#if COMPILER_MSVC
# define Trap() __debugbreak()
#else
# define Trap() __builtin_trap()
#endif

#define AssertAlways(x) do{if(!(x)) {Trap();}}while(0)
#if ARENA_DEBUG
# define Assert(x) AssertAlways(x)
#else
# define Assert(x) (void)(x)
#endif



#define arena_push_array_no_zero_aligned(a, T, c, align) (T *)arena_push((a), sizeof(T)*(c), (align), (0))
#define arena_push_array_aligned(a, T, c, align) (T *)arena_push((a), sizeof(T)*(c), (align), (1))
#define arena_push_array_no_zero(a, T, c) arena_push_array_no_zero_aligned(a, T, c, Max(8, AlignOf(T)))
#define arena_push_array(a, T, c) arena_push_array_aligned(a, T, c, Max(8, AlignOf(T)))
#define arena_push_struct(a, T) arena_push_array(a, T, 1)

#endif // ARENA_H
