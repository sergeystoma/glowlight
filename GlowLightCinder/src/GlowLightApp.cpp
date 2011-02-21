#include "cinder/app/AppBasic.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Texture.h"
#include "cinder/gl/Fbo.h"
#include "cinder/Surface.h"
#include "cinder/Capture.h"
#include "cinder/Camera.h"
#include "cinder/Serial.h"
#include "cinder/Text.h"
#include "cinder/Timer.h"

using namespace ci;
using namespace ci::app;
using namespace std;

// Frames per second.
static const int FPS = 10;
// Size of the image capture from camera.
static const int WIDTH = 512, HEIGHT = 512;
// Number of image smoothing steps, each reduced the image size twice.
static const int SMOOTH = 1;
// Number of control points for sample lines.
static const int CONTROL_POINTS = 4;
// Number of LEDs on the other end.
static const int GLOW_COLORS = 16;
// Size of displayed color sample.
static const int BLOCK_SIZE = 20;

// Stores color samples along the straigh line.
class SampleLine
{
public:
  // Initialize default instance.
  SampleLine()
    : _points(0)
  {
  }

  // Initialize using line segment and number of steps.
  SampleLine(int points, cinder::Vec2f start, cinder::Vec2f end)
    : _points(points), _start(start), _end(end)
  {
    _colors.resize(points);
  }

  // Sample colors on the segment.
  void sample()
  {
    cinder::Vec2f delta = (_end - _start) * (1 / (float)(_points - 1));

    cinder::Vec2f current = _start;

    // Read pixels from the currently bound frame buffer.      
    // Assumes that currenty bound buffer is the smallest in the blur sequence.
    int width = WIDTH / (1 << SMOOTH) - 1;
    int height = HEIGHT / (1 << SMOOTH) - 1;

    for (int i = 0; i < _points; ++i)
    {
      int x = width - (int)(current.x * width);
      int y = (int)(current.y * height);

      unsigned char pixel[4];      
      glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);      
      Color sample = Color(pixel[0] / 255.0f, pixel[1] / 255.0f, pixel[2] / 255.0f);

      // Slowly fade colors, reducing flicker effects.
      _colors[i] = _colors[i] * 0.9f +  sample * 0.1f;

      current += delta;
    }
  }

  // Gets sampled color at given point.
  cinder::Color get_color(int i)
  {
    return _colors[i];
  }   

  // Gets sampled color position.
  cinder::Vec2f get_position(int i)
  {
    return _start + (_end - _start) * (1 / (float)(_points - 1)) * i;
  }

  // Renders sampled colors.
  void draw(int width, int height)
  {
    const int size = BLOCK_SIZE;

    for (int i = 0; i < _points; ++i)
    {
      cinder::Vec2f p = get_position(i) * cinder::Vec2f(width, height);

      // Draw frame around the color box.
      gl::color(Color::black());
      gl::drawSolidRect(Rectf(p.x - size - 3, p.y - size - 3, p.x + size + 3, p.y + size + 3));
      gl::color(Color(0.5f, 0.5f, 0.5f));
      gl::drawSolidRect(Rectf(p.x - size - 2, p.y - size - 2, p.x + size + 2, p.y + size + 2));
      gl::color(Color::black());
      gl::drawSolidRect(Rectf(p.x - size - 1, p.y - size - 1, p.x + size + 1, p.y + size + 1));

      gl::color(get_color(i));
      gl::drawSolidRect(Rectf(p.x - size, p.y - size, p.x + size, p.y + size));
    }    
  }
  
private:
  vector<cinder::Color> _colors;
  
  cinder::Vec2f _start;
  cinder::Vec2f _end;

  int _points;
};

// Cinder based app.
class GlowLightAppApp : public AppBasic 
{
public:
  GlowLightAppApp()
    : _show_capture(true), _point_count(0), _flip(true), _show_smooth(true), _arduino_connected(false), _show_sampled(true)
  {
  }
  
