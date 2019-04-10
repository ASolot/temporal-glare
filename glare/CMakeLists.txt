include_directories("${PROJECT_SOURCE_DIR}/include" ${OpenCL_INCLUDE_DIRS} ${Qt5Widgets_INCLUDE_DIRS})

QT5_WRAP_CPP(tg_renderer_HEADERS_MOC TGViewerWidget.h TGViewerWindow.h)

add_executable(glare main.cpp TGViewerWindow.cpp TGViewerWidget.cpp TemporalGlareRenderer.cpp image.cpp ${tg_renderer_HEADERS_MOC})
target_compile_features(glare PRIVATE cxx_range_for)
target_link_libraries(glare ${OpenCL_LIBRARIES} Qt5::Widgets -lGL -lGLU -lGLEW -lglut ${PROJECT_SOURCE_DIR}/include/clFFT/libclFFT.so.2) #Qt5::OpenGL