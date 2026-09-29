#include <dayan.h>

#if ELF_PROFILE

#define PROF_MAX_FIELDS 4096
#define PROF_MAX_STACK  128

typedef struct
{
	ProfSite *site;
	i64       inclusive_ticks;
	i64       self_ticks;
	i64       minimum_ticks;
	i64       maximum_ticks;
	u32       calls;
}
ProfField;

typedef struct
{
	ProfField *field;
	i64        start;
	i64        child_ticks;
}
ProfEntry;

typedef struct
{
	b32       enabled;
	ProfField fields[PROF_MAX_FIELDS];
	ProfEntry stack[PROF_MAX_STACK];
	i32       stack_count;
	i64       counters[PROF_COUNTER_COUNT_];
	i64       begin_ticks;
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
		case PROF_COUNTER_STRING_LOOKUP: return "string.lookup";
		case PROF_COUNTER_STRING_PROBE:  return "string.probe";
		case PROF_COUNTER_STRING_HIT:    return "string.hit";
		case PROF_COUNTER_STRING_MISS:   return "string.miss";
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

static ProfField *prof_find_field(ProfSite *site)
{
	ASSERT(site != 0);
	ASSERT(site->name != 0);

	u32 mask = PROF_MAX_FIELDS - 1;
	u32 slot = prof_hash_ptr(site) & mask;
	for (u32 miss = 0; miss < PROF_MAX_FIELDS; ++miss)
	{
		ProfField *field = prof_thread.fields + slot;
		if (!field->site)
		{
			field->site = site;
			return field;
		}
		if (field->site == site) {
			return field;
		}
		slot = (slot + 1) & mask;
	}

	ASSERT(!"profiler field table full");
	return prof_thread.fields;
}

void prof_begin_capture(void)
{
	zero_memory(&prof_thread, sizeof(prof_thread));
	prof_thread.enabled = true;
	prof_thread.begin_ticks = (i64)day_counter();
}

ProfScope prof_scope_begin(ProfSite *site)
{
	ProfScope scope = {};
	if (!prof_thread.enabled) {
		return scope;
	}

	ASSERT(prof_thread.stack_count < PROF_MAX_STACK);
	ProfField *field = prof_find_field(site);
	field->calls += 1;

	u32 stack_index = (u32)prof_thread.stack_count++;
	ProfEntry *entry = prof_thread.stack + stack_index;
	entry->field = field;
	entry->start = (i64)day_counter();
	entry->child_ticks = 0;

	scope.field = field;
	scope.stack_index = stack_index;
	scope.active = true;
	return scope;
}

void prof_scope_end(ProfScope *scope)
{
	if (!scope || !scope->active || !prof_thread.enabled) {
		return;
	}

	ASSERT(prof_thread.stack_count > 0);
	ASSERT(scope->stack_index == (u32)(prof_thread.stack_count - 1));
	ProfEntry entry = prof_thread.stack[--prof_thread.stack_count];
	ASSERT(entry.field == (ProfField *)scope->field);

	i64 elapsed = (i64)day_counter() - entry.start;
	i64 self = elapsed - entry.child_ticks;
	entry.field->inclusive_ticks += elapsed;
	entry.field->self_ticks += self;
	if (entry.field->calls == 1 || elapsed < entry.field->minimum_ticks) {
		entry.field->minimum_ticks = elapsed;
	}
	if (elapsed > entry.field->maximum_ticks) {
		entry.field->maximum_ticks = elapsed;
	}
	if (prof_thread.stack_count) {
		prof_thread.stack[prof_thread.stack_count - 1].child_ticks += elapsed;
	}
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
		while (j > 0 && fields[j - 1]->self_ticks < field->self_ticks)
		{
			fields[j] = fields[j - 1];
			j -= 1;
		}
		fields[j] = field;
	}
}

void prof_dump(void)
{
	if (!prof_thread.enabled) {
		return;
	}
	ASSERT(prof_thread.stack_count == 0);
	i64 end_ticks = (i64)day_counter();
	i64 frequency = (i64)day_counter_frequency();
	ProfField *fields[PROF_MAX_FIELDS];
	u32 count = 0;

	for (u32 i = 0; i < PROF_MAX_FIELDS; ++i)
	{
		if (prof_thread.fields[i].site && prof_thread.fields[i].calls) {
			fields[count++] = prof_thread.fields + i;
		}
	}

	prof_sort_fields(fields, count);

	f64 captured_ms = 1000.0 * (f64)(end_ticks - prof_thread.begin_ticks) / (f64)frequency;
	fprintf(stderr, "\n-- profiler counters (%.3f ms captured) --\n", captured_ms);
	for (u32 i = 0; i < PROF_COUNTER_COUNT_; ++i)
	{
		if (prof_thread.counters[i]) {
			fprintf(stderr, "%-24s %lld\n", prof_counter_name((ProfCounter)i), prof_thread.counters[i]);
		}
	}

	fprintf(stderr, "\n-- profiler timings (sorted by self time) --\n");
	fprintf(stderr, "%-34s %10s %10s %10s %10s %10s %8s  %s\n"
	, "scope", "total ms", "self ms", "avg us", "min us", "max us", "calls", "location");
	for (u32 i = 0; i < count; ++i)
	{
		ProfField *field = fields[i];
		f64 inclusive_ms = 1000.0 * (f64)field->inclusive_ticks / (f64)frequency;
		f64 self_ms = 1000.0 * (f64)field->self_ticks / (f64)frequency;
		f64 average_us = field->calls ? (inclusive_ms * 1000.0) / (f64)field->calls : 0;
		f64 minimum_us = 1000000.0 * (f64)field->minimum_ticks / (f64)frequency;
		f64 maximum_us = 1000000.0 * (f64)field->maximum_ticks / (f64)frequency;
		fprintf(stderr, "%-34s %10.3f %10.3f %10.3f %10.3f %10.3f %8u  %s:%u\n"
		, field->site->name
		, inclusive_ms
		, self_ms
		, average_us
		, minimum_us
		, maximum_us
		, field->calls
		, field->site->file
		, field->site->line);
	}
}

#endif
