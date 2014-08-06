include_directories(../font)
include_directories(SYSTEM ${FREETYPE_INCLUDE_DIRS})

set(OLF_SOURCES olf.c)

set(OLF_INCLUDE_FILES allegro5/allegro_olf.h)

set_our_header_properties(${OLF_INCLUDE_FILES})

configure_file(
    allegro5/internal/aintern_olf_cfg.h.cmake
    ${CMAKE_BINARY_DIR}/include/allegro5/internal/aintern_olf_cfg.h
    )

set(OLF_LIBRARIES ${FREETYPE_LIBRARIES})
set(OLF_INCLUDE_DIRECTORIES ${FREETYPE_INCLUDE_DIRS})

# FREETYPE_LIBRARIES should include zlib automatically if statically linking,
# but it doesn't.
find_package(ZLIB)
if(ZLIB_FOUND AND NOT IPHONE)
    list(APPEND OLF_LIBRARIES ${ZLIB_LIBRARIES})
endif()

add_our_addon_library(allegro_olf
    "${OLF_SOURCES};${OLF_INCLUDE_FILES}"
    "-DALLEGRO_OLF_SRC"
    "${FONT_LINK_WITH};${OLF_LIBRARIES}"
    )

set_our_framework_properties(allegro_olf AllegroTTF-${ALLEGRO_SOVERSION})

install_our_headers(${OLF_INCLUDE_FILES})

add_addon(olf)

#-----------------------------------------------------------------------------#
# vi: set ts=8 sts=4 sw=4 et:
