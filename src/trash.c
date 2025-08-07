
// #if defined(ELF_EXPERIMENTAL_FEATURES)
// 		if (R->bytetracking) {
// 			elf_Integer track = ++ M->track[module_instr];
// 			if (track == 64) {
// 				elf_Proto file;
// 				char *line;
// 				int linenum;
// 				file=M->files[elf_query_file_for_instr(M,module_instr)];
// 				line=M->lines[module_instr];
// 				get_source_info(file.lines,line,&linenum,0);
// 				elf_debug_log("%s %i: %lli: %lli detected hot path",file.name,linenum,module_instr,track);
// 			}
// 		}
// #endif