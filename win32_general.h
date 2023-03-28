#include "general.h"

#define PAGE_SIZE 65536

#define round_up(n, power) {\
  n += power - 1;\
  n &= ~(power - 1);\
}

bool BumpInit(BumpAllocator *allocator) {
	allocator->start = this->current = (u8 *)VirtualAlloc(NULL, reserve_size, MEM_RESERVE, PAGE_READWRITE);

	allocator->mem_committed = 0;
	return (allocator->start == NULL);
}

void *BumpPush(BumpAllocator *allocator, u64 size) {
	return BumpPushAligned(allocator, size, 1);
}

void *BumpPushAligned(BumpAllocator *allocator, u64 size, u32 alignment) {
	u64 current = *(u64 *)&allocator->current;
	round_up(current, (u64)alignment);
	allocator->current = *(u8 **)&current;

	void *ptr = (void *)current;
	allocator->current += size;

	// if we need more memory than is committed to physical memory
	if (allocator->current - this->start > this->mem_committed) {
		void *address = allocator->start + mem_committed;

		SIZE_T size = allocator->current - (u8 *)address;
		round_up(size, (u64)PAGE_SIZE);

		if (!VirtualAlloc(address, size, MEM_COMMIT, PAGE_READWRITE))
			return 0;

		allocator->mem_committed += size;
	}

	return ptr;
}

void *BumpPushString(BumpAllocator *allocator, char *str, u32 length) {
	char *c = BumpPush(allocator, length + 1);

	for (u32 i = 0; i < length; ++i) {
		c[i] = str[i];
	}

	c[length] = 0;

	return c;
}

void BumpPop(BumpAllocator *allocator, void *ptr) {
	u64 length = allocator->current - (u8 *)ptr;
	
	for (u32 i = 0; i < length; ++i) {
		((u8 *)ptr)[i] = 0;
	}

	allocator->current = ptr;
}

void BumpReset(BumpAllocator *allocator) {
	u64 length = allocator->current - (u8 *)allocator->start;
	
	for (u32 i = 0; i < length; ++i) {
		allocator->start[i] = 0;
	}

	allocator->current = allocator->start;
}

void BumpFreeUnused(BumpAllocator *allocator) {
	// VirtualFree using MEM_RESET
}

void BumpDestroy(BumpAllocator *allocator) {
	VirtualFree(allocator->start, 0, MEM_RELEASE);
}

FileMapHandle MemoryMapOpen(char *file_name) {

	FileMapHandle handle = {0};

	handle.file_handle = CreateFileA(file_name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle.file_handle == INVALID_HANDLE_VALUE) return handle;

	LARGE_INTEGER file_size = {0};
	if (!GetFileSizeEx(file_handle, &file_size)) goto error;
	handle.size = file_size.QuadPart;

	handle.file_mapping_handle = CreateFileMapping(file_handle, NULL, PAGE_READONLY, file_size.HighPart, file_size.LowPart, NULL);
	if (handle.file_mapping_handle == INVALID_HANDLE_VALUE) goto error;

	handle.data = MapViewOfFile(file_mapping_handle, FILE_MAP_READ, 0, 0, 0);
	if (handle.data == NULL) goto error;

	return handle;

error:
	MemoryMapClose(handle);
	return handle;
}

void MemoryMapClose(FileMapHandle *handle) {
	UnmapViewOfFile(handle->data);
	CloseHandle(handle->file_mapping_handle);
	CloseHandle(handle->file_handle);
	*handle = (FileMapHandle){ 0, 0, 0, 0 };
}

void *ReadEntireFile(char *file_name, u32 *file_size) {
	HANDLE file_handle = CreateFile(file_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file_handle == INVALID_HANDLE_VALUE) return 0;
	
	*file_size = GetFileSize(file_handle, 0);
	if (*file_size == 0) return 0;
	
	u8 *buffer = BumpAlloc(*file_size + 1);
	
	DWORD bytes_read = 0;
	ReadFile(file_handle, buffer, *file_size, &bytes_read, NULL);
	CloseHandle(file_handle);
	if (bytes_read != *file_size) {
		*file_size = 0;
		BumpFree(buffer);
		return 0;
	};
	
	return buffer;
}

bool FlushToFile(char *file_name, void *buffer, u32 size) {

	HANDLE handle = CreateFileA(file_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE) return false;

	bool result = WriteFile(handle, buffer, size, NULL, NULL);

	CloseHandle(handle);

	return result;
}
