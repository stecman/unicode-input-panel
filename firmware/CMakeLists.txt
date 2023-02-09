cmake_minimum_required(VERSION 3.13)

option(DEBUG "Output a debug build (only for emscripten currently)" OFF)

if(NOT EMSCRIPTEN)
	include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)
endif()

project(unicode-switch-panel C CXX)

if(NOT EMSCRIPTEN)
	pico_sdk_init()
	include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

	# Enable Tiny USB
	set(FAMILY rp2040)
	set(BOARD pico_sdk)
	set(TINYUSB_FAMILY_PROJECT_NAME_PREFIX "tinyusb_dev_")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

# Make CMake's install command a no-op
# This is required as the install tasks in libpng's CMakeLists otherwise break configuration
macro(install)
endmacro(install)

#
# Download libraries and resources using CMake's FetchContent
#

add_definitions(-ggdb3)

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

# Source dependencies
FetchContent_Declare(freetype URL https://download.savannah.gnu.org/releases/freetype/freetype-2.12.1.tar.gz)
FetchContent_Declare(libpng URL https://cytranet.dl.sourceforge.net/project/libpng/libpng16/1.6.37/libpng-1.6.37.tar.xz)
FetchContent_Declare(lunasvg URL https://github.com/sammycage/lunasvg/archive/refs/tags/v2.3.5.tar.gz)
FetchContent_Declare(unicode_blocks URL https://unicode.org/Public/UNIDATA/Blocks.txt DOWNLOAD_NO_EXTRACT TRUE)
FetchContent_Declare(unicode_codepoints URL https://unicode.org/Public/UNIDATA/UnicodeData.txt DOWNLOAD_NO_EXTRACT TRUE)
FetchContent_Declare(zlib URL https://versaweb.dl.sourceforge.net/project/libpng/zlib/1.2.11/zlib-1.2.11.tar.xz)


# Unicode data files
if(NOT unicode_blocks_POPULATED)
	FetchContent_Populate(unicode_blocks)
endif()

if (NOT unicode_codepoints_POPULATED)
	FetchContent_Populate(unicode_codepoints)
endif()


# zlib (required for libpng)
FetchContent_GetProperties(zlib)
if(NOT zlib_POPULATED)
	FetchContent_Populate(zlib)
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_subdirectory(${zlib_SOURCE_DIR} ${zlib_BINARY_DIR} EXCLUDE_FROM_ALL)


# libpng (for drawing PNG glyphs like colour emoji)
FetchContent_GetProperties(libpng)
if(NOT libpng_POPULATED)
	FetchContent_Populate(libpng)
endif()

set(PNG_BUILD_ZLIB ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR} CACHE INTERNAL "")
set(ZLIB_INCLUDE_DIR ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR} CACHE INTERNAL "")
set(PNG_SHARED OFF CACHE INTERNAL "")
set(PNG_TESTS OFF CACHE INTERNAL "")

if(EMSCRIPTEN)
	set(M_FOUND TRUE CACHE INTERNAL "Emscripten doesn't provide libm: force libpng to skip it")
	set(M_LIBRARY "" CACHE INTERNAL "")
endif()

add_subdirectory(${libpng_SOURCE_DIR} ${libpng_BINARY_DIR} EXCLUDE_FROM_ALL)


# FreeType2
FetchContent_GetProperties(freetype)
if(NOT freetype_POPULATED)
	FetchContent_Populate(freetype)
endif()

# Disable searches for system libraries
set(FT_DISABLE_ZLIB TRUE CACHE INTERNAL "")
set(FT_DISABLE_BZIP2 TRUE CACHE INTERNAL "")
set(FT_DISABLE_PNG TRUE CACHE INTERNAL "")
set(FT_DISABLE_HARFBUZZ TRUE CACHE INTERNAL "")
set(FT_DISABLE_BROTLI TRUE CACHE INTERNAL "")

set(BUILD_SHARED_LIBS FALSE CACHE INTERNAL "")
set(SKIP_INSTALL_LIBRARIES TRUE CACHE INTERNAL "")

# Use of locally built zlib and libpng
set(ZLIB_FOUND TRUE CACHE INTERNAL "")
set(ZLIB_LIBRARIES zlibstatic CACHE INTERNAL "")
set(ZLIB_INCLUDE_DIRS ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR} CACHE INTERNAL "")

set(PNG_FOUND TRUE CACHE INTERNAL "")
set(PNG_LIBRARIES png_static CACHE INTERNAL "")
set(PNG_INCLUDE_DIRS ${libpng_SOURCE_DIR} ${libpng_BINARY_DIR} CACHE INTERNAL "")

add_subdirectory(${freetype_SOURCE_DIR} ${freetype_BINARY_DIR} EXCLUDE_FROM_ALL)

# lunasvg
FetchContent_GetProperties(lunasvg)
if(NOT lunasvg_POPULATED)
	FetchContent_Populate(lunasvg)
endif()

add_subdirectory(${lunasvg_SOURCE_DIR} ${lunasvg_BINARY_DIR} EXCLUDE_FROM_ALL)


#
# Support embedding resources in the binary (GCC-specific)
# Based on https://stackoverflow.com/a/56006001/1470907
#
set(RC_DEPENDS "")

function(add_resource target input)
	if(EMSCRIPTEN)
		# The WASM linker doesn't support converting arbitrary binary files into
		# objects, so we have to embed them in the virtual filesystem instead.
		# The "SHELL:" syntax forces CMake to use exactly what's written without
		# trying to combine options which breaks the build.
		target_link_options(firmware PRIVATE "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/${input}@${input}")
	else()
	    string(MAKE_C_IDENTIFIER ${input} input_identifier)
	    set(output "${CMAKE_CURRENT_BINARY_DIR}/${input_identifier}.o")
	    target_link_libraries(${target} ${output})

	    add_custom_command(
	        OUTPUT ${output}
	        COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR} && ${CMAKE_LINKER} --relocatable --format binary --output ${output} ${input}
	        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${input}
	    )

	    set(RC_DEPENDS ${RC_DEPENDS} ${output} PARENT_SCOPE)
    endif()
endfunction()


#
# Configure code-gen for unicode metadata
#
ADD_CUSTOM_COMMAND(
	OUTPUT unicode_data_generated.c
    COMMAND
    	python3 ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/build-metadata.py
        	--blocks ${unicode_blocks_SOURCE_DIR}/Blocks.txt
        	--codepoints ${unicode_codepoints_SOURCE_DIR}/UnicodeData.txt
        	--include unicode_data.h
        	--output unicode_data_generated.c
    DEPENDS
    	${CMAKE_CURRENT_SOURCE_DIR}/../scripts/build-metadata.py
    	${CMAKE_CURRENT_SOURCE_DIR}/../scripts/namedb.py
    	${unicode_blocks_SOURCE_DIR}/Blocks.txt
    	${unicode_codepoints_SOURCE_DIR}/UnicodeData.txt
    COMMENT "Generate C source for embedding Unicode metadata"
)


#
# Configure firmware build
#

include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_subdirectory(fatfs_spi)

set(BASE_SOURCES
	embeds.cpp
	font_indexer.cpp
	svg.cpp
	ui/codepoint_view.cpp
	ui/common.cpp
	ui/font.cpp
	ui/glyph_display.cpp
	ui/icons.cpp
	ui/main_ui.cpp
	ui/numeric_view.cpp
	ui/utf8_view.cpp
	unicode_db.cpp
	util.cpp

	${CMAKE_CURRENT_BINARY_DIR}/unicode_data_generated.c
)

set(HOST_SOURCES
	host/host_main.cpp
	host/host_filesystem.cpp
	host/host_st7789.c
)

set(DEVICE_SOURCES
	main.cpp
	filesystem.cpp
	hw_config.c
	st7789.c
	usb.c
	usb_descriptors.c
)

set(FIRMWARE_LIBS
	freetype
	lunasvg
	png_static
	zlibstatic
)

if(PICO_PLATFORM STREQUAL "host" OR EMSCRIPTEN)
	# Build application for testing on host
	add_executable(firmware ${BASE_SOURCES} ${HOST_SOURCES})

	# Find and link SDL (for emulating the device's display and controls)
	find_package(SDL2 REQUIRED)
	include_directories(${SDL2_INCLUDE_DIRS})
	list(APPEND FIRMWARE_LIBS ${SDL2_LIBRARIES} pthread)

	# Build with max debug information
	add_definitions(-ggdb3 -O0)

	if(EMSCRIPTEN)
		add_definitions(-s USE_SDL=2)
		target_link_options(firmware PRIVATE -sASSERTIONS -sPTHREAD_POOL_SIZE=1)
		set(CMAKE_EXECUTABLE_SUFFIX ".html")

		option(FONTDIR "Directory for packaging fonts with the web build")
		target_link_options(firmware PRIVATE "SHELL:--preload-file ${FONTDIR}@assets/fonts")

		if(DEBUG)
			# Provide some level of error information in the browser
			# This makes a much larger WASM file, so can't be enabled for deployment
			target_link_options(firmware PRIVATE -gsource-map)
		endif()
	else()
		# We're not actually using this as the application is already abstracted away
		# from the platform it's running on. This could be removed.
		list(APPEND FIRMWARE_LIBS pico_stdlib)
	endif()

else()
	# Firmware for deployment to pico
	add_executable(firmware ${BASE_SOURCES} ${DEVICE_SOURCES})

	# Need to build the SD card driver
	list(APPEND FIRMWARE_LIBS
		hardware_pwm
		hardware_spi
		hardware_uart
		FatFs_SPI
		tinyusb_board
		tinyusb_device
	)

	add_definitions(-DPICO_MALLOC_PANIC=0)

	# pico_enable_stdio_uart(firmware ENABLED)
	pico_enable_stdio_usb(firmware ENABLED)
	pico_add_extra_outputs(firmware) # Create map/bin/hex/uf2 file in addition to ELF

	# Show firmware size stats after build
	add_custom_command(
		TARGET firmware POST_BUILD
		COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/size.sh
			${CMAKE_CURRENT_BINARY_DIR}/firmware.elf 2097152 270336
	)


endif()

# Explicitly select targets to build from libpng and zlib (since we're EXCLUDE_FROM_ALL)
add_dependencies(png_static zlibstatic)
add_dependencies(freetype png_static zlibstatic)
add_dependencies(firmware png_static zlibstatic freetype lunasvg)

target_include_directories(firmware PRIVATE
    ${libpng_SOURCE_DIR}
    ${libpng_BINARY_DIR}
    ${zlib_SOURCE_DIR}
    ${zlib_BINARY_DIR}
    ${FREETYPE_INCLUDE_DIRS}
)

target_link_libraries(firmware ${FIRMWARE_LIBS})

# Assets embedded here need to be manually added to embeds.hh and embeds.cc
# See the Notes section in the readme for how to make a reduced-size fonts
add_resource(firmware "assets/OpenSans-Regular-Stripped.ttf")
add_resource(firmware "assets/NotoSansMono-Regular-Stripped.otf")
add_resource(firmware "assets/unicode-logo.png")

# Convert registered resources into object files
add_custom_target(rc ALL DEPENDS ${RC_DEPENDS})