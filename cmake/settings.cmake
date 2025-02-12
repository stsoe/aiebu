# --- version settings ---

set(AIEBU_INSTALL_DIR "aiebu")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(AIEBU_VERSION_RELEASE 202510)
SET(AIEBU_VERSION_MAJOR 1)
SET(AIEBU_VERSION_MINOR 0)

set(AIEBU_SPECIFICATION_INSTALL_DIR "aiebu/share/specification")

if (DEFINED $ENV{AIEBU_VERSION_PATCH})
  SET(AIEBU_VERSION_PATCH $ENV{AIEBU_VERSION_PATCH})
else(DEFINED $ENV{AIEBU_VERSION_PATCH})
  SET(AIEBU_VERSION_PATCH 0)
endif(DEFINED $ENV{AIEBU_VERSION_PATCH})

# Also update cache to set version for external plug-in .so
set(AIEBU_SOVERSION ${AIEBU_VERSION_MAJOR} CACHE INTERNAL "")
set(AIEBU_VERSION_STRING ${AIEBU_VERSION_MAJOR}.${AIEBU_VERSION_MINOR}.${AIEBU_VERSION_PATCH} CACHE INTERNAL "")
