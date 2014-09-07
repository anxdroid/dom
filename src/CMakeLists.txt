PROJECT( dom )
FIND_PACKAGE( OpenCV REQUIRED )
ADD_EXECUTABLE( dom Domotica.cpp )
TARGET_LINK_LIBRARIES( dom ${OpenCV_LIBS} lept tesseract )
