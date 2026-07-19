#if ELF_PROFILE

#define PROF_MAX_FIELDS 4096
#define PROF_MAX_STACK  128

typedef struct
{
	void       *id;
	const char *name;
	i64         ticks;
	u32         calls;
	u32         depth;
}
ProfField;

typedef struct
{
	ProfField *field;
	i64        start;
}
ProfEntry;

typedef struct
{
	b32       enabled;
	ProfField fields[PROF_MAX_FIELDS];
	ProfEntry stack[PROF_MAX_STACK];
	i32       stack_count;
	i64       counters[PROF_COUNTER_COUNT_];
}
ProfThread;

static _Thread_local ProfThread prof_thread;

static const char *prof_counter_name(ProfCounter counter)
{
	switch (counter)
	{
		case PROF_COUNTER_TABLE_LOOKUP: return "table.lookup";
		case PROF_COUNTER_TABLE_PROBE:  return "table.probe";
		case PROF_COUNTER_TABLE_HIT:    return "table.hit";
		case PROF_COUNTER_TABLE_MISS:   return "table.miss";
		case PROF_COUNTER_ATOM_LOOKUP:  return "atom.lookup";
		case PROF_COUNTER_ATOM_PROBE:   return "atom.probe";
		case PROF_COUNTER_ATOM_HIT:     return "atom.hit";
		case PROF_COUNTER_ATOM_MISS:    return "atom.miss";
		default:                        return "unknown";
	}
}

static u32 prof_hash_ptr(void *ptr)
{
	u64 value = (u64)ptr;
	value ^= value >> 33;
	value *= 0xff51afd7ed558ccdull;
	value ^= value >> 33;
	value *= 0xc4ceb9fe1a85ec53ull;
	value ^= value >> 33;
	return (u32)value;
}

static ProfField *prof_find_field(void *id, const char *name)
{
	ASSERT(id != 0);

	u32 mask = PROF_MAX_FIELDS - 1;
	u32 slot = prof_hash_ptr(id) & mask;
	for (u32 miss = 0; miss < PROF_MAX_FIELDS; ++miss)
	{
		ProfField *field = prof_thread.fields + slot;
		if (!field->id)
		{
			field->id = id;
			field->name = name;
			return field;
		}
		if (field->id == id) {
			return field;
		}
		slot = (slot + 1) & mask;
	}

	ASSERT(!"profiler field table full");
	return prof_thread.fields;
}

void prof_begin_frame(void)
{
	zero_memory(&prof_thread, sizeof(prof_thread));
	prof_thread.enabled = true;
}

ProfScope prof_scope_begin(void *id, const char *name)
{
	ProfScope scope = {};
	if (!prof_thread.enabled) {
		return scope;
	}

	ASSERT(prof_thread.stack_count < PROF_MAX_STACK);
	ProfField *field = prof_find_field(id, name);
	field->calls += 1;
	if (field->calls == 1) {
		field->depth = (u32)prof_thread.stack_count;
	}

	i64 start = elf_platform_counter();
	ProfEntry *entry = prof_thread.stack + prof_thread.stack_count++;
	entry->field = field;
	entry->start = start;

	scope.field = field;
	scope.start = start;
	scope.active = true;
	return scope;
}

void prof_scope_end(ProfScope *scope)
{
	if (!scope || !scope->active || !prof_thread.enabled) {
		return;
	}

	ASSERT(prof_thread.stack_count > 0);
	ProfEntry entry = prof_thread.stack[--prof_thread.stack_count];
	ASSERT(entry.field == (ProfField *)scope->field);

	i64 end = elf_platform_counter();
	entry.field->ticks += end - entry.start;
	scope->active = false;
}

void prof_add_counter(ProfCounter counter, i64 value)
{
	if (!prof_thread.enabled) {
		return;
	}
	ASSERT((u32)counter < PROF_COUNTER_COUNT_);
	prof_thread.counters[counter] += value;
}

static void prof_sort_fields(ProfField **fields, u32 count)
{
	for (u32 i = 1; i < count; ++i)
	{
		ProfField *field = fields[i];
		u32 j = i;
		while (j > 0 && fields[j - 1]->ticks < field->ticks)
		{
			fields[j] = fields[j - 1];
			j -= 1;
		}
		fields[j] = field;
	}
}

void prof_dump(void)
{
	i64 frequency = elf_platform_counter_frequency();
	ProfField *fields[PROF_MAX_FIELDS];
	u32 count = 0;

	for (u32 i = 0; i < PROF_MAX_FIELDS; ++i)
	{
		if (prof_thread.fields[i].id && prof_thread.fields[i].calls) {
			fields[count++] = prof_thread.fields + i;
		}
	}

	prof_sort_fields(fields, count);

	fprintf(stderr, "\n-- profiler counters --\n");
	for (u32 i = 0; i < PROF_COUNTER_COUNT_; ++i)
	{
		if (prof_thread.counters[i]) {
			fprintf(stderr, "%-24s %lld\n", prof_counter_name((ProfCounter)i), prof_thread.counters[i]);
		}
	}

	fprintf(stderr, "\n-- profiler timings --\n");
	for (u32 i = 0; i < count; ++i)
	{
		ProfField *field = fields[i];
		f64 ms = 1000.0 * (f64)field->ticks / (f64)frequency;
		f64 avg_us = field->calls ? (ms * 1000.0) / (f64)field->calls : 0;
		fprintf(stderr, "%*s%-40s %8.3f ms  %8.3f us/call  x%u\n"
		,	field->depth * 2, ""
		,	field->name
		,	ms
		,	avg_us
		,	field->calls);
	}
}

#endif
