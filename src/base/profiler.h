#ifndef ELF_BASE_PROFILER_H
#define ELF_BASE_PROFILER_H

#if !defined(ELF_PROFILE)
#define ELF_PROFILE 0
#endif

typedef enum
{
	PROF_COUNTER_TABLE_LOOKUP,
	PROF_COUNTER_TABLE_PROBE,
	PROF_COUNTER_TABLE_HIT,
	PROF_COUNTER_TABLE_MISS,
	PROF_COUNTER_STRING_LOOKUP,
	PROF_COUNTER_STRING_PROBE,
	PROF_COUNTER_STRING_HIT,
	PROF_COUNTER_STRING_MISS,
	PROF_COUNTER_COUNT_
}
ProfCounter;

typedef struct
{
	const char *name;
	const char *file;
	u32         line;
}
ProfSite;

typedef struct
{
	void *field;
	u32   stack_index;
	b32   active;
}
ProfScope;

#if ELF_PROFILE

void prof_begin_capture(void);
void prof_dump(void);
ProfScope prof_scope_begin(ProfSite *site);
void prof_scope_end(ProfScope *scope);
void prof_add_counter(ProfCounter counter, i64 value);

#define PROF_JOIN_(a, b) a##b
#define PROF_JOIN(a, b) PROF_JOIN_(a, b)
#define PROF_SITE(name) { name, __FILE__, __LINE__ }

#define PROF_BLOCK_(name, id) \
	static ProfSite PROF_JOIN(prof_site_, id) = PROF_SITE(name); \
	for (ProfScope PROF_JOIN(prof_scope_, id) = prof_scope_begin(&PROF_JOIN(prof_site_, id)); \
		PROF_JOIN(prof_scope_, id).active; \
		prof_scope_end(&PROF_JOIN(prof_scope_, id)))

#if defined(__COUNTER__)
#define PROF_BLOCK(name) PROF_BLOCK_(name, __COUNTER__)
#else
#define PROF_BLOCK(name) PROF_BLOCK_(name, __LINE__)
#endif
#define PROF_ADD(counter, value) prof_add_counter(counter, value)

#else

#define prof_begin_capture() ((void)0)
#define prof_dump() ((void)0)
#define prof_scope_begin(site) ((ProfScope) {0})
#define prof_scope_end(scope) ((void)0)
#define prof_add_counter(counter, value) ((void)0)
#define PROF_SITE(name) { name, __FILE__, __LINE__ }
#define PROF_BLOCK(name) if (1)
#define PROF_ADD(counter, value) ((void)0)

#endif

#endif
