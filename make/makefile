project_dir := $(abspath $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))/../)
sources = $(wildcard $(project_dir)/oscar64/*.cpp)
objects = $(patsubst $(project_dir)/oscar64/%.cpp,%.o,$(sources))
srcdir := $(if $(srcdir),$(srcdir),$(project_dir)/build)

CXX = c++
CPPFLAGS = -g -O2 -std=c++11 -Wno-switch
SED = /usr/bin/sed

 $(shell mkdir -p $(project_dir)/bin)
 
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
 
%.o: $(project_dir)/oscar64/%.cpp
	@echo "Compiling compiler file" $@ "..."
	@$(CXX) -c $(CPPFLAGS) $< -o $(srcdir)/$@

%.d: $(project_dir)/oscar64/%.cpp
	@echo "Transforming file" $@ "..."
	@set -e; $(RM) $(srcdir)/$@; \
	$(CC) -MM $(CPPFLAGS) $< > $(srcdir)/$@.$$$$; \
	$(SED) 's,\($*\)\.o[ :]*,\1.o $(srcdir)/$@ : ,g' < $(srcdir)/$@.$$$$ > $(srcdir)/$@; \
	$(RM) $(srcdir)/$@.$$$$

$(project_dir)/bin/oscar64 : $(objects)
	@echo "Linking compiler" $@ "..."
	@$(CXX) $(CPPFLAGS) $(objects) $(linklibs) -o $(project_dir)/bin/oscar64

.PHONY : clean
clean :
	@$(RM) $(srcdir)/*.o
	@$(RM) $(srcdir)/*.d
	@$(RM) $(project_dir)/bin/oscar64

ifeq ($(UNAME_S), Darwin)

else

include $(objects:.o=.d)

endif
