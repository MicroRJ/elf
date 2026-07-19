//
// See Copyright Notice In elf.h
//

void elf_platform_sleep(i64 ms)
{
	Sleep((DWORD)ms);
}

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
