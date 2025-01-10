/*
** See Copyright Notice In elf.h
** error.h
** elf_Error Codes
*/


#define LERROR "error.h"


#if !defined(ERROR_XITEM)

#define FAILED(err) ((err) != Error_None)
#define PASSED(err) ((err) == Error_None)

#define ERNAME(xx) (lErrorNames[xx])

enum {
	Error_None = 0,

#define ERROR_XITEM(NAME,DESC) Error_##NAME,
	#include LERROR
#undef ERROR_XITEM

};


GLOBAL char const *lErrorNames[] = {
	"No elf_Error",

#define ERROR_XITEM(NAME,DESC) DESC,
	#include LERROR
#undef ERROR_XITEM

};

#else
ERROR_XITEM(AsssertionTriggered,            "Assertion Triggered")
ERROR_XITEM(InternalError,                  "Internal elf_Error")
ERROR_XITEM(OutOfMemory,                    "Out of Memory")
ERROR_XITEM(InvalidArguments,               "Invalid Arguments")
ERROR_XITEM(FileNameIsInvalid,              "File Name is Invalid")
ERROR_XITEM(FileNotFound,                   "File Was Not Found")
ERROR_XITEM(CouldNotLoadLibrary,            "Could Not Load Library")
ERROR_XITEM(CouldNotOpenFile,               "Could Not Open File")
ERROR_XITEM(CouldNotReadEntireFile,         "Could Not Read Entire File")
ERROR_XITEM(CouldNotWriteEntireFile,        "Could Not Write Entire File")
ERROR_XITEM(CouldNotReadFile,               "Could Not Read Entire File")
ERROR_XITEM(CouldNotLoadFile,               "Could Not Load Entire File")
ERROR_XITEM(Halted,                         "Halted")
ERROR_XITEM(Breaked,                        "Breaked")
#endif

