noinst_LTLIBRARIES += %D%/libocdflashemmc.la
%C%_libocdflashemmc_la_SOURCES = \
	%D%/core.c \
	%D%/fileio.c \
	%D%/tcl.c \
	$(EMMC_DRIVERS) \
	%D%/driver.c \
	$(EMMCHEADERS)

EMMC_DRIVERS = \
	%D%/dwcmshc.c \
	%D%/dwcmshc_subs.c

EMMCHEADERS = \
	%D%/core.h \
	%D%/driver.h \
	%D%/fileio.h \
	%D%/imp.h \
    %D%/dwcmshc.h \
	%D%/dwcmshc_subs.h \
	%D%/dwcmshc_regs.h
