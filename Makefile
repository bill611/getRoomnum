MAKEROOT = $(shell pwd)
COMPILER_PATH=$(NUVOTON_CROOS_PATH)
# 主程序Makefile

ifeq ($(V), 1)
CC = $(COMPILER_PATH)/bin/arm-none-linux-gnueabi-gcc
AR = $(COMPILER_PATH)/bin/arm-none-linux-gnueabi-ar
CFLAGS = -muclibc
else
CC := gcc
AR := ar
endif

# 在指定目录下生成的应用程序
EXE = v
BIN_TARGET = ${BIN_DIR}/${EXE}
CFLAGS += -g

SRC_DIR = $(MAKEROOT)/src
OBJ_DIR = $(MAKEROOT)/obj
BIN_DIR = $(MAKEROOT)

# INC_DIR 目录下为相应库的头文件
INC_DIR = \
		  $(MAKEROOT)/src\
		  $(MAKEROOT)/src/include\

CFLAGS += ${addprefix -I,${INC_DIR}}

# wildcard:扩展通配符，notdir;去除路径，patsubst;替换通配符

SRC = $(wildcard ${SRC_DIR}/*.c)
OBJ = $(patsubst %.c,${OBJ_DIR}/%.o,$(notdir ${SRC}))
DEPS = $(patsubst %.c, ${OBJ_DIR}/%.d, $(notdir ${SRC}))

# 链接路径
# -Xlinker编译时可重复查找依赖库，和库的次序无关
LIB_DIR = \
		  $(MAKEROOT)/src/libs \

XLINKER = -Xlinker "-(" -lstateMachine -ltimer -lrt -lpthread -Xlinker "-)" 

export CC AR BIN_DIR LIB_DIR CFLAGS OBJ_DIR INC_DIR DEPS
# $@：表示目标文件，$^：表示所有的依赖文件，$<：表示第一个依赖文件，$?：表示比目标还要新的依赖文件列表
all: make_C ${BIN_TARGET} 

make_C:
	@mkdir -p ${OBJ_DIR}
	@make -C src 

# 在指定目录下，将所有的.c文件编译为相应的同名.o文件
${BIN_TARGET}:${OBJ}
	@$(CC) $(CFLAGS) -o $@ $(OBJ) ${addprefix -L,${LIB_DIR}} ${XLINKER}
	@-rm -rf libmain.a libmain.so 

debug:
	make -C src debug

.PHONY:clean clean_all
clean:
	@-rm -rf ${OBJ_DIR}
	@-rm -rf ${BIN_TARGET} *.a *.so 

clean_all: clean
	@-rm -rf *.out tags ${OBJ_DIR}
