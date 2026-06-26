cmake_minimum_required(VERSION 3.10.2)
project(nativechat)

# main.cpp를 nativechat이라는 이름의 공유 라이브러리로 빌드
add_library(nativechat SHARED main.cpp)

# 헤더 파일 폴더 경로 지정
target_include_directories(nativechat PRIVATE include)

# 안드로이드 시스템 라이브러리 찾기
find_library(log-lib log)
find_library(android-lib android)
find_library(egl-lib EGL)
find_library(gles3-lib GLESv3)

# 라이브러리들을 nativechat에 링크
target_link_libraries(nativechat
    ${log-lib}
    ${android-lib}
    ${egl-lib}
    ${gles3-lib}
)
