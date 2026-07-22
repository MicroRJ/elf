//
// Optional Win32 sleep support.
//

void elf_platform_sleep(i64 ms)
{
	Sleep((DWORD)ms);
}

i64 battery_counter(void)
{
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);
	return counter.QuadPart;
}

i64 battery_counter_frequency(void)
{
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	return frequency.QuadPart;
}
