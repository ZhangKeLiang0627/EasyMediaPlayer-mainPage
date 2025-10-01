#!/bin/bash

# 默认为交叉编译（COMPILE_MODE=1），传参0/local可切换为本地编译
COMPILE_MODE=1
[ "$1" = "0" ] || [ "$1" = "local" ] && COMPILE_MODE=0

# 新增：是否移动目标文件（默认移动）
MOVE_FILES=1
[ "$2" = "nomove" ] || [ "$1" = "nomove" ] && MOVE_FILES=0

# 交叉编译设置环境变量
[ $COMPILE_MODE -eq 1 ] && export STAGING_DIR=/home/hugokkl/tina-sdk/out/t113-pi/staging_dir/target

# 执行编译（失败则退出）
make CROSS=$COMPILE_MODE -j32 || exit 1

# 移动目标文件（仅当启用时）
if [ $MOVE_FILES -eq 1 ]; then
    mkdir -p build
    mv *.o build/ 2>/dev/null
    find ./src -type f -name "*.o" -exec mv {} ./build/ \;  
    find ./utils -type f -name "*.o" -exec mv {} ./build/ \;  
fi