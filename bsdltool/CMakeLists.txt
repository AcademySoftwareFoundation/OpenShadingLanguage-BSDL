add_executable(bsdltool main.cpp parse_bsdf.cpp tracer.cpp png.cpp rng.cpp)
target_link_libraries(bsdltool PRIVATE BSDL::BSDL ZLIB::ZLIB)
