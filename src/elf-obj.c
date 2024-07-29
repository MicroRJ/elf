/*
** See Copyright Notice In elf.h
** elf-obj.c
** Objects And Values
*/


elBool elf_isnumeric(elValueTag tag) {
	return (tag == TAG_NUM) || (tag == TAG_INT);
}

elNumber elf_tonum(elValue v) {
	return v.tag == TAG_INT ? (elNumber) v.x_int : v.x_num;
}

elInteger elf_toint(elValue v) {
	return v.tag == TAG_NUM ? (elInteger) v.x_num : v.x_int;
}


int elf_isnil(elValue x) {
	return (x.tag == TAG_NIL) || (!elf_isnumeric(x.tag) && (x.p == elNil));
}


elBool elf_isobj(elValueTag tag) {
	return tag > TAG_OBJ;
	// switch (tag) {
	// 	case TAG_STR: case TAG_TAB:
	// 	case TAG_OBJ: case TAG_CLS: {
	// 		return elTrue;
	// 	}
	// 	default: return elFalse;
	// }
}


elValueTag elf_object_type_to_value_tag(elObjType type) {
	switch(type) {
		case OBJ_STRING:  return TAG_STR;
		case OBJ_CLOSURE: return TAG_CLS;
		case OBJ_TAB:     return TAG_TAB;
		default: elNOCODE;
	}
	return -1;
}


elf_api elValue elf_tab(elTable *tab) {
	elValue v = LITC(elValue){TAG_TAB};
	v.x_tab = tab;
	return v;
}


elf_api elValue elf_obj(elObject *obj) {
	elValue v = LITC(elValue){TAG_OBJ};
	v.x_obj = obj;
	return v;
}


elf_api elValue elf_binding_value(elBinding c) {
	elValue v = LITC(elValue){TAG_BID};
	v.c = c;
	return v;
}


elf_api elValue elf_handle_value(elHandle h) {
	elValue v = LITC(elValue){TAG_SYS};
	v.x_sys = h;
	return v;
}


elf_api elValue elf_string_value(elString *s) {
	elValue v = LITC(elValue){TAG_STR};
	v.x_str = s;
	return v;
}


elf_api elValue elf_closure_value(elClosure *f) {
	elValue v = LITC(elValue){TAG_CLS};
	v.x_cls = f;
	return v;
}


elf_api elValue elf_integer_value(elInteger i) {
	elValue v = (elValue){TAG_INT};
	v.x_int = i;
	return v;
}


elf_api elValue elf_number_value(elNumber n) {
	elValue v = (elValue){TAG_NUM};
	v.x_num = n;
	return v;
}


elf_api elValue elf_nil_value() {
	elValue v = (elValue){TAG_NIL};
	v.x_int = 0;
	return v;
}


/*
** If an object is white it marks it black
** and marks its references.
** Returns the number of objects that have
** been uniquely marked. If black, 0.
** If white, 1 + references.
*/
elInteger elf_mark_object(elObject *obj) {
	elf_ensure(obj != 0);
	elf_ensure(obj->color != GC_RED);
	/* black object simply means it was
	already marked and we found another
	path to it, since the object was
	already accounted for, return 0 */
	if (obj->color == GC_BLACK) {
		return 0;
	}
	elInteger num = 1;
	/* Here we check whether the object
	is explicitly white, because there
	are other colors that we don't want
	to get rid of */
	if (obj->color == GC_WHITE) {
		obj->color = GC_BLACK;
	}
	if (obj->metatable) {
		num += elf_mark_object((elObject*)obj->metatable);
	}
	if (obj->type == OBJ_CLOSURE) {
		elClosure *cls = (elClosure*) obj;
		elInteger k;
		for (k=0; k<cls->prototype.zcache; ++k) {
			if (elf_isobj(cls->enclosure[k].tag)) {
				num += elf_mark_object(cls->enclosure[k].x_obj);
			}
		}
	} else if (obj->type == OBJ_TAB) {
		elTable *table = (elTable*) obj;
		elValue *array = table->array;
		elEntry *slots = table->slots;
		elInteger k;
		for (k=0; k<table->ntotal; ++k) {
			if (elf_isobj(slots[k].key.tag)) {
				num += elf_mark_object(slots[k].key.x_obj);
			}
		}
		for (k=0; k<array_length(array); ++k) {
			if (elf_isobj(array[k].tag)) {
				num += elf_mark_object(array[k].x_obj);
			}
		}
	}

	return num;
}


