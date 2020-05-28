all: head_hunter

CFLAGS=-fPIC -g -Wall -std=c++11 -Wall -Wextra -Wpedantic
INCLUDE = -I/usr/local/include/libfreenect -I/usr/include/libusb-1.0 -I/usr/local/include/opencv4/
LIBS = -lfreenect -lpthread -L/build_opencv/lib -lopencv_core -lopencv_highgui -lopencv_imgcodecs -lopencv_imgproc -lopencv_objdetect

head_hunter:  kinect_opencv_face_detect.cpp
	$(CXX) $(INCLUDE) $(CFLAGS) $? -o $@  $(LIBS)

%.o: %.cpp
	$(CXX) -c $(CFLAGS) $< -o $@

clean:
	rm -rf *.o head_hunter
