
AM_CFLAGS = -DPKGDATADIR=\"$(pkgdatadir)\" \
	-DPACKAGE_DATA_DIR=\""$(pkgdatadir)"\" \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/gui/common \
	@CCNET_CFLAGS@ \
	@LIBNOTIFY_CFLAGS@ @GTK_CFLAGS@ @APP_INDICATOR_CFLAGS@ \
	@SEARPC_CFLAGS@

noinst_HEADERS = trayicon.h misc.h \
	../common/ccnet-init.h \
	../common/opendir-proc.h ../common/applet-rpc-service.h \
	../common/applet-log.h ../common/rpc-wrapper.h \
	../common/applet-po.h ../common/applet-common.h

bin_PROGRAMS = seafile-applet

seafile_applet_SOURCES = seafile-applet.c trayicon.c \
    init-ccnet.c misc.c \
	../common/opendir-proc.c ../common/applet-log.c \
	../common/ccnet-init.c ../common/rpc-wrapper.c \
	../common/applet-common.c ../common/applet-rpc-service.c

seafile_applet_LDADD  = @CCNET_LIBS@  \
	$(top_builddir)/lib/libseafile.la \
	@GTK_LIBS@ -lssl -lcrypto @LIB_RT@ @LIB_UUID@ \
	-lsqlite3 -levent \
	$(LIBNOTIFY_LIBS) $(APP_INDICATOR_LIBS) @SEARPC_LIBS@

seafile_applet_LDFLAGS = @STATIC_COMPILE@ @CONSOLE@

ui = seafile-trayicon.ui init-ccnet-window.ui init-seafile-window.ui

uidir = $(pkgdatadir)
ui_DATA = $(ui)

EXTRA_DIST = $(ui) 
