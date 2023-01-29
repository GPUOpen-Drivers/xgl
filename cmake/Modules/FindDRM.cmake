##
 #######################################################################################################################
 #
 #  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
 #
 #  Permission is hereby granted, free of charge, to any person obtaining a copy
 #  of this software and associated documentation files (the "Software"), to deal
 #  in the Software without restriction, including without limitation the rights
 #  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 #  copies of the Software, and to permit persons to whom the Software is
 #  furnished to do so, subject to the following conditions:
 #
 #  The above copyright notice and this permission notice shall be included in all
 #  copies or substantial portions of the Software.
 #
 #  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 #  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 #  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 #  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 #  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 #  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 #  SOFTWARE.
 #
 #######################################################################################################################

if(UNIX)
    find_package(PkgConfig)
    pkg_check_modules(PKG_DRM QUIET libdrm)

    set(DRM_DEFINITIONS ${PKG_DRM_CFLAGS_OTHER})
    set(DRM_VERSION ${PKG_DRM_VERSION})

    find_path(DRM_INCLUDE_DIRS
        NAMES
            xf86drm.h
        HINTS
            ${PKG_DRM_INCLUDE_DIRSS}
    )
    find_library(DRM_LIBRARIES
        NAMES
            drm
        HINTS
            ${PKG_DRM_LIBRARIES_DIRS}
    )

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(DRM
        FOUND_VAR
            DRM_FOUND
        REQUIRED_VARS
            DRM_LIBRARIES
            DRM_INCLUDE_DIRS
        VERSION_VAR
            DRM_VERSION
    )

    if(DRM_FOUND AND NOT TARGET DRM::DRM)
        add_library(DRM::DRM UNKNOWN IMPORTED)
        set_target_properties(DRM::DRM PROPERTIES
            IMPORTED_LOCATION "${DRM_LIBRARIES}"
            INTERFACE_COMPILE_OPTIONS "${DRM_DEFINITIONS}"
            INTERFACE_INCLUDE_DIRECTORIES "${DRM_INCLUDE_DIRS}"
            INTERFACE_INCLUDE_DIRECTORIES "${DRM_INCLUDE_DIRS}/libdrm"
        )
    endif()

    mark_as_advanced(DRM_LIBRARIES DRM_INCLUDE_DIRS)

elseif()
    set(DRM_FOUND FALSE)
endif()
