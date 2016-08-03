CXX = clang++
LINK.o = $(LINK.cc)
CXXFLAGS += -std=c++11 -g

dot_clean: dot_clean.o mapped_file.o

mapped_file.o : mapped_file.cpp mapped_file.h

dot_clean.o : dot_clean.cpp mapped_file.h applefile.h

