# version compatibility check
cmake_minimum_required(VERSION 2.8)

# a global project naame
project(dineroIV)

set(SRC_DINEROIV src/binaryfmt.c
                 src/cmdargs.c 
                 src/cmdmain.c
                 src/dinfmt.c
                 src/misc.c 
                 src/pixie32fmt.c
                 src/pixie64fmt.c
                 src/ref.c
                 src/tracein.c
                 src/xdinfmt.c)



# set flags to compiler
add_definitions(-D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE)
set(CMAKE_C_FLAGS "-g -Wall -Wundef -Wwrite-strings -Wmissing-prototypes -Wstrict-prototypes -Wredundant-decls -std=c99")

add_executable(dineroIV ${SRC_DINEROIV})


