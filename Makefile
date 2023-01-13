CXX?=g++

all: bin/demo

bin/%.o: %.cpp
	@mkdir -p bin
	$(CXX) -g -Og -std=c++20 -c -W -Wall $(CXXFLAGS-$(basename $@)) -o$@ $<

bin/demo: bin/memorysafety.o bin/demo.o
	$(CXX) -o$@ $^
