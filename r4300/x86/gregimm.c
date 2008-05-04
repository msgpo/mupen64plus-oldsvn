/**
 * Mupen64 - gregimm.c
 * Copyright (C) 2002 Hacktarux
 *
 * Mupen64 homepage: http://mupen64.emulation64.com
 * email address: hacktarux@yahoo.fr
 * 
 * If you want to contribute to the project please contact
 * me first (maybe someone is already making what you are
 * planning to do).
 *
 *
 * This program is free software; you can redistribute it and/
 * or modify it under the terms of the GNU General Public Li-
 * cence as published by the Free Software Foundation; either
 * version 2 of the Licence, or any later version.
 *
 * This program is distributed in the hope that it will be use-
 * ful, but WITHOUT ANY WARRANTY; without even the implied war-
 * ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public Licence for more details.
 *
 * You should have received a copy of the GNU General Public
 * Licence along with this program; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139,
 * USA.
 *
**/

#include <stdio.h>
#include "../recomph.h"
#include "../recomp.h"
#include "../r4300.h"
#include "assemble.h"
#include "../ops.h"
#include "../../memory/memory.h"
#include "../macros.h"
#include "interpret.h"

void genbltz_test()
{
   int rs_64bit = is64((unsigned int *)dst->f.i.rs);
   
   if (!rs_64bit)
     {
    int rs = allocate_register((unsigned int *)dst->f.i.rs);
    
    cmp_reg32_imm32(rs, 0);
    jge_rj(12);
    mov_m32_imm32((unsigned int *)(&branch_taken), 1); // 10
    jmp_imm_short(10); // 2
    mov_m32_imm32((unsigned int *)(&branch_taken), 0); // 10
     }
   else if (rs_64bit == -1)
     {
    cmp_m32_imm32(((unsigned int *)dst->f.i.rs)+1, 0);
    jge_rj(12);
    mov_m32_imm32((unsigned int *)(&branch_taken), 1); // 10
    jmp_imm_short(10); // 2
    mov_m32_imm32((unsigned int *)(&branch_taken), 0); // 10
     }
   else
     {
    int rs2 = allocate_64_register2((unsigned int *)dst->f.i.rs);
    
    cmp_reg32_imm32(rs2, 0);
    jge_rj(12);
    mov_m32_imm32((unsigned int *)(&branch_taken), 1); // 10
    jmp_imm_short(10); // 2
    mov_m32_imm32((unsigned int *)(&branch_taken), 0); // 10
     }
}

void genbltz()
{
#ifdef INTERPRET_BLTZ
   gencallinterp((unsigned int)BLTZ, 1);
#else
   if (((dst->addr & 0xFFF) == 0xFFC && 
       (dst->addr < 0x80000000 || dst->addr >= 0xC0000000))||no_compiled_jump)
     {
    gencallinterp((unsigned int)BLTZ, 1);
    return;
     }
   
   genbltz_test();
   gendelayslot();
   gentest();
#endif
}

void genbltz_out()
{
#ifdef INTERPRET_BLTZ_OUT
   gencallinterp((unsigned int)BLTZ_OUT, 1);
#else
   if (((dst->addr & 0xFFF) == 0xFFC && 
       (dst->addr < 0x80000000 || dst->addr >= 0xC0000000))||no_compiled_jump)
     {
    gencallinterp((unsigned int)BLTZ_OUT, 1);
    return;
     }
   
   genbltz_test();
   gendelayslot();
   gentest_out();
#endif
}

void genbltz_idle()
{
#ifdef INTERPRET_BLTZ_IDLE
   gencallinterp((unsigned int)BLTZ_IDLE, 1);
#else
   if (((dst->addr & 0xFFF) == 0xFFC && 
       (dst->addr < 0x80000000 || dst->addr >= 0xC0000000))||no_compiled_jump)
     {
    gencallinterp((unsigned int)BLTZ_IDLE, 1);
    return;
     }
   
   genbltz_test();
   gentest_idle();
   genbltz();
#endif
}

