## Process this file with automake to produce Makefile.in

INCLUDES = @GTK_CFLAGS@

bin_PROGRAMS = eek

eek_LDADD = @GTK_LIBS@
eek_SOURCES = \
	eek.h main.c \
	cpu.h cpu.c \
	electron.h electron.c \
	util.h util.c \
	monitor.h monitor.c \
	video.h video.c \
	stypes.h \
	byteorder.h \
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
	intl.h

EXTRA_DIST = eekmarshalers.list

eekmarshalers.h : eekmarshalers.list
	glib-genmarshal --header --prefix=eek_marshal $< > $@

eekmarshalers.c : eekmarshalers.list
	glib-genmarshal --body --prefix=eek_marshal $< > $@
