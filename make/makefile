project_dir := $(abspath $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))/../)
sources = $(wildcard $(project_dir)/oscar64/*.cpp)
objects = $(patsubst $(project_dir)/oscar64/%.cpp,%.o,$(sources))

CXX = c++
CPPFLAGS = -g -O2 -std=c++11 -Wno-switch
 
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
	$(CXX) -c $(CPPFLAGS) $< -o $@

%.d: $(project_dir)/oscar64/%.cpp
	@set -e; rm -f $@; \
	$(CC) -MM $(CPPFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

$(project_dir)/bin/oscar64 : $(objects)
	$(CXX) $(CPPFLAGS) $(objects) $(linklibs) -o $(project_dir)/bin/oscar64

.PHONY : clean
clean :
	-rm *.o *.d $(project_dir)/bin/oscar64

ifeq ($(UNAME_S), Darwin)

else

include $(objects:.o=.d)

endif
