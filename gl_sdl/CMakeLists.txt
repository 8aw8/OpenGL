project(gl_sdl)

add_executable(gl_sdl dmemcpy.cpp с3ds.cpp main.cpp)

target_link_libraries(gl_sdl /usr/lib/libGL.so /usr/lib/libGLU.so /usr/lib/libglut.so  /usr/lib/libSDL.so /usr/lib/libSDL_ttf.so /home/aw/sdl/lib3ds-20080909/src/.libs/lib3ds.so)


