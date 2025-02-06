
#if defined(ELF_EXPERIMENTAL_FEATURES)
		if (R->bytetracking) {
			elf_Int track = ++ M->track[module_instr];
			if (track == 64) {
				elf_Proto file;
				char *line;
				int linenum;
				file=M->files[elf_get_instr_file(M,module_instr)];
				line=M->lines[module_instr];
				elf_get_line_source_info(file.lines,line,&linenum,0);
				elf_debug_log("%s %i: %lli: %lli detected hot path",file.name,linenum,module_instr,track);
			}
		}
#endif