/*
** See Copyright Notice In elf.h
** obj.c
*/

#if 1

void _gc_check(elf_State *R, int size);

void _init_obj(elf_State *R, elf_Object *obj, int type, int size){
	obj->color=R->G.phase;
	obj->type=type;
	obj->size=size;
}

void *elf_alloc_object(elf_State *R, elf_GCTy type, elf_i64 size) {
	if (R->gc.phase!=GC_COLLECTABLE) {
		elf_fail(R,NO_BYTE,"object allocation out of phase");
	}
	_gc_check(R,size);
	elf_Object *obj=calloc(size,1);
	obj->color=R->gc.phase;
	obj->type=type;
	obj->size=size;
	ARRAY_ADD(R->gc.objects,obj);
	return obj;
}

void _dealloc_entry_chunk(elf_Table_Entry *entries, elf_i32 num_entries){
	elf_Entry_Chunk *chunk = &((elf_Entry_Chunk*)entries)[-1];
	if(num_entries<=4){
		chunk->prox=elf_first_free_chunk_of_4;
		elf_first_free_chunk_of_4=chunk;
	}else{
		free(chunk);
	}
}

elf_Table_Entry *_alloc_entry_chunk(elf_i32 num_entries){
	enum{sizeof_chunk_of_4=sizeof(elf_Entry_Chunk)+sizeof(elf_Table_Entry)*4};
	elf_Entry_Chunk *chunk;
	if(num_entries<=4){
		chunk=elf_first_free_chunk_of_4;
		if(chunk){
			elf_first_free_chunk_of_4=chunk->prox;
			clear_memory(chunk,sizeof_chunk_of_4);
		}else{
			chunk=calloc(1,sizeof_chunk_of_4);
		}
	}else{
		chunk=calloc(1,sizeof(elf_Entry_Chunk)+sizeof(elf_Table_Entry)*num_entries);
	}
	return chunk->entries;
}

elf_Table *elf_alloc_table2(elf_State *R, elf_i64 ntotal) {
	// enum{size=sizeof(elf_Table)};
	// _gc_check(R,size);
	// elf_Object *obj = calloc(size,1);
	// _init_obj(R,obj,GC_TAB,size);
	// elf_Table *table = (elf_Table *) obj;
	elf_Table *table = elf_alloc_object(R,GC_TAB,sizeof(elf_Table));
	table->obj.meta = R->metatables.table;

	table->ntotal = ntotal;
	table->nslots = 0;
	table->slots = _alloc_entry_chunk(ntotal);
	return table;
}

// todo: just make this take the length and if the
// length is zero then use some default value
elf_Table *elf_alloc_table(elf_State *R) {
	return elf_alloc_table2(R,4);
}

void elf_free_table_contents(elf_Table *tab) {
	// dealloc_memory(GLOBAL_ALLOCATOR,tab->slots-1);
	_dealloc_entry_chunk(tab->slots,tab->ntotal);
	ARRAY_DELETE(tab->array);
	tab->array = 0;
	tab->slots = 0;
}

void elf_check_table(elf_Table *table) {
	if (table->ntotal * 3 < table->nslots * 4) {
		// DEBUG_CODE( table->ncollisions = 0 );
		/* todo: better strat */
		elf_Table new_table = *table;
		new_table.ntotal = table->ntotal << 2;
		if (new_table.ntotal < table->ntotal) NO_CODE;
		// new_table.slots = calloc_memory(GLOBAL_ALLOCATOR,new_table.ntotal*sizeof(elf_Table_Entry));
		new_table.slots = _alloc_entry_chunk(new_table.ntotal);

		elf_Table_Entry old_slot;
		elf_i64 new_slot;
		FOR_RANGE(i,0,table->ntotal) {
			old_slot=table->slots[i];
			if (old_slot.key.tag==elf_tag_nil||old_slot.key.tag==elf_tag_tomb) continue;

			new_slot=elf_table_try(&new_table,old_slot.key);
			ASSERT(new_slot>=0);
			new_table.slots[new_slot]=old_slot;
		}
		// dealloc_memory(GLOBAL_ALLOCATOR,table->slots);
		_dealloc_entry_chunk(table->slots,table->nslots);

		table->ntotal=new_table.ntotal;
		table->slots=new_table.slots;
	}
}










