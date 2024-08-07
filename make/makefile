project_dir := $(abspath $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))/../)
sources = $(wildcard $(project_dir)/oscar64/*.cpp)
objects = $(patsubst $(project_dir)/oscar64/%.cpp,%.o,$(sources))
srcdir := $(if $(srcdir),$(srcdir),$(project_dir)/build)

CXX = c++
CPPFLAGS = -g -O2 -std=c++11 -Wno-switch
SED = /usr/bin/sed
REMOVE_FORCE_ALL = $(RM) --recursive --dir

 
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


all: --prep-build-dir compiler


%.o: $(project_dir)/oscar64/%.cpp
	@echo "Compiling compiler file" $@ "..."
	@$(CXX) -c $(CPPFLAGS) $< -o $(srcdir)/$@


%.d: $(project_dir)/oscar64/%.cpp
	@echo "Transforming file" $@ "..."
	@set -e; \
	$(RM) $(srcdir)/$@; \
	$(CC) -MM $(CPPFLAGS) $< > $(srcdir)/$@.$$$$; \
	$(SED) 's,\($*\)\.o[ :]*,\1.o $(srcdir)/$@ : ,g' < $(srcdir)/$@.$$$$ > $(srcdir)/$@; \
	$(RM) $(srcdir)/$@.$$$$


compiler: $(objects)
	@echo "Linking compiler..."
	$(CXX) $(CPPFLAGS) $(objects) $(linklibs) -o $(project_dir)/bin/oscar64


.PHONY : clean
clean :
	@$(RM) $(srcdir)/*.o
	@$(RM) $(srcdir)/*.d
	@$(RM) $(project_dir)/bin/oscar64


.PHONY : distclean
distclean :
	@$(REMOVE_FORCE_ALL) $(srcdir)
	@$(REMOVE_FORCE_ALL) $(project_dir)/bin


ifeq ($(UNAME_S), Darwin)

else

include $($(srcdir)/objects:.o=.d)

endif


--prep-build-dir:
	@if [[ ! -d $(srcdir) ]]; then mkdir --parents $(srcdir); fi
	@if [[ ! -d $(project_dir)/bin ]]; then mkdir --parents $(project_dir)/bin; fi
