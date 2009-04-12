/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - input.h                                                 *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2008 Scott Gorman (okaygo)                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef INPUT_H
#define INPUT_H

#ifndef __LINUX__
#include <windows.h>
#else
# include "../main/winlnxdefs.h"
#endif

#define STR_VERSION "0.1"
#define PLUGIN_NAME "Wiinput64 wiimote plugin"
#define PLUGIN_FULL_NAME "Wiinput64 " STR_VERSION " plugin by Funto"
#define CONFIG_FILE_NAME "wiinput64.conf"

typedef char bluetooth_addr[18];	// address of the form : xx:xx:xx:xx:xx:xx (ending with a null character)

extern char* config_dir;	// Configuration files directory, WITHOUT the ending '/' if there is one
extern bluetooth_addr wiimote_addresses[4];	// Bluetooth addresses of the wiimotes

#endif
