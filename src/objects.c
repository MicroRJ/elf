/*
** See Copyright Notice In elf.h
** obj.c
*/


void _gc_check(elf_State *R, int size);

void *elf_alloc_object(elf_State *R, elf_GCTy type, elf_i64 size) {
	if (R->G.phase != GC_COLLECTABLE) {
		elf_error(R,NO_BYTE,"object allocation out of phase");
	}
	_gc_check(R,size);
	elf_Object *obj = calloc(size,1);
	obj->color = R->G.phase;
	obj->type = type;
	obj->size = size;

	R->G.open_object_slots[R->G.num_objects ++] = obj;
	return obj;
}

elf_Closure *elf_alloc_closure(elf_State *S, elf_Proto proto) {
	elf_Closure *cls = (elf_Closure *) elf_alloc_object(S,GC_CLS,sizeof(elf_Closure) + sizeof(elf_Value) * (proto.stacksize-1));
	cls->proto = proto;
	return cls;
}

elf_Table *elf_alloc_table2(elf_State *R, elf_i64 num_initial_entries) {
	elf_Table *table = elf_alloc_object(R, GC_TAB, sizeof(elf_Table));
	table->obj.meta = R->metatables.table;
	table->ndebug = 0;
	table->slots = calloc(1, num_initial_entries * sizeof(elf_Entry));
	table->ntotal = num_initial_entries;
	table->nslots = 0;
	return table;
}

// todo: just make this take the length and if the
// length is zero then use some default value
elf_Table *elf_alloc_table(elf_State *R) {
	return elf_alloc_table2(R,4);
}

void _dealloc_table_contents(elf_Table *tab) {
	free(tab->slots);
	ARRAY_DELETE(tab->array);
	tab->array = 0;
	tab->slots = 0;
}

static void resize_table(elf_Table *table) {
	if (table->ntotal * 3 < table->nslots * 4) {
		elf_Table new_table = *table;
		new_table.ntotal = table->ntotal << 1;
		new_table.slots = calloc(1,new_table.ntotal * sizeof(elf_Entry));

		FOR_RANGE(i,0,table->ntotal) {
			elf_Entry prev_entry = table->slots[i];
			if(prev_entry.key.tag != elf_tag_nil && prev_entry.key.tag != elf_tag_tomb){
				elf_i64 prev_index = elf_table_try(&new_table,prev_entry.key);
				ASSERT(prev_index >= 0);
				new_table.slots[prev_index] = prev_entry;
			}
		}
		free(table->slots);

		table->ntotal = new_table.ntotal;
		table->slots = new_table.slots;
	}
}

elf_String *elf_alloc_string2(elf_State *R, elf_i32 length) {
	elf_String *obj = elf_alloc_object(R,GC_STR,sizeof(elf_String)+length+1);
	if (R) obj->obj.meta = R->metatables.string;
	obj->length = length;
	obj->hash = -1;
	obj->text[length] = 0;
	return obj;
}

elf_String *elf_alloc_string(elf_State *R, const char *text) {
	int length;
	elf_Hash hash;
	elf_String *string;
	elf_Table *registry;

	length=text_length(text);
	hash=hash_text(text);
	string=0;
	registry=R->M->strings;


	if (length < 64 && registry != 0) {
		resize_table(registry);
		elf_Int slot=elf_table_try_text(registry,text,length,hash);
		ASSERT(slot != -1);
		elf_Entry entry=registry->slots[slot];
		if (entry.key.tag != elf_tag_nil) {
			elf_Value target=registry->array[registry->slots[slot].idx];
			string=target.x_str;
		} else {
			string = elf_alloc_string2(R,length);
			copy_memory(string->text,text,length);
			string->hash = hash;

			elf_Int i = ARRAY_GROW(registry->array,1);
			registry->array[i]=VALUE_STRING(string);
			registry->slots[slot].key=VALUE_STRING(string);
			registry->slots[slot].idx=i;
			registry->nslots ++;
		}
	} else {
		string = elf_alloc_string2(R,length);
		copy_memory(string->text,text,length);
		string->hash = hash;
	}
	return string;
}

