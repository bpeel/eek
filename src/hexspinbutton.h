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

#ifndef _HEX_SPIN_BUTTON_H
#define _HEX_SPIN_BUTTON_H

#include <gtk/gtkspinbutton.h>

#define TYPE_HEX_SPIN_BUTTON (hex_spin_button_get_type ())
#define HEX_SPIN_BUTTON(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                              TYPE_HEX_SPIN_BUTTON, HexSpinButton))
#define HEX_SPIN_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                      TYPE_HEX_SPIN_BUTTON, HexSpinButtonClass))
#define IS_HEX_SPIN_BUTTON(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                 TYPE_HEX_SPIN_BUTTON))
#define IS_HEX_SPIN_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                         TYPE_HEX_SPIN_BUTTON))
#define HEX_SPIN_BUTTON_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                        TYPE_HEX_SPIN_BUTTON, HexSpinButtonClass))

typedef struct _HexSpinButton HexSpinButton;
typedef struct _HexSpinButtonClass HexSpinButtonClass;

struct _HexSpinButton
{
  GtkSpinButton parent_object;

  gboolean hex;
};

struct _HexSpinButtonClass
{
  GtkSpinButtonClass parent_class;
};

GType hex_spin_button_get_type ();
GtkWidget *hex_spin_button_new ();
void hex_spin_button_set_hex (HexSpinButton *hexspin, gboolean hex);
gboolean hex_spin_button_get_hex (HexSpinButton *hexspin);

#endif /* _HEX_SPIN_BUTTON_H */
