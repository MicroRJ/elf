//
// See Copyright Notice In elf.h
//


#include "string_builder.c"
// todo: we have our own assert
#include <assert.h>


enum {
	PATH_NAME = 0,
	PATH_CURRENT, // "."
	PATH_PARENT,  // ".."
};


typedef struct {
	union {
		String_Builder sb;
		struct {
			// has to match string builder
			int   pcap;
			int   pcur;
			char *path;
		};
	};

	short segs;
	char  type;
	// since name gets set each time a push or pop
	// happens it won't matter that the buffer is
	// dynamic
	char *name;
} Path_Builder;



static void pullpath(Path_Builder *pb) {
	assert(pb->segs > 0);

	pb->segs -= 1;

	// name is at '/' + 1
	pb->name -= 1;

	pb->pcur = pb->name - pb->path;
	pb->path[pb->pcur] = '\0';

	// find start or next '/'
	while (pb->name > pb->path && pb->name[-1] != '\\' && pb->name[-1] != '/') {
		pb->name --;
	}
}


static void pushpath(Path_Builder *pb, const char *name) {

	if (pb->segs) {
		sb_writechar(&pb->sb, '\\');
	}

	pb->segs += 1;

	// determine the type of thing we're putting in
	pb->type = PATH_NAME;
	if (name[0] == '.') {
		pb->type = PATH_CURRENT;
		if (name[1] == '.') {
			pb->type = PATH_PARENT;
		}
	}

	int namecur = pb->pcur;

	sb_writestr(&pb->sb, name);

	pb->name = pb->path + namecur;
}