#include "general.h"

// == stripped down Windows.h ==
#define _AMD64_
#include <windef.h>
#include <fileapi.h>

LPVOID VirtualAlloc(
	LPVOID lpAddress,
	SIZE_T dwSize,
	DWORD  flAllocationType,
	DWORD  flProtect
);
BOOL VirtualFree(
	LPVOID lpAddress,
	SIZE_T dwSize,
	DWORD  dwFreeType
);
LPVOID MapViewOfFile(
	HANDLE hFileMappingObject,
	DWORD  dwDesiredAccess,
	DWORD  dwFileOffsetHigh,
	DWORD  dwFileOffsetLow,
	SIZE_T dwNumberOfBytesToMap
);
BOOL UnmapViewOfFile(
	LPCVOID lpBaseAddress
);
#define FILE_MAP_READ 0x4

// other
BOOL CloseHandle(HANDLE hObject);
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
HANDLE CreateFileMappingA(
	HANDLE hFile,
	LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
	DWORD flProtect,
	DWORD dwMaximumSizeHigh,
	DWORD dwMaximumSizeLow,
	LPCSTR lpName
);

#define PAGE_SIZE 65536

#define round_up(n, power) {\
  n += power - 1;\
  n &= ~(power - 1);\
}

bool BumpInit(BumpAllocator *allocator, u64 reserve_size) {
	allocator->start = allocator->current = (u8 *)VirtualAlloc(NULL, reserve_size, MEM_RESERVE, PAGE_READWRITE);

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
	if (allocator->current - allocator->start > allocator->mem_committed) {
		void *address = allocator->start + allocator->mem_committed;

		SIZE_T size = allocator->current - (u8 *)address;
		round_up(size, (u64)PAGE_SIZE);

		if (!VirtualAlloc(address, size, MEM_COMMIT, PAGE_READWRITE))
			return 0;

		allocator->mem_committed += size;
	}

	return ptr;
}

void *BumpPushString(BumpAllocator *allocator, char *str, u32 length) {
	char *c = BumpPush(allocator, length);

	for (u32 i = 0; i < length; ++i) {
		c[i] = str[i];
	}

	return c;
}

// TODO: fix 4GB limitation
void *BumpPushFile(BumpAllocator *allocator, char *file_path, u64 *size) {
	*size = 0;
	HANDLE handle = CreateFileA(file_path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE) return 0;

	LARGE_INTEGER win32_file_size = {0};
	if (!GetFileSizeEx(handle, &win32_file_size)) goto end;
	if (win32_file_size.QuadPart == 0) goto end;

	void *buffer = BumpPush(allocator, win32_file_size.QuadPart);
	if (!ReadFile(handle, buffer, win32_file_size.QuadPart, NULL, NULL)) {
		BumpPop(allocator, buffer);
		buffer = 0;
		goto end;
	}

	*size = win32_file_size.QuadPart;

end:
	CloseHandle(handle);
	return buffer;
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
	if (!GetFileSizeEx(handle.file_handle, &file_size)) goto error;
	handle.size = file_size.QuadPart;

	handle.file_mapping_handle = CreateFileMappingA(handle.file_handle, NULL, PAGE_READONLY, file_size.HighPart, file_size.LowPart, NULL);
	if (handle.file_mapping_handle == INVALID_HANDLE_VALUE) goto error;

	handle.data = MapViewOfFile(handle.file_mapping_handle, FILE_MAP_READ, 0, 0, 0);
	if (handle.data == NULL) goto error;

	return handle;

error:
	MemoryMapClose(&handle);
	return handle;
}

void MemoryMapClose(FileMapHandle *handle) {
	UnmapViewOfFile(handle->data);
	CloseHandle(handle->file_mapping_handle);
	CloseHandle(handle->file_handle);
	*handle = (FileMapHandle){ 0, 0, 0, 0 };
}

bool FlushToFile(char *file_name, void *buffer, u32 size) {

	HANDLE handle = CreateFileA(file_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE) return false;

	bool result = WriteFile(handle, buffer, size, NULL, NULL);

	CloseHandle(handle);

	return result;
}
