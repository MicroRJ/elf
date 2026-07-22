//
// See Copyright Notice In elf.h
//

typedef enum
{
	LOG_LEVEL_INFO = 0,
	LOG_LEVEL_SUCCESS,
	LOG_LEVEL_WARNING,
	LOG_LEVEL_ERROR,
	LOG_LEVEL_FATAL,
}
LogLevel;

typedef void LogHook(LogLevel level, const char *message, void *user);

static LogHook *global_log_hook;
static void *global_log_hook_user;

static void log_set_hook(LogHook *hook, void *user)
{
	global_log_hook = hook;
	global_log_hook_user = user;
}

static const char *log_level_color(LogLevel level)
{
	switch (level)
	{
		case LOG_LEVEL_SUCCESS: return "\x1b[32m";
		case LOG_LEVEL_WARNING: return "\x1b[33m";
		case LOG_LEVEL_ERROR:   return "\x1b[31m";
		case LOG_LEVEL_FATAL:   return "\x1b[1;31m";
		default:                return "";
	}
}

static void log_write(LogLevel level, const char *message)
{
	static b32 console_colors_enabled;
	if (!console_colors_enabled)
	{
		platform_enable_console_colors(PLATFORM_STANDARD_OUTPUT);
		platform_enable_console_colors(PLATFORM_STANDARD_ERROR);
		console_colors_enabled = 1;
	}

	if (global_log_hook)
	{
		global_log_hook(level, message, global_log_hook_user);
		return;
	}

	const char *color = log_level_color(level);
	const char *reset = color[0] ? "\x1b[0m" : "";
	fprintf(stderr, "%s%s%s", color, message, reset);
}

static void log_line(LogLevel level, const char *message)
{
	log_write(level, message);
	log_write(LOG_LEVEL_INFO, "\n");
}

static void logfv(LogLevel level, const char *format, va_list args)
{
	char message[4096];
	vsnprintf(message, sizeof(message), format, args);
	log_write(level, message);
}

static void log_format(LogLevel level, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	logfv(level, format, args);
	va_end(args);
}

static void log_linef(LogLevel level, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	logfv(level, format, args);
	va_end(args);
	log_write(LOG_LEVEL_INFO, "\n");
}



