#ifndef ELF_SOURCE_H
#define ELF_SOURCE_H

#include "base.h"

// Points at a byte range in one source participating in the compilation.
// Todo, compress this, especially with the line map!
typedef struct
{
	u32 offset;
	u32 size;
	u32 line_offset;
	u32 line_index;
}
SourceSite;

typedef struct
{
	u32 byte_start;
	u32 byte_end;
	u32 source_offset;
	u32 source_size;
	u32 line_offset;
	u32 line_index;
}
BcSourceMapEntry;

#endif
