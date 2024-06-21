bin_dir = bin
build_dir = build
sources = $(wildcard oscar64/*.cpp)
objects = $(patsubst oscar64/%.cpp,$(build_dir)/%.o,$(sources))

CXX = c++
CPPFLAGS = -g -O2 -std=c++11 -Wno-switch
 
 $(shell mkdir -p $(bin_dir) $(build_dir))

ifdef WINDIR
	linklibs = -lpthread
else
	UNAME_S := $(shell uname -s)
	
	ifeq ($(UNAME_S), Darwin)

		linklibs = -lpthread
  	else
		linklibs = -lrt -lpthread
	endif
endif
 
$(build_dir)/%.o: oscar64/%.cpp
	$(CXX) -c $(CPPFLAGS) $< -o $@

$(build_dir)/%.d: oscar64/%.cpp
	@set -e; rm -f $@; \
	$(CC) -MM $(CPPFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

$(bin_dir)/oscar64 : $(objects)
	$(CXX) $(CPPFLAGS) $(linklibs) $(objects) -o $(bin_dir)/oscar64

.PHONY : clean
clean :
	-rm $(build_dir)/*.o $(build_dir)/*.d $(bin_dir)/oscar64

ifeq ($(UNAME_S), Darwin)

else

include $(objects:.o=.d)

endif
