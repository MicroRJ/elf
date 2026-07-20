//
// Optional Win32 process support.
//

int elf_platform_process_id(void)
{
	return GetCurrentProcessId();
}

int elf_platform_work_dir(char *buf, int bufsize)
{
	return GetCurrentDirectoryA(bufsize, buf);
}

int elf_platform_set_work_dir(const char *buf)
{
	return SetCurrentDirectoryA(buf);
}

int elf_platform_last_error(void)
{
	return GetLastError();
}

void elf_platform_error_message(int error, char *buf, int len)
{
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, error, LANG_USER_DEFAULT, buf, len, NULL);
}

elf_PlatformFile elf_platform_create_process(const char *file, const char *args)
{
	STARTUPINFOA startup = {sizeof(startup)};
	PROCESS_INFORMATION process = {0};

	elf_Scratch scratch = elf_begin_scratch();
	char *command_line = elf_arena_push_text(scratch.arena, args);
	elf_arena_push_char(scratch.arena, 0);
	b32 started = CreateProcessA(file, command_line, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process);
	elf_end_scratch(scratch);

	if (!started) return 0;
	CloseHandle(process.hThread);
	return (elf_PlatformFile)process.hProcess;
}

static void elf_platform_drain_process_pipe(HANDLE pipe, elf_Arena *output)
{
	for (;;)
	{
		DWORD available = 0;
		if (!PeekNamedPipe(pipe, NULL, 0, NULL, &available, NULL) || available == 0) return;

		DWORD request = MIN(available, 64 * 1024);
		char *data = elf_arena_push(output, request);
		DWORD read = 0;
		if (!ReadFile(pipe, data, request, &read, NULL)) {
			output->in_use -= request;
			return;
		}
		output->in_use -= request - read;
	}
}

elf_PlatformProcessResult elf_platform_run_process(const char *command_line, elf_Arena *standard_output, elf_Arena *standard_error)
{
	elf_PlatformProcessResult result = {.exit_code = -1};
	SECURITY_ATTRIBUTES security = {
		.nLength = sizeof(security),
		.bInheritHandle = TRUE,
	};
	HANDLE stdout_read = 0;
	HANDLE stdout_write = 0;
	HANDLE stderr_read = 0;
	HANDLE stderr_write = 0;
	PROCESS_INFORMATION process = {0};

	if (!CreatePipe(&stdout_read, &stdout_write, &security, 0)
	||  !SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0)
	||  !CreatePipe(&stderr_read, &stderr_write, &security, 0)
	||  !SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0))
	{
		result.error_code = GetLastError();
		goto esc;
	}

	STARTUPINFOA startup = {sizeof(startup)};
	startup.dwFlags = STARTF_USESTDHANDLES;
	startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	startup.hStdOutput = stdout_write;
	startup.hStdError = stderr_write;

	elf_Scratch scratch = elf_begin_scratch();
	char *mutable_command_line = elf_arena_push_text(scratch.arena, command_line);
	elf_arena_push_char(scratch.arena, 0);
	result.started = CreateProcessA(NULL, mutable_command_line, NULL, NULL, TRUE,
		CREATE_NO_WINDOW, NULL, NULL, &startup, &process);
	if (!result.started) result.error_code = GetLastError();
	elf_end_scratch(scratch);

	CloseHandle(stdout_write);
	stdout_write = 0;
	CloseHandle(stderr_write);
	stderr_write = 0;

	if (!result.started) goto esc;
	CloseHandle(process.hThread);
	process.hThread = 0;

	for (;;)
	{
		elf_platform_drain_process_pipe(stdout_read, standard_output);
		elf_platform_drain_process_pipe(stderr_read, standard_error);
		if (WaitForSingleObject(process.hProcess, 1) == WAIT_OBJECT_0) break;
	}

	elf_platform_drain_process_pipe(stdout_read, standard_output);
	elf_platform_drain_process_pipe(stderr_read, standard_error);

	DWORD exit_code = 0;
	if (GetExitCodeProcess(process.hProcess, &exit_code)) result.exit_code = (i32)exit_code;
	else result.error_code = GetLastError();

esc:
	if (process.hThread) CloseHandle(process.hThread);
	if (process.hProcess) CloseHandle(process.hProcess);
	if (stdout_read) CloseHandle(stdout_read);
	if (stdout_write) CloseHandle(stdout_write);
	if (stderr_read) CloseHandle(stderr_read);
	if (stderr_write) CloseHandle(stderr_write);
	return result;
}
