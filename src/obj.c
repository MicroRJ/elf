/*
** See Copyright Notice In elf.h
** obj.c
*/

#if 0
int core_lib_pause_collector(elf_State *R) {
	R->gc.paused = 1;
	if (elf_get_num_args(R) == 1) {
		R->gc.paused = elf_get_int(R,0) != 0;
	}
	return 0;
}


int core_lib_get_allocated_objects(elf_State *R) {
	elf_push_int(R,ARRAY_LENGTH(R->gc.objects));
	return 1;
}


int core_lib_get_allocated_memory(elf_State *R) {
	elf_push_int(R,R->gc.memory_allocated);
	return 1;
}


int core_lib_get_collector_threshold(elf_State *R) {
	elf_push_int(R,R->gc.memory_threshold);
	return 1;
}


int core_lib_mark_object(elf_State *R) {
	elf_Int num = elf_mark_object(elf_get_obj(R,0));
	elf_push_int(R,num);
	return 1;
}


int core_lib_collect(elf_State *R) {
	_gc_cycle(R);
	return 0;
}
#endif

/* returns number of objects uniquely marked */
static elf_i64 _mark(elf_Object *obj) {
	ASSERT(obj != 0);
	ASSERT(obj->color != elf_GC_RED);
	/* already accounted for */
	if (obj->color == elf_GC_BLACK) {
		return 0;
	}
	elf_i64 num = 1;
	if ((OBJ_COLOR(obj) == elf_GC_WHITE) || (OBJ_COLOR(obj) == elf_GC_TRAP)) {
		OBJ_COLOR(obj) = elf_GC_BLACK;
	}
	if (obj->meta) {
		num += _mark((elf_Object*)obj->meta);
	}
	if (obj->type == GC_CLS) {
		elf_Closure *cls = (elf_Closure*) obj;
		// if (cls->proto.name != 0) {
		// 	_mark(POBJ(cls->proto.name));
		// }
		// if (cls->proto.contents != 0) {
		// 	_mark(POBJ(cls->proto.contents));
		// }
		// if (cls->proto.parent != -1) {
		// }
		FOR_RANGE(i, 0, cls->proto.nlocals) {
			if (ISOBJT(cls->values[i].tag)) {
				num += _mark(cls->values[i].x_obj);
			}
		}
	} else if (obj->type == GC_TAB) {
		elf_Table *table;
		elf_Value *array;
		elf_Entry *slots;

		table=(elf_Table*)obj;
		array=table->array;
		slots=table->slots;

		FOR_RANGE(k,0,table->ntotal) {
			if (ISOBJT(slots[k].key.tag)) {
				num += _mark(slots[k].key.x_obj);
			}
		}
		FOR_RANGE(k,0,ARRAY_LENGTH(array)) {
			if (ISOBJT(array[k].tag)) {
				num += _mark(array[k].x_obj);
			}
		}
	}
	return num;
}

static elf_i64 _gc_mark(elf_State *R) {
	ASSERT(R->gc.phase == elf_GC_PHASE_MARK);
	R->gc.phase ^= 1;

	elf_i64 num_objs = 0;
	elf_Value *ptr;
	for (ptr = R->stack; ptr < GET_TOP(R); ++ ptr) {
		if (ISOBJT(ptr->tag)) {
			num_objs += _mark(ptr->x_obj);
		}
	}
	return num_objs;
}


