/**
 * Mupen64 - savestates.c
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

#include <zlib.h>
#include <stdlib.h>
#include <string.h>

#include "savestates.h"
#include "main.h"
#include "guifuncs.h"
#include "translate.h"
#include "rom.h"
#include "osd.h"
#include "../memory/memory.h"
#include "../memory/flashram.h"
#include "../r4300/r4300.h"
#include "../r4300/interupt.h"

extern unsigned int interp_addr;

int savestates_job = 0;

static unsigned int slot = 1;
static int autoinc_save_slot = 0;
static char fname[1024] = {0};

void savestates_select_slot(unsigned int s)
{
    if (s < 1 || s > 10 || s == slot) return;
    slot = s;

    char buffer[1024];
    if(rom)
        snprintf(buffer, 1023, "%s: %s", tr("Selected state file"), savestates_get_filename());
    else 
        snprintf(buffer, 1023, "%s: %d", tr("Selected state slot"), slot);

   osd_new_message(OSD_BOTTOM_LEFT, buffer);
}

// returns the currently selected save slot
unsigned int savestates_get_slot(void)
{
    return slot;
}

// sets save state slot autoincrement on or off
void savestates_set_autoinc_slot(int b)
{
    autoinc_save_slot = b;
}

// returns save state slot autoincrement on or off
int savestates_get_autoinc_slot(void)
{
    return autoinc_save_slot != 0;
}

// increment save slot
void savestates_inc_slot(void)
{
    if (++slot > 10)
        slot = 1;
}

void savestates_select_filename(const char *fn)
{
   if (strlen((char *) fn) >= 1024) return;
   strcpy(fname, fn);
}

char* savestates_get_filename()
{
    size_t length;
    length = strlen(ROM_SETTINGS.goodname)+4+1;
    char *filename = (char*)malloc(length);
    snprintf(filename, length, "%s.st%d", ROM_SETTINGS.goodname, slot - 1);
    return filename;
} 

void savestates_save()
{
    char *filename, *file, buffer[1024];
    gzFile f;
    size_t length;
    int queuelength, i;

    if (autoinc_save_slot)
        savestates_inc_slot();

    if (fname[0] != 0)  // a specific filename was given
    {
        file = malloc(strlen(fname)+1);
        filename = malloc(strlen(fname)+1);
        strcpy(file, fname);
        strcpy(filename, fname);
        fname[0] = 0;
    }
    else
    {
        filename = savestates_get_filename();
        length = strlen(get_savespath())+strlen(filename)+1;
        file = malloc(length);
        snprintf(file, length, "%s%s", get_savespath(), filename);
    }

    f = gzopen(file, "wb");
    free(file);

    gzwrite(f, ROM_SETTINGS.MD5, 32);

    gzwrite(f, &rdram_register, sizeof(RDRAM_register));
    gzwrite(f, &MI_register, sizeof(mips_register));
    gzwrite(f, &pi_register, sizeof(PI_register));
    gzwrite(f, &sp_register, sizeof(SP_register));
    gzwrite(f, &rsp_register, sizeof(RSP_register));
    gzwrite(f, &si_register, sizeof(SI_register));
    gzwrite(f, &vi_register, sizeof(VI_register));
    gzwrite(f, &ri_register, sizeof(RI_register));
    gzwrite(f, &ai_register, sizeof(AI_register));
    gzwrite(f, &dpc_register, sizeof(DPC_register));
    gzwrite(f, &dps_register, sizeof(DPS_register));
    gzwrite(f, rdram, 0x800000);
    gzwrite(f, SP_DMEM, 0x1000);
    gzwrite(f, SP_IMEM, 0x1000);
    gzwrite(f, PIF_RAM, 0x40);

    save_flashram_infos(buffer);
    gzwrite(f, buffer, 24);

    gzwrite(f, tlb_LUT_r, 0x100000);
    gzwrite(f, tlb_LUT_w, 0x100000);

    gzwrite(f, &llbit, 4);
    gzwrite(f, reg, 32*8);
    for ( i = 0; i < 32; i++ ) 
        { gzwrite(f, reg_cop0+i, 8); } //*8 for compatibility with older versions.
    gzwrite(f, &lo, 8);
    gzwrite(f, &hi, 8);
    gzwrite(f, reg_cop1_fgr_64, 32*8);
    gzwrite(f, &FCR0, 4);
    gzwrite(f, &FCR31, 4);
    gzwrite(f, tlb_e, 32*sizeof(tlb));
    if(!dynacore&&interpcore)
        gzwrite(f, &interp_addr, 4);
    else
        gzwrite(f, &PC->addr, 4);

    gzwrite(f, &next_interupt, 4);
    gzwrite(f, &next_vi, 4);
    gzwrite(f, &vi_field, 4);

    queuelength = save_eventqueue_infos(buffer);
    gzwrite(f, buffer, queuelength);

    gzclose(f);
    snprintf(buffer, 1023, "%s: %s", tr("Saved state to"), filename);
    osd_new_message(OSD_BOTTOM_LEFT, buffer);
    free(filename);
}

void savestates_load()
{
    char *filename, *file, buffer[1024];
    gzFile f;
    size_t length;
    int queuelength, i;

    if (fname[0] != 0)  // a specific filename was given
    {
        file = malloc(strlen(fname)+1);
        filename = malloc(strlen(fname)+1);
        strcpy(file, fname);
        strcpy(filename, fname);
        fname[0] = 0;
    }
    else
    {
        filename = savestates_get_filename();
        length = strlen(get_savespath())+strlen(filename)+1;
        file = malloc(length);
        snprintf(file, length, "%s%s", get_savespath(), filename);
    }

    f = gzopen(file, "rb");
    free(file);

    if (f == NULL)
    {
        osd_new_message(OSD_BOTTOM_LEFT, tr("Error: state file doesn't exist"), filename);
        alert_message(tr("The save state you're trying to load doesn't exist"));
        free(filename);
        return;
    }

    gzread(f, buffer, 32);
    if(memcmp(buffer, ROM_SETTINGS.MD5, 32))
    {
        const char *msg = tr("Load state error: Saved state ROM doesn't match current ROM");
        osd_new_message(OSD_BOTTOM_LEFT, msg);
        alert_message(msg);
        free(filename);
        gzclose(f);
        return;
    }

    gzread(f, &rdram_register, sizeof(RDRAM_register));
    gzread(f, &MI_register, sizeof(mips_register));
    gzread(f, &pi_register, sizeof(PI_register));
    gzread(f, &sp_register, sizeof(SP_register));
    gzread(f, &rsp_register, sizeof(RSP_register));
    gzread(f, &si_register, sizeof(SI_register));
    gzread(f, &vi_register, sizeof(VI_register));
    gzread(f, &ri_register, sizeof(RI_register));
    gzread(f, &ai_register, sizeof(AI_register));
    gzread(f, &dpc_register, sizeof(DPC_register));
    gzread(f, &dps_register, sizeof(DPS_register));
    gzread(f, rdram, 0x800000);
    gzread(f, SP_DMEM, 0x1000);
    gzread(f, SP_IMEM, 0x1000);
    gzread(f, PIF_RAM, 0x40);

    gzread(f, buffer, 24);
    load_flashram_infos(buffer);

    gzread(f, tlb_LUT_r, 0x100000);
    gzread(f, tlb_LUT_w, 0x100000);

    gzread(f, &llbit, 4);
    gzread(f, reg, 32*8);
    for ( i = 0; i < 32; i++ ) 
    {
        gzread(f, reg_cop0+i, 4);
        gzread(f, buffer, 4); //for compatibility with older versions.
    }
    gzread(f, &lo, 8);
    gzread(f, &hi, 8);
    gzread(f, reg_cop1_fgr_64, 32*8);
    gzread(f, &FCR0, 4);
    gzread(f, &FCR31, 4);
    gzread(f, tlb_e, 32*sizeof(tlb));
    if(!dynacore&&interpcore)
        gzread(f, &interp_addr, 4);
    else
    {
        int i;
        gzread(f, &queuelength, 4);
        for ( i = 0; i < 0x100000; i++ ) 
            invalid_code[i] = 1;
        jump_to(queuelength);
    }

    gzread(f, &next_interupt, 4);
    gzread(f, &next_vi, 4);
    gzread(f, &vi_field, 4);

    queuelength = 0;
    while(1)
    {
        gzread(f, buffer+queuelength, 4);
        if (*((unsigned int*)&buffer[queuelength]) == 0xFFFFFFFF)
            { break; }
        gzread(f, buffer+queuelength+4, 4);
        queuelength += 8;
    }
    load_eventqueue_infos(buffer);

    gzclose(f);
    if(!dynacore&&interpcore)
        last_addr = interp_addr;
    else
        last_addr = PC->addr;

    snprintf(buffer, 1023, "%s: %s", tr("State loaded from"), filename);
    osd_new_message(OSD_BOTTOM_LEFT, buffer);
    free(filename);

}

