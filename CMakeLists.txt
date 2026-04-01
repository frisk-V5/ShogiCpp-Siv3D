cmake_minimum_required(VERSION 3.20)
project(Shogi VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── OpenSiv3D SDK ルートパス (CI では -DSIV3D_ROOT=... で渡す) ──
if(NOT DEFINED SIV3D_ROOT)
  if(WIN32)
    # Windows: デフォルトは C:\Siv3D\OpenSiv3D_SDK_*
    file(GLOB _SIV3D_CANDIDATES "C:/Siv3D/OpenSiv3D_SDK_*")
    if(_SIV3D_CANDIDATES)
      list(GET _SIV3D_CANDIDATES 0 SIV3D_ROOT)
    endif()
  elseif(APPLE)
    set(SIV3D_ROOT "/usr/local")
  else()
    set(SIV3D_ROOT "/usr/local")
  endif()
endif()

message(STATUS "OpenSiv3D SDK root: ${SIV3D_ROOT}")

# ── インクルードパス ──
include_directories("${SIV3D_ROOT}/include")

# ── ライブラリ探索 ──
find_library(SIV3D_LIB
    NAMES Siv3D
    PATHS "${SIV3D_ROOT}/lib" /usr/local/lib /usr/lib
    REQUIRED
)

# ── 実行ファイル ──
add_executable(Shogi WIN32      # Windowsでコンソールウィンドウを出さない
    src/main.cpp
)

# ── プラットフォーム別リンク設定 ──
if(WIN32)
  target_link_libraries(Shogi PRIVATE
      ${SIV3D_LIB}
      d3d11 dxgi dxguid d2d1 dwrite
      winmm ole32 oleaut32 strmiids
      Shlwapi
  )
elseif(APPLE)
  find_library(COCOA_FW        Cocoa)
  find_library(IOKIT_FW        IOKit)
  find_library(OPENGL_FW       OpenGL)
  find_library(AUDIOTOOLBOX_FW AudioToolbox)
  target_link_libraries(Shogi PRIVATE
      ${SIV3D_LIB}
      ${COCOA_FW} ${IOKIT_FW} ${OPENGL_FW} ${AUDIOTOOLBOX_FW}
  )
else()   # Linux
  target_link_libraries(Shogi PRIVATE
      ${SIV3D_LIB}
      GL GLU X11 Xi Xrandr Xinerama Xcursor
      asound pulse
      avcodec avformat avutil swresample swscale
      turbojpeg png freetype harfbuzz
      pthread dl
  )
endif()

# ── アセットをビルドディレクトリにコピー ──
if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
  add_custom_command(TARGET Shogi POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/assets"
          "$<TARGET_FILE_DIR:Shogi>/assets"
      COMMENT "Copying game assets..."
  )
endif()
