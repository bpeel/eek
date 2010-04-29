/*
 * eek - An emulator for the Acorn Electron
 * Copyright (C) 2010  Neil Roberts
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _BREAKPOINT_EDIT_DIALOG_H
#define _BREAKPOINT_EDIT_DIALOG_H

#include <gtk/gtkwindow.h>
#include "electronmanager.h"

void breakpoint_edit_dialog_run (GtkWindow *parent, ElectronManager *electron);

#endif /* _BREAKPOINT_EDIT_DIALOG_H */
