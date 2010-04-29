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

#ifndef _INTL_H
#define _INTL_H

#ifdef ENABLE_NLS

#include <libintl.h>

#define _(String) gettext(String)

#ifdef gettext_noop
#define N_(String) gettext_noop(String)
#else
#define N_(String) (String)
#endif

#else /* NLS is disabled */

#define _(String) (String)
#define N_(String) (String)
#define textdomain(String) (String)
#define gettext(String) (String)
#define dgettext(Domain,String) (String)
#define dcgettext(Domain,String,Type) (String)
#define ngettext(String1,String2,Num) \
 ((Num) == 1 ? (String1) : (String2))
#define dngettext(Domain,String1,String2,Num) \
 ((Num) == 1 ? (String1) : (String2))
#define dcngettext(Domain,String1,String2,Num,Category) \
 ((Num) == 1 ? (String1) : (String2))
#define bindtextdomain(Domain,Directory) (Domain) 
#define bind_textdomain_codeset(Domain,Codeset) (Codeset) 

#endif /* ENABLE_NLS */

#endif /* _INTL_H */
