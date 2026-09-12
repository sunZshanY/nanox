# Install layout and CPack packaging.
#
#   cmake --install build [--prefix <dir>]
#       installs the nanox binary, the docs and the hello example.
#
#   cpack --config build/CPackConfig.cmake -B dist
#       produces the release archives: TGZ + ZIP everywhere, plus a DEB on
#       Linux (the DEB generator needs the dpkg tools, which the ubuntu CI
#       runner has; it is therefore appended on Linux only).
#
# The package-manager manifests under packaging/ (Homebrew, Scoop, AUR) are
# filled from these artifacts; see packaging/README.md.

if(NOT TARGET nanox)
    return()  # packaging only makes sense with the CLI built
endif()

include(GNUInstallDirs)

# Keep the documentation path lowercase even though the project is "NanoX".
set(CMAKE_INSTALL_DOCDIR "share/doc/nanox")

install(TARGETS nanox RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES "${CMAKE_SOURCE_DIR}/README.md" "${CMAKE_SOURCE_DIR}/LICENSE"
        DESTINATION "${CMAKE_INSTALL_DOCDIR}")
install(FILES "${CMAKE_SOURCE_DIR}/examples/hello.nx"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/nanox/examples")

set(CPACK_PACKAGE_NAME "nanox")
set(CPACK_PACKAGE_VENDOR "NanoX")
set(CPACK_PACKAGE_CONTACT "NanoX maintainers")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "NanoX programming language: lexer, project tools and IDE-style TUI")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/sunZshanY/nanox")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

# The archives keep the install tree at their root (bin/, share/). That is the
# layout the Homebrew formula, the Scoop manifest and the AUR PKGBUILD expect,
# so no manifest has to know about a versioned wrapper directory.
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)

set(CPACK_GENERATOR "TGZ;ZIP")

if(UNIX AND NOT APPLE)
    list(APPEND CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "NanoX maintainers")
    set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
endif()

include(CPack)
