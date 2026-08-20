#ifndef ARM64_PEEPHOLE_H
#define ARM64_PEEPHOLE_H

int arm64_bootstrap_peephole_enabled(void);
int arm64_peephole_optimize_file(const char *asm_path);

#endif /* ARM64_PEEPHOLE_H */
