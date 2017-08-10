LINK.o = $(LINK.cc)
CXXFLAGS += -std=c++11 -g -Wall

CPPFLAGS += -I afp/include

# static link if using mingw32 or mingw64 to make redistribution easier.
# also add mingw directory.
ifeq ($(MSYSTEM),MINGW32)
        LDFLAGS += -static
endif

ifeq ($(MSYSTEM),MINGW64)
        LDFLAGS += -static
endif



.PHONY: all
all : dot_clean applesingle appledouble

.PHONY: clean
clean :
	$(RM) *.o dot_clean applesingle appledouble
	$(MAKE) -C afp clean

.PHONY: submodules
submodules :
	$(MAKE) -C afp

afp/libafp.a : submodules

dot_clean : dot_clean.o mapped_file.o afp/libafp.a

applesingle : applesingle.o mapped_file.o afp/libafp.a
appledouble : appledouble.o mapped_file.o afp/libafp.a


mapped_file.o : mapped_file.cpp mapped_file.h unique_resource.h
dot_clean.o : dot_clean.cpp mapped_file.h applefile.h defer.h
applesingle.o : applesingle.cpp mapped_file.h applefile.h defer.h
appledouble.o : appledouble.cpp mapped_file.h applefile.h defer.h

