//
// See Copyright Notice In elf.h
//

#ifndef ELF_BATTERY_FILE_H
#define ELF_BATTERY_FILE_H

#include "battery_platform_types.h"

typedef enum
{
	ELF_PLATFORM_CREATE_ALWAYS,
	ELF_PLATFORM_CREATE_NEW,
	ELF_PLATFORM_TRUNCATE_EXISTING,
	ELF_PLATFORM_OPEN_ALWAYS,
	ELF_PLATFORM_OPEN_EXISTING,
}
elf_PlatformOpenMode;

typedef enum
{
	ELF_PLATFORM_SEEK_CURRENT = 0,
	ELF_PLATFORM_SEEK_BEGIN,
	ELF_PLATFORM_SEEK_END,
}
elf_PlatformSeekOrigin;

typedef enum
{
	ELF_PLATFORM_OPEN_READ    = 1,
	ELF_PLATFORM_OPEN_WRITE   = 2,
	ELF_PLATFORM_OPEN_EXECUTE = 4,
	ELF_PLATFORM_SHARE_READ   = 8,
	ELF_PLATFORM_SHARE_WRITE  = 16,
	ELF_PLATFORM_NO_BUFFERING = 32,
}
elf_PlatformOpenFlags;

typedef enum
{
	ELF_PLATFORM_FILE      = 0,
	ELF_PLATFORM_FOLDER    = 1,
	ELF_PLATFORM_SYMLINK   = 2,
}
elf_PlatformFileType;

typedef struct
{
	elf_PlatformFileType type;
	i64 size_bytes;
	i64 created_unix_ms;
	i64 accessed_unix_ms;
	i64 modified_unix_ms;
}
elf_PlatformFileInfo;

elf_PlatformFile elf_platform_open_file(const char *name, int access, int options);
void elf_platform_close_file(elf_PlatformFile file);
i64 elf_platform_file_size(elf_PlatformFile file);
i64 elf_platform_read_file(elf_PlatformFile file, void *buf, i64 size);
i64 elf_platform_write_file(elf_PlatformFile file, void *buf, i64 size);
i64 elf_platform_seek_file(elf_PlatformFile file, int origin, i64 distance);
void elf_platform_flush_file(elf_PlatformFile file);

b32 elf_platform_delete_file(const char *name);
int elf_platform_make_dir(const char *path);
b32 elf_platform_get_file_info(const char *path, elf_PlatformFileInfo *info);

/* Platform implementations of the elf.fs path traversal bindings. */
ELF_FUNCTION(elf_platform_fs_get_paths);
ELF_FUNCTION(elf_platform_fs_for_each_path);

#endif
