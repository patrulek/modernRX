#pragma once

/*
* JIT Compiler for RandomX's Superscalar programs.
* Compiler uses AVX2 instructions. 
*/

#include <vector>


namespace modernRX {
    struct SuperscalarProgram;

    // JIT-compile a superscalar program using AVX2 instructions.
    // Important to note: 
    //   * prologue includes pushing rax and ymm0-ymm15 registers to stack to not mess up with the caller's state.
    //   * epilogue includes popping rax and ymm0-ymm15 registers from stack to restore the caller's state.
    //   * compilation does not apply to dataset item initialization and finalization; it compiles only superscalar program's instructions.
    //   * register YMM7 is used to hold 32-bit mask const for mul instructions.
    //   * registers YMM0-YMM6 and RAX are used for temporary values, registers YMM8-YMM15 are used to hold dataset items.
    // Whole program will look like this:
    // JitProgram(registers):                               // registers is a vector of 4 dataset items (passed as vector of 8x32 byte values) that a single call to the program will process;
    //                                                      // contiguous 32-byte value stores the same 8-byte register for each of the 4 items
    //                                                      // address of registers is passed in rcx register
    //   push rax                                           // push gpr registers
    //   sub rsp, 0x200                                     // allocate 512 bytes for 16 ymm registers
    //   vmovdqu ymmword ptr [rsp + offset], ymm0-ymm15     // push ymm registers
    //   vmovdqa ymm8-ymm15, ymmword ptr [rcx + offset]     // load 32-byte values from memory to ymm registers
    //   program's 1st instruction                          // compiled 1st instruction
    //   program's 2nd instruction                          // compiled 2st instruction
    //   ...                                                // compile all other instructions
    //   program's last instruction                         // compiled last instruction
    //   vmovdqa ymmword ptr [rcx + offset], ymm8-ymm15     // store 32-byte values from ymm registers to memory
    //   vmovdqu ymm0-ymm15, ymmword ptr [rsp + offset]     // pop ymm registers
    //   add rsp, 0x200                                     // restore stack pointer
    //   pop rax                                            // pop gpr registers
    //   ret                                                // return
    // 
	// Makes the compiled code executable and stores a pointer to it in the program.
    // May throw.
    void compile(SuperscalarProgram& program);
}
