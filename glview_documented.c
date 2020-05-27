/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified, you
 * may:
 *   1) Leave this header intact and distribute it under the same terms,
 *      accompanying it with the APACHE20 and GPL20 files, or
 *   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
 *   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 *
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */

// C Libraries
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// 3rd Party Libraries
#include <cv.h>

// Project libraries
#include "libfreenect.h"

#ifdef _MSC_VER
#define HAVE_STRUCT_TIMESPEC
#endif
#include <pthread.h>

#if defined(__APPLE__)
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#define _USE_MATH_DEFINES
#include <math.h>

#define RESOLUTION_X 640
#define RESOLUTION_Y 480

pthread_t freenect_thread;
volatile int die = 0;

int glut_argc;
char **glut_argv;

int window;

pthread_mutex_t gl_backbuf_mutex = PTHREAD_MUTEX_INITIALIZER;

// back: owned by libfreenect (implicit for depth)
// mid: owned by callbacks, "latest frame ready"
// front: owned by GL, "currently being drawn"
uint8_t *depth_mid, *depth_active;
uint8_t *rgb_kinect, *rgb_cache, *rgb_active;

GLuint gl_depth_texure;
GLuint gl_rgb_texture;
int camera_rotate = 0;
int tilt_changed = 0;

freenect_context *f_ctx;
freenect_device *f_dev;
int freenect_angle = 0;
int freenect_led;

freenect_video_format requested_format = FREENECT_VIDEO_RGB;
freenect_video_format current_format = FREENECT_VIDEO_RGB;

pthread_cond_t gl_frame_cond = PTHREAD_COND_INITIALIZER;
int got_rgb = 0;  // synchronized under gl_backbuf_mutex
int got_depth = 0;  // synchronized under gl_backbuf_mutex

