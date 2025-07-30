/*
** See Copyright Notice In elf.h
** elf.c
*/


#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4100)
#pragma warning(disable:4245)
#pragma warning(disable:4057)
#pragma warning(disable:4189)
#pragma warning(disable:4201)
#pragma warning(disable:4244)
#pragma warning(disable:4267)
#pragma warning(disable:4389)
#pragma warning(disable:4996)
#endif
/* both __clang__ and _MSC_VER can be defined
at the same time when using clang-cl */
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wparentheses-equality"
#pragma clang diagnostic ignored "-Wnon-literal-null-conversion"
#pragma clang diagnostic ignored "-Wmissing-braces"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wmissing-braces"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wpointer-sign"
#pragma clang diagnostic ignored "-Wunused-function"
#endif



STATIC_ASSERT(sizeof(elf_Int)==sizeof(elf_i64));
STATIC_ASSERT(sizeof(elf_Num)==sizeof(elf_f64));
STATIC_ASSERT(sizeof(elf_i64)==8);
STATIC_ASSERT(sizeof(elf_f64)==8);




// #include "alloc.h"
// #include "array.c"
// #include "log.h"
// #include "text.h"



#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__clang__)
#pragma clang diagnostic pop
#endif
