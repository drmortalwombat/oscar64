

sources = $(wildcard ../oscar64/*.cpp)
objects = $(patsubst ../oscar64/%.cpp,%.o,$(sources))

CXX = c++
CPPFLAGS = -g -O2 -std=c++11 -Wno-switch
 
 $(shell mkdir -p ../bin)
 
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
 
%.o: ../oscar64/%.cpp
	$(CXX) -c $(CPPFLAGS) $< -o $@

%.d: ../oscar64/%.cpp
	@set -e; rm -f $@; \
	$(CC) -MM $(CPPFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

../bin/oscar64 : $(objects)
	$(CXX) $(CPPFLAGS) $(objects) $(linklibs) -o ../bin/oscar64

.PHONY : clean
clean :
	-rm *.o *.d ../bin/oscar64

ifeq ($(UNAME_S), Darwin)

else

include $(objects:.o=.d)

endif
