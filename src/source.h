#ifndef ELF_SOURCE_H
#define ELF_SOURCE_H

#include "base.h"

typedef struct
{
	const char *data;
	u32         size;
}
BcSourceBuffer;

// Points at a byte range in one source participating in the compilation.
// source is the stable index used by tokens, AST nodes, and runtime source maps.
typedef struct
{
	// TODO(RJ): probably remove this entirely!
	u32 source;
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
