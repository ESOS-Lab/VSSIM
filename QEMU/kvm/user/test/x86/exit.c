#include "runtime.h"

void exit(unsigned code)
{
	asm volatile("out %al, %dx" : : "a"(code), "d"(0xf4));
	asm volatile("cli; hlt");
}
