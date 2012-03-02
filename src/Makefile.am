bin_PROGRAMS = xmlproc
xmlproc_SOURCES = data-cpu.c data-pci.c xml.c xmlproc.c xml.h
xmlproc_CFLAGS = -Wall
if USE_PKGCONFIG
  xmlproc_CPPFLAGS = `@PKGCONFIG@ libxml-2.0 --cflags`
endif
