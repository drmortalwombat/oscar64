project_dir := $(abspath $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))/../)
sources = $(wildcard $(project_dir)/oscar64/*.cpp)
srcdir := $(if $(srcdir),$(srcdir),$(project_dir)/build)
objects = $(patsubst $(project_dir)/oscar64/%.cpp,$(srcdir)/%.o,$(sources))

CXX = c++
CPPFLAGS = -g -O2 -std=c++11 -Wno-switch
SED = /usr/bin/sed
REMOVE_FORCE_ALL = $(RM) --recursive --dir
export OSCAR64_CC = $(project_dir)/bin/oscar64
export OSCAR64_CFLAGS =
export OSCAR64_CXX = $(project_dir)/bin/oscar64
MKDIR_PARENT = /bin/mkdir -p -m 755
INSTALL = /usr/bin/install
INSTALL_PROGRAM = $(INSTALL) -m 755
INSTALL_DATA = $(INSTALL) -m 644
DESTDIR =
prefix = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
includedir = $(prefix)/include

 
ifdef WINDIR
	linklibs = -lpthread
else
	UNAME_S := $(shell uname -s)
	
	ifeq ($(UNAME_S), Darwin)

		linklibs = -lpthread
	else
		# MSVC
		linklibs = -lrt -lpthread
		# MinGW
		#linklibs = -lversion -lpthread
	endif
endif


all: --prep-build-dir compiler samples tests


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


compiler: $(objects)
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
	@$(MKDIR_PARENT) $(DESTDIR)$(includedir)/oscar64/{audio,c64,c128,cx16,gfx,nes,opp,plus4,vic20}
	$(INSTALL_DATA) $(project_dir)/include/*.{h,c} $(DESTDIR)$(includedir)/oscar64
	$(INSTALL_DATA) $(project_dir)/include/audio/*.{h,c} $(DESTDIR)$(includedir)/oscar64/audio
	$(INSTALL_DATA) $(project_dir)/include/c64/*.{h,c} $(DESTDIR)$(includedir)/oscar64/c64
	$(INSTALL_DATA) $(project_dir)/include/c128/*.{h,c} $(DESTDIR)$(includedir)/oscar64/c128
	$(INSTALL_DATA) $(project_dir)/include/cx16/*.{h,c} $(DESTDIR)$(includedir)/oscar64/cx16
	$(INSTALL_DATA) $(project_dir)/include/gfx/*.{h,c} $(DESTDIR)$(includedir)/oscar64/gfx
	$(INSTALL_DATA) $(project_dir)/include/nes/*.{h,c} $(DESTDIR)$(includedir)/oscar64/nes
	$(INSTALL_DATA) $(project_dir)/include/opp/*.{h,cpp} $(DESTDIR)$(includedir)/oscar64/opp
	$(INSTALL_DATA) $(project_dir)/include/plus4/*.{h,c} $(DESTDIR)$(includedir)/oscar64/plus4
	$(INSTALL_DATA) $(project_dir)/include/vic20/*.{h,c} $(DESTDIR)$(includedir)/oscar64/vic20


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
