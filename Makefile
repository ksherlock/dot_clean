LINK.o = $(LINK.cc)
CXXFLAGS += -std=c++11 -g -Wall

dot_clean: dot_clean.o mapped_file.o finder_info_helper.o xattr.o

applesingle : applesingle.o mapped_file.o finder_info_helper.o xattr.o


mapped_file.o : mapped_file.cpp mapped_file.h unique_resource.h

dot_clean.o : dot_clean.cpp mapped_file.h applefile.h defer.h

applesingle.o : applesingle.cpp mapped_file.h applefile.h defer.h


finder_info_helper.o: finder_info_helper.cpp finder_info_helper.h

xattr.o : xattr.c xattr.h
