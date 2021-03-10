

/*
json setup 
beware the keywords are parsed so it will have to be different keywords for all options
=======
{
    "accessory": "HttpPushRgb",
    "name": "LED X",
    "service": "Light",
    "timeout": 3000,
    "switch": {
        "status": "http://192.168.0.91:80/status",
        "powerOn": "http://192.168.0.91:80/on",
        "powerOff": "http://192.168.0.91:80/off"
    },
    "color": {
        "status": "http://192.168.0.91:80/color",
        "url": "http://192.168.0.91:80/set/%s"
    },
    "brightness": {
        "status": "http://192.168.0.91:80/brightness",
        "url": "http://192.168.0.91:80/light/%s"
    }
}
=======
 */

//#include <WiFi.h>
#include <analogWrite.h>
#include <math.h>

// code version
String codetitle = "HomeBridge LED Strip server OTA - ESP32";
String codeversion = "0.1.2";

// serial print activated
int serialdebug = 1;

// pin set up
#define redPin 13 //D7 - Red channel
#define grnPin 12 //D6 - Green channel
#define bluPin 14 //D5 - Blue channel

// server set-up
WiFiServer server(80); //Set server port

// ititial variables
String readString;           //String to hold incoming request
String hexString = "000000"; //Define inititial color here (hex value)

String decString = "000"; //Define inititial brightness here (dec value)

int state; // state of the LED (0 or 1)

int r;
int g;
int b;

float R;
float G;
float B;

int x; // placeholder for calculating the brightness (max of R and G)
int V; // value of brightness (max of x and B)


// Serial print 
/*
void debugserialprint (String text, format) {
  Serial.print(text, format);

  
}
*/

// RGB HSV routines
// ***************************

// 
typedef struct RgbColor
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
} RgbColor;

typedef struct HsvColor
{
    unsigned char h;
    unsigned char s;
    unsigned char v;
} HsvColor;


// convert HSV to RGB
RgbColor HsvToRgb(HsvColor hsv)
{
    RgbColor rgb;
    unsigned char region, remainder, p, q, t;

    if (hsv.s == 0)
    {
        rgb.r = hsv.v;
        rgb.g = hsv.v;
        rgb.b = hsv.v;
        return rgb;
    }

    region = hsv.h / 43;
    remainder = (hsv.h - (region * 43)) * 6; 

    p = (hsv.v * (255 - hsv.s)) >> 8;
    q = (hsv.v * (255 - ((hsv.s * remainder) >> 8))) >> 8;
    t = (hsv.v * (255 - ((hsv.s * (255 - remainder)) >> 8))) >> 8;

    switch (region)
    {
        case 0:
            rgb.r = hsv.v; rgb.g = t; rgb.b = p;
            break;
        case 1:
            rgb.r = q; rgb.g = hsv.v; rgb.b = p;
            break;
        case 2:
            rgb.r = p; rgb.g = hsv.v; rgb.b = t;
            break;
        case 3:
            rgb.r = p; rgb.g = q; rgb.b = hsv.v;
            break;
        case 4:
            rgb.r = t; rgb.g = p; rgb.b = hsv.v;
            break;
        default:
            rgb.r = hsv.v; rgb.g = p; rgb.b = q;
            break;
    }

    return rgb;
}

// convert RGB to HSV
HsvColor RgbToHsv(RgbColor rgb)
{
    HsvColor hsv;
    unsigned char rgbMin, rgbMax;

    rgbMin = rgb.r < rgb.g ? (rgb.r < rgb.b ? rgb.r : rgb.b) : (rgb.g < rgb.b ? rgb.g : rgb.b);
    rgbMax = rgb.r > rgb.g ? (rgb.r > rgb.b ? rgb.r : rgb.b) : (rgb.g > rgb.b ? rgb.g : rgb.b);

    hsv.v = rgbMax;
    if (hsv.v == 0)
    {
        hsv.h = 0;
        hsv.s = 0;
        return hsv;
    }

    hsv.s = 255 * long(rgbMax - rgbMin) / hsv.v;
    if (hsv.s == 0)
    {
        hsv.h = 0;
        return hsv;
    }

    if (rgbMax == rgb.r)
        hsv.h = 0 + 43 * (rgb.g - rgb.b) / (rgbMax - rgbMin);
    else if (rgbMax == rgb.g)
        hsv.h = 85 + 43 * (rgb.b - rgb.r) / (rgbMax - rgbMin);
    else
        hsv.h = 171 + 43 * (rgb.r - rgb.g) / (rgbMax - rgbMin);

    return hsv;
}

// returns Hex value from rgb
unsigned long createRGB(int r, int g, int b)
{   
    return ((r & 0xff) << 16) + ((g & 0xff) << 8) + (b & 0xff);
}


// LED routines
// ***********************

//Turn all Off
void allOff() {
  state = 0;
  analogWrite(redPin, 0);
  analogWrite(grnPin, 0);
  analogWrite(bluPin, 0);
}

//Write requested hex-color to the pins
void setHex() {
  // this reads the value from hexString as set
  state = 1;
  long number = (long) strtol( &hexString[0], NULL, 16);
  r = number >> 16;
  g = number >> 8 & 0xFF;
  b = number & 0xFF;
  analogWrite(redPin, (r));
  analogWrite(grnPin, (g));
  analogWrite(bluPin, (b));
}

//Computes and returns current brightness value
void getV() {
  R = roundf(r / 2.55);
  G = roundf(g / 2.55);
  B = roundf(b / 2.55);
  x = _max(R, G);
  V = _max(x, B);
}

