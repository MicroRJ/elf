//
// See Copyright Notice In elf.h
//




#define getgc(S) (&(S)->gc)




void gccheck(elf_State *S);



int elf_gcstate(elf_State *S, int state) {
	int prior = S->gc.state;
	if (state != ELF_GC_GETSTATE) {
		S->gc.state = state;
	}
	return prior;
}



static void *gcalloc(elf_State *S, GCType type, int size) {
	GCState *gc = getgc(S);

	gc->memcounter += size;

	if (gc->state == ELF_GC_ACTIVE) {
		gccheck(S);
	}

	Ref ref = calloc(size, 1);
	ref->status = 0;
	ref->type   = type;
	ref->size   = size;

	// this is pretty accurate regardless
	ref->debugsrc = S->byte;

	gc->tail->next = ref;
	gc->tail = ref;

	gc->objcounter ++;
	return ref;
}





int gcfree(elf_State *S) {
	GCState *gc = getgc(S);

	int objcounter = 0;
	int freecounter = 0;

	Ref iter, next=0, tail=&gc->head;

	for (iter=gc->head.next; iter; iter=next) {
		next=iter->next;

		if (iter->status & NODE_REACHABLE) {
			iter->status &= ~NODE_REACHABLE;
			tail = iter;
			objcounter ++;
		}
		else {
			freecounter ++;
			tail->next = next;

			gc->memcounter -= iter->size;

#if !defined(_GC_IS_BROKEN)
			{
				if (iter->type == GC_TAB) {
					_table_freeinternalmemory((Tab) iter);
				}
				free(iter);
			}
#else
			{
				iter->status |= NODE_DEBUGTRAP;
			}
#endif
		}
	}
	gc->tail = tail;
	gc->objcounter = objcounter;
	return freecounter;
}








static int gcmarkstack(elf_State *S)
{
	int counter = 0;
	for (V *ptr = S->stack; ptr < S->stack_ptr; ++ ptr) {

		if (tisobject(ptr->tag)) {
			checktrap(S, ptr->x_obj);

			counter += markreachable(S, ptr->x_obj);

		}
	}
	return counter;
}






static int gccycle(elf_State *S) {
	// Time time = prof_get_time();

	int markcounter = gcmarkstack(S);
	int freecounter = gcfree(S);

	// elf_f64 took = prof_time_diff_ms(time);
	// elf_ldebug("marked: %i, freed: %i, took: %.4fMS", markcounter, freecounter, took);
	return freecounter;
}



void gccheck(elf_State *S) {
	GCState *gc = getgc(S);

	int num_objects, num_collected;
	num_objects = gc->objcounter;

	if (num_objects > gc->objthreshold) {

		num_collected = gccycle(S);

		ASSERT(num_collected <= num_objects);
		gc->objthreshold += GC_OBJ_THRESHOLD_MIN - num_collected;

	} else if (gc->memcounter > gc->memthreshold) {

		gc->memthreshold <<= 1;
		if (gc->memthreshold > GC_MEM_THRESHOLD_MAX) {
			gc->memthreshold = GC_MEM_THRESHOLD_MAX;
		}

		num_collected = gccycle(S);
		ASSERT(num_collected <= num_objects);

		if (gc->memcounter > gc->memthreshold) {
			reporterrorf(S, NO_BYTE, "out of memory, %lliMB allocated, %lliMB threshold"
			, gc->memcounter / MEGABYTES(1)
			, gc->memthreshold / MEGABYTES(1));
		}
	}
}