  // Initializes everything.
  void setup();
  // Handle mouse events.
  void mouseDown(MouseEvent event);	
  // Handle key events.
  void keyDown(KeyEvent event);
  // Process frame.
  void update();
  // Draw scene.
  void draw();

private: 
  // Initializes sampling buffers.
  void setup_samplers();
  // Initializes serial communications.
  void setup_serial();
  // Initializes webcam capture. 
  void setup_capture();
  // Initializes timer.
  void setup_timer();

  // Processes captured webcam image.
  void process_capture();
  // Prepares color array for Arduino from webcam image.
  void prepare_sampled_colors();
  // Prepares color array for Arduino using demo mode.
  void prepare_demo_colors();
  // Sends colors to Arduino.
  void send_colors();

  // Adjusts color for display.
  cinder::Color adjust_color(cinder::Color color);

private:
  // Webcam images.
  vector<Capture>	_captures;
  vector<gl::Texture>	_textures;
    
  // Main captured image fbo.
  gl::Fbo _fbo;
  // Blur sequence.
  gl::Fbo _smooth_fbo[SMOOTH];

  // Indicates whether to render anything at all.
  bool _show_capture;
  // Show blurred image vs. the original.
  bool _show_smooth;
  // Indicates whether left and right sides of controlled LEDs chain must be flipped.
  bool _flip;
  // Indicates whether to draw sampled colors.
  bool _show_sampled;

  // Coordicates of control points.
  cinder::Vec2f _points[CONTROL_POINTS];
  int _point_count;

  // Sampled line segments.
  SampleLine _lines[CONTROL_POINTS - 1];

  // Color array to be sent to Arduino.
  cinder::Color _colors[GLOW_COLORS];

  // Serial port for communicating with Arduino.
  cinder::Serial _arduino;
  bool _arduino_connected;

  // Timer.
  cinder::Timer _timer;
  // Seconds counter at the last frame, used to calculate delta.
  double _seconds;
};

void GlowLightAppApp::setup()
{
  setup_samplers();
  setup_serial();
  setup_capture();
  setup_timer();
}      

void GlowLightAppApp::setup_timer()
{
  _timer.start();
  _seconds = _timer.getSeconds();
}

void GlowLightAppApp::setup_serial()
{
  // Find Arduino serial port.
  cinder::Serial::Device device = Serial::findDeviceByNameContains("Arduino");
  if (!device.getPath().empty())
  {
    _arduino_connected = true;
    _arduino = cinder::Serial(device, 9600); 
  }
}

void GlowLightAppApp::setup_capture()
{
  vector<Capture::DeviceRef> devices(Capture::getDevices());

	for (vector<Capture::DeviceRef>::const_iterator device_iterator = devices.begin(); device_iterator != devices.end(); ++device_iterator) 
  {
		Capture::DeviceRef device = *device_iterator;
    console() << "Found Device " << device->getName() << " ID: " << device->getUniqueId() << std::endl;

		try 
    {
			if (device->checkAvailable()) 
      {
        if (_captures.empty())
        {
				  _captures.push_back(Capture(WIDTH, HEIGHT, device));
          _captures.back().start();			
				  _textures.push_back(gl::Texture());
        }
			}
      else
			  console() << "device is NOT available" << std::endl;
		}
		catch (CaptureExc &) 
    {
			console() << "Unable to initialize device: " << device->getName() << endl;
		}
	}
}

void GlowLightAppApp::setup_samplers()
{
  gl::Fbo::Format format;	
  _fbo = gl::Fbo(WIDTH, HEIGHT, format);
  
  for (int i = 0; i < SMOOTH; ++i)
    _smooth_fbo[i] = gl::Fbo(WIDTH / (1 << (i + 1)), HEIGHT / (1 << (i + 1)), format);
}

void GlowLightAppApp::mouseDown(MouseEvent event)
{
  // Reset control points if all segments were built previously.
  if (_point_count == CONTROL_POINTS)
    _point_count = 0;  
  
  // Normalize click coordinates, helps with different window and capture sizes.
  _points[_point_count++] = cinder::Vec2f(
    event.getX() / (float)getWindowWidth(),
    event.getY() / (float)getWindowHeight());

  // If there are enough control points, create sample segments.
  if (_point_count == CONTROL_POINTS)
  {
    _lines[0] = SampleLine(5, _points[0], _points[1]);
    _lines[1] = SampleLine(8, _points[1], _points[2]);
    _lines[2] = SampleLine(5, _points[2], _points[3]);
  }
}

