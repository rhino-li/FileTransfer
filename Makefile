CROSS_COMPILE ?= X86_64-linux-gnu-
CC := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++
AR := $(CROSS_COMPILE)ar

STATIC_CLIENT_TARGET = libfiletrans_client_static.a
# DYNAMIC_TARGET = libfiletrans_dynamic.so
# CXXFLAGS = -fPIC
# LDFLAGS = -lpthread

mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
cur_makefile_path := $(dir $(mkfile_path))
PROJECT_PATH := $(cur_makefile_path)
BUILD_DIR := $(PROJECT_PATH)build
BIN_DIR := $(PROJECT_PATH)bin
SRC_DIR := $(PROJECT_PATH)src
LIB_PROTOBUF_DIR := 

# SRCS = $(shell find $(SRC_DIR) -name '*.cpp')
SRCS = $($(SRC_DIR)file_client.cpp $(SRC_DIR)file_operator.cpp $(SRC_DIR)socket.cpp $(SRC_DIR)protocol/protocol.pb.cc)
OBJS = $(SRCS:.cpp=.o)
LIBS = -L$(LIB_PROTOBUF_DIR) -lprotobuf


# all: $(STATIC_TARGET) $(DYNAMIC_TARGET)
all: $(STATIC_CLIENT_TARGET)

$(OBJS) : %.o : %.cpp
	$(CXX) -fPIC -c $< -o $@ -lpthread

$(STATIC_CLIENT_TARGET) : $(OBJS)
	$(AR) cqs $@ $^

# $(DYNAMIC_TARGET) : $(OBJS)
# 	$(CC) -fPIC -shared -o $@ $^

install:
	@mkdir -p $(BIN_DIR)
	rm -f $(SRC_DIR)/*.o
	cp $(STATIC_CLIENT_TARGET) $(BIN_DIR)
#	cp $(DYNAMIC_TARGET) $(BIN_DIR)

clean:
	rm -rf $(BUILD_DIR)/*
	rm -f $(SRC_DIR)/*.o
	rm -rf $(BIN_DIR)

