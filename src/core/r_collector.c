//
// See Copyright Notice In elf.h
//

int elf_gc_state(elf_State *S, elf_GC_State state) {
	elf_GC_State prevstate = S->G.state;
	if (state != ELF_GC_GETSTATE) {
		S->G.state = state;
	}
	return prevstate;
}

void *elf_gc_alloc(elf_State *R, elf_GC_Ty type, elf_i64 size) {
	elf_Collector *gc = & R->G;

	if (gc->phase != GC_COLLECTABLE) {
		elf_error(R,NO_BYTE,"object allocation out of phase");
	}

	gc->memory_allocated += size;

	if (gc->state == ELF_GC_ACTIVE) {
		elf_gc_check(R);
	}

	elf_Object *obj = calloc(size,1);
	obj->color = gc->phase;
	obj->type = type;
	obj->size = size;

	gc->open_object_slots[gc->num_objects ++] = obj;
	return obj;
}


static elf_i64 _mark(elf_Object *obj) {
	ASSERT(obj != 0);
	elf_i64 num = 0;
	if (obj->color != GC_NOCOLLECT) {
		num = 1;
		ASSERT(obj->color == GC_COLLECTABLE);
		obj->color = GC_NOCOLLECT;
		if (obj->meta) {
			num += _mark((elf_Object*) obj->meta);
		}
		if (obj->type == GC_CLS) {
			elf_Closure *closure = (elf_Closure*) obj;
			FOR_RANGE(i, 0, closure->proto.ncaptures) {
				if (tisobject(closure->captures[i].tag)) {
					num += _mark(closure->captures[i].x_obj);
				}
			}
		} else if (obj->type == GC_TAB) {
			elf_Table *table = (elf_Table *) obj;
			elf_Value *array = table->array;
			elf_Table_Entry *slots = table->slots;
			FOR_RANGE(i,0,table->ntotal) {
				if (tisobject(slots[i].key.tag)) {
					num += _mark(slots[i].key.x_obj);
				}
			}
			FOR_RANGE(i,0,arrlen(array)) {
				if (tisobject(array[i].tag)) {
					num += _mark(array[i].x_obj);
				}
			}
		}
	}
	return num;
}

elf_rawapi
int _gc_mark(elf_State *inter)
{
	ASSERT(inter->gc.phase == GC_PHASE_MARK);
	inter->gc.phase ^= 1;

	int nmarked = 0;

	// todo: cache line!
	elf_Value *ptr;
	for (ptr = inter->stack; ptr < inter->stack; ++ ptr) {
		if (tisobject(ptr->tag)) {
			nmarked += _mark(ptr->x_obj);
		}
	}

	// elf_debug_log("mark took: %fms", prof_time_diff_ms(time));
	return nmarked;
}


elf_i64 _gc_free(elf_State *R) {
	elf_Collector *gc = &R->G;
	ASSERT(gc->phase == GC_PHASE_FREE);
	gc->phase ^= 1;

	// elf_Object **new_objects=R->G.new_objects;
	// elf_Object **objects=R->G.objects;
	// if (new_objects) {
	// 	ARRAY_SET_MIN(new_objects,0);
	// }
	elf_Object **open_object_slots = gc->open_object_slots;
	elf_Object **close_object_slots = gc->close_object_slots;

	elf_i64 num_objects = 0;
	elf_i64 num_free_objects = 0;
	for(elf_i64 i = 0; i < gc->num_objects; ++ i) {
		elf_Object *obj = open_object_slots[i];

		if (obj->color == GC_NOCOLLECT) {

			obj->color = GC_COLLECTABLE;
			close_object_slots[num_objects ++] = obj;

		} else if(obj->color == GC_COLLECTABLE) {
			gc->memory_allocated -= obj->size;
			if (obj->type == GC_TAB) {
				elf_tableK_recycle((elf_Table *) obj);
			}
			free(obj);
		}
	}
	gc->num_objects = num_objects;
	gc->open_object_slots = close_object_slots;
	gc->close_object_slots = open_object_slots;
	return num_free_objects;
}

static elf_i64 elf_gc_cycle(elf_State *R) {
	elf_i64 time_ = prof_get_time();

	elf_i64 num_marked;
	{
		// elf_i64 _time = prof_get_time();
		num_marked = _gc_mark(R);
		// elf_debug_log("mark took: %fms", prof_time_diff_ms(_time));
	}
	elf_i64 num_objects = R->G.num_objects; // arrlen(R->G.objects);
	elf_i64 num_to_collect = num_objects - num_marked;
	elf_i64 obj_trigger_threshold = R->G.object_trigger_threshold;
	// elf_debug_log("GC: %lli - %lli -> %lli (%lli), (total - marked = expected) (threshold)",num_objects,num_to_collect,num_marked,obj_trigger_threshold);

	elf_i64 num_collected;
	{
		// elf_i64 _time = prof_get_time();
		num_collected = _gc_free(R);
		// elf_debug_log("free took: %fms", prof_time_diff_ms(_time));
	}
	num_to_collect -= num_collected;
	// elf_debug_log("	(%fms) => leaked: %lli", prof_time_diff_ms(time_),num_to_collect);
	return num_collected;
}


void elf_gc_check(elf_State *R) {
	elf_Collector *G = & R->G;


	if (G->memory_threshold <= 0) {
		G->memory_threshold = GC_MEM_THRESHOLD_MIN;
	}
	if (G->object_trigger_threshold <= 0) {
		G->object_trigger_threshold = GC_OBJ_THRESHOLD_MIN;
	}

	elf_i64 num_objects, num_collected;
	num_objects = G->num_objects;

	if (num_objects > G->object_trigger_threshold) {

		num_collected = elf_gc_cycle(R);

		ASSERT(num_collected <= num_objects);
		G->object_trigger_threshold += GC_OBJ_THRESHOLD_MIN - num_collected;

	} else if (G->memory_allocated > G->memory_threshold) {

		G->memory_threshold <<= 1;
		if (G->memory_threshold > GC_MEM_THRESHOLD_MAX) {
			G->memory_threshold = GC_MEM_THRESHOLD_MAX;
		}

		num_collected = elf_gc_cycle(R);
		ASSERT(num_collected <= num_objects);

		if (G->memory_allocated > G->memory_threshold) {
			elf_error(R,NO_BYTE,elf_tpf("out of memory, %lliMB allocated, %lliMB threshold"
			, G->memory_allocated / MEGABYTES(1)
			, G->memory_threshold / MEGABYTES(1)));
		}
	}
}