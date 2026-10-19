//
// See Copyright Notice In elf.h
//

typedef struct elf_State elf_State;
typedef struct elf_Prototype elf_Prototype;
typedef struct IrModule IrModule;

elf_Prototype *bc_emit_module(elf_State *target, IrModule *module);
