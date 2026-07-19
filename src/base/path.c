//
// See Copyright Notice In elf.h
//

elf_PathStack elf_alloc_path_stack(elf_Arena *arena, u32 capacity)
{
	ASSERT(capacity > 0);

	elf_PathStack builder = {};
	builder.pcap = capacity;
	builder.path = elf_arena_push_zero(arena, capacity);
	builder.name = builder.path;
	return builder;
}

static void push_raw(elf_PathStack *pb, const char *text)
{
	u32 size = (u32)strlen(text);
	ASSERT(pb->pcur + size + 1 <= pb->pcap);

	memcpy(pb->path + pb->pcur, text, size);
	pb->pcur += size;
	pb->path[pb->pcur] = 0;
}

static void pb_refresh_last_segment(elf_PathStack *pb)
{
	if (pb->segs == 0) {
		pb->name = pb->path;
		return;
	}

	pb->name = pb->path + pb->pcur;
	while (pb->name > pb->path && pb->name[-1] != '\\' && pb->name[-1] != '/') {
		pb->name --;
	}
}

void elf_path_pop(elf_PathStack *pb)
{
	assert(pb->segs > 0);

	pb->segs -= 1;

	if (pb->segs == 0)
	{
		pb->pcur = 0;
		pb->path[0] = '\0';
		pb_refresh_last_segment(pb);
		return;
	}

	char *end = pb->name;
	if (end > pb->path && (end[-1] == '\\' || end[-1] == '/')) {
		end --;
	}

	pb->pcur = (int)(end - pb->path);
	pb->path[pb->pcur] = '\0';
	pb_refresh_last_segment(pb);
}


void elf_path_push(elf_PathStack *pb, const char *name)
{

	if (pb->segs) {
		push_raw(pb, "\\");
	}

	pb->segs += 1;

	int namecur = pb->pcur;

	push_raw(pb, name);

	pb->name = pb->path + namecur;
}
