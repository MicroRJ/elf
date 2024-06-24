/*
** See Copyright Notice In elf.h
** elf-obj.c
** Objects And Values
*/


/* we have to factor in additional heuristics
for this, sometimes memory usage isn't what
boggles the GC, instead is the sheer quantity
of objects, so that's something we have to
take into account, and most of the time you
make small allocations tightly, so that's
where most of the spikes occur, of course this
allocator is too trivial as of now... we're
counting on future SMC optimizations that'll
be capable of removing redundant intermediate
heap allocations... */
#define L_GC_THRESHOLD_MIN (elInteger) MEGABYTES(4)
#define L_GC_THRESHOLD_MAX (elInteger) MEGABYTES(16)


#define L_GC_OBJNUM_THRESHOLD_MIN (elInteger) (8192*1)
#define L_GC_OBJNUM_THRESHOLD_MAX (elInteger) (8192*32)



elBool elf_value_tag_is_numeric(elObjectTag tag) {
	return (tag == TAG_NUM) || (tag == TAG_INT);
}


int elf_is_value_nil(elValue x) {
	return (x.tag == TAG_NIL) || (!elf_value_tag_is_numeric(x.tag) && (x.p == elNil));
}


elBool elf_is_object_tag(elObjectTag tag) {
	switch (tag) {
		case TAG_STR: case TAG_TAB:
		case TAG_OBJ: case TAG_CLS: {
			return elTrue;
		}
		default: return false;
	}
}


elObjectTag elf_objtotag(elObjType type) {
	switch(type) {
		case OBJ_CLOSURE: return TAG_CLS;
		case OBJ_TAB: return TAG_TAB;
		case OBJ_STRING: return TAG_STR;
		default: elf_unreachable;
	}
	return -1;
}


elf_api elValue elf_valtab(elTable *tab) {
	elValue v = LITC(elValue){TAG_TAB};
	v.x_tab = tab;
	return v;
}


elf_api elValue elf_valbid(elBinding c) {
	elValue v = LITC(elValue){TAG_BID};
	v.c = c;
	return v;
}


elf_api elValue elf_valsys(elHandle h) {
	elValue v = LITC(elValue){TAG_SYS};
	v.h = h;
	return v;
}


elf_api elValue elf_valstr(elString *s) {
	elValue v = LITC(elValue){TAG_STR};
	v.s = s;
	return v;
}


elf_api elValue elf_valcls(elClosure *f) {
	elValue v = LITC(elValue){TAG_CLS};
	v.f = f;
	return v;
}


elf_api elValue elf_valint(elInteger i) {
	elValue v = (elValue){TAG_INT};
	v.i = i;
	return v;
}


elf_api elValue elf_valnum(elNumber n) {
	elValue v = (elValue){TAG_NUM};
	v.n = n;
	return v;
}


void elf_collect(elState *fs);


void elf_gcpause(elState *S) {
	S->memory.flags = elTrue;
}


void elf_gcresume(elState *S) {
	S->memory.flags = elFalse;
}


void *elf_allocate_new_object(elState *R, elObjType type, elInteger tell) {
	/* this is temporary! */
	if (R != 0) {
		R->memory.allocated += tell;
		if (!R->memory.flags) {
			if (R->memory.threshold <= 0) {
				R->memory.threshold = L_GC_THRESHOLD_MIN;
			}
			if (elf_xarray_length(R->memory.articles) > L_GC_OBJNUM_THRESHOLD_MAX) {
				elf_collect(R);
			} else
			if (R->memory.allocated >= R->memory.threshold) {
				R->memory.threshold *= 2;
				if (R->memory.threshold > L_GC_THRESHOLD_MAX) {
					R->memory.threshold = L_GC_THRESHOLD_MAX;
				}
				elf_collect(R);
			}
		}
	}

	elObject *obj = elf_clear_alloc(lHEAP,tell);
 	elf_ensure(obj->color == GC_BLACK);
	obj->type = type;
	obj->tell = tell;

	if (R != 0) {
		elf_xarray_add(R->memory.articles,obj);
	}
	return obj;
}


void elf_remobj(elState *fs, elInteger i) {
	elObject **articles = fs->memory.articles;
	if (articles == 0) return;
	elInteger n = elf_xarray_length(articles);
	elf_ensure(i >= 0 && i < n);
	articles[i] = articles[n-1];
	((elArray*)(articles))[-1].min --;
}


void elf_delobj(elState *R, elObject *obj) {
	if (obj != elNil) {
		R->memory.allocated -= obj->tell;
		if (obj->type == OBJ_TAB) {
			elf_dealloc_table((elTable*)obj);
		}
		elf_dealloc(lHEAP,obj);
	}
}

elBool elf_mark_value(elValue *v);
elInteger elf_marktab(elTable *table);


/* todo: remove this function */
elInteger elf_markcl(elClosure *cl) {
	elInteger n = 0, k;
	for (k=0; k<cl->fn.zcache; ++k) {
		n += elf_mark_value(&cl->enclosure[k]);
	}
	return n;
}


/* todo: remove this function */
elInteger elf_marktab(elTable *table) {
	if (table->ntotal > 1024) {
		elf_logdebug("marked high count table: %lli/%lli"
		, table->nslots,table->ntotal);
	}
	elInteger n = 0, k;
	for (k=0; k<table->ntotal; ++k) {
		n += elf_mark_value(&table->slots[k].k);
	}
	elf_xarray_foreachi(table->v) {
		n += elf_mark_value(&table->v[i]);
	}
	return n;
}


elBool elf_mark_object(elObject *obj) {
	if (obj == elNil || obj->color != GC_WHITE) {
		return obj->color == GC_BLACK;
	}
	obj->color = GC_BLACK;
	if (obj->type == OBJ_CLOSURE) {
		return 1 + elf_markcl((elClosure*)obj);
	}
	if (obj->type == OBJ_TAB) {
		return 1 + elf_marktab((elTable*)obj);
	}
	return 1;
}


elBool elf_mark_value(elValue *v) {
	return elf_is_object_tag(v->tag) ? elf_mark_object(v->x_obj) : elFalse;
}


elInteger elf_mark_everything(elState *R) {
	elInteger num = elf_mark_object((elObject*)R->M->globals);
	elValue *val;
	for (val = R->stk; val < R->top; ++ val) {
		num += elf_mark_value(val);
	}
	return num;
}


/* todo: iterate backwards instead */
void elf_collect(elState *R) {
	elInteger num = elf_mark_everything(R);
#if defined(LLOGGING)
	elInteger time_ = elf_clocktime();
	elInteger ngc = elf_xarray_length(R->memory.articles);
	elInteger tbf = ngc-num;
	elInteger nwo = 0;
	elf_logdebug("tbf: %lli/%lli -> %lli",tbf,ngc,num);
#endif

	for (int i = 0; i < elf_xarray_length(R->memory.articles); i ++) {
		elObject *it = R->memory.articles[i];
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
			for (j = i; j < elf_xarray_length(R->memory.articles); ++ j) {
				if (R->memory.articles[j].color != GC_WHITE) {
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
			elf_delobj(R,it);
			elf_remobj(R,i);
			-- i;
		}
	#if defined(LLOGGING)
		else nwo ++;
	#endif
	}

	elf_logdebug("	(%f) => leaked: %lli, %lli, %lli"
	, elf_timediffms(time_),tbf,nwo,num);
}
