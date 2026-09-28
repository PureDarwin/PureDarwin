// plain char is signed on darwin, riscv included, as arm64 darwin has it
_Static_assert((char)-1 < 0, "plain char must be signed on riscv darwin");
#ifdef __CHAR_UNSIGNED__
#error "__CHAR_UNSIGNED__ must not be defined on riscv darwin"
#endif
int signed_char_ok;
