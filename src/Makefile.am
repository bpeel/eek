## Process this file with automake to produce Makefile.in

INCLUDES = \
	@GLADE_CFLAGS@ \
	@GTK_CFLAGS@ \
	@GCONF_CFLAGS@ \
	@GLIB_CFLAGS@ \
	-DEEK_GLADE_DIR=\""$(datadir)/eek/glade/"\"

bin_PROGRAMS = eek

eek_LDADD = \
	@GLADE_LIBS@ \
	@GTK_LIBS@ \
	@GCONF_LIBS@ \
	@GLIB_LIBS@

eek_SOURCES = \
	main.c \
	cpu.h cpu.c \
	electron.h electron.c \
	video.h video.c \
	electronwidget.h electronwidget.c \
	electronmanager.h electronmanager.c \
	framesource.h framesource.c \
	debugger.h debugger.c \
	memorydisplay.h memorydisplay.c \
	memdispcombo.h memdispcombo.c \
	disassemble.h disassemble.c \
	dismodel.h dismodel.c \
	hexspinbutton.h hexspinbutton.c \
	eekmarshalers.h eekmarshalers.c \
	mainwindow.h mainwindow.c \
	aboutdialog.h aboutdialog.c \
	breakpointeditdialog.h breakpointeditdialog.c \
	disdialog.h disdialog.c \
	tapebuffer.h tapebuffer.c \
	tapeuef.h tapeuef.c \
	preferencesdialog.h preferencesdialog.c \
	gladeutil.h gladeutil.c \
	intl.h

EXTRA_DIST = eekmarshalers.list

eekmarshalers.h : eekmarshalers.list
	glib-genmarshal --header --prefix=eek_marshal $< > $@

eekmarshalers.c : eekmarshalers.list
	glib-genmarshal --body --prefix=eek_marshal $< > $@
