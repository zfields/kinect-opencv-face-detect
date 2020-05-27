// C/C++ Libraries
#include <cmath>
#include <iostream>
#include <mutex>
#include <vector>

// 3rd Party Libraries
#include <libfreenect.hpp>
#include <opencv2/opencv.hpp>

class MicrosoftKinect : public Freenect::FreenectDevice
{
public:
  MicrosoftKinect(
      freenect_context *_ctx,
      int _index
      ) : Freenect::FreenectDevice(_ctx, _index),
                    _rgb_frame_available(false),
                    _depth_frame_available(false)
  {
    setVideoResolution(FREENECT_RESOLUTION_MEDIUM);

    // Load the gamma array with color values to represent 11-bit
    // (2^11 or 0 - 2047) depth data capture by the Microsoft Kinect
    // (enables later heat map visualization)
    for (unsigned int i = 0; i < 2048; ++i)
    {
      float v = i / 2048.0f;
      v = std::pow(v, 3) * 6;
      _gamma[i] = v * 6 * 256;
    }
    setLed(LED_GREEN);
    setTiltDegrees(0);
  }

  virtual ~MicrosoftKinect() override
  {
    setTiltDegrees(0);
    setLed(LED_OFF);
  }

  bool getBGRVideo(cv::Mat &bgr_image)
  {
    std::lock_guard<std::mutex> rgb_lock(_rgb_mutex);
    if (_rgb_frame_available)
    {
      cv::cvtColor(_live_rgb_feed, bgr_image, cv::COLOR_RGB2BGR);
      _rgb_frame_available = false;
      return true;
    }
    else
    {
      return false;
    }
  }

  bool getDepthHeatMap(cv::Mat &heat_map)
  {
    std::lock_guard<std::mutex> depth_lock(_depth_mutex);

    static const size_t B(0), G(1), R(2);
    if (_depth_frame_available)
    {
      // Loop through depth array data
      for (int r = 0; r < _live_depth_feed.rows; ++r)
      {
        for (int c = 0; c < _live_depth_feed.cols; ++c)
        {
          auto depth_value = _live_depth_feed.at<uint16_t>(r, c);

          // Map the depth value to _gamma values
          uint16_t heat_value = _gamma[depth_value];
          uint8_t fine_heat = static_cast<uint8_t>(heat_value & 0xFF);
          uint8_t coarse_heat = static_cast<uint8_t>(heat_value >> 8);

          // Examine the pval with the low byte removed
          switch (coarse_heat)
          {
          // white fading to red
          case 0:
            heat_map.at<cv::Vec3b>(r, c)[B] = (255 - fine_heat);
            heat_map.at<cv::Vec3b>(r, c)[G] = (255 - fine_heat);
            heat_map.at<cv::Vec3b>(r, c)[R] = 255;
            break;
          // red fading to yellow
          case 1:
            heat_map.at<cv::Vec3b>(r, c)[B] = 0;
            heat_map.at<cv::Vec3b>(r, c)[G] = fine_heat;
            heat_map.at<cv::Vec3b>(r, c)[R] = 255;
            break;
          // yellow fading to green
          case 2:
            heat_map.at<cv::Vec3b>(r, c)[B] = 0;
            heat_map.at<cv::Vec3b>(r, c)[G] = 255;
            heat_map.at<cv::Vec3b>(r, c)[R] = (255 - fine_heat);
            break;
          // green fading to cyan
          case 3:
            heat_map.at<cv::Vec3b>(r, c)[B] = fine_heat;
            heat_map.at<cv::Vec3b>(r, c)[G] = 255;
            heat_map.at<cv::Vec3b>(r, c)[R] = 0;
            break;
          // cyan fading to blue
          case 4:
            heat_map.at<cv::Vec3b>(r, c)[B] = 255;
            heat_map.at<cv::Vec3b>(r, c)[G] = (255 - fine_heat);
            heat_map.at<cv::Vec3b>(r, c)[R] = 0;
            break;
          // blue fading to magenta
          case 5:
            heat_map.at<cv::Vec3b>(r, c)[B] = 255;
            heat_map.at<cv::Vec3b>(r, c)[G] = 0;
            heat_map.at<cv::Vec3b>(r, c)[R] = fine_heat;
            break;
          // magenta fading to black
          case 6:
            heat_map.at<cv::Vec3b>(r, c)[B] = (255 - fine_heat);
            heat_map.at<cv::Vec3b>(r, c)[G] = 0;
            heat_map.at<cv::Vec3b>(r, c)[R] = (255 - fine_heat);
            break;
          // uncategorized values are rendered gray
          default:
            heat_map.at<cv::Vec3b>(r, c)[B] = 128;
            heat_map.at<cv::Vec3b>(r, c)[G] = 128;
            heat_map.at<cv::Vec3b>(r, c)[R] = 128;
            break;
          }
        }
      }
      _depth_frame_available = false;
      return true;
    }
    else
    {
      return false;
    }
  }

