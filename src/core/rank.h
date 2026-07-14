//
// See Copyright Notice In elf.h
//

#ifndef ELF_CORE_RANK_H
#define ELF_CORE_RANK_H

typedef struct
{
	i64       rank;
	elf_Value value;
}
RankValue;
STATIC_ASSERT(sizeof(RankValue) == 24);

static void rank_values(RankValue *start, RankValue *end);

#endif