void DrawGLScene
{
  // CRITICAL SECTION
  /////////////////////

  // Take the mutex
  pthread_mutex_lock(&gl_backbuf_mutex);

  // When using YUV_RGB mode, RGB frames only arrive at 15Hz,
  // so we shouldn't force them to draw in lock-step.
  // However, drawing in lock-step is CPU/GPU intensive when we are receiving frames in lock-step.
  if (current_format == FREENECT_VIDEO_YUV_RGB) {
    while (!got_depth && !got_rgb) {
      pthread_cond_wait(&gl_frame_cond, &gl_backbuf_mutex);
    }
  } else { // current_format != FhREENECT_VIDEO_YUV_RGB
    //? This logic seems wrong. Should it be `requested_format == current_format`?
    while ((!got_depth || !got_rgb) && requested_format != current_format) {
      pthread_cond_wait(&gl_frame_cond, &gl_backbuf_mutex);
    }

  }

  // Exit early if new format has been requested
  if (requested_format != current_format) {
    pthread_mutex_unlock(&gl_backbuf_mutex);
    return;
  }

  uint8_t *swap;

  // Depth frame has been processed
  if (got_depth) {
    // swap active and cached textures
    swap = depth_active;
    depth_active = depth_cache;
    depth_mid = swap;
    got_depth = 0;
  }

  // Video frame has been processed
  if (got_rgb) {
    // swap active and cached textures
    swap = rgb_active;
    rgb_active = rgb_cache;
    rgb_cache = swap;
    got_rgb = 0;
  }

  // Release the mutex
  pthread_mutex_unlock(&gl_backbuf_mutex);

  // END CRITICAL SECTION
  /////////////////////////

  GLfloat camera_angle = 0.0;

  // Bind Depth Texture to 2D Texture Primative
  glBindTexture(GL_TEXTURE_2D, gl_depth_texture);

  // Specify 2D Texture Image
  glTexImage2D(
    GL_TEXTURE_2D, // target
    0, // level
    3, // internal format (number of color components in the texture)
    RESOLUTION_X, // width
    RESOLUTION_Y, // height
    0, // border width
    GL_RGB, // format
    GL_UNSIGNED_BYTE, // type
    depth_active // data
  );

  // Calculate video rotation in degrees (based on Kinect orientation)
  if (camera_rotate) {
    // Clear specified buffers to preset values
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Capture accelerometer reading
    freenect_raw_tilt_state* state;
    freenect_update_tilt_state(f_dev);
    state = freenect_get_tilt_state(f_dev);
    GLfloat x_accel_raw, cosine, y_accel_raw, sine;
    x_accelerometer_raw = (GLfloat)state->accelerometer_x/819.0;
    y_accelerometer_raw = (GLfloat)state->accelerometer_y/819.0;

    // Sloppy acceleration vector cleanup (unsure of trigonometric naming)
    GLfloat hypotenuse = sqrt(x_accelerometer_raw * x_accelerometer_raw + y_accelerometer_raw * y_accelerometer_raw);
    cosine = x_accelerometer_raw/hypotenuse;
    sine = y_accelerometer_raw/hypotenuse;
    double camera_radians = atan2(sine,cosine);
    camera_angle = camera_radians * (180/M_PI) - 90.0;
  } else { // !camera_rotate
    camera_angle = 0;
  }

  // Load the identity matrix
  // 1 0 0 0
  // 0 1 0 0
  // 0 0 1 0
  // 0 0 0 1
  glLoadIdentity();

  // Duplicate the identity matrix on the stack
  glPushMatrix();

  // Translate the identity matrix WITH:
  // 1 0 0 320
  // 0 1 0 240
  // 0 0 1  0
  // 0 0 0  1
  glTranslatef((RESOLUTION_X/2.0),(RESOLUTION_Y/2.0) ,0.0);

  // Multiply the current matrix with the rotation matrix
  glRotatef(camera_angle, 0.0, 0.0, 1.0);

  // Translate the current matrix WITH:
  // 1 0 0 -320
  // 0 1 0 -240
  // 0 0 1   0
  // 0 0 0   1
  glTranslatef(-(RESOLUTION_X/2.0),-(RESOLUTION_Y/2.0) ,0.0);

  // Draw a connected group of triangles. One triangle is defined
  // for each vertex presented after the first two vertices.
  // Vertices 1 , n + 1 , and n + 2 define triangle n. N - 2 triangles are drawn.
  glBegin(GL_TRIANGLE_FAN);

  // Set the current color intesity (R, G, B, A)
  // 1.0 (full intensity) - 0.0 (zero intensity).
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

  //? Why is it necessary to specify texture coordinates and polygon vertices
  glTexCoord2f(0, 1); glVertex3f(0,0,1.0);
  glTexCoord2f(1, 1); glVertex3f(RESOLUTION_X,0,1.0);
  glTexCoord2f(1, 0); glVertex3f(RESOLUTION_X,RESOLUTION_Y,1.0);
  glTexCoord2f(0, 0); glVertex3f(0,RESOLUTION_Y,1.0);

  // End render
  glEnd();

  // Pop current matrix to restore identity matrix
  glPopMatrix();

  // Bind RGB Texture to 2D Texture Primative
  glBindTexture(GL_TEXTURE_2D, gl_rgb_texture);

  // Specify 2D Texture Image (RGB must be handled differently than IR)
  if (current_format == FREENECT_VIDEO_RGB || current_format == FREENECT_VIDEO_YUV_RGB)
    glTexImage2D(GL_TEXTURE_2D, 0, 3, RESOLUTION_X, RESOLUTION_Y, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb_active);
  else
    //? Why is the data offset
    glTexImage2D(GL_TEXTURE_2D, 0, 1, RESOLUTION_X, RESOLUTION_Y, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, rgb_active+RESOLUTION_X*4);

  // Duplicate the identity matrix on the stack
  glPushMatrix();

  // Translate the identity matrix WITH:
  // 1 0 0 960
  // 0 1 0 240
  // 0 0 1  0
  // 0 0 0  1
  glTranslatef(RESOLUTION_X+(RESOLUTION_X/2.0),(RESOLUTION_Y/2.0) ,0.0);

  // Multiply the current matrix with the rotation matrix
  glRotatef(camera_angle, 0.0, 0.0, 1.0);

  // Translate the identity matrix WITH:
  // 1 0 0 -960
  // 0 1 0 -240
  // 0 0 1   0
  // 0 0 0   1
  glTranslatef(-(RESOLUTION_X+(RESOLUTION_X/2.0)),-(RESOLUTION_Y/2.0) ,0.0);

  // Draw a connected group of triangles. One triangle is defined
  // for each vertex presented after the first two vertices.
  // Vertices 1 , n + 1 , and n + 2 define triangle n. N - 2 triangles are drawn.
  glBegin(GL_TRIANGLE_FAN);

  // Set the current color intesity (R, G, B, A)
  // 1.0 (full intensity) - 0.0 (zero intensity).
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

  //? Why is it necessary to specify texture coordinates and polygon vertices
  glTexCoord2f(0, 1); glVertex3f(RESOLUTION_X,0,0);
  glTexCoord2f(1, 1); glVertex3f((RESOLUTION_X*2),0,0);
  glTexCoord2f(1, 0); glVertex3f((RESOLUTION_X*2),RESOLUTION_Y,0);
  glTexCoord2f(0, 0); glVertex3f(RESOLUTION_X,RESOLUTION_Y,0);

  // End render
  glEnd();

  // Pop current matrix to restore identity matrix
  glPopMatrix();

  // Swap the buffers of the current window
  glutSwapBuffers();
}

