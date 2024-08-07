CC = g++
STATIC_TARGET = libfiletrans_static.a
DYNAMIC_TARGET = libfiletrans_dynamic.so
# CXXFLAGS = -fPIC
# LDFLAGS = -lpthread

mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
cur_makefile_path := $(dir $(mkfile_path))
PROJECT_PATH := $(cur_makefile_path)
BUILD_DIR := $(PROJECT_PATH)build
BIN_DIR := $(PROJECT_PATH)bin
SRC_DIR := $(PROJECT_PATH)src

SRCS = $(shell find $(SRC_DIR) -name '*.cpp')
OBJS = $(SRCS:.cpp=.o)


all: $(STATIC_TARGET) $(DYNAMIC_TARGET)

$(OBJS) : %.o : %.cpp
	$(CC) -fPIC -c $< -o $@ -lpthread

$(STATIC_TARGET) : $(OBJS)
	ar cqs $@ $^

$(DYNAMIC_TARGET) : $(OBJS)
	$(CC) -fPIC -shared -o $@ $^

install:
	@mkdir -p $(BIN_DIR)
	rm -f $(SRC_DIR)/*.o
	cp $(STATIC_TARGET) $(BIN_DIR)
	cp $(DYNAMIC_TARGET) $(BIN_DIR)

clean:
	rm -rf $(BUILD_DIR)/*
	rm -f $(SRC_DIR)/*.o
	rm -rf $(BIN_DIR)