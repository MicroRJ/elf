//
// See Copyright Notice In elf.h
//
#include "elf_os_services.h"

#include <string.h>

static elf_b32 elf_path_is_separator(char character)
{
	return character == '/' || character == '\\';
}

elf_b32 elf_path_builder_init(elf_PathBuilder *path, char *storage, elf_u64 capacity, const char *root, elf_u64 root_size)
{
	if (path) *path = (elf_PathBuilder) {0};
	if (!path || !storage || !capacity || (!root && root_size) || root_size >= capacity) return 0;
	for (elf_u64 index = 0; index < root_size; ++index) {
		storage[index] = root[index] == '\\' ? '/' : root[index];
	}
	storage[root_size] = 0;
	*path = (elf_PathBuilder) {
		.data     = storage,
		.size     = root_size,
		.capacity = capacity,
	};
	return 1;
}

elf_PathMark elf_path_mark(const elf_PathBuilder *path)
{
	return path ? path->size : 0;
}

elf_b32 elf_path_push(elf_PathBuilder *path, const char *component, elf_u64 component_size)
{
	if (!path || !path->data || !path->capacity || (!component && component_size)) return 0;
	elf_b32 separator = path->size && component_size && !elf_path_is_separator(path->data[path->size - 1]) && !elf_path_is_separator(component[0]);
	if (component_size > path->capacity - path->size - 1) return 0;
	if (separator && component_size == path->capacity - path->size - 1) return 0;
	if (separator) path->data[path->size++] = '/';
	for (elf_u64 index = 0; index < component_size; ++ index) {
		path->data[path->size++] = component[index] == '\\' ? '/' : component[index];
	}
	path->data[path->size] = 0;
	return 1;
}

void elf_path_pop(elf_PathBuilder *path, elf_PathMark mark)
{
	if (!path || !path->data || mark > path->size) return;
	path->size = mark;
	path->data[path->size] = 0;
}

static elf_b32 elf_os_remove_tree_internal(elf_PathBuilder *path)
{
	elf_OS_FileInfo root;
	if (!elf_os_get_file_info(path->data, &root)) return elf_os_remove_directory(path->data);
	if (!root.is_directory) return 0;
	if (root.is_symbolic_link) return elf_os_remove_directory(path->data);

	elf_OS_Directory directory;
	elf_OS_DirectoryEntry entry;
	elf_OS_DirectoryStatus status = elf_os_find_first_file(path, &directory, &entry);
	if (status == ELF_OS_DIRECTORY_ERROR) return 0;
	elf_b32 result = 1;
	while (status == ELF_OS_DIRECTORY_ENTRY)
	{
		elf_PathMark mark = elf_path_mark(path);
		if (!elf_path_push(path, entry.name, entry.name_size))
		{
			result = 0;
			break;
		}
		if (entry.info.is_directory)
		{
			result = entry.info.is_symbolic_link ? elf_os_remove_directory(path->data) : elf_os_remove_tree_internal(path);
		}
		else result = elf_os_remove_file(path->data);
		elf_path_pop(path, mark);
		if (!result) break;
		status = elf_os_find_next_file(&directory, &entry);
	}
	if (status == ELF_OS_DIRECTORY_ERROR) result = 0;
	elf_os_close_directory(&directory);
	return result && elf_os_remove_directory(path->data);
}

elf_b32 elf_os_remove_tree(const char *path)
{
	if (!path) return 0;
	char storage[ELF_OS_PATH_CAPACITY];
	elf_PathBuilder builder;
	elf_u64 size = strlen(path);
	if (!elf_path_builder_init(&builder, storage, sizeof(storage), path, size)) return 0;
	return elf_os_remove_tree_internal(&builder);
}
