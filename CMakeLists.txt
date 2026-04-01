cmake_minimum_required(VERSION 3.20)
project(Shogi VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── OpenSiv3D ライブラリを探す ──
find_library(SIV3D_LIB
    NAMES Siv3D
    PATHS /usr/local/lib
    REQUIRED
)

# ── インクルードパス ──
include_directories(/usr/local/include)

# ── 実行ファイル ──
add_executable(Shogi
    src/main.cpp
)

# ── リンク ──
target_link_libraries(Shogi
    PRIVATE
        ${SIV3D_LIB}
        # OpenSiv3D が依存するシステムライブラリ
        GL GLU X11 Xi Xrandr Xinerama Xcursor
        asound pulse
        avcodec avformat avutil swresample swscale
        turbojpeg png freetype harfbuzz
        pthread dl
)

# ── アセット（フォントなど）をビルドディレクトリにコピー ──
add_custom_command(TARGET Shogi POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:Shogi>/assets
    COMMENT "Copying assets..."
)