elInteger elf_unmark_objects(elState *R) {
	elInteger result = 0;
	elInteger i;
	elObject **objects = R->memory.objects;
	for (i = 0; i < array_length(objects); i ++) {
		elObject *it = objects[i];
		switch (it->color) {
			case GC_RED:
			elf_debugger("internal error: gc failed");
			break;
			case GC_BLACK: {
				it->color = GC_WHITE;
				result += 1;
			} break;
			default: break;
		}
	}
	return result;
}


elInteger elf_hold_phase(elState *R) {
	elf_ensure(R->memory.phase == elGC_PHASE_HOLD);
	R->memory.phase ^= 1;

	elInteger num = elf_mark_object((elObject*)R->M->globals);
	elValue *val;
	for (val=R->stack; val<R->stack_top; ++val) {
		if (elf_isobj(val->tag)) {
			num += elf_mark_object(val->x_obj);
		}
	}
	return num;
}


elInteger elf_free_phase(elState *R) {
	elf_ensure(R->memory.phase == elGC_PHASE_FREE);
	R->memory.phase ^= 1;


	elObject **new_objects = R->memory.new_objects;
	elObject **objects = R->memory.objects;
	if (new_objects) {
		elf_vararr(new_objects).min = 0;
	}
	elInteger n = 0;
	elInteger k;
	for (k=0; k<array_length(objects); ++ k) {
		elObject *it = objects[k];
		if (it == 0 || it->color == GC_RED) {
			elf_throw(R,NO_BYTE,"internal error, GC failed");
		}
		if (it->color == GC_BLACK) {
			it->color = GC_WHITE;
			/* todo: instead simply ensure 'new_objects' is big enough */
			elf_varadd(new_objects,it);
		} else if (it->color == GC_WHITE) {
			n += 1;
			it->color = GC_RED;
			R->memory.allocated -= it->tell;
			if (it->type == OBJ_TAB) {
				elf_dealloc_table((elTable*)it);
			}
			elf_dealloc(lHEAP,it);
		} else n += 1;
	}
	R->memory.objects = new_objects;
	R->memory.new_objects = objects;
	return n;
}


void elf_trigger_collection_cycle(elState *R) {
	elInteger time_ = elf_clocktime();
	elInteger num_marked = elf_hold_phase(R);
	elInteger num_objects = array_length(R->memory.objects);
	elInteger num_to_collect = num_objects - num_marked;
	elf_debug_log("GC: %lli - %lli -> %lli, (total - marked = expected)",num_objects,num_to_collect,num_marked);
	num_to_collect -= elf_free_phase(R);
	elf_debug_log("	(%f) => leaked: %lli"
	, elf_timediffms(time_),num_to_collect);
}


void elf_collect(elState *R) {
	if (R->memory.paused) {
		return;
	}
	if (R->memory.threshold <= 0) {
		R->memory.threshold = elGC_MEM_THRESHOLD_MIN;
	}
	if (array_length(R->memory.objects) > elGC_OBJ_THRESHOLD_MAX) {
		elf_trigger_collection_cycle(R);
		// if (array_length(R->memory.objects) > elGC_OBJ_THRESHOLD_MAX) {
		// 	elf_throw(R,NO_BYTE,elf_tpf("out of memory, %lli objects",array_length(R->memory.objects)));
		// }
	} else if (R->memory.allocated > R->memory.threshold) {
		R->memory.threshold *= 2;
		if (R->memory.threshold > elGC_MEM_THRESHOLD_MAX) {
			R->memory.threshold = elGC_MEM_THRESHOLD_MAX;
		}
		elf_trigger_collection_cycle(R);
		if (R->memory.allocated > R->memory.threshold) {
			elf_throw(R,NO_BYTE,elf_tpf("out of memory, %lliMB allocated",R->memory.allocated / MEGABYTES(1)));
		}
	}
}


void *elf_new_object(elState *R, elObjType type, elInteger tell) {
	R->memory.allocated += tell;
	elf_collect(R);

	elObject *obj = elf_clear_alloc(lHEAP,tell);
	obj->color = (elGCColor) R->memory.phase;
	if (obj->color != GC_WHITE) {
		elf_throw(R,NO_BYTE,"object allocation out of phase");
	}
	obj->type  = type;
	obj->tell  = tell;

	elf_varadd(R->memory.objects,obj);
	return obj;
}