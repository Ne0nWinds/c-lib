#pragma once

/* === Syntax === */

typedef __UINT8_TYPE__ u8;
typedef __UINT16_TYPE__ u16;
typedef __UINT32_TYPE__ u32;
typedef __UINT64_TYPE__ u64;

typedef __INT8_TYPE__ s8;
typedef __INT16_TYPE__ s16;
typedef __INT32_TYPE__ s32;
typedef __INT64_TYPE__ s64;

typedef float f32;
typedef double f64;

typedef _Bool bool;
#define true 1
#define false 0

#define len(arr) (sizeof(arr) / sizeof(*arr))

/* === Memory Allocation === */

typedef struct {
	u8 *start;
	u8 *current;
	u64 mem_committed;
} BumpAllocator;

bool BumpInit(BumpAllocator *allocator, u64 reserve_size);
void *BumpPush(BumpAllocator *allocator, u64 size);
void *BumpPushAligned(BumpAllocator *allocator, u64 size, u32 alignment);
void *BumpPushString(BumpAllocator *allocator, char *str, u32 length);
void BumpPop(BumpAllocator *allocator, void *ptr);
void BumpReset(BumpAllocator *allocator);
void BumpFreeUnused(BumpAllocator *allocator);
void BumpDestroy(BumpAllocator *allocator);

/* === File IO == */

typedef struct {
	void *data;
	u64 size;
	void *file_handle;
	void *file_mapping_handle;
} FileMapHandle;

FileMapHandle MemoryMapOpen(char *file_name);
void MemoryMapClose(FileMapHandle *handle);

void *ReadEntireFile(char *file_path, u32 *file_size);

bool FlushToFile(char *file_name, void *buffer, u32 size);
