#ifndef ELF_SOURCE_H
#define ELF_SOURCE_H

typedef struct
{
	const char *data;
	u32         size;
}
SourceBuffer;

// Points at a byte range in one source participating in the compilation.
// source is the stable index used by tokens, AST nodes, and runtime source maps.
typedef struct
{
	u32 source;
	u32 offset;
	u32 size;
	u32 line_offset;
	u32 line_index;
}
SourceSite;

typedef struct
{
	u32        byte_start;
	u32        byte_end;
	SourceSite site;
}
SourceMapEntry;

#endif
