noinst_LTLIBRARIES += %D%/libflash.la
%C%_libflash_la_SOURCES = \
	%D%/common.c \
	%D%/loader_io.c \
	%D%/common.h \
	%D%/loader_io.h

%C%_libflash_la_LIBADD = \
	%D%/nor/libocdflashnor.la \
	%D%/nand/libocdflashnand.la\
	%D%/emmc/libocdflashemmc.la

STARTUP_TCL_SRCS += %D%/startup.tcl
STARTUP_TCL_SRCS += %D%/startup-nand.tcl

include %D%/nor/Makefile.am
include %D%/nand/Makefile.am
include %D%/emmc/Makefile.am
