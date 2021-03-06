

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

// TODO: correct hsv rgb code to account for potential arduino overflow as mentioned here https://stackoverflow.com/a/22120275/5510832

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

/*******************************************************************
 * Code to convert RGB value to HSV and back in order to calculate expected V as brightness
 * code taken from https://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both
*******************************************************************/


// required variables
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



//Compute current brightness value
void getV() {
  R = roundf(r/2.55);
  G = roundf(g/2.55);
  B = roundf(b/2.55);
  x = _max(R,G);
  V = _max(x, B);
  decString = String(V); // save the value in string variable
  
}

//Write requested hex-color to the pins
void setHex() {
  state = 1;
  Serial.print("Recorded hexstring: "); // uncomment for serial output
  Serial.println(hexString); // uncomment for serial output
  long number = (long) strtol( &hexString[0], NULL, 16);
  r = number >> 16;
  g = number >> 8 & 0xFF;
  b = number & 0xFF;
  analogWrite(redPin, (r));
  analogWrite(grnPin, (g));
  analogWrite(bluPin, (b));
  getV(); // update the brightness from new color
}



//Set new brightness value
void setV() {

  int brightness = (int) atoi( &decString[0]); // get the numer from the string passed (atoi will stop at any non number char)
  decString = String(V); // save the number value back to conserve the brightness as set
//  Serial.print("Brightness requested: "); // uncomment for serial output
//  Serial.print(brightness); // uncomment for serial output
//  Serial.println("%"); // uncomment for serial output
//  Serial.print("Current RGB: "); // uncomment for serial output
//  
//  Serial.print(r); // uncomment for serial output
//  Serial.print("."); // uncomment for serial output
//  Serial.print(g); // uncomment for serial output
//  Serial.print("."); // uncomment for serial output
//  Serial.println(b); // uncomment for serial output

// get the HSV equivalent of current color, recalculate V and back to RGB

  if (brightness == 0) {
    // this will send /off we shoud not set the color to 0
    
    
  }
  else {
    //state = 1; // we might not need this as brightness setup does turn on
    RgbColor rgb;
    rgb.r = r;
    rgb.g = g;
    rgb.b = b;
    HsvColor hsv = RgbToHsv(rgb);
    hsv.v = brightness * 2.55; // convert 0-100 to 0-255
//    Serial.print("V for HSV: "); // uncomment for serial output
//    Serial.println(hsv.v); // uncomment for serial output
    RgbColor rgb2 = HsvToRgb(hsv);
    
    hexString = String(createRGB(rgb2.r, rgb2.g, rgb2.b), HEX); // change the ref for hexstring
    //setHex(); // will refer to hexString value
    // above is not needed as brightness set via homekit request is always followed by /on request, which will call setHex() and use hexstring
   
    // reset hexString to reflect new color rgb in Hex 
    // should not change from previous if brightness 0
//  hexString = String(createRGB(r, g, b), HEX);
 
//  Serial.print("New RGB: "); // uncomment for serial output
//  Serial.print(r); // uncomment for serial output
//  Serial.print("."); // uncomment for serial output
//  Serial.print(g); // uncomment for serial output
//  Serial.print("."); // uncomment for serial output
//  Serial.println(b); // uncomment for serial output
//  Serial.print("Updated Hex string: "); // uncomment for serial output
//  Serial.println(hexString); // uncomment for serial output

    // set the pins // not needed, sethex does it
//  analogWrite(redPin, (r));
//  analogWrite(grnPin, (g));
//  analogWrite(bluPin, (b));

  }
  
//  showValues(); //Uncomment for serial output
}


//For serial debugging only
void showValues() {
  
  getV();
  
  Serial.print("Status on/off: ");
  Serial.println(state);
  Serial.print("RGB color:");
  Serial.print(r);
  Serial.print(".");
  Serial.print(g);
  Serial.print(".");
  Serial.println(b);
  Serial.print("RGB color: full value in decimal ");
  Serial.println(createRGB(r, g, b));
  Serial.print("RGB color: #");
  Serial.println(String(createRGB(r, g, b), HEX));
  Serial.print("Value of hex color memorized: #");
  Serial.println(hexString);
  Serial.print("Value of brightness memorized: ");
  Serial.println(decString);
  Serial.print("Current Brightness: ");
  Serial.println(V);
  Serial.println("");
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
          
                    
          /*********************
          * commands:
          * - on
          * - off
          * - set/[RGB HEX color value]
          * - light/[Decimal 0-100 brightness value]
          *********************/
          
          //On
          if ((readString.indexOf("on") > 0 ) && (readString.indexOf("favicon") < 0) && (readString.indexOf("version") < 0)) {
            // we want to avoid call to 'on' when the string has the two letters in it
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
            decString = (readString.substring(11)); // not stopping as am not sure where to stop. Anyway atoi will take only the number
            Serial.println(decString);
            setV();
            showValues();
          }

                   
          /*********************
          * information requests:
          * - status
          * - color
          * - brightness
          *********************/
      

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