// Button press handler
void KeyPressed(unsigned char key, int x, int y)
{
  // [Esc] Clean up and exit program
  if (key == 27) {
    die = 1;
    pthread_join(freenect_thread, NULL);
    glutDestroyWindow(window);
    free(depth_mid);
    free(depth_active);
    free(rgb_kinect);
    free(rgb_cache);
    free(rgb_active);
    // Not pthread_exit because OSX leaves a thread lying around and doesn't exit
    exit(0);
  }

  // [w] Tilt Up
  if (key == 'w') {
    freenect_angle++;
    if (freenect_angle > 30) {
      freenect_angle = 30;
    }
    tilt_changed++;
  }

  // [s] Zero Tilt
  if (key == 's') {
    freenect_angle = 0;
    tilt_changed++;
  }

  // [x] Tilt Down
  if (key == 'x') {
    freenect_angle--;
    if (freenect_angle < -30) {
      freenect_angle = -30;
    }
    tilt_changed++;
  }

  // [f] Toggle Video Format
  if (key == 'f') {
    if (requested_format == FREENECT_VIDEO_IR_8BIT)
      requested_format = FREENECT_VIDEO_RGB;
    else if (requested_format == FREENECT_VIDEO_RGB)
      requested_format = FREENECT_VIDEO_YUV_RGB;
    else
      requested_format = FREENECT_VIDEO_IR_8BIT;
  }

  // [e] Toggle Auto Exposure
  if (key == 'e') {
    static freenect_flag_value auto_exposure = FREENECT_OFF;
    auto_exposure = auto_exposure ? FREENECT_OFF : FREENECT_ON;
    freenect_set_flag(f_dev, FREENECT_AUTO_EXPOSURE, auto_exposure);
  }

  // [b] Toggle White Balance
  if (key == 'b') {
    static freenect_flag_value white_balance = FREENECT_OFF;
    white_balance = white_balance ? FREENECT_OFF : FREENECT_ON;
    freenect_set_flag(f_dev, FREENECT_AUTO_WHITE_BALANCE, white_balance);
  }

  // [r] Toggle Raw Color
  if (key == 'r') {
    static freenect_flag_value raw_color = FREENECT_OFF;
    raw_color = raw_color ? FREENECT_OFF : FREENECT_ON;
    freenect_set_flag(f_dev, FREENECT_RAW_COLOR, raw_color);
  }

  // [m] Toggle Mirror Image
  if (key == 'm') {
    static freenect_flag_value mirror = FREENECT_OFF;
    mirror = mirror ? FREENECT_OFF : FREENECT_ON;
    freenect_set_flag(f_dev, FREENECT_MIRROR_DEPTH, mirror);
    freenect_set_flag(f_dev, FREENECT_MIRROR_VIDEO, mirror);
  }

  // [n] Toggle Near Mode
  if (key == 'n') {
    static freenect_flag_value near_mode = FREENECT_OFF;
    near_mode = near_mode ? FREENECT_OFF : FREENECT_ON;
    freenect_set_flag(f_dev, FREENECT_NEAR_MODE, near_mode);
  }

  // [+] Increase IR Brightness
  if (key == '+') {
    uint16_t brightness = freenect_get_ir_brightness(f_dev) + 2;
    if (brightness > 50) brightness = 50;
    freenect_set_ir_brightness(f_dev, brightness);
  }

  // [-] Decrease IR Brightness
  if (key == '-') {
    uint16_t brightness = freenect_get_ir_brightness(f_dev) - 2;
    if (brightness < 1) brightness = 1;
    freenect_set_ir_brightness(f_dev, brightness);
  }

  // [0] Turn LED Off
  if (key == '0') {
    freenect_set_led(f_dev,LED_OFF);
  }

  // [1] Turn LED Green
  if (key == '1') {
    freenect_set_led(f_dev,LED_GREEN);
  }

  // [2] Turn LED Red
  if (key == '2') {
    freenect_set_led(f_dev,LED_RED);
  }

  // [3] Turn LED Yellow
  if (key == '3') {
    freenect_set_led(f_dev,LED_YELLOW);
  }

  // [4|5] Blink LED Green
  if (key == '4' || key == '5') {
    freenect_set_led(f_dev,LED_BLINK_GREEN);
  }

  // [6] Blink LED Red and Yellow
  if (key == '6') {
    freenect_set_led(f_dev,LED_BLINK_RED_YELLOW);
  }

  // [o] Toggle Camera Rotation
  if (key == 'o') {
    camera_rotate = !camera_rotate;
    if (camera_rotate) {
      glDisable(GL_DEPTH_TEST);
    } else {
      glEnable(GL_DEPTH_TEST);
    }
  }

  // Update tilt angle
  if (tilt_changed) {
    freenect_set_tilt_degs(f_dev, freenect_angle);
    tilt_changed = 0;
  }
}

