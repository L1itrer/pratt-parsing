#include "arena.h"
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

// TODO: ARENA_FREE_LIST

#ifdef ARENA_DEBUG
#include <stdio.h>
#include <stdarg.h>
#else
#endif


typedef struct {
    uint64_t page_size;
    uint64_t allocation_graniularity;
}SysInfo;

static SysInfo __info_static;
static SysInfo* __sys_info = NULL;

void sys_info_get(SysInfo* info);


static void* sys_reserve_memory(uint64_t size);
static void sys_commit_memory(void *ptr, uint64_t size);
static void sys_release_memory(void *ptr, uint64_t size);

Arena* arena_alloc_params(ArenaParams* params)
{
    uint64_t reserve_size = params->reserve_size;
    uint64_t commit_size  = params->commit_size;

    void* base = params->optional_backing_buffer;
    if (__sys_info == NULL)
    {
        sys_info_get(&__info_static);
        __sys_info = &__info_static;
    }

    if (base == NULL)
    {
        if (params->flags & ArenaFlag_LargePages)
        {
            // not yet implemented
            Trap();
        }
        else
        {
            reserve_size = AlignPow2(reserve_size, __sys_info->page_size);
            commit_size  = AlignPow2(commit_size,  __sys_info->page_size);
            base = sys_reserve_memory(reserve_size);
            sys_commit_memory(base, commit_size);
        }
        AsanPoisonMemoryRegion(base, commit_size);
    }
    else
    {
        AsanPoisonMemoryRegion(base, params->reserve_size);
    }
    if (base == NULL) Trap();

    AsanUnpoisonMemoryRegion(base, ARENA_HEADER_SIZE);

    Arena* arena = base;
    arena->curr = arena;
    arena->flags = params->flags;
    arena->commited_size = params->commit_size;
    arena->reserved_size = params->reserve_size;
    arena->base_position = 0;
    arena->position = ARENA_HEADER_SIZE;
    arena->commited = commit_size;
    arena->reserved = reserve_size;
    arena->allocation_site_line = params->allocation_site_line;
    arena->allocation_site_file = params->allocation_site_file;
    arena->name = params->name;

    return arena;
}

void arena_release(Arena* arena)
{
    for (Arena* next = arena->curr, *prev = NULL;next != NULL;next = prev)
    {
        prev = next->prev;
        AsanUnpoisonMemoryRegion(next, next->commited);
        sys_release_memory(next, next->reserved);
    }
}


void* arena_push(Arena *arena, uint64_t  size, uint64_t alignment, bool should_zero)
{
    Arena* curr = arena->curr;
    uint64_t pos_pre = AlignPow2(curr->position, alignment);
    uint64_t pos_pst = pos_pre + size;

    uint64_t size_to_zero = 0;
    if (should_zero)
    {
        size_to_zero = Min(curr->commited, pos_pst) - pos_pre;
    }
    if (curr->reserved < pos_pst && !(arena->flags & ArenaFlag_NoChain))
    {
        Arena* new_block = 0;
        if(new_block == 0)
        {
            uint64_t res_size = curr->reserved_size;
            uint64_t cmt_size = curr->commited_size;
            if (size + ARENA_HEADER_SIZE > res_size)
            {
                res_size = AlignPow2(size + ARENA_HEADER_SIZE, alignment);
                cmt_size = AlignPow2(size + ARENA_HEADER_SIZE, alignment);
            }
            ArenaParams params = {
                .reserve_size = res_size,
                .commit_size = cmt_size,
                .flags = curr->flags,
                .allocation_site_file = curr->allocation_site_file,
                .allocation_site_line = curr->allocation_site_line,
            };
            new_block = arena_alloc_params(&params);
            size_to_zero = 0;
        }
        else
        {
            size_to_zero = size;
        }
        new_block->base_position = curr->base_position + curr->reserved;
        new_block->prev = arena->curr;
        arena->curr = new_block;

        curr = new_block;
        pos_pre = AlignPow2(curr->position, alignment);
        pos_pst = pos_pre + size;
    }

    if (curr->commited < pos_pst)
    {
        uint64_t cmt_pst_aligned = pos_pst + curr->commited_size-1;
        cmt_pst_aligned -= cmt_pst_aligned % curr->commited_size;
        uint64_t cmt_pst_clamped = Max(cmt_pst_aligned, curr->reserved);
        uint64_t cmt_size = cmt_pst_clamped - curr->commited;
        uint8_t *cmt_ptr = (uint8_t *)curr + curr->commited;
        if (curr->flags & ArenaFlag_LargePages)
        {
            Trap();
        }
        else
        {
            sys_commit_memory(cmt_ptr, cmt_size);
        }
        AsanPoisonMemoryRegion(cmt_ptr, cmt_size);
        curr->commited = cmt_pst_clamped;
    }

    void* result = NULL;

    if (curr->commited >= pos_pst)
    {
        result = (uint8_t*)curr+pos_pre;
        curr->position = pos_pst;
        AsanUnpoisonMemoryRegion(result, size);
        uint8_t* result_u8 = result;
        for (uint64_t i = 0;i < size_to_zero;++i)
        {
            result_u8[i] = 0;
        }
    }

    return result;
}