elf_i64 _gc_free(elf_State *R) {
	ASSERT(R->gc.phase==elf_GC_PHASE_FREE);
	R->gc.phase^=1;


	elf_Object **new_objects=R->gc.new_objects;
	elf_Object **objects=R->gc.objects;
	if (new_objects) {
		ARRAY_SET_MIN(new_objects,0);
	}
	elf_i64 n = 0;
	FOR_ARRAY(i,objects) {
		elf_Object *it = objects[i];
		ASSERT(it != 0);
#if 0
		if ((OBJ_COLOR(it) == elf_GC_RED) || (OBJ_COLOR(it) == elf_GC_TRAP)) {
			for(elf_Value *Ki = R->stack; Ki < R->T; Ki += 1) {
				if (Ki->x_obj == it) {
					elf_debug_log("Object '%p' found in stack at: '%p'. From top '%p' -> %lli", it, Ki, R->T, (R->T - Ki));
				}
			}
			elf_fail(R,it->byte,elf_tpf("internal error, GC failed, attempted to collect object '%p'", it));
		}
#endif
		if (OBJ_COLOR(it) == elf_GC_BLACK) {
			OBJ_COLOR(it) = elf_GC_WHITE;
			/* todo: instead simply ensure 'new_objects' is big enough */
			ARRAY_ADD(new_objects,it);
		} else if (OBJ_COLOR(it) == elf_GC_WHITE) {
			n += 1;
			OBJ_COLOR(it) = elf_GC_RED;
			R->gc.memory_allocated -= it->size;
			if (it->type == GC_TAB) {
				elf_free_table_contents((elf_Table*)it);
			}
			dealloc_memory(GLOBAL_ALLOCATOR,it);
		} else n += 1;
	}
	R->gc.objects = new_objects;
	R->gc.new_objects = objects;
	return n;
}


static elf_i64 _gc_cycle(elf_State *R) {
	elf_i64 time_, num_marked, num_objects, num_to_collect, obj_trigger_threshold, num_collected;

	time_ = elf_get_clock_time();
	num_marked = _gc_mark(R);
	num_objects = ARRAY_LENGTH(R->gc.objects);
	num_to_collect = num_objects - num_marked;
	obj_trigger_threshold = R->gc.object_trigger_threshold;

	// elf_debug_log("GC: %lli - %lli -> %lli (%lli), (total - marked = expected) (threshold)",num_objects,num_to_collect,num_marked,obj_trigger_threshold);

	num_collected = _gc_free(R);
	num_to_collect -= num_collected;

	// elf_debug_log("	(%f) => leaked: %lli", elf_time_diff_ms(time_),num_to_collect);
	return num_collected;
}


void _gc_check(elf_State *R) {
	if (R->gc.paused) {
		return;
	}
	if (R->gc.memory_threshold <= 0) {
		R->gc.memory_threshold = elGC_MEM_THRESHOLD_MIN;
	}
	if (R->gc.object_trigger_threshold <= 0) {
		R->gc.object_trigger_threshold = elGC_OBJ_THRESHOLD_MIN;
	}

	elf_i64 num_objects, num_collected;
	num_objects = ARRAY_LENGTH(R->gc.objects);

	if (num_objects > R->gc.object_trigger_threshold) {
		num_collected = _gc_cycle(R);
		ASSERT(num_collected <= num_objects);
		R->gc.object_trigger_threshold += elGC_OBJ_THRESHOLD_MIN - num_collected;
	} else if (R->gc.memory_allocated > R->gc.memory_threshold) {
		R->gc.memory_threshold <<= 1;
		if (R->gc.memory_threshold > elGC_MEM_THRESHOLD_MAX) {
			R->gc.memory_threshold = elGC_MEM_THRESHOLD_MAX;
		}
		num_collected = _gc_cycle(R);
		ASSERT(num_collected <= num_objects);
		if (R->gc.memory_allocated > R->gc.memory_threshold) {
			elf_fail(R,NO_BYTE,elf_tpf("out of memory, %lliMB allocated, %lliMB threshold"
			, R->gc.memory_allocated / MEGABYTES(1)
			, R->gc.memory_threshold / MEGABYTES(1)));
		}
	}
}


void *elf_alloc_object(elf_State *R, elf_GCTy type, elf_i64 size) {
	if (R->gc.phase!=elf_GC_WHITE) {
		elf_fail(R,NO_BYTE,"object allocation out of phase");
	}

	R->gc.memory_allocated += size;
	_gc_check(R);

	elf_Object *obj;

	obj=calloc_memory(GLOBAL_ALLOCATOR,size);
	obj->color=R->gc.phase;
	obj->type=type;
	obj->size=size;
	ARRAY_ADD(R->gc.objects,obj);
	return obj;
}
