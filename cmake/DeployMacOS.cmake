cmake_minimum_required(VERSION 3.24)

if(NOT CMAKE_HOST_APPLE OR NOT IS_DIRECTORY "${APP_BUNDLE}/Contents")
    message(FATAL_ERROR "APP_BUNDLE must point to a built macOS application bundle.")
endif()

# Include dynamically loaded Qt plugins when resolving non-Qt dependencies.
file(GLOB_RECURSE plugins "${APP_BUNDLE}/Contents/PlugIns/*.dylib")
set(BU_CHMOD_BUNDLE_ITEMS ON)
include(BundleUtilities)
fixup_bundle("${APP_BUNDLE}" "${plugins}" "${SEARCH_DIRS}")
