//
// See Copyright Notice In elf.h
//

static b32 source_slice_is_valid(SourceSite site)
{
	return site.data != 0 && site.line_start != 0;
}

static u64 source_slice_column(SourceSite site)
{
	if (!source_slice_is_valid(site) || site.data < site.line_start) {
		return 0;
	}
	return 1 + (u64)(site.data - site.line_start);
}

static Source source_slice_line_end(SourceSite site, elf_StrSlice source)
{
	Source line_end = site.line_start;
	Source source_end = source.data ? source.data + source.size : 0;

	if (source_end)
	{
		while (line_end < source_end && *line_end != '\r' && *line_end != '\n') {
			++line_end;
		}
	}
	else
	{
		while (*line_end && *line_end != '\r' && *line_end != '\n') {
			++line_end;
		}
	}

	return line_end;
}

static void print_source_slice_marker(SourceSite site, elf_StrSlice source)
{
	if (!source_slice_is_valid(site)) {
		log_line(LOG_LEVEL_ERROR, "| source information could not be found");
		return;
	}

	Source line_start = site.line_start;
	Source line_end = source_slice_line_end(site, source);

	while (line_start < site.data && line_start < line_end && (*line_start == '\t' || *line_start == ' ')) {
		++line_start;
	}

	if (site.data < line_start) {
		line_start = site.line_start;
	}

	u64 column = (u64)(site.data - line_start);
	u64 line_size = (u64)(line_end - line_start);
	u32 highlight_size = site.size ? site.size : 1;

	enum { MARKER_CAPACITY = 256 };
	char marker[MARKER_CAPACITY];

	if (column >= MARKER_CAPACITY)
	{
		u64 shift = column - (MARKER_CAPACITY - 1);
		line_start += shift;
		line_size -= MIN(line_size, shift);
		column = MARKER_CAPACITY - 1;
	}

	if (line_size > column + highlight_size + 32) {
		line_size = column + highlight_size + 32;
	}

	u64 visible_highlight_size = highlight_size;
	if (visible_highlight_size > MARKER_CAPACITY - column) {
		visible_highlight_size = MARKER_CAPACITY - column;
	}
	if (visible_highlight_size > line_size - MIN(line_size, column)) {
		visible_highlight_size = line_size > column ? line_size - column : 1;
	}
	if (visible_highlight_size == 0) {
		visible_highlight_size = 1;
	}

	for (u64 i = 0; i < column; ++i) {
		marker[i] = line_start[i] == '\t' ? '\t' : ' ';
	}

	marker[column] = '^';
	for (u64 i = 1; i < visible_highlight_size; ++i) {
		marker[column + i] = '~';
	}

	log_linef(LOG_LEVEL_INFO, "| %.*s", (i32)line_size, line_start);
	log_linef(LOG_LEVEL_ERROR, "| %.*s", (i32)(column + visible_highlight_size), marker);
}
