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
	PROF_COUNTER_ATOM_LOOKUP,
	PROF_COUNTER_ATOM_PROBE,
	PROF_COUNTER_ATOM_HIT,
	PROF_COUNTER_ATOM_MISS,
	PROF_COUNTER_COUNT_
}
ProfCounter;

#if ELF_PROFILE

typedef struct
{
	void *field;
	i64   start;
	b32   active;
}
ProfScope;

void prof_begin_frame(void);
void prof_dump(void);
ProfScope prof_scope_begin(void *id, const char *name);
void prof_scope_end(ProfScope *scope);
void prof_add_counter(ProfCounter counter, i64 value);

#define PROF_JOIN_(a, b) a##b
#define PROF_JOIN(a, b) PROF_JOIN_(a, b)
#define PROF_BLOCK_(name, line) static u8 PROF_JOIN(prof_id_, line); for (ProfScope PROF_JOIN(prof_scope_, line) = prof_scope_begin(&PROF_JOIN(prof_id_, line), name); PROF_JOIN(prof_scope_, line).active; prof_scope_end(&PROF_JOIN(prof_scope_, line)))
#define PROF_BLOCK(name) PROF_BLOCK_(name, __LINE__)
#define PROF_ADD(counter, value) prof_add_counter(counter, value)

#else

#define prof_begin_frame() ((void)0)
#define prof_dump() ((void)0)
#define PROF_BLOCK(name) if (1)
#define PROF_ADD(counter, value) ((void)0)

#endif

#endif
