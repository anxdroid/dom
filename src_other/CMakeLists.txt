PROJECT( dom )
FIND_PACKAGE( OpenCV REQUIRED )
ADD_EXECUTABLE( dom Domotica2.cpp)
TARGET_LINK_LIBRARIES( dom ${OpenCV_LIBS} opencv_nonfree opencv_features2d)
