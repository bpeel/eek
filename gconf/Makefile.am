gconfdir = $(GCONF_SCHEMA_FILE_DIR)
gconf_DATA = eek.schemas

EXTRA_DIST = $(gconf_DATA) 

if GCONF_SCHEMAS_INSTALL
install-data-local:
	for p in $(gconf_DATA) ; do \
		GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
		$(GCONFTOOL) --makefile-install-rule $(top_builddir)/gconf/$$p ; \
	done ;
else
install-data-local:
endif