static elf_i64 _mark(elf_Object *obj) {
	ASSERT(obj != 0);
	ASSERT(obj->color != GC_REUSEABLE);
	elf_i64 num = 0;
	if (obj->color != GC_NOCOLLECT) {
		num = 1;
		ASSERT(obj->color == GC_COLLECTABLE);
		obj->color = GC_NOCOLLECT;
		if (obj->meta) {
			num += _mark((elf_Object*)obj->meta);
		}
		if (obj->type == GC_CLS) {
			elf_Closure *closure = (elf_Closure*) obj;
			FOR_RANGE(i, 0, closure->proto.numvalues) {
				if (ISOBJT(closure->values[i].tag)) {
					num += _mark(closure->values[i].x_obj);
				}
			}
		} else if (obj->type == GC_TAB) {
			elf_Table *table = (elf_Table *) obj;
			elf_Value *array = table->array;
			elf_Entry *slots = table->slots;
			FOR_RANGE(i,0,table->ntotal) {
				if (ISOBJT(slots[i].key.tag)) {
					num += _mark(slots[i].key.x_obj);
				}
			}
			FOR_RANGE(i,0,ARRAY_LENGTH(array)) {
				if (ISOBJT(array[i].tag)) {
					num += _mark(array[i].x_obj);
				}
			}
		}
	}
	return num;
}

static elf_i64 _gc_mark(elf_State *R) {
	ASSERT(R->G.phase == elf_GC_PHASE_MARK);
	R->G.phase ^= 1;
	elf_i64 time = elf_get_clock_time();

	elf_i64 num_objs = 0;
	elf_Value *ptr;
	// todo: cache line!
	for (ptr = R->stack; ptr < GET_TOP(R); ++ ptr) {
		if (ISOBJT(ptr->tag)) {
			num_objs += _mark(ptr->x_obj);
		}
	}
	// elf_debug_log("mark took: %fms", elf_time_diff_ms(time));
	return num_objs;
}


elf_i64 _gc_free(elf_State *R) {
	elf_Collector *gc = &R->G;
	ASSERT(gc->phase == elf_GC_PHASE_FREE);
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
		if (OBJ_COLOR(obj) == GC_NOCOLLECT) {
			OBJ_COLOR(obj) = GC_COLLECTABLE;
			close_object_slots[num_objects ++] = obj;
		} else if(OBJ_COLOR(obj) == GC_COLLECTABLE) {
			gc->memory_allocated -= obj->size;
			if (obj->type == GC_TAB) {
				_dealloc_table_contents((elf_Table *)obj);
			}
			free(obj);
		}
	}
	gc->num_objects = num_objects;
	gc->open_object_slots = close_object_slots;
	gc->close_object_slots = open_object_slots;
	return num_free_objects;
}

static elf_i64 _gc_cycle(elf_State *R) {
	elf_i64 time_ = elf_get_clock_time();

	elf_i64 num_marked;
	{
		// elf_i64 _time = elf_get_clock_time();
		num_marked = _gc_mark(R);
		// elf_debug_log("mark took: %fms", elf_time_diff_ms(_time));
	}
	elf_i64 num_objects = R->G.num_objects; // ARRAY_LENGTH(R->G.objects);
	elf_i64 num_to_collect = num_objects - num_marked;
	elf_i64 obj_trigger_threshold = R->G.object_trigger_threshold;
	// elf_debug_log("GC: %lli - %lli -> %lli (%lli), (total - marked = expected) (threshold)",num_objects,num_to_collect,num_marked,obj_trigger_threshold);

	elf_i64 num_collected;
	{
		// elf_i64 _time = elf_get_clock_time();
		num_collected = _gc_free(R);
		// elf_debug_log("free took: %fms", elf_time_diff_ms(_time));
	}
	num_to_collect -= num_collected;
	// elf_debug_log("	(%fms) => leaked: %lli", elf_time_diff_ms(time_),num_to_collect);
	return num_collected;
}


void _gc_check(elf_State *R, int size) {
	elf_Collector *G = & R->G;
	G->memory_allocated += size;
	if (!G->paused) {
		if (G->memory_threshold <= 0) {
			G->memory_threshold = elGC_MEM_THRESHOLD_MIN;
		}
		if (G->object_trigger_threshold <= 0) {
			G->object_trigger_threshold = elGC_OBJ_THRESHOLD_MIN;
		}

		elf_i64 num_objects, num_collected;
		num_objects = G->num_objects; // ARRAY_LENGTH(G->objects);

		if (num_objects > G->object_trigger_threshold) {
			num_collected = _gc_cycle(R);
			ASSERT(num_collected <= num_objects);
			G->object_trigger_threshold += elGC_OBJ_THRESHOLD_MIN - num_collected;
		} else
		if (G->memory_allocated > G->memory_threshold) {
			G->memory_threshold <<= 1;
			if (G->memory_threshold > elGC_MEM_THRESHOLD_MAX) {
				G->memory_threshold = elGC_MEM_THRESHOLD_MAX;
			}
			num_collected = _gc_cycle(R);
			ASSERT(num_collected <= num_objects);
			if (G->memory_allocated > G->memory_threshold) {
				elf_error(R,NO_BYTE,elf_tpf("out of memory, %lliMB allocated, %lliMB threshold"
				, G->memory_allocated / MEGABYTES(1)
				, G->memory_threshold / MEGABYTES(1)));
			}
		}
	}
}