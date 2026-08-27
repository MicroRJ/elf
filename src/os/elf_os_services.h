//
// See Copyright Notice In elf.h
//
#ifndef ELF_OS_SERVICES_H
#define ELF_OS_SERVICES_H

#include "elf_os.h"

typedef struct
{
	uintptr_t value;
}
elf_OS_File;

typedef struct
{
	elf_u64 size;
	elf_i64 created_unix_ms;
	elf_i64 accessed_unix_ms;
	elf_i64 modified_unix_ms;
	elf_b32 is_directory;
	elf_b32 is_symbolic_link;
}
elf_OS_FileInfo;

elf_u64 elf_os_write_console(elf_OS_StandardStream stream, const void *data, elf_u64 size);
void elf_os_sleep(elf_u64 milliseconds);

elf_b32 elf_os_get_file_info(const char *path, elf_OS_FileInfo *info);
elf_OS_File elf_os_open_file_read(const char *path);
elf_OS_File elf_os_create_file(const char *path);
elf_b32 elf_os_file_is_valid(elf_OS_File file);
void elf_os_close_file(elf_OS_File file);
elf_b32 elf_os_read_file(elf_OS_File file, void *data, elf_u64 size, elf_u64 *bytes_read);
elf_b32 elf_os_write_file(elf_OS_File file, const void *data, elf_u64 size, elf_u64 *bytes_written);

#endif
