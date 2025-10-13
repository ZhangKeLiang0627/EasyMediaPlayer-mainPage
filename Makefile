#
# Makefile
#

# 使用方式：
# 本地编译：make CROSS=0 -j32
# 交叉编译：make CROSS=1 -j32

# 编译模式控制宏，默认本地编译
CROSS ?= 0

# ==============================================================================
# 交叉编译分支 (CROSS=1)
# ==============================================================================
ifeq ($(CROSS), 1)
    # 编译器设置
    CC = /home/hugokkl/tina-sdk/prebuilt/gcc/linux-x86/arm/toolchain-sunxi-musl/toolchain/bin/arm-openwrt-linux-gcc
    CXX = /home/hugokkl/tina-sdk/prebuilt/gcc/linux-x86/arm/toolchain-sunxi-musl/toolchain/bin/arm-openwrt-linux-g++

    # 可执行文件名
    BIN = eMP_mainPage

    # 路径设置
    LVGL_DIR_NAME ?= lvgl
    LVGL_DIR ?= ./libs
    BUILD_DIR = ./build
    PROJECT_DIR ?= ${shell pwd}

    # 编译选项
    CFLAGS ?= -O3 -g0 -I$(LVGL_DIR)/ -Wall -Wno-unused-function -Wno-unused-variable -Wno-return-type -Wno-sign-compare -Wno-memset-transposed-args 
    LDFLAGS ?= -lm
    CXXFLAGS ?= $(CFLAGS) -std=c++11

    # 头文件路径
    CFLAGS += -I/home/hugokkl/tina-sdk/out/t113-pi/staging_dir/target/usr/include
    CFLAGS += -I/home/hugokkl/tina-sdk/out/t113-pi/staging_dir/target/usr/include/allwinner
    CFLAGS += -I/home/hugokkl/tina-sdk/out/t113-pi/staging_dir/target/usr/include/allwinner/include 
    CFLAGS += -I$(PROJECT_DIR)/inc
    CFLAGS += -I$(PROJECT_DIR)/utils
    CFLAGS += -I/home/hugokkl/tina-sdk/out/t113-pi/compile_dir/target/freetype-2.13.2/include
    CFLAGS += -I$(PROJECT_DIR)/libs/spdlog/include

    # 架构相关参数
    CFLAGS += -pipe -march=armv7-a -mtune=cortex-a7 -mfpu=neon -mfloat-abi=hard -fstack-protector  

    # 链接选项
    LDFLAGS += -L/home/hugokkl/tina-sdk/out/t113-pi/staging_dir/target/lib
    LDFLAGS += -L/home/hugokkl/tina-sdk/out/t113-pi/staging_dir/target/usr/lib  
    LDFLAGS += -ltplayer -lcdx_base -lncurses -lpthread -lstdc++ -lfreetype -lstdc++fs 

    # 源文件收集
    MAINSRC += ./main.cpp
    CSRCS += $(shell find -L $(PROJECT_DIR)/src -name "*.c")
    CXXSRCS += $(shell find -L $(PROJECT_DIR)/src -name "*.cpp")
    CSRCS += $(shell find -L $(PROJECT_DIR)/utils -name "*.c")
    CXXSRCS += $(shell find -L $(PROJECT_DIR)/utils -name "*.cpp")

    # 包含LVGL的Makefile
    include $(LVGL_DIR)/lvgl/lvgl.mk
    include $(LVGL_DIR)/lv_drivers/lv_drivers.mk

    # 目标文件处理
    OBJEXT ?= .o
    AOBJS = $(ASRCS:.S=$(OBJEXT))
    COBJS = $(CSRCS:.c=$(OBJEXT))
    CXXOBJS = $(CXXSRCS:.cpp=$(OBJEXT))
    MAINOBJ = $(MAINSRC:.cpp=$(OBJEXT))
    SRCS = $(ASRCS) $(CSRCS) $(CXXSRCS) $(MAINSRC)
    OBJS = $(AOBJS) $(COBJS) $(CXXOBJS) $(MAINOBJ)

# ==============================================================================
# 本地编译分支 (CROSS=0)
# ==============================================================================
else
    # 编译器设置
    CC = gcc
    CXX = g++

    # 可执行文件名
    BIN = eMP_mainPage

    # 路径设置
    LVGL_DIR_NAME ?= lvgl
    LVGL_DIR ?= ./libs
    PROJECT_DIR ?= ${shell pwd}

    # 编译选项
    CFLAGS ?= -O3 -g -I$(LVGL_DIR)/ -Wall -Wno-unused-function -Wno-unused-variable
	CFLAGS += -DLV_USE_DRAW_SDL=0 -DUSE_SDL=1 -DUSE_MOUSE=1
    CXXFLAGS ?= $(CFLAGS) -std=c++11

    # 头文件路径
    CFLAGS += -I$(PROJECT_DIR)/inc
    CFLAGS += -I$(PROJECT_DIR)/utils
    CFLAGS += -I$(PROJECT_DIR)/libs/spdlog/include
    CFLAGS += -I/usr/include/freetype2
    CFLAGS += $(shell pkg-config --cflags sdl2)

    # 链接选项
    LDFLAGS ?= -lm
    LDFLAGS += -lpthread -lstdc++ 
    LDFLAGS += $(shell pkg-config --libs sdl2)
    LDFLAGS += -lfreetype -lncurses -lstdc++fs 

    # 源文件收集
    MAINSRC += ./main.cpp
    CSRCS += $(shell find -L $(PROJECT_DIR)/src -name "*.c")
    CXXSRCS += $(shell find -L $(PROJECT_DIR)/src -name "*.cpp")
    CSRCS += $(shell find -L $(PROJECT_DIR)/utils -name "*.c")
    CXXSRCS += $(shell find -L $(PROJECT_DIR)/utils -name "*.cpp")

    # 包含LVGL的Makefile
    include $(LVGL_DIR)/lvgl/lvgl.mk
    include $(LVGL_DIR)/lv_drivers/lv_drivers.mk

    # 目标文件处理
    OBJEXT ?= .o
    AOBJS = $(ASRCS:.S=$(OBJEXT))
    COBJS = $(CSRCS:.c=$(OBJEXT))
    CXXOBJS = $(CXXSRCS:.cpp=$(OBJEXT))
    MAINOBJ = $(MAINSRC:.cpp=$(OBJEXT))
    SRCS = $(ASRCS) $(CSRCS) $(CXXSRCS) $(MAINSRC)
    OBJS = $(AOBJS) $(COBJS) $(CXXOBJS) $(MAINOBJ)

    # 本地编译的BUILD_DIR定义（与交叉编译保持一致）
    BUILD_DIR = ./build
endif

# ==============================================================================
# 公共编译规则
# ==============================================================================
.PHONY: clean all

all: default

# C文件编译规则
%.o: %.c
	@$(CC)  $(CFLAGS) -c $< -o $@
	@echo "CC $<"

# C++文件编译规则
%.o: %.cpp
	@$(CXX)  $(CXXFLAGS) -c $< -o $@
	@echo "CXX $<"
    
# 链接生成可执行文件
default: $(OBJS)
	$(CXX) -o $(BIN) $(MAINOBJ) $(AOBJS) $(COBJS) $(CXXOBJS) $(LDFLAGS)

# 清理目标文件
clean: 
	rm -f $(BIN) $(AOBJS) $(COBJS) $(MAINOBJ) $(CXXOBJS)
	rm -r $(BUILD_DIR)
