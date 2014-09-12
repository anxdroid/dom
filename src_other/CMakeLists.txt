PROJECT( posterize )
FIND_PACKAGE( OpenCV REQUIRED )
ADD_EXECUTABLE( posterize posterize.cpp)
TARGET_LINK_LIBRARIES( posterize ${OpenCV_LIBS})
