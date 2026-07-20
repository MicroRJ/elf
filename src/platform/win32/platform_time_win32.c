//
// See Copyright Notice In elf.h
//

i64 elf_platform_counter_frequency(void)
{
	LARGE_INTEGER value;
	QueryPerformanceFrequency(&value);
	return value.QuadPart;
}

i64 elf_platform_counter(void)
{
	LARGE_INTEGER value;
	QueryPerformanceCounter(&value);
	return value.QuadPart;
}
