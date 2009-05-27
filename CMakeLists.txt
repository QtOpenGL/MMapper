cmake_minimum_required(VERSION 2.6)
project(mmapper)

find_package(Qt4 REQUIRED)
set(QT_USE_QTNETWORK TRUE)
set(QT_USE_QTXML TRUE)
set(QT_USE_QTOPENGL TRUE)
include(${QT_USE_FILE})

if (NOT MINGW)
    find_package(ZLIB REQUIRED)
else (NOT MINGW)
    message(STATUS "Warning: MingW requires zlib to be installed manually")
endif (NOT MINGW)

# Get SVN revision number
find_package(Subversion)
if (Subversion_FOUND)
    if (EXISTS ${PROJECT_SOURCE_DIR}/.svn)
        Subversion_WC_INFO(${PROJECT_SOURCE_DIR} PROJECT)
        message(STATUS "Current SVN revision is ${PROJECT_WC_REVISION}")
        add_definitions(-DSVN_REVISION=${PROJECT_WC_REVISION})
	set(SVN_BUILD ${PROJECT_WC_REVISION})
    endif (EXISTS ${PROJECT_SOURCE_DIR}/.svn)
endif (Subversion_FOUND)

if (UNIX)
    SET(SHARE_INSTALL_PREFIX
        "${CMAKE_INSTALL_PREFIX}/share"
        CACHE PATH "Base directory for files which go to share/"
    )

    SET(CMAKE_CXX_FLAGS_DEBUG 
      "-g -Wall -g3 -ggdb -gdwarf-2 -Wunused-variable -Wno-long-long -Wno-unknown-pragmas -Wno-system-headers"
      CACHE STRING "Debug builds CMAKE CXX flags " FORCE )
endif (UNIX)

if (APPLE)
    # In order to build a universal binary we need the following two lines:
    add_definitions(-DCMAKE_OSX_SYSROOT="/Developer/SDKs/MacOSX10.4u.sdk")
    add_definitions(-DCMAKE_OSX_ARCHITECTURES="ppc;i386")
    message(STATUS "Warning: For deployment you should have compiled Qt statically")
endif (APPLE)

add_subdirectory(src)