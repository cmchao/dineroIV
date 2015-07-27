# version compatibility check
cmake_minimum_required(VERSION 2.8)

# a global project naame
project(dineroIV)

#option
option(DINEROIV_UNIT_TEST  OFF)

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

set(SRC_P2D      testing/p2d.c)
set(SRC_P2B      testing/p2b.c)

# set flags to compiler
add_definitions(-D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE)
set(CMAKE_C_FLAGS "-g -Wall -Wundef -Wwrite-strings -Wmissing-prototypes -Wstrict-prototypes -Wredundant-decls -std=c99")
include_directories(src/) 

add_executable(dineroIV ${SRC_DINEROIV})

if (${DINEROIV_UNIT_TEST})
    add_executable(p2d ${SRC_P2D})
    add_executable(p2b ${SRC_P2B})
endif (${DINEROIV_UNIT_TEST})


#install
install(TARGETS dineroIV DESTINATION bin)
if (${DINEROIV_UNIT_TEST})
    install(DIRECTORY testing/ DESTINATION testing FILES_MATCHING PATTERN mm.*)
    install(FILES testing/test-parms DESTINATION testing)
    install(FILES testing/testscript DESTINATION testing PERMISSIONS
                          OWNER_EXECUTE OWNER_WRITE OWNER_READ
                          GROUP_EXECUTE GROUP_READ)

    install(TARGETS p2d p2b DESTINATION testing)
endif (${DINEROIV_UNIT_TEST})