void genbgez_test()
{
   int rs_64bit = is64((unsigned int *)dst->f.i.rs);
   
   if (!rs_64bit)
     {
    int rs = allocate_register((unsigned int *)dst->f.i.rs);
    
    cmp_reg32_imm32(rs, 0);
    jl_rj(12);
    mov_m32_imm32((unsigned int *)(&branch_taken), 1); // 10
    jmp_imm_short(10); // 2
    mov_m32_imm32((unsigned int *)(&branch_taken), 0); // 10
     }
   else if (rs_64bit == -1)
     {
    cmp_m32_imm32(((unsigned int *)dst->f.i.rs)+1, 0);
    jl_rj(12);
    mov_m32_imm32((unsigned int *)(&branch_taken), 1); // 10
    jmp_imm_short(10); // 2
    mov_m32_imm32((unsigned int *)(&branch_taken), 0); // 10
     }
   else
     {
    int rs2 = allocate_64_register2((unsigned int *)dst->f.i.rs);
    
    cmp_reg32_imm32(rs2, 0);
    jl_rj(12);
    mov_m32_imm32((unsigned int *)(&branch_taken), 1); // 10
    jmp_imm_short(10); // 2
    mov_m32_imm32((unsigned int *)(&branch_taken), 0); // 10
     }
}

void genbgez()
{
#ifdef INTERPRET_BGEZ
   gencallinterp((unsigned int)BGEZ, 1);
#else
   if (((dst->addr & 0xFFF) == 0xFFC && 
       (dst->addr < 0x80000000 || dst->addr >= 0xC0000000))||no_compiled_jump)
     {
    gencallinterp((unsigned int)BGEZ, 1);
    return;
     }
   
   genbgez_test();
   gendelayslot();
   gentest();
#endif
}

void genbgez_out()
{
#ifdef INTERPRET_BGEZ_OUT
   gencallinterp((unsigned int)BGEZ_OUT, 1);
#else
   if (((dst->addr & 0xFFF) == 0xFFC && 
       (dst->addr < 0x80000000 || dst->addr >= 0xC0000000))||no_compiled_jump)
     {
    gencallinterp((unsigned int)BGEZ_OUT, 1);
    return;
     }
   
   genbgez_test();
   gendelayslot();
   gentest_out();
#endif
}

void genbgez_idle()
{
#ifdef INTERPRET_BGEZ_IDLE
   gencallinterp((unsigned int)BGEZ_IDLE, 1);
#else
   if (((dst->addr & 0xFFF) == 0xFFC && 
       (dst->addr < 0x80000000 || dst->addr >= 0xC0000000))||no_compiled_jump)
     {
    gencallinterp((unsigned int)BGEZ_IDLE, 1);
    return;
     }
   
   genbgez_test();
   gentest_idle();
   genbgez();
#endif
}

void genbltzl()
{
#ifdef INTERPRET_BLTZL
   gencallinterp((unsigned int)BLTZL, 1);
#else
   if (((dst->addr & 0xFFF) == 0xFFC && 
       (dst->addr < 0x80000000 || dst->addr >= 0xC0000000))||no_compiled_jump)
     {
    gencallinterp((unsigned int)BLTZL, 1);
    return;
     }
   
   genbltz_test();
   free_all_registers();
   gentestl();
#endif
}

void genbltzl_out()
{
#ifdef INTERPRET_BLTZL_OUT
   gencallinterp((unsigned int)BLTZL_OUT, 1);
#else
   if (((dst->addr & 0xFFF) == 0xFFC && 
       (dst->addr < 0x80000000 || dst->addr >= 0xC0000000))||no_compiled_jump)
     {
    gencallinterp((unsigned int)BLTZL_OUT, 1);
    return;
     }
   
   genbltz_test();
   free_all_registers();
   gentestl_out();
#endif
}

void genbltzl_idle()
{
#ifdef INTERPRET_BLTZL_IDLE
   gencallinterp((unsigned int)BLTZL_IDLE, 1);
#else
   if (((dst->addr & 0xFFF) == 0xFFC && 
       (dst->addr < 0x80000000 || dst->addr >= 0xC0000000))||no_compiled_jump)
     {
    gencallinterp((unsigned int)BLTZL_IDLE, 1);
    return;
     }
   
   genbltz_test();
   gentest_idle();
   genbltzl();
#endif
}

void genbgezl()
{
#ifdef INTERPRET_BGEZL
   gencallinterp((unsigned int)BGEZL, 1);
#else
   if (((dst->addr & 0xFFF) == 0xFFC && 
       (dst->addr < 0x80000000 || dst->addr >= 0xC0000000))||no_compiled_jump)
     {
    gencallinterp((unsigned int)BGEZL, 1);
    return;
     }
   
   genbgez_test();
   free_all_registers();
   gentestl();
#endif
}

