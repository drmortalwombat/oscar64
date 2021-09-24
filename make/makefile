

sources = $(wildcard ../oscar64/*.cpp)
objects = $(patsubst ../oscar64/%.cpp,%.o,$(sources))

CXX = g++
CPPFLAGS = -g -O -march=athlon64 -mfpmath=sse
 

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
 
%.o: ../oscar64/%.cpp
	g++ -c $(CPPFLAGS) $< -o $@

%.d: ../oscar64/%.cpp
	@set -e; rm -f $@; \
	$(CC) -MM $(CPPFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

oscar64 : $(objects)
	g++ $(CPPFLAGS) $(linklibs) $(objects) -o oscar64

.PHONY : clean
clean :
	-rm *.o *.d

include $(objects:.o=.d)
