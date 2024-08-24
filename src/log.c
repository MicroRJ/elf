/*
** See Copyright Notice In elf.h
** log.c
** Simple Logging Tools
*/


char *S_filename(char *s) {
	char *r;

	for(r = s; *s != 0; s += 1) {
		if (*s=='/' || *s=='\\') {
			r=s+1;
		}
	}
	return r;
}


char *log2s(int type) {
	switch (type) {
		case LOG_KDEBUG: return "DEBUG";
		case LOG_KINFO: return "INFO";
		case LOG_KWARNING: return "WARN";
		case LOG_KERROR: return "ERROR";
		case LOG_KFATAL: return "FATAL";
		default: return "OTHER";
	}
}


elAPI void elf_log_(int type, SourceInfo source, const char *fmt, ...) {

	char b[0x1000];

	va_list xx;
	va_start(xx,fmt);

	stbsp_vsnprintf(b,sizeof(b),fmt,xx);

	va_end(xx);

	int line = source.lineNumber;
	char *file = (char*) source.fileName;
	char *func = (char*) source.func;

	printf("%s %s[%i] %s(): %s\n",log2s(type),S_filename(file),line,func,b);
}