void GlowLightAppApp::keyDown(KeyEvent event)
{
  // Toggle fullscreen.
  if (event.getChar() == 'f')
    setFullScreen(!isFullScreen());
  // Show captured or blurred image.	  
  if (event.getChar() == 's')
    _show_capture = !_show_capture;
  // Flip left-right sides on LEDs. Screen is unaffected.
  if (event.getChar() == 'm')
    _flip = !_flip;
  // Show blurred image instead of captured original.
  if (event.getChar() == 'b')
    _show_smooth = !_show_smooth;
  // Show sampled colors.
  if (event.getChar() == 'c')
    _show_sampled = !_show_sampled;
}

void GlowLightAppApp::update()
{
  for (vector<Capture>::iterator capture = _captures.begin(); capture != _captures.end(); ++capture)
  {
		if (capture->checkNewFrame())
    {
			Surface8u surface = capture->getSurface();
      _textures[std::distance(_captures.begin(), capture)] = gl::Texture(surface);
		}
	}
  
  // Process captured image.
  process_capture();

  // If there is enough time passed between data packets, create and submit color arrays.
  double now = _timer.getSeconds();
  if (now - _seconds > 1 / (float)FPS)
  {
    if (_point_count == CONTROL_POINTS)
      prepare_sampled_colors();
    else
      prepare_demo_colors();

    _seconds = now;
  }
}

cinder::Color GlowLightAppApp::adjust_color(cinder::Color color)
{
  cinder::Vec3f hsv = cinder::rgbToHSV(color);

  // Adjust gamma for ShiftBrites.
  float s = powf(hsv.y, 0.75f);
  float v = powf(hsv.z, 1.5f);

  return cinder::Color(CM_HSV, hsv.x, s, v);
}

void GlowLightAppApp::prepare_sampled_colors()
{
  int p = 0;

  for (int i = 0; i < 4; ++i)
    _colors[p++] = adjust_color(_lines[0].get_color(i));

  for (int i = 0; i < 7; ++i)
    _colors[p++] = adjust_color(_lines[1].get_color(i));

  for (int i = 0; i < 5; ++i)
    _colors[p++] = adjust_color(_lines[2].get_color(i));

  send_colors();
}

void GlowLightAppApp::prepare_demo_colors()
{
  float time = (float)_timer.getSeconds();

  for (int i = 0; i < GLOW_COLORS; ++i)
    _colors[i] = Color(CM_HSV, (sinf(time + i * 0.1f) + 1.0f) * 0.5f, 1.0f, sinf(time - i * 0.033f) * 0.5f + 0.5f);

  send_colors();
}

void GlowLightAppApp::send_colors()
{
  static std::vector<char> buffer;

  buffer.clear();

  // Packet signature.
  buffer.push_back('G');
  buffer.push_back('L');
  buffer.push_back('O');
  buffer.push_back('W');

  buffer.push_back((char)GLOW_COLORS);

  if (_flip)
  {
    for (int i = GLOW_COLORS - 1; i >= 0; --i)
    {
      buffer.push_back((char)(_colors[i].r * 255));
      buffer.push_back((char)(_colors[i].g * 255));
      buffer.push_back((char)(_colors[i].b * 255));
    }
  }
  else
  {
    for (int i = 0; i < GLOW_COLORS; ++i)
    {
      buffer.push_back((char)(_colors[i].r * 255));
      buffer.push_back((char)(_colors[i].g * 255));
      buffer.push_back((char)(_colors[i].b * 255));
    }
  }  

  if (_arduino_connected)
    _arduino.writeBytes(&*buffer.begin(), buffer.size());  
}

