/* Todo: we need a way to store many profiler frames in some
sort of circular buffer, every profiler frame could have a
different size tho, not sure how to approach this.... */

typedef struct prof_subscope_t prof_subscope_t;
struct prof_subscope_t {
	i32 source_id;
	i64 time_ticks;
	i32 occurrences;
	i32 nsubscopes;
};

typedef struct {
	const char *name;
	const char *func;
	i32         line;
} prof_source_t;

/* memory for the entire profiler subsytem */
global Stack            _prof_mem;

/* profiler sources stored here, looked up by id */
global prof_source_t   *_prof_sources;
global i32              _prof_nsources;

/* memory for the current profiler frame being built,
reset on frame start */
global Stack            _prof_build_stack;

/* stack of current profiler subscopes, pushed and popped */
global prof_subscope_t *_time_subscope_stack[16];
global i32              _time_subscope_index;
global prof_subscope_t *_time_subscope;

/* stack of current time start values, pushed and popped
accordingly, every time a new subscope is created a time
start is added to the stack, when the frame is popped
this is too, the value is used to tell the difference in time */
global i64              _time_start_stack[16];
global i32              _time_start_index;
global i64              _time_start;

/* the current time frame, one per frame */
global prof_subscope_t *_time_frame;


#define PROFILE(NAME) for(i32 _dummy = 0; (_dummy < 1) && (PROF_BEGIN(NAME),1); _dummy ++, PROF_END())

#if defined(PROFILE2_ON)
	#define PROFILE2(NAME) PROFILE2
#else
	#define PROFILE2(NAME)
#endif

#define PROF_END()       prof_end()
#define PROF_BEGIN(NAME) prof_begin(__COUNTER__,NAME,(char*)__FILE__,__LINE__,(char*)__func__)


internal
void prf_setup() {
	/* profiler memory must be contiguous */
	_prof_mem = new_stack(MB(2));
	_prof_nsources = 1024;
	_prof_sources = stack_pushz(_prof_mem,sizeof(*_prof_sources) * _prof_nsources);
	_prof_build_stack = new_substack(_prof_mem,0);
}

internal
void prof_end_frame() {
	debug_check(_time_start_index == 0);
	debug_check(_time_subscope_index == 0);
	i64 time_now = os_get_time();
	_time_frame->time_ticks = time_now - _time_start;
}

internal
void prf_begin_frame() {
	debug_check(_time_start_index == 0);
	debug_check(_time_subscope_index == 0);
	stack_reset(_prof_build_stack);
	_time_frame = stack_pushz(_prof_build_stack,sizeof(*_time_frame));
	_time_subscope = _time_frame;
	_time_start = os_get_time();
}


/* hmmm... Seems we can't get rid of the lookup!
Will have to profile the profiler at some point!

PROFILE("OUTTER") {
	for (i32 i = 0; i < 256; i ++) {
		PROFILE("INNER0") {
		}
		PROFILE("INNER1") {
		}
	}
}
*/
internal
prof_subscope_t *get_sibling_scope(prof_subscope_t *scope) {
	prof_subscope_t *subscope = scope + 1;
	for (i32 i = 0; i < scope->nsubscopes; i ++) {
		subscope = get_sibling_scope(subscope);
	}
	return subscope;
}
internal
prof_subscope_t *find_subscope(prof_subscope_t *scope, i32 id) {
	prof_subscope_t *subscope = scope + 1;
	for (i32 i = 0; i < scope->nsubscopes; i ++) {
		if (subscope->source_id == id) {
			return subscope;
		}
		subscope = get_sibling_scope(subscope);
	}
	return 0;
}

internal
void prof_end() {
	i64 time_now = os_get_time();

	_time_subscope->time_ticks += time_now - _time_start;

	debug_check(_time_start_index > 0);
	debug_check(_time_subscope_index > 0);
	/* restore */
	_time_start = _time_start_stack[-- _time_start_index];
	_time_subscope = _time_subscope_stack[-- _time_subscope_index];
}

internal
prof_source_t prof_get_src(i32 id) { return _prof_sources[id]; }

internal
void prof_begin(i32 id, char *name, char *file, i32 line, char *func) {
	// id -= __COUNTER__;
	debug_check(id >= 0 && id < _prof_nsources);
	if (_prof_sources[id].name == 0) {
		_prof_sources[id].line = line;
		_prof_sources[id].func = func;
		_prof_sources[id].name = name;
	}

	/* Todo: is there a way we could get rid of this lookup? */
	prof_subscope_t *subscope = find_subscope(_time_subscope, id);

	if (!subscope) {
		subscope = stack_pushz(_prof_build_stack,sizeof(*subscope));
		subscope->source_id = id;
		_time_subscope->nsubscopes ++;
	}

	subscope->occurrences += 1;

	_time_subscope_stack[_time_subscope_index ++] = _time_subscope;
	_time_subscope = subscope;

	/* begin timer */
	_time_start_stack[_time_start_index ++] = _time_start;
	_time_start = os_get_time();
}