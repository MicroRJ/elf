/*
** See Copyright Notice In elf.h
** elf-cli.c
** Helper for parsing command line
** arguments.
*/


typedef struct elf_cliopts {
	char *filename;
	int 	logging;
	int 	dump;
	char *dumpfilename;
} elf_cliopts;



int elf_loadcliopts(elf_cliopts *cl, int n, char **c);



int elf_loadcliopts(elf_cliopts *cli, int n, char **c) {
	int i = 1;
	int NOP = 0; (void) NOP;
	#define HAS() (i < n)
	#define NEXT() (i ++)
	#define ISOP(N) (!strncmp(c[i],"--"N,2+sizeof(N)-1))
	#define GETOP(N) (ISOP(N) ? NEXT() : NOP)
	#define ISARG() (strncmp(c[i],"--",2))
	#define GETARG(E) (ISARG() ? c[NEXT()] : E)
	#define FAIL(S) (printf("error: "S),elNIL)
	#define SETFIELD(N,V) (cli->N = V)


#define OPLIST(_) \
	_("help", "displays command line interface help",{})\
	_("logging", "log bytecode instructions as they execute (only in debug mode)",{SETFIELD(logging,elTrue);})\
	_("dump", "[filename] whether to dump the resulting module (when program exits)",\
	{	SETFIELD(dump,elTrue);\
		SETFIELD(dumpfilename,GETARG(FAIL("dump: missing filename, tip: you can use [stdout]"))); })

	if (!HAS()) goto help;

	if (ISARG()) {
		cli->filename = GETARG(0);
		printf("using file: %s\n",cli->filename);
	}
	while (HAS()) {
		if (GETOP("help")) {
			help:
			printf("usage: --op [args...]\n");
#define DEFOP(N,I,A) printf("  --%s: %s\n", N,I);
			OPLIST(DEFOP)
#undef DEFOP
			return 1;
		}
#define DEFOP(N,I,A) if (GETOP(N)) { printf("%s:\n",N); A; continue; };
		OPLIST(DEFOP)
#undef DEFOP
		printf("error: unrecognized verb: %s\n",c[i]);
		break;
	}

	return 0;
}