void ReSizeGLScene(int Width, int Height)
{
  glViewport(0,0,Width,Height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho (0, 1280, 0, 480, -5.0f, 5.0f);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

// https://www.opengl.org/resources/libraries/glut/spec3/node10.html
void ConfigureGLUT(void)
{
  // Initialize the GLUT library (consumes command line parameters)
  glutInit(&glut_argc, glut_argv);

  // Select display modes for rendering
  // GLUT_DOUBLE - Select a double-buffered window
  // GLUT_ALPHA  - The RGBA color model does not request any bits of alpha
  //               (sometimes called an alpha buffer or destination alpha)
  //               be allocated without GLUT_ALPHA.
  // GLUT_DEPTH  - Select a window with depth buffer
  // GLUT_RGBA   - Select a window with RGBA mode buffer
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_ALPHA | GLUT_DEPTH);

  // Create a single window capable of displaying a side-by-side comparison
  glutInitWindowSize((RESOLUTION_X*2), RESOLUTION_Y);

  // Start the window in the upper left corner of the screen
  glutInitWindowPosition(0, 0);

  // Create a window titled "LibFreenect"
  window = glutCreateWindow("LibFreenect");

  // Set GLUT callback functions
  glutDisplayFunc(&DrawGLScene);
  glutIdleFunc(&DrawGLScene);
  glutReshapeFunc(&ReSizeGLScene);
  glutKeyboardFunc(&KeyPressed);
}

// https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/
void ConfigureGL(int Width, int Height)
{
  // Select values to be applied when the color buffers are cleard
  glClearColor(
    0.0f, // red
    0.0f, // green
    0.0f, // blue
    0.0f  // alpha
  );

  // Select values to be applied when the depth buffers are cleared
  //glClearDepth(0.0);

  // Select comparison logic for depth functions
  //glDepthFunc(GL_LESS);

  // Set depth buffer to Read-Only
  //glDepthMask(GL_FALSE);

  // Disable depth comparisons
  glDisable(GL_DEPTH_TEST);

  // Disable blending computed fragment color values with buffered color values
  glDisable(GL_BLEND);

  // Disable alpha testing
  glDisable(GL_ALPHA_TEST);

  // Enable 2D texturing
  glEnable(GL_TEXTURE_2D);

  // Select blending logic (disabled)
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Select flat shading
  glShadeModel(GL_FLAT);

  // Configure the depth texture
  glGenTextures(1, &gl_depth_texture);
  glBindTexture(GL_TEXTURE_2D, gl_depth_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // Configure the RGB texture
  glGenTextures(1, &gl_rgb_texture);
  glBindTexture(GL_TEXTURE_2D, gl_rgb_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  ReSizeGLScene(Width, Height)
}

void *gl_threadfunc(void *arg)
{
  printf("GL thread\n");

  // Configure OpenGLUT
  ConfigureGLUT();

  // Configure OpenGL
  ConfigureGL((RESOLUTION_X * 2), RESOLUTION_Y);

  // Window processing loop
  glutMainLoop();

  return NULL;
}

// The Kinect has 11-bit depth image resolution
// 2^11 = 2048
uint16_t t_gamma[2048];

void depth_cb(freenect_device *dev, void *v_depth, uint32_t timestamp)
{
  // Apply structure to depth data to facilitate work
  uint16_t *depth = (uint16_t*)v_depth;

  // Take the mutex
  pthread_mutex_lock(&gl_backbuf_mutex);

  // Loop through depth array data
  for (int i = 0 ; i < (RESOLUTION_X * RESOLUTION_Y) ; ++i) {
    uint16_t depth_value = depth[i];

    // Map the depth value to t_gamma values
    int pval = t_gamma[depth_value];
    int low_byte = pval & 0xff;

    // Examine the pval with the low byte removed
    switch (pval>>8) {
     // white fading to red
     case 0:
      depth_mid[3*i+0] = 255;
      depth_mid[3*i+1] = 255-lb;
      depth_mid[3*i+2] = 255-lb;
      break;
     // red fading to yellow
     case 1:
      depth_mid[3*i+0] = 255;
      depth_mid[3*i+1] = lb;
      depth_mid[3*i+2] = 0;
      break;
     // yellow fading to green
     case 2:
      depth_mid[3*i+0] = 255-lb;
      depth_mid[3*i+1] = 255;
      depth_mid[3*i+2] = 0;
      break;
     // green fading to cyan
     case 3:
      depth_mid[3*i+0] = 0;
      depth_mid[3*i+1] = 255;
      depth_mid[3*i+2] = lb;
      break;
     // cyan fading to blue
     case 4:
      depth_mid[3*i+0] = 0;
      depth_mid[3*i+1] = 255-lb;
      depth_mid[3*i+2] = 255;
      break;
     // blue fading to black
     case 5:
      depth_mid[3*i+0] = 0;
      depth_mid[3*i+1] = 0;
      depth_mid[3*i+2] = 255-lb;
      break;
     // uncategorized values are rendered gray
     default:
      depth_mid[3*i+0] = 128;
      depth_mid[3*i+1] = 128;
      depth_mid[3*i+2] = 128;
      break;
    }
  }

  // Signal depth has been processed
  got_depth++;
  pthread_cond_signal(&gl_frame_cond);

  // Release the mutex
  pthread_mutex_unlock(&gl_backbuf_mutex);
}

void rgb_cb(freenect_device *dev, void *rgb, uint32_t timestamp)
{
  // Take the mutex
  pthread_mutex_lock(&gl_backbuf_mutex);

  // swap buffers
  assert (rgb_kinect == rgb);
  rgb_kinect = rgb_cache;

  // Set the current middle buffer as the new storage buffer
  // and share the storage buffer to OpenGL via the middle buffer
  freenect_set_video_buffer(dev, rgb_kinect);
  rgb_cache = (uint8_t*)rgb;

  // Signal depth has been processed
  got_rgb++;
  pthread_cond_signal(&gl_frame_cond);

  // Release the mutex
  pthread_mutex_unlock(&gl_backbuf_mutex);
}

void *freenect_threadfunc(void *arg)
{
  int accelerometer_delay = 0;

  // Update the Kinect tilt angle
  freenect_set_tilt_degs(f_dev,freenect_angle);

  // Set the color of the LED on the face of the device to red.
  // LED_OFF, LED_GREEN, LED_RED, LED_YELLOW, LED_BLINK_GREEN, LED_BLINK_RED_YELLOW
  freenect_set_led(f_dev,LED_RED);

  // Set the callback to process depth information
  freenect_set_depth_callback(f_dev, depth_cb);

  // Set the callback to process video information
  freenect_set_video_callback(f_dev, rgb_cb);

  // Fetch a video mode descriptor matching the specified resolution and format
  freenect_frame_mode new_video_mode = freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, current_format);

  // Set the specified video mode
  freenect_set_video_mode(f_dev, new_video_mode);

  // Fetch a depth mode descriptor matching the specified resolution and format
  freenect_frame_mode new_depth_mode = freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT);

  // Set the specified depth mode
  freenect_set_depth_mode(f_dev, new_depth_mode);

  // Associate the video buffer
  freenect_set_video_buffer(f_dev, rgb_kinect);

  // Start the depth information stream
  freenect_start_depth(f_dev);

  // Start the video information stream
  freenect_start_video(f_dev);

  // Print user controls
  printf("'w' - tilt up, 's' - level, 'x' - tilt down, '0'-'6' - select LED mode, '+' & '-' - change IR intensity \n");
  printf("'f' - change video format, 'm' - mirror video, 'o' - rotate video with accelerometer \n");
  printf("'e' - auto exposure, 'b' - white balance, 'r' - raw color, 'n' - near mode (K4W only) \n");

  // Process USB video stream
  while (!die && freenect_process_events(f_ctx) >= 0) {
    // Throttle the text update by only sampling every 2000th loop
    if (accelerometer_delay++ >= 2000)
    {
      freenect_raw_tilt_state * state;
      double dx,dy,dz;

      // Reset the counter
      accelerometer_delay = 0;

      // Read tilt state from Kinect
      freenect_update_tilt_state(f_dev);

      // Pull tilt state from device handle
      state = freenect_get_tilt_state(f_dev);

      // Pull accelerometer data from tilt state
      freenect_get_mks_accel(state, &dx, &dy, &dz);

      // Report accelerometer readings
      printf("\r raw accelerometer reading: %4d %4d %4d | mks accelerometer reading: %4f %4f %4f", state->accelerometer_x, state->accelerometer_y, state->accelerometer_z, dx, dy, dz);
      fflush(stdout);
    }

    // Update video format if change is requested (video must be stopped in order to change modes)
    if (requested_format != current_format) {
      freenect_stop_video(f_dev);
      freenect_set_video_mode(f_dev, freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, requested_format));
      freenect_start_video(f_dev);
      current_format = requested_format;
    }
  }

  // Shutdown Procedure
  ///////////////////////

  printf("\nshutting down streams...\n");

  freenect_stop_depth(f_dev);
  freenect_stop_video(f_dev);

  freenect_close_device(f_dev);
  freenect_shutdown(f_ctx);

  printf("-- done!\n");
  return NULL;
}