//Set new brightness value
void setV() {
  state = 1; // we turn on
  int brightness = (int) atoi( &decString[0]);
  Serial.print("Brightness requested: ");
  Serial.print(brightness);
  Serial.println("%");
  Serial.print("Current RGB: ");
  
  Serial.print(r);
  Serial.print(".");
  Serial.print(g);
  Serial.print(".");
  Serial.println(b);
  if (brightness == 0) {
    state = 0; // turn it off, if brightness is 0
    r = 0;
    g = 0;
    b = 0;
  }
  else
  { 
    RgbColor rgb; // we'll convert RGB to HSV to find what new V value should be
    rgb.r = r;
    rgb.g = g;
    rgb.b = b;
    HsvColor hsv = RgbToHsv(rgb);
    hsv.v = brightness * 2.55;
    
    Serial.print("V for HSV: ");
    Serial.println(hsv.v);
    RgbColor rgb2 = HsvToRgb(hsv);
    r = rgb2.r;
    g = rgb2.g;
    b = rgb2.b;
    
  }
  
  Serial.print("New RGB: ");
  Serial.print(r);
  Serial.print(".");
  Serial.print(g);
  Serial.print(".");
  Serial.println(b);
  // reset hexString to reflect new color rgb in Hex
  hexString = String(createRGB(r, g, b), HEX);
  Serial.print("Updated Hex string: ");
  Serial.println(hexString);

  // set the pins
  analogWrite(redPin, (r));
  analogWrite(grnPin, (g));
  analogWrite(bluPin, (b));

}

//For serial debugging only
void showValues() {
  Serial.print("Status on/off: ");
  Serial.println(state);
  Serial.print("RGB color: ");
  Serial.print(r);
  Serial.print(".");
  Serial.print(g);
  Serial.print(".");
  Serial.println(b);
  Serial.print("Hex color: ");
  Serial.println(createRGB(r, g, b));
  
  Serial.println(String(createRGB(r, g, b), HEX));
  Serial.print("Value of string passed in previous queries (either hex color or brightness requested): ");
  Serial.println(hexString);
  Serial.println(decString);
  getV();
  Serial.print("Calculated Brightness: ");
  Serial.println(V);
}

// Initial setup (called by main code setup() )
//********************
void setup_run() {
  Serial.begin(115200);
  Serial.print(codetitle);
  Serial.print(" ");
  Serial.println(codeversion);
  setHex(); //Set initial color after booting. Value defined above
  //WiFi.mode(WIFI_STA);
  //WiFiStart();
  //showValues(); //Uncomment for serial output
  server.begin();
  Serial.println("setup done from sub-program");
}

// Loop part (called by main code loop() ) 
//********************
void main_run() {

  //Serial.println("Running main_run in loop");
  WiFiClient client = server.available();

  //Respond on certain Homebridge HTTP requests (parsing the http request string)
  // ! beware this has to do text matching, not exactly neat
  // TODO : a 400 response to unexpected request 
  if (client) {
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (readString.length() < 100) {
          readString += c;
        }
        if (c == '\n') {
          Serial.print("Request: "); //Uncomment for serial output
          Serial.println(readString); //Uncomment for serial output

          //Send reponse // we should also send 400 on error
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println();

          // TODO : switch case on matched string so that we can wrap it and do a error response too
          //On
          if (readString.indexOf("on") > 0) {
            setHex();
            showValues();
          }

          //Off
          if (readString.indexOf("off") > 0) {
            
            Serial.println("Requesting OFF");
            allOff();
            showValues();
          }

          //Set color
          if (readString.indexOf("set") > 0) {
            
            Serial.println("Requesting color change");
            hexString = "";
            hexString = (readString.substring(9, 15));
            setHex();
            showValues();
            }
            
          //Set brightness
          if (readString.indexOf("light") > 0) {
            
            Serial.println("Requesting Brightness change");
            //Serial.print("index: ");
            //Serial.println(readString.indexOf("light"));
            Serial.print("Set brightness: ");
            decString = "";
            decString = (readString.substring(11));
            Serial.println(decString);
            setV();
            showValues();
          }

          //Status on/off
          if (readString.indexOf("status") > 0) {
            Serial.println("Requesting LED status => ");
            Serial.println(state);
            
            client.println(state);
          }

          //Status color (hex)
          // this was sending back the last set value of the hexstring which is obviously wrong, it should get the current r g b
          if (readString.indexOf("color") > 0) {
            //client.println(hexString);
            
            Serial.println("Requesting color status");
            Serial.print("RGB color: ");
            Serial.print(r);
            Serial.print(".");
            Serial.print(g);
            Serial.print(".");
            Serial.println(b);
            Serial.println(createRGB(r, g, b), HEX);
            client.print(createRGB(r, g, b), HEX);
            //client.print(b, "HEX"); 
          }

          //Status brightness (%)
          if (readString.indexOf("brightness") > 0) {
            Serial.print("Requesting brightness status => ");
            Serial.println(V);
            
            getV();
            client.println(V);
          }
          // Version code
          if (readString.indexOf("version") > 0) {
            Serial.print("Version request ");
            Serial.print(codetitle);
            Serial.print(" - ");
            Serial.println(codeversion);
            client.print(codetitle);
            client.print("-");
            client.println(codeversion);
          }
          

          delay(1);
          client.stop();
          readString = "";

           
        }
      }
    }
  }
}