uint64_t arena_pos(Arena *arena)
{
    Arena* current = arena->curr;
    uint64_t pos = current->base_position + current->position;
    return pos;
}

void arena_pop_to(Arena *arena, uint64_t pos)
{
    uint64_t big_pos = Max(ARENA_HEADER_SIZE, pos);
    Arena* curr = arena->curr;

    for (Arena* prev = 0;curr->base_position >= big_pos;curr = prev)
    {
        prev = curr->prev;
        AsanUnpoisonMemoryRegion(curr, curr->commited);
        sys_release_memory(curr, curr->reserved);
    }
    arena->curr = curr;
    uint64_t new_pos = big_pos - curr->base_position;
    AssertAlways(new_pos <= curr->position);
    AsanPoisonMemoryRegion((uint8_t*)curr + new_pos, (curr->position - new_pos));
    curr->position = new_pos;
}

void arena_clear(Arena *arena)
{
    arena_pop_to(arena, 0);
}

void arena_pop(Arena *arena, uint64_t amount)
{
    uint64_t pos_old = arena_pos(arena);
    uint64_t pos_new = pos_old;
    if(amount < pos_old)
    {
      pos_new = pos_old - amount;
    }
    arena_pop_to(arena, pos_new);
    Trap();
}

Temp temp_begin(Arena *arena)
{
    uint64_t pos = arena_pos(arena);
    return (Temp){arena, pos};
}

void temp_end(Temp temp)
{
    arena_pop_to(temp.arena, temp.pos);
}

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void sys_info_get(SysInfo* info)
{
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    info->page_size = (uint64_t)sysinfo.dwPageSize;
    info->allocation_graniularity = (uint64_t)sysinfo.dwAllocationGranularity;
}


static void* sys_reserve_memory(uint64_t size)
{
    return VirtualAlloc(0, size, MEM_RESERVE, PAGE_READWRITE);
}
static void sys_commit_memory(void *ptr, uint64_t size)
{
    VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
}


static void sys_release_memory(void *ptr, uint64_t size)
{
    (void)size;
    VirtualFree(ptr, 0, MEM_RELEASE);
}
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>



#ifndef AT_PAGESZ
#define AT_PAGESZ 6
#endif
void sys_info_get(SysInfo* info)
{
    info->page_size = KB(4);
    int f = open("/proc/self/auxv", O_RDONLY);
    if (f > 0)
    {
        typedef struct
        {
            uint64_t type, value;
        }Entry;
        ssize_t n = 0;
        Entry en;
        while ((n = read(f, &en, sizeof(en))))
        {
            if (en.type == AT_PAGESZ)
            {
                info->page_size = en.value;
                break;
            }
        }
    }
    info->allocation_graniularity = info->page_size;

    close(f);
}


static void* sys_reserve_memory(uint64_t size)
{
    void *result = mmap(0, size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if(result == MAP_FAILED)
    {
      result = 0;
    }
    return result;
}
static void sys_commit_memory(void *ptr, uint64_t size)
{
  mprotect(ptr, size, PROT_READ|PROT_WRITE);
}


static void sys_release_memory(void *ptr, uint64_t size)
{
    munmap(ptr, size);
}
#endif

