/*
** See Copyright Notice In elf.h
** ldebug.h
** Debug Tools
*/




void lang_setasserthook(int (*hook)(elSourceInfo));
void lang_assertfn(elSourceInfo ind, char const *name, elBool expr);


#define LHERE (elSourceInfo){__FILE__,__LINE__,__func__}


#define elWITHIN(X,XMIN,XMAX) ((XMIN) <= (X) && (X) < (XMAX))


#define LASSERTALWAYS(xx) lang_assertfn(LHERE,elTOTEXT(xx),xx)


#if defined(_DEBUG)
	#define elASSERT(xx) LASSERTALWAYS(xx)
#else
	#define elASSERT(xx)
#endif


#if defined(_DEBUG)
	#define LDODEBUG(xx) do { xx; } while(0)
#else
	#define LDODEBUG(xx)
#endif


#if !defined(elNOCODE)
	#define elNOCODE elf_debugger(__FILE__" ["elTOTEXT(__LINE__)"]: internal error: unexpected code branch")
#endif


#if defined(_DEBUG)
	#define LCHECKPRINTF(FORMAT,...) ((false)?(snprintf(0,0,FORMAT,##__VA_ARGS__),0):0)
#else
	#define LCHECKPRINTF(FORMAT,...) 0
#endif



