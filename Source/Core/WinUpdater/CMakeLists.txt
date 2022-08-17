set (MANIFEST_FILE Updater.exe.manifest)

add_executable(winupdater WIN32
  Main.cpp
  WinUI.cpp
  ${MANIFEST_FILE})

target_link_libraries(winupdater PRIVATE
  uicommon
  updatercommon
  Comctl32
)

set_target_properties(winupdater PROPERTIES OUTPUT_NAME "Updater")

if (NOT CMAKE_SYSTEM_NAME MATCHES "WindowsStore")
  if (MSVC)
    # Add precompiled header
    target_link_libraries(winupdater PRIVATE use_pch)
  endif()
endif()