  int getWindowColumnAndRowCount(int & _cols, int & _rows)
  {
    // Check resolution and create image canvas
    return videoResolutionToColumnsAndRows(getVideoResolution(), _cols, _rows);
  }

private:
  uint16_t _gamma[2048];
  std::mutex _rgb_mutex;
  std::mutex _depth_mutex;
  bool _rgb_frame_available;
  bool _depth_frame_available;
  cv::Mat _live_depth_feed;
  cv::Mat _live_rgb_feed;

  int setVideoResolution (freenect_resolution _resolution) {
    int result;
    int cols, rows;

    setVideoFormat(FREENECT_VIDEO_RGB, _resolution);
		setDepthFormat(FREENECT_DEPTH_11BIT, _resolution);
    if ( (result = videoResolutionToColumnsAndRows(_resolution, cols, rows)) ) {
      // forward error and exit
    } else {
      _live_depth_feed = cv::Mat(cv::Size(cols, rows), CV_16UC1);
      _live_rgb_feed = cv::Mat(cv::Size(cols, rows), CV_8UC3, cv::Scalar(0));
    }

    return result;
  }

  int videoResolutionToColumnsAndRows (
    freenect_resolution _resolution,
    int & _cols,
    int & _rows
  ) {
    int result;

    switch (_resolution)
    {
    case FREENECT_RESOLUTION_LOW:
      _cols = 320;
      _rows = 240;
      result = 0;
      break;
    case FREENECT_RESOLUTION_MEDIUM:
      _cols = 640;
      _rows = 480;
      result = 0;
      break;
    case FREENECT_RESOLUTION_HIGH:
      _cols = 1280;
      _rows = 1024;
      result = 0;
      break;
    default:
      std::cerr << "Unrecognized Resolution ( " << _resolution << ")" << std::endl;
      result = -1;
    }

    return result;
  }

  // Do not call directly (even in child)
  virtual void VideoCallback(
      void *_rgb,
      uint32_t timestamp) override
  {
    (void)timestamp;
    std::lock_guard<std::mutex> rgb_lock(_rgb_mutex);

    // Load data into CV compatible `Mat` (matrix) object
    _live_rgb_feed.data = static_cast<uint8_t *>(_rgb);
    _rgb_frame_available = true;
  };