static elf_i64 _mark(elf_Object *obj) {
	ASSERT(obj != 0);
	// ASSERT(obj->color != elf_GC_RED);
	/* already accounted for */
	if (obj->color == GC_NOCOLLECT) {
		return 0;
	}
	elf_i64 num = 1;
	if ((OBJ_COLOR(obj) == GC_COLLECTABLE)) {
		OBJ_COLOR(obj) = GC_NOCOLLECT;
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
		elf_Table_Entry *slots;

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

	int num_objs[4]={};
	int tot_size[4]={};
	int tot_age[4]={};

	int k_num_objs[4]={};
	int k_tot_size[4]={};
	int k_tot_age[4]={};

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
		if (OBJ_COLOR(it) == GC_NOCOLLECT) {
			// k_num_objs[it->type] ++;
			// k_tot_size[it->type] += it->size;
			// k_tot_age[it->type] += it->age;

			it->age += 1;

			OBJ_COLOR(it) = GC_COLLECTABLE;
			/* todo: instead simply ensure 'new_objects' is big enough */
			ARRAY_ADD(new_objects,it);
		} else if (OBJ_COLOR(it) == GC_COLLECTABLE) {
			// num_objs[it->type] ++;
			// tot_size[it->type] += it->size;
			// tot_age[it->type] += it->age;
			n += 1;
			// OBJ_COLOR(it) = elf_GC_RED;
			R->gc.memory_allocated -= it->size;
			if (it->type == GC_TAB) {
				elf_free_table_contents((elf_Table*)it);
			}
			dealloc_memory(GLOBAL_ALLOCATOR,it);
		} else n += 1;
	}

	// for (int i=0;i<4;i++){
		// elf_debug_log("(free) '%s': num: %i, tot size: %i, avg size: %i, avg age: %i",obj2s[i],num_objs[i],tot_size[i],tot_size[i]/(num_objs[i]?num_objs[i]:1),tot_age[i]/(num_objs[i]?num_objs[i]:1));
	// }
	// for (int i=0;i<4;i++){
		// elf_debug_log("(kept) '%s': num: %i, tot size: %i, avg size: %i, avg age: %i",obj2s[i],k_num_objs[i],k_tot_size[i],k_tot_size[i]/(k_num_objs[i]?k_num_objs[i]:1),k_tot_age[i]/(k_num_objs[i]?k_num_objs[i]:1));
	// }

	R->gc.objects = new_objects;
	R->gc.new_objects = objects;
	return n;
}


static elf_i64 _gc_cycle(elf_State *R) {
	elf_i64 time_ = elf_get_clock_time();
	elf_i64 num_marked = _gc_mark(R);
	elf_i64 num_objects = ARRAY_LENGTH(R->gc.objects);
	elf_i64 num_to_collect = num_objects - num_marked;
	elf_i64 obj_trigger_threshold = R->gc.object_trigger_threshold;
	// elf_debug_log("GC: %lli - %lli -> %lli (%lli), (total - marked = expected) (threshold)",num_objects,num_to_collect,num_marked,obj_trigger_threshold);

	elf_i64 num_collected = _gc_free(R);
	num_to_collect -= num_collected;

	elf_debug_log("	(%fms) => leaked: %lli", elf_time_diff_ms(time_),num_to_collect);
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
		num_objects = ARRAY_LENGTH(G->objects);

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
				elf_fail(R,NO_BYTE,elf_tpf("out of memory, %lliMB allocated, %lliMB threshold"
				, G->memory_allocated / MEGABYTES(1)
				, G->memory_threshold / MEGABYTES(1)));
			}
		}
	}
}


