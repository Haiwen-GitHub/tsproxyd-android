#abi
APP_ABI := armeabi-v7a arm64-v8a x86_64

#调试模式
#APP_OPTIM := debug

#C FLG
#APP_CFLAGS :=

#CXX FLG
#APP_CPPFLAGS :=


#基于 ARMv7 的设备上的硬件 FPU 指令 APP_ABI := armeabi-v7a
#ARMv8 AArch64   APP_ABI := arm64-v8a
#IA-32   APP_ABI := x86
#Intel64 APP_ABI := x86_64
#MIPS32  APP_ABI := mips
#MIPS64 (r6) APP_ABI := mips64
#所有支持的指令集    APP_ABI := all


#
APP_PLATFORM := android-19

# ibstdc++（默认）    默认最小系统 C++ 运行时库。    不适用
# gabi++_static   GAbi++ 运行时（静态）。 C++ 异常和 RTTI
# gabi++_shared   GAbi++ 运行时（共享）。 C++ 异常和 RTTI
# stlport_static  STLport 运行时（静态）。    C++ 异常和 RTTI；标准库
# stlport_shared  STLport 运行时（共享）。    C++ 异常和 RTTI；标准库
# gnustl_static   GNU STL（静态）。    C++ 异常和 RTTI；标准库
# gnustl_shared   GNU STL（共享）。    C++ 异常和 RTTI；标准库
# c++_static  LLVM libc++ 运行时（静态）。    C++ 异常和 RTTI；标准库
# c++_shared  LLVM libc++ 运行时（共享）。    C++ 异常和 RTTI；标准库

#C++标准库
APP_STL := c++_shared