int main(int argc, char **argv)
{
  // Used to capture the result (error code) of call to pthread_create
  // SUCCESS (0), EAGAIN, EINVAL, EPERM
  int result;

  // Allocate depth image arrays in (RESOLUTION_X * RESOLUTION_Y) resolution
  // (3 channels - synthesized R,G,B based on depth data heatmap)
  depth_mid = (uint8_t*)malloc(RESOLUTION_X*RESOLUTION_Y*3);
  depth_active = (uint8_t*)malloc(RESOLUTION_X*RESOLUTION_Y*3);

  // Allocate video image arrays in (RESOLUTION_X * RESOLUTION_Y) resolution (3 channels - R,G,B)
  rgb_kinect = (uint8_t*)malloc(RESOLUTION_X*RESOLUTION_Y*3);
  rgb_cache = (uint8_t*)malloc(RESOLUTION_X*RESOLUTION_Y*3);
  rgb_active = (uint8_t*)malloc(RESOLUTION_X*RESOLUTION_Y*3);

  printf("Kinect camera test\n");

  // Load the t_gamma array with color values to represent 11-bit (2^11 or 0 - 2047) depth data (generates a heat map)
  for (int i=0; i<2048; ++i) {
    // Generate a floating-point value linearly increasing from 0 to 1
    float v = i/2048.0;
    // Generate a floating-point value logarithmically increasing from 0 to 6
    v = powf(v, 3) * 6;
    // Load t_gamma array with value scaled by 1536 [0 9216]
    t_gamma[i] = (v * 6 * 256);
  }

  // Pass command line arguments to GLUT
  glut_argc = argc;
  glut_argv = argv;

  // Initialize the working context (or exit)
  if (freenect_init(&f_ctx, NULL) < 0) {
    printf("freenect_init() failed\n");
    return 1;
  }

  // Set logging level
  freenect_set_log_level(f_ctx, FREENECT_LOG_DEBUG);

  // Select level of device control
  freenect_select_subdevices(f_ctx, (freenect_device_flags)(FREENECT_DEVICE_MOTOR | FREENECT_DEVICE_CAMERA));

  // Detect number of devices available
  int number_of_devices = freenect_num_devices (f_ctx);
  printf ("Number of devices found: %d\n", number_of_devices);

  // Parse command line parameter (integer) to select device (defaults to 0 if not provided)
  int user_device_number = 0;
  if (argc > 1)
    user_device_number = atoi(argv[1]);

  // If no devices were detected, then cleanup and exit
  if (number_of_devices < 1) {
    freenect_shutdown(f_ctx);
    return 1;
  }

  // Attach to Kinect (or cleanup and exit)
  if (freenect_open_device(f_ctx, &f_dev, user_device_number) < 0) {
    printf("Could not open device\n");
    freenect_shutdown(f_ctx);
    return 1;
  }

  // Create the freenect image processing thread (with default attributes)
  result = pthread_create(
    &freenect_thread,
    NULL, // Default thread attributes
    freenect_threadfunc,
    NULL // Empty thread function parameters
  );

  // Ensure freenect image processing thread started successfully
  if (result) {
    printf("pthread_create failed\n");
    freenect_shutdown(f_ctx);
    return 1;
  }

  // OS X requires GLUT to run on the main thread
  gl_threadfunc(NULL);

  return 0;
}
