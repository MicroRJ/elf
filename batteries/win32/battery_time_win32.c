//
// Optional Win32 sleep support.
//

void elf_platform_sleep(i64 ms)
{
	Sleep((DWORD)ms);
}
