/*
** See Copyright Notice In elf.h
** elf-obj.c
** Objects And Values
*/


/* this collector is dog ... */
#define elGC_MEM_THRESHOLD_MIN (elInteger) MEGABYTES(4)
#define elGC_MEM_THRESHOLD_MAX (elInteger) MEGABYTES(64)

#define elGC_OBJ_THRESHOLD_MIN (elInteger) (8192*1)
#define elGC_OBJ_THRESHOLD_MAX (elInteger) (8192*32)

void elf_collect(elState *fs);
elBool elf_mark_value(elValue *v);
elInteger elf_mark_table(elTable *table);

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
	switch (tag) {
		case TAG_STR: case TAG_TAB:
		case TAG_OBJ: case TAG_CLS: {
			return elTrue;
		}
		default: return elFalse;
	}
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


elf_api elValue elf_table_value(elTable *tab) {
	elValue v = LITC(elValue){TAG_TAB};
	v.x_tab = tab;
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


void *elf_new_object(elState *R, elObjType type, elInteger tell) {
	/* this is temporary! */
	if (R != 0) {
		R->memory.allocated += tell;
		if (R->memory.paused) {
			goto allocate;
		}
		if (R->memory.threshold <= 0) {
			R->memory.threshold = elGC_MEM_THRESHOLD_MIN;
		}
		if (elf_xarray_length(R->memory.objects) > elGC_OBJ_THRESHOLD_MAX) {
			elf_collect(R);
			if (elf_xarray_length(R->memory.objects) > elGC_OBJ_THRESHOLD_MAX) {
				elf_throw(R,NO_BYTE,"out of memory");
			}
		} else if (R->memory.allocated > R->memory.threshold) {
			R->memory.threshold *= 2;
			if (R->memory.threshold > elGC_MEM_THRESHOLD_MAX) {
				R->memory.threshold = elGC_MEM_THRESHOLD_MAX;
			}
			elf_collect(R);
			if (R->memory.allocated > R->memory.threshold) {
				elf_throw(R,NO_BYTE,"out of memory");
			}
		}
	}

	allocate:

	elObject *obj = elf_clear_alloc(lHEAP,tell);
	elf_ensure(obj->color == GC_BLACK);
	obj->type = type;
	obj->tell = tell;

	if (R != 0) {
		elf_xarray_add(R->memory.objects,obj);
	}

	// fprintf(_logging_io,"++ object: %p %s\n"
	// , obj, tag2s[elf_object_type_to_value_tag(type)]);

	#if defined(elMEMORY_DEBUGGING)
	if (R != 0) {
		int fileid = elf_find_file_info_by_byte(R->M,R->byte);
		if (fileid != -1) {
			elFileInfo *file = &R->M->files[fileid];
			elFileLine line = elf_get_line_for_byte(R->M,R->byte);
			int linenum;
			char *lineloc;
			elf_get_line_location_info(file->contents,line,&linenum,&lineloc);
			fprintf(_logging_io," - %i:%i", linenum,(int) (line-lineloc));
		}
	}
	#endif

	return obj;
}


void elf_remove_object(elState *fs, elInteger i) {
	elObject **objects = fs->memory.objects;
	if (objects == 0) return;
	elInteger n = elf_xarray_length(objects);
	elf_ensure(i >= 0 && i < n);
	objects[i] = objects[n-1];
	((elArray*)(objects))[-1].min --;
}


void elf_collect_object(elState *R, elObject *obj) {
	if (obj != 0) {
		R->memory.allocated -= obj->tell;

		if (obj->type == OBJ_TAB) {
			elf_dealloc_table((elTable*)obj);
		}
		// fprintf(_logging_io,"-- object: %p %s"
		// , obj, tag2s[elf_object_type_to_value_tag(obj->type)]);
	#if 0
		int fileid = elf_find_file_info_by_byte(R->M,R->byte);
		if (fileid != -1) {
			// __debugbreak();
			elFileInfo *file = &R->M->files[fileid];
			elFileLine line = elf_get_line_for_byte(R->M,R->byte);
			int linenum;
			char *lineloc;
			elf_get_line_location_info(file->contents,line,&linenum,&lineloc);
			// fprintf(_logging_io," - %i:%i", linenum,(int) (lineloc-line));
		}
	#endif
		// fprintf(_logging_io,"\n");



		elf_dealloc(lHEAP,obj);
	}
}


elInteger elf_mark_closure(elClosure *cls) {
	elInteger n = 0, k;
	for (k=0; k<cls->fn.zcache; ++k) {
		n += elf_mark_value(&cls->enclosure[k]);
	}
	return n;
}


elInteger elf_mark_table(elTable *table) {
	if (table->ntotal > 1024) {
		elf_debug_log("marked high count table: %lli/%lli",table->nslots,table->ntotal);
	}
	elInteger n = 0, k;
	for (k=0; k<table->ntotal; ++k) {
		n += elf_mark_value(&table->slots[k].k);
	}
	elf_xarray_foreachi(table->array) {
		n += elf_mark_value(&table->array[i]);
	}
	return n;
}

/* todo: are pink objects being handled properly here? */
elInteger elf_mark_object(elObject *obj) {
	elf_ensure(obj != 0);
	elInteger result = 0;
	if (obj->color == GC_BLACK) {
		return 1;
	}
	if (obj->color == GC_WHITE) {
		obj->color = GC_BLACK;
		result = 1;
	}
	if (obj->metatable != 0) {
		elf_mark_object((elObject*)obj->metatable);
	}
	if (obj->type == OBJ_CLOSURE) {
		result += elf_mark_closure((elClosure*)obj);
	} else if (obj->type == OBJ_TAB) {
		result += elf_mark_table((elTable*)obj);
	}

	return result;
}


elBool elf_mark_value(elValue *v) {
	return elf_isobj(v->tag) ? elf_mark_object(v->x_obj) : elFalse;
}


elInteger elf_mark_everything(elState *R) {
	elInteger num = elf_mark_object((elObject*)R->M->globals);
	elf_ensure(R->top >= R->call->base + (R->call->cl != 0 ? R->call->cl->fn.zstack : 0));
	elValue *val;
	for (val = R->stk; val < R->top; ++ val) {
		num += elf_mark_value(val);
	}
	return num;
}


elInteger elf_unmark_objects(elState *R) {
	elInteger result = 0;
	elInteger i;
	elObject **objects = R->memory.objects;
	for (i = 0; i < elf_xarray_length(objects); i ++) {
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


void elf_collect(elState *R) {
	elInteger num = elf_mark_everything(R);
#if defined(LLOGGING)
	elInteger time_ = elf_clocktime();
	elInteger ngc = elf_xarray_length(R->memory.objects);
	elInteger tbf = ngc-num;
	elInteger nwo = 0;
	elf_debug_log("tbf: %lli/%lli -> %lli",tbf,ngc,num);
#endif

	/* todo: iterate backwards instead */
	for (int i = 0; i < elf_xarray_length(R->memory.objects); i ++) {
		elObject *it = R->memory.objects[i];
		if (it == elNil) continue;
		if (it->color == GC_RED) {
			elf_debugger("internal error: gc failed");
		}
		if (it->color == GC_BLACK) {
	#if defined(LLOGGING)
			num --;
	#endif
			it->color = GC_WHITE;
		} else
		if (it->color == GC_WHITE) {
			#if 0
			int j;
			for (j = i; j < elf_xarray_length(R->memory.objects); ++ j) {
				if (R->memory.objects[j].color != GC_WHITE) {
					break;
				}
			}
			#endif

			if (it == (elObject*) R->M->globals) {
				elf_debugger("internal error: gc failed");
			}
			it->color = GC_RED;
	#if defined(LLOGGING)
			tbf --;
	#endif
			/* todo: instead of doing it this way, remove
			objects in large ranges */
			elf_collect_object(R,it);
			elf_remove_object(R,i);
			-- i;
		}
	#if defined(LLOGGING)
		else nwo ++;
	#endif
	}

	elf_debug_log("	(%f) => leaked: %lli, %lli, %lli"
	, elf_timediffms(time_),tbf,nwo,num);
}
