# CMakeの最低バージョンを指定
cmake_minimum_required(VERSION 3.10)

# プロジェクト名を設定
project(ProjectChimera)

# C++17標準を使用
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SDL2ライブラリを検索
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf REQUIRED)

# llama.cpp をサブディレクトリとして追加
add_subdirectory(llama.cpp)

# 実行ファイルを作成するために必要なソースファイルを追加
add_executable(game
    main.cpp
    Game.cpp
    LLM.cpp
)

# 実行ファイルに必要なライブラリをリンク
target_link_libraries(game
    PRIVATE
    SDL2::SDL2
    SDL2_image::SDL2_image
    SDL2_ttf::SDL2_ttf
    llama
    ggml
    gdi32
    winmm
    imm32
    version
    ole32
    oleaut32
    setupapi
)

# ★★★ デバッグのため、一時的にWIN32_EXECUTABLEの行をコメントアウト ★★★
# これにより、実行時にコンソールウィンドウが表示され、エラーメッセージを確認できる
# if(WIN32)
#     set_target_properties(game PROPERTIES WIN32_EXECUTABLE ON)
# endif()
