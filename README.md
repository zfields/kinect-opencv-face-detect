OpenCV Test Project
===================
## [Hackster.io Guide](https://www.hackster.io/zachary_fields/facial-recognition-using-xbox-kinect-and-opencv-3e7ac2)

Instructions
--------
### Pre Requisites 
- Docker 

### Suggested: test run a ready to run image
```
xhost +local:docker
docker run --device /dev/snd --env DISPLAY --interactive --net host --privileged --rm --tty zfields/kinect-opencv-face-detect
```
It will open a window with the video stream

- Press [Esc] or [q] to exit
- Press [d] to toggle depth heat map
- Press [f] to toggle facial recognition
- Press [s] to capture a screenshot

### Download, modify, build and run 
- clone:
`git clone https://github.com/zfields/kinect-opencv-face-detect.git`

- If you want, modify as you wish the files `kinect-opencv-face-detect.cpp`

- Build (will take some time and may show some warnings) 
`docker build --tag kinect-opencv-face-detect .`

It wil finish with something like (see the tag `kinect-opencv-face-detect:latest`): 
```
Successfully built dfacdd726593
Successfully tagged kinect-opencv-face-detect:latest
```
- Run with
`sudo docker run --device /dev/snd --env DISPLAY --interactive --net host --privileged --rm --tty kinect-opencv-face-detect:latest `

Ideation
--------

### BOM

#### Embedded Linux Machine

- Raspberry Pi
- BeagleBone Black

#### Camera

- Windows Kinect v1
- D-Link Network Camera

### Plan

1. Research
1. Setup local laptop with the Kinect
1. Create Dockerfile to isolate the build environment
1. Install OpenCV
1. Create ARM based Docker image from Dockerfile
1. Setup a Raspberry Pi with the Kinect
1. Track a person in the video

Research
--------

### Web Searches

#### zak::waitKey(millis)

