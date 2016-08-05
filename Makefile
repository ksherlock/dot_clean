CXX = clang++
LINK.o = $(LINK.cc)
CXXFLAGS += -std=c++11 -g -Wall

dot_clean: dot_clean.o mapped_file.o applefile.h defer.h

mapped_file.o : mapped_file.cpp mapped_file.h unique_resource.h

dot_clean.o : dot_clean.cpp mapped_file.h applefile.h