  // Do not call directly (even in child)
  virtual void DepthCallback(
      void *_depth,
      uint32_t timestamp) override
  {
    (void)timestamp;
    std::lock_guard<std::mutex> depth_lock(_depth_mutex);

    // Load data into CV compatible `Mat` (matrix) object
    _live_depth_feed.data = static_cast<uint8_t *>(_depth);
    _depth_frame_available = true;
  }
};

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  // Loop control variables
  bool quit(false);
  int key_value(-1);

  // Windowing variables
  bool enable_facial_recognition = false, enable_depth_heat_map = false;
  int window_columns, window_rows;

  // Screen shot variables
  char filename[] = "screenshot";
  char suffix[] = ".png";
  int snap_count(0);

  // Microsoft Kinect variables
  double tilt_degrees(0);
  Freenect::Freenect freenect;
  MicrosoftKinect &kinect = freenect.createDevice<MicrosoftKinect>(0);

  // Image canvas variables
  if (kinect.getWindowColumnAndRowCount(window_columns, window_rows))
  {
    exit(1);
  }
  cv::Mat bgr_image(cv::Size(window_columns, window_rows), CV_8UC3, cv::Scalar(0));
  cv::Mat depth_heat_map(cv::Size(window_columns, window_rows), CV_8UC3);

  // Facial recognition variables
  cv::CascadeClassifier face_detection("/usr/local/share/opencv4/haarcascades/haarcascade_frontalface_alt2.xml");
  float cascade_image_scale = 1.5f;

  // Load BGR Video Window
  namedWindow("Microsoft Kinect (v1)", cv::WINDOW_AUTOSIZE);
  kinect.startVideo();

  // Print console commands
  std::cout << "Press [Esc] or [q] to exit" << std::endl;
  std::cout << "Press [d] to toggle depth heat map" << std::endl;
  std::cout << "Press [f] to toggle facial recognition" << std::endl;
  std::cout << "Press [Backspace] to capture a screenshot" << std::endl;

  // Process Video
  while (!quit)
  {
    // Update depth image
    if (enable_depth_heat_map)
    {
      kinect.getDepthHeatMap(depth_heat_map);
      cv::imshow("Microsoft Kinect (v1)", depth_heat_map);
    }
    else
    {
      // Update video image
      kinect.getBGRVideo(bgr_image);

      // Facial recognition
      if (enable_facial_recognition)
      {
        cv::Mat cascade_grayscale;
        cv::resize(bgr_image, cascade_grayscale, cv::Size((bgr_image.size().width / cascade_image_scale), (bgr_image.size().height / cascade_image_scale)));
        cv::cvtColor(cascade_grayscale, cascade_grayscale, cv::COLOR_BGR2GRAY);

        // Detect faces
        std::vector<cv::Rect> faces;
        face_detection.detectMultiScale(cascade_grayscale, faces, 1.1, 3, 0, cv::Size(25, 25));

        // Draw detection rectangles on original image
        if (!faces.size())
        {
          kinect.setLed(LED_BLINK_RED_YELLOW);
        }
        else
        {
          int avg_face_y = (cascade_grayscale.size().height / 2), sum_face_y = 0;

          // Apply rectangles to BGR image
          for (auto &face : faces)
          {
            sum_face_y += face.y;
            kinect.setLed(LED_RED);
            cv::rectangle(
                bgr_image,
                cv::Point(cvRound(face.x * cascade_image_scale), cvRound(face.y * cascade_image_scale)),                                            // Upper left point
                cv::Point(cvRound((face.x + (face.width - 1)) * cascade_image_scale), cvRound((face.y + (face.height - 1)) * cascade_image_scale)), // Lower right point
                cv::Scalar(0, 0, 255)                                                                                                               // Red line
            );
          }

          // Calculate avgerage y-axis value of faces
          avg_face_y = (sum_face_y / faces.size());

          // Track face (vertical only)
          if (avg_face_y < ((cascade_grayscale.size().height / 2) - 25))
          {
            if (++tilt_degrees >= 30)
            {
              tilt_degrees = 30;
            }
            kinect.setTiltDegrees(tilt_degrees);
          }
          else if (avg_face_y > ((cascade_grayscale.size().height / 2) + 25))
          {
            if (--tilt_degrees <= -30)
            {
              tilt_degrees = -30;
            }
            kinect.setTiltDegrees(tilt_degrees);
          }
        }
      }

      // Render image
      cv::imshow("Microsoft Kinect (v1)", bgr_image);
    }

    // Process User Input
    key_value = cv::waitKey(5);
    switch (key_value)
    {
    // [Backspace] Screen Shot
    case 8:
    {
      std::ostringstream file;
      file << filename << snap_count << suffix;
      cv::imwrite(file.str(), bgr_image);
      ++snap_count;
      break;
    }
    // [Esc], [q] - Exit
    case 27:
    case 113:
      quit = true;
      if (enable_depth_heat_map)
      {
        kinect.stopDepth();
      }
      else
      {
        kinect.stopVideo();
      }
      cv::destroyWindow("Microsoft Kinect (v1)");
      break;
    // [d] - Toggle Depth Heat Map Window
    case 100:
      enable_depth_heat_map = !enable_depth_heat_map;
      if (enable_depth_heat_map)
      {
        // Disable facial recognition
        enable_facial_recognition = false;
        kinect.setLed(LED_GREEN);

        // Swap input from video to depth
        kinect.stopVideo();
        kinect.startDepth();
      }
      else
      {
        // Swap input from depth to video
        kinect.stopDepth();
        kinect.startVideo();
      }
      break;
    // [f] - Toggle Facial Recognition
    case 102:
      // Facial recognition is not available in depth mode
      if (!enable_depth_heat_map)
      {
        enable_facial_recognition = !enable_facial_recognition;
      }

      if (enable_facial_recognition)
      {
        kinect.setLed(LED_BLINK_RED_YELLOW);
      }
      else
      {
        kinect.setTiltDegrees(0);
        kinect.setLed(LED_GREEN);
      }
      break;
    // No input received
    case -1:
      break;
    // Unregistered key press received
    default:
      break;
    }
  }

  return 0;
}