void genbgezl_out()
{
#ifdef INTERPRET_BGEZL_OUT
   gencallinterp((unsigned int)BGEZL_OUT, 1);
#else
   if (((dst->addr & 0xFFF) == 0xFFC && 
       (dst->addr < 0x80000000 || dst->addr >= 0xC0000000))||no_compiled_jump)
     {
    gencallinterp((unsigned int)BGEZL_OUT, 1);
    return;
     }
   
   genbgez_test();
   free_all_registers();
   gentestl_out();
#endif
}

void genbgezl_idle()
{
#ifdef INTERPRET_BGEZL_IDLE
   gencallinterp((unsigned int)BGEZL_IDLE, 1);
#else
   if (((dst->addr & 0xFFF) == 0xFFC && 
       (dst->addr < 0x80000000 || dst->addr >= 0xC0000000))||no_compiled_jump)
     {
    gencallinterp((unsigned int)BGEZL_IDLE, 1);
    return;
     }
   
   genbgez_test();
   gentest_idle();
   genbgezl();
#endif
}

void genbranchlink()
{
   int r31_64bit = is64((unsigned int*)&reg[31]);
   
   if (!r31_64bit)
     {
    int r31 = allocate_register_w((unsigned int *)&reg[31]);
    
    mov_reg32_imm32(r31, dst->addr+8);
     }
   else if (r31_64bit == -1)
     {
    mov_m32_imm32((unsigned int *)&reg[31], dst->addr + 8);
    if (dst->addr & 0x80000000)
      mov_m32_imm32(((unsigned int *)&reg[31])+1, 0xFFFFFFFF);
    else
      mov_m32_imm32(((unsigned int *)&reg[31])+1, 0);
     }
   else
     {
    int r311 = allocate_64_register1_w((unsigned int *)&reg[31]);
    int r312 = allocate_64_register2_w((unsigned int *)&reg[31]);
    
    mov_reg32_imm32(r311, dst->addr+8);
    if (dst->addr & 0x80000000)
      mov_reg32_imm32(r312, 0xFFFFFFFF);
    else
      mov_reg32_imm32(r312, 0);
     }
}

void genbltzal()
{
#ifdef INTERPRET_BLTZAL
   gencallinterp((unsigned int)BLTZAL, 1);
#else
   if (((dst->addr & 0xFFF) == 0xFFC && 
       (dst->addr < 0x80000000 || dst->addr >= 0xC0000000))||no_compiled_jump)
     {
    gencallinterp((unsigned int)BLTZAL, 1);
    return;
     }
   
   genbltz_test();
   genbranchlink();
   gendelayslot();
   gentest();
#endif
}

void genbltzal_out()
{
#ifdef INTERPRET_BLTZAL_OUT
   gencallinterp((unsigned int)BLTZAL_OUT, 1);
#else
   if (((dst->addr & 0xFFF) == 0xFFC && 
       (dst->addr < 0x80000000 || dst->addr >= 0xC0000000))||no_compiled_jump)
     {
    gencallinterp((unsigned int)BLTZAL_OUT, 1);
    return;
     }
   
   genbltz_test();
   genbranchlink();
   gendelayslot();
   gentest_out();
#endif
}

void genbltzal_idle()
{
#ifdef INTERPRET_BLTZAL_IDLE
   gencallinterp((unsigned int)BLTZAL_IDLE, 1);
#else
   if (((dst->addr & 0xFFF) == 0xFFC && 
       (dst->addr < 0x80000000 || dst->addr >= 0xC0000000))||no_compiled_jump)
     {
    gencallinterp((unsigned int)BLTZAL_IDLE, 1);
    return;
     }
   
   genbltz_test();
   genbranchlink();
   gentest_idle();
   genbltzal();
#endif
}

void genbgezal()
{
#ifdef INTERPRET_BGEZAL
   gencallinterp((unsigned int)BGEZAL, 1);
#else
   if (((dst->addr & 0xFFF) == 0xFFC && 
       (dst->addr < 0x80000000 || dst->addr >= 0xC0000000))||no_compiled_jump)
     {
    gencallinterp((unsigned int)BGEZAL, 1);
    return;
     }
   
   genbgez_test();
   genbranchlink();
   gendelayslot();
   gentest();
#endif
}

