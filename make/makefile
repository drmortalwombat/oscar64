project_dir := $(abspath $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))/../)
sources = $(wildcard $(project_dir)/oscar64/*.cpp)
srcdir := $(if $(srcdir),$(srcdir),$(project_dir)/build)
objects = $(patsubst $(project_dir)/oscar64/%.cpp,$(srcdir)/%.o,$(sources))

CXX = c++
CPPFLAGS = -g -O2 -std=c++11 -Wno-switch
SED = sed
REMOVE_FORCE_ALL = $(RM) --recursive --dir
export OSCAR64_CC = $(project_dir)/bin/oscar64
export OSCAR64_CFLAGS =
export OSCAR64_CXX = $(project_dir)/bin/oscar64
MKDIR_PARENT = mkdir -p -m 755
INSTALL = install
INSTALL_PROGRAM = $(INSTALL) -m 755
INSTALL_DATA = $(INSTALL) -m 644
DESTDIR =
prefix = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
includedir = $(prefix)/include
mandir = $(prefix)/man/man1

ifdef WINDIR
	UNAME_S := $(shell uname -s 2>/dev/null || echo "Windows")

	ifneq ($(findstring MINGW,$(UNAME_S)),)
		# MinGW
		linklibs = -lversion -lpthread -ldbghelp
	else
		# Native Windows (MSVC or other)
		linklibs = -lpthread -ldbghelp
	endif
else
	UNAME_S := $(shell uname -s)

	ifeq ($(UNAME_S), Darwin)
		linklibs = -lpthread
	else
		linklibs = -lrt -lpthread
	endif
endif


all: compiler samples check


$(srcdir)/%.o: $(project_dir)/oscar64/%.cpp
	@echo "Compiling compiler file" $@ "..." $<
	@$(CXX) -c $(CPPFLAGS) $< -o $@


$(srcdir)/%.d: $(project_dir)/oscar64/%.cpp
	@$(MKDIR_PARENT) $(srcdir)
	@echo "Transforming file" $@ "..." $<
	@set -e; \
	$(RM) $@; \
	$(CC) -MM -MT $(patsubst %.d,%.o,$@) $(CPPFLAGS) $< > $@.$$$$; \
	$(SED) 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	$(RM) $@.$$$$


compiler: --prep-build-dir $(objects)
	@$(MKDIR_PARENT) $(srcdir)
	@echo "Linking compiler..."
	$(CXX) $(CPPFLAGS) $(objects) $(linklibs) -o $(project_dir)/bin/oscar64


.PHONY : clean
clean :
	@echo "Cleaning compiler..."
	@$(RM) $(srcdir)/*.o
	@$(RM) $(srcdir)/*.d
	@$(RM) $(project_dir)/bin/oscar64
	@$(MAKE) -C $(project_dir)/samples clean
	@$(MAKE) -C $(project_dir)/autotest clean


.PHONY : distclean
distclean :
	@echo "Distribution cleaning compiler..."
	@$(REMOVE_FORCE_ALL) $(srcdir)
	@$(REMOVE_FORCE_ALL) $(project_dir)/bin
	@$(MAKE) -C $(project_dir)/samples clean
	@$(MAKE) -C $(project_dir)/autotest clean


samples: compiler
	@$(MAKE) -C $(project_dir)/samples all


check: compiler
	@$(MAKE) -C $(project_dir)/autotest all

install: compiler
	@echo "Installing to" $(DESTDIR)$(prefix)
	@$(MKDIR_PARENT) $(DESTDIR)$(bindir)
	$(INSTALL_PROGRAM) $(project_dir)/bin/oscar64 $(DESTDIR)$(bindir)
	$(INSTALL_DATA) $(wildcard $(project_dir)/include/*.h $(project_dir)/include/*.c) $(DESTDIR)$(includedir)/oscar64
	@$(MKDIR_PARENT) $(DESDIR)$(includedir)/oscar64/audio
	$(INSTALL_DATA) $(wildcard $(project_dir)/include/audio/*.h $(project_dir)/include/audio/*.c) $(DESTDIR)$(includedir)/oscar64/audio
	@$(MKDIR_PARENT) $(DESDIR)$(includedir)/oscar64/c64
	$(INSTALL_DATA) $(wildcard $(project_dir)/include/c64/*.h $(project_dir)/include/c64/*.c) $(DESTDIR)$(includedir)/oscar64/c64
	@$(MKDIR_PARENT) $(DESDIR)$(includedir)/oscar64/c128
	$(INSTALL_DATA) $(wildcard $(project_dir)/include/c128/*.h $(project_dir)/include/c128/*.c) $(DESTDIR)$(includedir)/oscar64/c128
	@$(MKDIR_PARENT) $(DESDIR)$(includedir)/oscar64/cx16
	$(INSTALL_DATA) $(wildcard $(project_dir)/include/cx16/*.h $(project_dir)/include/cx16/*.c) $(DESTDIR)$(includedir)/oscar64/cx16
	@$(MKDIR_PARENT) $(DESDIR)$(includedir)/oscar64/gfx
	$(INSTALL_DATA) $(wildcard $(project_dir)/include/gfx/*.h $(project_dir)/include/gfx/*.c) $(DESTDIR)$(includedir)/oscar64/gfx
	@$(MKDIR_PARENT) $(DESDIR)$(includedir)/oscar64/nes
	$(INSTALL_DATA) $(wildcard $(project_dir)/include/nes/*.h $(project_dir)/include/nes/*.c) $(DESTDIR)$(includedir)/oscar64/nes
	@$(MKDIR_PARENT) $(DESDIR)$(includedir)/oscar64/opp
	$(INSTALL_DATA) $(wildcard $(project_dir)/include/opp/*.h $(project_dir)/include/opp/*.cpp) $(DESTDIR)$(includedir)/oscar64/opp
	@$(MKDIR_PARENT) $(DESDIR)$(includedir)/oscar64/plus4
	$(INSTALL_DATA) $(wildcard $(project_dir)/include/plus4/*.h $(project_dir)/include/plus4/*.c) $(DESTDIR)$(includedir)/oscar64/plus4
	@$(MKDIR_PARENT) $(DESDIR)$(includedir)/oscar64/vic20
	$(INSTALL_DATA) $(wildcard $(project_dir)/include/vic20/*.h $(project_dir)/include/vic20/*.c) $(DESTDIR)$(includedir)/oscar64/vic20
	@$(MKDIR_PARENT) $(mandir)
	$(INSTALL_DATA) $(project_dir)/oscar64.1 $(mandir)

uninstall:
	@echo "Uninstalling..."
	@$(RM) $(DESTDIR)$(bindir)/oscar64
	@$(REMOVE_FORCE_ALL) $(DESTDIR)$(includedir)/oscar64/


ifeq ($(UNAME_S), Darwin)

else

include $(objects:.o=.d)

endif


--prep-build-dir:
	echo "makedir"
	@if [ ! -d $(srcdir) ]; then $(MKDIR_PARENT) $(srcdir); fi
	@if [ ! -d $(project_dir)/bin ]; then $(MKDIR_PARENT)  $(project_dir)/bin; fi
