//
// See Copyright Notice In elf.h
//







Buf new_buffer(elf_State *S, Index min, Index max) {
	Buf buf = gcalloc(S, GC_BUF, sizeof(*buf));
	if (S) setmeta(buf, S->metatables.buffer);

	if (max == 0) max = 1;

	buf->min = min;
	buf->max = max;
	buf->mem = calloc(1, max);
	return buf;
}






static char *bufalloc(Buf buf, Index res, Index com) {

	if (buf->min + res > buf->max) {
		buf->max <<= 1;

		if(buf->min + res > buf->max) {
			buf->max = buf->min + res;
		}

		buf->mem = realloc(buf->mem, buf->max);
	}

	char *ret = buf->mem + buf->min;
	buf->min += com;
	return ret;
}






static int sb_writebuf(Stringer *sb, Buf buf) {
	sb_writetextl(sb, buf->mem, buf->min);
	return buf->min;
}





// :delete(index ?= -1, count ?= 1)
ELF_FUNCTION(l_buf_delete) {
	Buf buf = loadbuf(S, 0);

	int num = 1;

	if (nargs > 1) {
		int cur = loadint(S, 1);

		if (nargs > 2) {
			num = loadint(S, 2);
		}

		memmove(buf->mem + cur, buf->mem + cur + num, buf->min - cur - num);
	}

	buf->min -= num;
	buf->mem[buf->min] = 0;


	// return self
	loadpush(S, 0);
	return 1;
}







static char *buf_gap(Buf buf, int pos, int size) {
	memmove(buf->mem + pos + size, buf->mem + pos, buf->min - pos - size);
	return buf->mem + pos;
}




ELF_FUNCTION(l_buf_insert_char) {
	Buf buf = loadbuf(S, 0);
	int chr = loadint(S, 1);


	char *dst = bufalloc(buf, 2, 1);


	if (nargs > 2) {
		Index pos = loadint(S, 2);
		dst = buf_gap(buf, pos, 1);
	}

	*dst = chr;

	// terminate end of buffer
	buf->mem[buf->min] = 0;

	// return self
	loadpush(S, 0);
	return 1;
}






// insert a string into the buffer, if no position is
// specified, then it appends to the end
ELF_FUNCTION(l_buf_insert) {

	Buf buf = loadbuf(S, 0);
	Str str = loadstr(S, 1);
	int zstr = strl(str);
	const char *src = strt(str);

	char *dst = bufalloc(buf, zstr + 1, zstr);


	if (nargs > 2) {
		Index cur = loadint(S, 2);
		dst = buf_gap(buf, cur, zstr);
	}

	memcpy(dst, src, zstr);

	// terminate end of buffer
	buf->mem[buf->min] = 0;


	// return self
	loadpush(S, 0);
	return 1;
}












static const elf_Binding l_buffer[] = {
	{ "insert", l_buf_insert },
	{ "insert_char", l_buf_insert_char },
	{ "delete", l_buf_delete },
};