#else
/* returns number of objects uniquely marked */
static elf_i64 _mark(elf_Object *obj) {
	ASSERT(obj != 0);
	ASSERT(obj->color != elf_GC_RED);
	/* already accounted for */
	if (obj->color == GC_NOCOLLECT) {
		return 0;
	}
	elf_i64 num = 1;
	if ((OBJ_COLOR(obj) == GC_COLLECTABLE) || (OBJ_COLOR(obj) == elf_GC_TRAP)) {
		OBJ_COLOR(obj) = GC_NOCOLLECT;
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
		elf_Table_Entry *slots;

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

	int num_objs[4]={};
	int tot_size[4]={};
	int tot_age[4]={};

	int k_num_objs[4]={};
	int k_tot_size[4]={};
	int k_tot_age[4]={};

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
		if (OBJ_COLOR(it) == GC_NOCOLLECT) {
			// k_num_objs[it->type] ++;
			// k_tot_size[it->type] += it->size;
			// k_tot_age[it->type] += it->age;

			it->age += 1;

			OBJ_COLOR(it) = GC_COLLECTABLE;
			/* todo: instead simply ensure 'new_objects' is big enough */
			ARRAY_ADD(new_objects,it);
		} else if (OBJ_COLOR(it) == GC_COLLECTABLE) {
			// num_objs[it->type] ++;
			// tot_size[it->type] += it->size;
			// tot_age[it->type] += it->age;
			n += 1;
			OBJ_COLOR(it) = elf_GC_RED;
			R->gc.memory_allocated -= it->size;
			if (it->type == GC_TAB) {
				elf_free_table_contents((elf_Table*)it);
			}
			dealloc_memory(GLOBAL_ALLOCATOR,it);
		} else n += 1;
	}

	// for (int i=0;i<4;i++){
		// elf_debug_log("(free) '%s': num: %i, tot size: %i, avg size: %i, avg age: %i",obj2s[i],num_objs[i],tot_size[i],tot_size[i]/(num_objs[i]?num_objs[i]:1),tot_age[i]/(num_objs[i]?num_objs[i]:1));
	// }
	// for (int i=0;i<4;i++){
		// elf_debug_log("(kept) '%s': num: %i, tot size: %i, avg size: %i, avg age: %i",obj2s[i],k_num_objs[i],k_tot_size[i],k_tot_size[i]/(k_num_objs[i]?k_num_objs[i]:1),k_tot_age[i]/(k_num_objs[i]?k_num_objs[i]:1));
	// }

	R->gc.objects = new_objects;
	R->gc.new_objects = objects;
	return n;
}


static elf_i64 _gc_cycle(elf_State *R) {
	elf_i64 time_ = elf_get_clock_time();
	elf_i64 num_marked = _gc_mark(R);
	elf_i64 num_objects = ARRAY_LENGTH(R->gc.objects);
	elf_i64 num_to_collect = num_objects - num_marked;
	elf_i64 obj_trigger_threshold = R->gc.object_trigger_threshold;
	elf_debug_log("GC: %lli - %lli -> %lli (%lli), (total - marked = expected) (threshold)",num_objects,num_to_collect,num_marked,obj_trigger_threshold);

	elf_i64 num_collected = _gc_free(R);
	num_to_collect -= num_collected;

	elf_debug_log("	(%fms) => leaked: %lli", elf_time_diff_ms(time_),num_to_collect);
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
	} else
	if (R->gc.memory_allocated > R->gc.memory_threshold) {
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
	if (R->gc.phase!=GC_COLLECTABLE) {
		elf_fail(R,NO_BYTE,"object allocation out of phase");
	}
	R->gc.memory_allocated += size;
	_gc_check(R);

	elf_Object *obj=calloc(size,1);
	obj->color=R->gc.phase;
	obj->type=type;
	obj->size=size;
	ARRAY_ADD(R->gc.objects,obj);
	return obj;
}
#endif