/*
** See Copyright Notice In elf.h
** lgc.c
** Garbage Collector
*/



/* we have to factor in additional heuristics
for this, sometimes memory usage isn't what
boggles the GC, instead is the sheer quantity
of objects, so that's something we have to
take into account, and most of the time you
make small allocations tightly, so that's
where most of the spikes occur, of course this
allocator is too trivial as of now... */
#define L_GC_THRESHOLD_MIN (elInteger) MEGABYTES(4)
#define L_GC_THRESHOLD_MAX (elInteger) MEGABYTES(16)


#define L_GC_OBJNUM_THRESHOLD_MIN (elInteger) (8192*1)
#define L_GC_OBJNUM_THRESHOLD_MAX (elInteger) (8192*32)


void elf_collect(elState *fs);


void elf_gcpause(elState *fs) {
	fs->gcflags = ltrue;
}


void elf_gcresume(elState *fs) {
	fs->gcflags = lfalse;
}


void *elf_newobj(elState *R, elf_objty type, elInteger tell) {
	/* this is temporary! */
	if (R != 0) {
		R->gcmemory += tell;
		if (!R->gcflags) {
	if (R->gcthreshold <= 0) {
		R->gcthreshold = L_GC_THRESHOLD_MIN;
	}
	if (elf_varlen(R->gc) > L_GC_OBJNUM_THRESHOLD_MAX) {
		elf_collect(R);
	} else
	if (R->gcmemory >= R->gcthreshold) {
		R->gcthreshold *= 2;
		if (R->gcthreshold > L_GC_THRESHOLD_MAX) {
			R->gcthreshold = L_GC_THRESHOLD_MAX;
		}
		elf_collect(R);
	}
		}
	}

	elObject *obj = elf_clearalloc(lHEAP,tell);
 	elf_ensure(obj->gccolor == GC_BLACK);
	obj->type = type;
	obj->tell = tell;
	LDODEBUG(
		obj->headtrap = FLYTRAP;
		obj->tailtrap = FLYTRAP;
	);
	if (R != 0) {
		elf_varadd(R->gc,obj);
		// LDODEBUG(elf_arrfori(R->gc) {
		// 	if (R->gc[i]->headtrap != FLYTRAP) elf_unreachable;
		// 	if (R->gc[i]->tailtrap != FLYTRAP) elf_unreachable;
		// });
	}
	return obj;
}


void elf_remobj(elState *fs, elInteger i) {
	elObject **gc = fs->gc;
	if (gc == 0) return;
	elInteger n = elf_varlen(gc);
	elf_ensure(i >= 0 && i < n);
	gc[i] = gc[n-1];
	((elf_var*)(gc))[-1].min --;
}


void elf_delobj(elState *R, elObject *obj) {
	if (obj != elNIL) {
		R->gcmemory -= obj->tell;
		if (obj->type == OBJ_TAB) {
			elf_deltab((elTable*)obj);
		}
		elf_dealloc(lHEAP,obj);
	}
}

elBool elf_markval(elValue *v);
elInteger elf_marktab(elTable *table);


/* todo: remove this function */
elInteger elf_markcl(elClosure *cl) {
	elInteger n = 0, k;
	for (k=0; k<cl->fn.ncaches; ++k) {
		n += elf_markval(&cl->caches[k]);
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
		n += elf_markval(&table->slots[k].k);
	}
	elf_arrfori(table->v) {
		n += elf_markval(&table->v[i]);
	}
	return n;
}


elBool elf_markobj(elObject *obj) {
	if (obj == elNIL || obj->gccolor != GC_WHITE) {
		return obj->gccolor == GC_BLACK;
	}
	obj->gccolor = GC_BLACK;
	if (obj->type == OBJ_CLOSURE) {
		return 1 + elf_markcl((elClosure*)obj);
	}
	if (obj->type == OBJ_TAB) {
		return 1 + elf_marktab((elTable*)obj);
	}
	return 1;
}


elBool elf_markval(elValue *v) {
	return elf_tagisobj(v->tag) ? elf_markobj(v->x_obj) : lfalse;
}


elInteger elf_markall(elState *R) {
	elInteger num = elf_markobj((elObject*)R->M->g);
	for (elValue *val = R->stk; val < R->top; ++ val) {
		num += elf_markval(val);
	}
	return num;
}


void elf_collect(elState *R) {
	elInteger num = elf_markall(R);
#if defined(LLOGGING)
	elInteger time_ = elf_clocktime();
	elInteger ngc = elf_varlen(R->gc);
	elInteger tbf = ngc-num;
	elInteger nwo = 0;
	elf_logdebug("tbf: %lli/%lli -> %lli",tbf,ngc,num);
#endif

	for (int i = 0; i < elf_varlen(R->gc); i ++) {
		elObject *it = R->gc[i];
		LDODEBUG(
			if (it->headtrap != FLYTRAP) elf_unreachable;
			if (it->tailtrap != FLYTRAP) elf_unreachable;
		);

		if (it == elNIL) continue;
		if (it->gccolor == GC_RED) {
			elf_debugger("internal error: gc failed");
		}
		if (it->gccolor == GC_BLACK) {
	#if defined(LLOGGING)
			num --;
	#endif
			it->gccolor = GC_WHITE;
		} else
		if (it->gccolor == GC_WHITE) {
	if (it == (elObject*) R->M->globals) {
		elf_debugger("internal error: gc failed");
	}
			it->gccolor = GC_RED;
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
	, elf_timediffs(time_),tbf,nwo,num);
}