void genbgezal_out()
{
#ifdef INTERPRET_BGEZAL_OUT
   gencallinterp((unsigned int)BGEZAL_OUT, 1);
#else
   if (((dst->addr & 0xFFF) == 0xFFC && 
       (dst->addr < 0x80000000 || dst->addr >= 0xC0000000))||no_compiled_jump)
     {
    gencallinterp((unsigned int)BGEZAL_OUT, 1);
    return;
     }
   
   genbgez_test();
   genbranchlink();
   gendelayslot();
   gentest_out();
#endif
}

void genbgezal_idle()
{
#ifdef INTERPRET_BGEZAL_IDLE
   gencallinterp((unsigned int)BGEZAL_IDLE, 1);
#else
   if (((dst->addr & 0xFFF) == 0xFFC && 
       (dst->addr < 0x80000000 || dst->addr >= 0xC0000000))||no_compiled_jump)
     {
    gencallinterp((unsigned int)BGEZAL_IDLE, 1);
    return;
     }
   
   genbgez_test();
   genbranchlink();
   gentest_idle();
   genbgezal();
#endif
}

void genbltzall()
{
#ifdef INTERPRET_BLTZALL
   gencallinterp((unsigned int)BLTZALL, 1);
#else
   if (((dst->addr & 0xFFF) == 0xFFC && 
       (dst->addr < 0x80000000 || dst->addr >= 0xC0000000))||no_compiled_jump)
     {
    gencallinterp((unsigned int)BLTZALL, 1);
    return;
     }
   
   genbltz_test();
   genbranchlink();
   free_all_registers();
   gentestl();
#endif
}

void genbltzall_out()
{
#ifdef INTERPRET_BLTZALL_OUT
   gencallinterp((unsigned int)BLTZALL_OUT, 1);
#else
   if (((dst->addr & 0xFFF) == 0xFFC && 
       (dst->addr < 0x80000000 || dst->addr >= 0xC0000000))||no_compiled_jump)
     {
    gencallinterp((unsigned int)BLTZALL_OUT, 1);
    return;
     }
   
   genbltz_test();
   genbranchlink();
   free_all_registers();
   gentestl_out();
#endif
}

void genbltzall_idle()
{
#ifdef INTERPRET_BLTZALL_IDLE
   gencallinterp((unsigned int)BLTZALL_IDLE, 1);
#else
   if (((dst->addr & 0xFFF) == 0xFFC && 
       (dst->addr < 0x80000000 || dst->addr >= 0xC0000000))||no_compiled_jump)
     {
    gencallinterp((unsigned int)BLTZALL_IDLE, 1);
    return;
     }
   
   genbltz_test();
   genbranchlink();
   gentest_idle();
   genbltzall();
#endif
}

void genbgezall()
{
#ifdef INTERPRET_BGEZALL
   gencallinterp((unsigned int)BGEZALL, 1);
#else
   if (((dst->addr & 0xFFF) == 0xFFC && 
       (dst->addr < 0x80000000 || dst->addr >= 0xC0000000))||no_compiled_jump)
     {
    gencallinterp((unsigned int)BGEZALL, 1);
    return;
     }
   
   genbgez_test();
   genbranchlink();
   free_all_registers();
   gentestl();
#endif
}

void genbgezall_out()
{
#ifdef INTERPRET_BGEZALL_OUT
   gencallinterp((unsigned int)BGEZALL_OUT, 1);
#else
   if (((dst->addr & 0xFFF) == 0xFFC && 
       (dst->addr < 0x80000000 || dst->addr >= 0xC0000000))||no_compiled_jump)
     {
    gencallinterp((unsigned int)BGEZALL_OUT, 1);
    return;
     }
   
   genbgez_test();
   genbranchlink();
   free_all_registers();
   gentestl_out();
#endif
}

void genbgezall_idle()
{
#ifdef INTERPRET_BGEZALL_IDLE
   gencallinterp((unsigned int)BGEZALL_IDLE, 1);
#else
   if (((dst->addr & 0xFFF) == 0xFFC && 
       (dst->addr < 0x80000000 || dst->addr >= 0xC0000000))||no_compiled_jump)
     {
    gencallinterp((unsigned int)BGEZALL_IDLE, 1);
    return;
     }
   
   genbgez_test();
   genbranchlink();
   gentest_idle();
   genbgezall();
#endif
}