void GlowLightAppApp::process_capture()
{
  // Render initial captured image.
  {
    gl::SaveFramebufferBinding bindingSaver;

    _fbo.bindFramebuffer();

    gl::setViewport(_fbo.getBounds()) ;
    CameraOrtho camera = CameraOrtho(0, WIDTH, 0, HEIGHT, -1, 1);
    gl::setMatrices(camera);

    gl::color(ColorA(1.0f, 1.0f, 1.0f, 1.0f));

    for (vector<Capture>::iterator capture = _captures.begin(); capture != _captures.end(); ++capture)
    {	
		  if (_textures[std::distance(_captures.begin(), capture)])
			  gl::draw(_textures[std::distance(_captures.begin(), capture)], Rectf(0, 0, WIDTH, HEIGHT));
	  }
  }

  gl::Fbo* previous = &_fbo;

  // Cascaded down-sampling for blur.
  for (int i = 0; i < SMOOTH; ++i)
  {
    gl::SaveFramebufferBinding bindingSaver;

    _smooth_fbo[i].bindFramebuffer();

    int width = WIDTH / (1 << (i + 1));
    int height = HEIGHT / (1 << (i + 1));

    gl::setViewport(Area(0, 0, width, height));
    CameraOrtho camera = CameraOrtho(0, width, 0, height, -1, 1);
    gl::setMatrices(camera);

    previous->bindTexture();
    gl::draw(previous->getTexture(), Rectf(0, 0, width, height));    

    previous = &_smooth_fbo[i];

    // Sample line segments if we just rendered the last reduced image.
    if (i == SMOOTH - 1)
    {
      if (_point_count == CONTROL_POINTS)
      {
        _lines[0].sample();
        _lines[1].sample();
        _lines[2].sample();
      }
    }
  }    
}

void GlowLightAppApp::draw()
{
	gl::clear(Color(0, 0, 0)); 

  if (_captures.empty())
		return;             
	
  if (_show_capture)
  {
    gl::setViewport(getWindowBounds());

    CameraOrtho camera = CameraOrtho(0, getWindowWidth(), getWindowHeight(), 0, -1, 1);
    gl::setMatrices(camera);

	  float width = getWindowWidth();	
	  float height = getWindowHeight();

    cinder::Vec2f bounds(width, height);

    // Draw captured texture.
	  if (_show_smooth)
    {
	    _smooth_fbo[SMOOTH - 1].bindTexture();
      gl::draw(_smooth_fbo[SMOOTH - 1].getTexture(), Rectf(width, 0, 0, height));
    }
    else
    {
       _fbo.bindTexture();
      gl::draw(_fbo.getTexture(), Rectf(width, 0, 0, height));
    }

    // Draw lines between control points.
    gl::color(Color::white());
    if (_point_count > 1)
      gl::drawLine(_points[0] * bounds, _points[1] * bounds);
    if (_point_count > 2)
      gl::drawLine(_points[1] * bounds, _points[2] * bounds);
    if (_point_count > 3)
      gl::drawLine(_points[2] * bounds, _points[3] * bounds);

    // Draw colors.
    if (_point_count == CONTROL_POINTS)
    {
      _lines[0].draw(width, height);
      _lines[1].draw(width, height);
      _lines[2].draw(width, height);
    }
    
    // Draw control points.
    if (_point_count > 0)
    {
      gl::color(ColorA(1.0f, 0.0f, 0.0f, 0.7f));
      gl::drawSolidCircle(_points[0] * bounds, 4.0f);
    }
    if (_point_count > 1)
    {
      gl::color(ColorA(0.0f, 1.0f, 0.0f, 0.7f));
      gl::drawSolidCircle(_points[1] * bounds, 4.0f);      
    }
    if (_point_count > 2)
    {
      gl::color(ColorA(0.0f, 0.0f, 1.0f, 0.7f));
      gl::drawSolidCircle(_points[2] * bounds, 4.0f);
    }
    if (_point_count > 3)
    {
      gl::color(ColorA(1.0f, 1.0f, 0.0f, 0.7f));
      gl::drawSolidCircle(_points[3] * bounds, 4.0f);
    }
  }
}

CINDER_APP_BASIC(GlowLightAppApp, RendererGl)
