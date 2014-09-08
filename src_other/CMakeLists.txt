PROJECT( testmatch2 )
FIND_PACKAGE( OpenCV REQUIRED )
ADD_EXECUTABLE( testmatch2 testmatch2.cpp RobustMatcher.cpp )
TARGET_LINK_LIBRARIES( testmatch2 ${OpenCV_LIBS} opencv_nonfree opencv_features2d)