- [StackOverflow: How to avoid pressing enter with getchar()](https://stackoverflow.com/questions/1798511/how-to-avoid-pressing-enter-with-getchar)
- [StackOverflow: How do you do non-blocking console I/O on Linux in C?](https://stackoverflow.com/questions/717572/how-do-you-do-non-blocking-console-i-o-on-linux-in-c)

#### Kinect

- [DuckDuckGo: Windows Kinect v1 on Linux](https://duckduckgo.com/?q=Windows+Kinect+v1+on+Linux&t=brave&ia=software)
  - [Open Kinect](https://openkinect.org/wiki/Main_Page)
  - [GitHub - OpenKinect/libfreenect: Open source drivers for ...](https://github.com/OpenKinect/libfreenect)
  - [Running GUI apps with Docker](http://fabiorehm.com/blog/2014/09/11/running-gui-apps-with-docker/)
- [Docker Forum: GUI](https://forums.docker.com/search?q=GUI)
  - [Start a GUI-Application as root in a Ubuntu Container](https://forums.docker.com/t/start-a-gui-application-as-root-in-a-ubuntu-container/17069)
  - [Still not sure how to run GUI apps in docker containers](https://forums.docker.com/t/still-not-sure-how-to-run-gui-apps-in-docker-containers/79103)

#### OpenCV

- [DuckDuckGo: libfreenect opencv](https://duckduckgo.com/?q=libfreenect+opencv&t=brave&ia=software)
  - [Experimenting with Kinect using opencv, python and open ...](https://naman5.wordpress.com/2014/06/24/experimenting-with-kinect-using-opencv-python-and-open-kinect-libfreenect/)
  - [**Jay Rambhia: Kinect with OpenCV using Freenect**](https://jayrambhia.com/blog/kinect-opencv)
    - [C++ OpenCv Example](https://openkinect.org/wiki/C++OpenCvExample)
    - [Catatan Fahmi: Kinect and OpenCV](https://fahmifahim.com/2011/05/16/kinect-and-opencv/)
      - [Tisham Dhar: Getting Data from Kinect to OpenCV](https://whatnicklife.blogspot.com/2010/11/getting-data-from-kinect-to-opencv.html)
    - [Tisham Dhar's Pastebin: C++ program which uses OpenCV with freenect](https://pastebin.com/GJu9mnhJ)
- [OpenCV High-level GUI API](https://docs.opencv.org/4.3.0/d7/dfc/group__highgui.html#ga5628525ad33f52eab17feebcfba38bd7)
- [OpenCV Tutorials](https://docs.opencv.org/master/d9/df8/tutorial_root.html)
- [OpenCV Basics - 03 - Windows](https://youtu.be/_WHrr_5xO3Q)
- [OpenCV Basics - 04 - Accessing Pixels using at Method](https://youtu.be/S4z-C-96xfU)
- [OpenCV Basics - 05 - Split and Merge](https://youtu.be/VjYtoL0wZMc)
- [OpenCV Basics - 12 - Webcam & Video Capture](https://youtu.be/zhEqiW3qnos)
- [Using OpenCV and Haar Cascades to Detect Faces in a Video [C++]](https://youtu.be/RY6fPxpN10E)

Journal
-------

**06 MAY 2020** - I was able to quickly and successfully setup the containerized build environment. However, I was unable to run the examples, because the require an X11 GUI for the video display. I found a couple of links to research possible paths forward, but in the interest of time I decided to install the example dependencies natively on my host machine in order to test the Kinect. The test was successful. The next steps will be to get the Kinect providing input to OpenCV.

**07 MAY 2020** - I was able to get the application to run via the CONTAINER! Previously there were complications providing Docker access to the X11 host. I have resolved this in a hacky, not safe manner, and made notes in the Dockerfile. I found several good blog posts (linked above) and upgraded the container to include OpenCV. Installing OpenCV in the container took over an hour and I fell asleep while it was running. The next step will be to test the OpenCV and kinect sample software provided by the OpenKinect project.

**09 MAY 2020** - I attempted to use the example code I found on the internet. Unfortunately, the examples were written for the Freenect API v1 and v2, and the project is currently at major version 4. Instead of installing an older version of the API, I am planning to understand the flow of the program, then remap the OpenCV usage to the newer Freenect examples.

**12 MAY 2020** - I began walking through the `glview.c` example of the Kinect library. I spent time looking up the documentation for the OpenGL and OpenGLUT API calls. I made comments in the example code with my findings. I documented the main loop, which processes the OpenGL thread. I still have the libfreenect thread to review and document. For my next steps, there does not appear to be API documentation for libfreenect, so I will have to read the code to understand each API call.

**13 MAY 2020** - I am disappointed about the lack of API documentation for libfreenect. My goal was to learn OpenCV, a library leaned upon heavily by the industry. However, I have instead gotten myself into the weeds of `libfreenect`. I am going to continue learning the Kinect, because I have been facinated by the hardware. That being said, I thought it was pertinent to call out the loss of productivity as a talking point for a retrospective. I spent 5 hours researching/documenting the API calls, and felt like I accomplished nothing. I stopped with only the `DrawGLScene` function left to research/document.

**18 MAY 2020** - Documenting `DrawGLScene` was by far the most difficult, as it contained trigonometric functions to calculate compensation for video rotation angles. I dug deep into the weeds - even researching and studying trigometric functions (of which I had long since forgotten). While doing it, documenting the example felt exhaustive and fruitless, but I believe it helps me identify exactly which code is necessary versus which code can be discarded. Stepping back, I think going deep on trigonometry was unnecessary when considering the goals of identifying non-critical code; however the subject matter remains fascinating. Having completed the research/documentation exercise, I feel as though I have a firm grasp of the flow of a Kinect program. The next steps will be to review the examples I found early on, and see if I can integrate OpenCV into the existing Kinect examples.

Later in the evening, I watched several YouTube video tutorials _(linked above)_ describing how to get started with OpenCV. I now feel as though I have a basic enough understanding of OpenCV to cipher the original OpenCV examples I found early on in my research.

**20 MAY 2020** - I rehydrated the OpenCV + Kinect example from OpenKinect.org. I updated the API calls in the example code as well as the Makefile, and got the example running in my Docker container.

**21 MAY 2020** - While updated the example code, I recognized several sections of unused or misleading code, and I'm removing the kruft before adding in the new facial recognition feature.

I updated the windowing scheme and added new key controls to toggle depth information and finally facial recognition. Adding facial recognition was simple to add. It required nearly verbatim usage of the example in the video _(linked above)_.

**27 MAY 2020** - I been fiddling with the API over the last few days, trying to upgrade the video resolution. Unfortunately, the API and wrappers are not very extensible. They would need to be completely rewritten to allow objects to be created with parameterized resolution. To create such a composition would be an interesting use case for the CRTP _([Curiously Recurring Template Pattern](https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern))_. However, I'll leave the refactor for another day. I plan to provide the examples I've created as is, and I am electing to make a note of the shortcoming in the corresponding blog post.

**28 MAY 2020** - As I was finalizing the source and preparing to share, I noticed in my notes that I had originally intended for this to run on the Raspberry Pi. Luckily, I had created Dockerfiles, so this really only amounted to rebuilding the image on ARM - or so I thought... It turns out I configured my Raspberry Pi to not have a GUI. So I created a headless version of the program. This required rewriting `cv::waitKey`, because it has a dependency on the HighGUI library, the OpenCV windowing framework.
