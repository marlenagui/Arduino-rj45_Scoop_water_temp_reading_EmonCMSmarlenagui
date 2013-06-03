/****************************************************************************
 * BEST VIEWED WITH NOTEPAD ++ or an editor with colored syntax             *
 ****************************************************************************
 * this is a template for the new "light" front end revisited SCOOP library *
 ****************************************************************************
 * Author: fabrice oudert                                                   *
 * Creation date: 31 may 2013                                               *
 ***************************************************************************/

#include "SCoopME.h"
#include <SPI.h>
#include <Ethernet.h>

// assign a MAC address for the ethernet controller.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
// fill in your address here:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

// fill in an available IP address on your network here,
// for manual configuration:
IPAddress ip(192,168,0,120);
// initialize the library instance:
EthernetClient client;

//debug var = 1 more trace are generated, should be 0 when code in production
volatile debug = 1;

//Initialise variables for waterflow reading
volatile byte WaterFlowInterrupt = 0;
volatile int WaterFlow = 2;
volatile float calibrationFactor = 7.5; // The hall-effect flow sensor outputs approximately 7.5 pulses per second per litre/minute of flow.
volatile float flowRate;
volatile byte pulseCount;
volatile int Newpulsecount = 0;			//used to define if water consumption has to be sent over to WWW

// initialize var for temperature
volatile int noAvg = 0; 			//when boot, you don't do average, otherwise you need 5 readings to get correct value.
volatile float AvgTemp = 0;     	//To average the mesured temp, will remove picks
volatile float AvgTempOld = 0;		//compare old temp to actual will set NewAvgTemp = 1 for ethernet sending
volatile int NewAvgTemp = 0;		//used to define if water consumption has to be sent over to WWW

// to see, might not be needed anymore with SCoopME
long lastConnectionTime = 0;        // last time you connected to the server, in milliseconds
long lastReadingTime = 0;           // last time you read
long lastReadWaterFlow = 0;         // last time you read the water flow
boolean lastConnected = false;      // state of the connection last time through the main loop
const int postingInterval = 10000;  // delay between updates to www.marlenagui.com


SCtimerMs timerwaterflowread;		// a basic uint16 timer in millisecond



//****************************************************************************************************************************************    
// read the temperature sensor
// read once every 5 min as task start by sleep (5000)
// default stack (150bytes) and quantum time (100us)
//****************************************************************************************************************************************
struct ReadTemp : SCoopTask< ReadTemp, 150, 100 > {	
static void setup() {
	int temperaturepin = 2;
}
static void loop() {
	sleep(300000);			//Wait 5 min before reading 
	float temperature = analogRead(tempraturepin)* .004882814;
    temperature = (temperature - .5) * 100;
    //following if avoid the AvgTemp value to start from tempature / 5 will work only at first loop of by any chance temperature = 0
    if ( AvgTemp == 0 ) {
      	Serial.println("Set AvgTemp to temperature");
      	AvgTemp = temperature;
    } else { 
    	AvgTemp = (4 * AvgTemp + temperature) / 5; 
    }
    if ( AvgTemp == AvgTempOld ) {
    	Serial.print ("\nNo temperature change");
    	NewAvgTemp = 0;
    	} else { 
    		NewAvgTemp = 1;
    	}
    Serial.print ("\nSensor Reading Value : ");
    Serial.println (temperature);
    Serial.print ("Avr Value : ");
    Serial.println (AvgTemp);
    } 
} ReadTemp;

//****************************************************************************************************************************************    
// read the Water flow sensor task 
// read once every second as task start by sleep (1000)
// default stack (100bytes) and quantum time (100us)
// no setup() needed
//****************************************************************************************************************************************
struct ReadPulseCount : SCoopTask< ReadPulseCount, 100, 100 > {	
static void loop() {
	sleep(1000);
	detachInterrupt(WaterFlowInterrupt);            // Disable the interrupt while calculating flow rate and sending the value to the host
    // Because this loop may not complete in exactly 1 second intervals we calculate the number of milliseconds that have passed since the last execution and use
    // that to scale the output. the calibrationFactor to scale the output based on the number of pulses per second per units of measure (litres/minute in
    // this case) coming from the sensor is applied on the web side.
    Serial.print("Pulse since last time:"); // Output separator
    Serial.println(pulsecount);
    pulseCount = ((1000.0 / timerwaterflowread) * pulseCount) 
    if ( pulsecount > 0 ) {
    	Newpulsecount = 1;
    } else { 
    	pulsecount = 0;
    }
    Serial.print("Pulse per seconde: ");
    Serial.println(pulseCount);
    
    // Enable the interrupt again
    attachInterrupt(WaterFlowInterrupt, pulseCounter, FALLING); 
    timerwaterflowread = 0;			//reset timerwaterflowread for next read
} 
} ReadPulseCount;


struct task3: SCoopTask< task3, 100, 150 > {// allocate 100bytes for stack and 150us
static void loop() { 						// example without setup()
  // user code here
} 
} task3;


struct timer1 : SCoopTimer< timer1, 100 > { // every 100ms
static void run() { 
  // user code go here. code must NOT be blocking in timers, and must NOT use yield()
}
} timer1;


SCtimerMs myTimer;		// exemple of an event based on a timer value
struct myTrigger1 {		// user must define a structure with "read" and "confirm"
static bool read() { return (myTimer > 1000); };
static void confirm(uint8_t status) { myTimer.add(1000); }
};

struct myEvent : SCoopEvent< myEvent, myTrigger1, RISING > {
static void run() { 
  // user code goes here and will be executed when myTriger1.read() is true
}
} myEvent;




void setup()
{ timer=0; 
	//************************************************
	// start serial port:
	Serial.begin(57600);
	//************************************************
	// start the Ethernet connection:
	// give the ethernet module time to boot up:
	delay(1000);
	// start the Ethernet connection:
	if (Ethernet.begin(mac) == 0) {
		Serial.println("Failed to configure Ethernet using DHCP");
    	// Configure manually:
    	Ethernet.begin(mac, ip);
	}
    //************************************************
  	//Set the Water flow variables
  	// The Hall-effect sensor is connected to pin 2 which uses interrupt 0.
 	// Configured to trigger on a RAISING state change 
  	pinMode(WaterFlow, INPUT);        //initializes digital pin 2 as an input
  	pulseCount = 0;
  	attachInterrupt(WaterFlowInterrupt, pulseCounter, RAISING);
}

void loop() {
  
yield();  // orchestrate everything.

//****************************************************************************************************************************************    
// read the Water flow sensor once every second
//****************************************************************************************************************************************
	if (timerwaterflowread>=1000) { 
		detachInterrupt(WaterFlowInterrupt);            // Disable the interrupt while calculating flow rate and sending the value to the host
    	// Because this loop may not complete in exactly 1 second intervals we calculate the number of milliseconds that have passed since the last execution and use
    	// that to scale the output. the calibrationFactor to scale the output based on the number of pulses per second per units of measure (litres/minute in
    	// this case) coming from the sensor is applied on the web side.
    	Serial.print("Pulse since last time:"); // Output separator
    	Serial.println(pulsecount);
    	pulseCount = ((1000.0 / timerwaterflowread) * pulseCount) 
    	Serial.print("Pulse per seconde: ");
    	Serial.println(pulseCount);
    
    	// Enable the interrupt again
    	attachInterrupt(WaterFlowInterrupt, pulseCounter, FALLING);
    	timerwaterflowread=0; 	//reset the timer
	} 

// if there's no net connection, but there was one last time
  // through the loop, then stop the client:
  if (!client.connected() && lastConnected) {
    Serial.println();
    Serial.println("disconnecting.");
    client.stop();
  }

  // if you're not connected, and ten seconds have passed since
  // your last connection, then connect again and send data:
  long time=millis();
  if(!client.connected() && (millis() - lastConnectionTime > postingInterval)) {
    sendDataCosm(AvgTemp);
    sendDataEmoncms(AvgTemp);
    sendDataEmoncms(pulseCount);
    // Reset the pulse counter so we can start incrementing again
    pulseCount = 0;
    // note the time that the connection was made: 
    lastConnectionTime = millis();
  }
  // store the state of the connection for next time through
  // the loop:
  lastConnected = client.connected();
}

//****************************************************************************************************************************************    
// this method makes a HTTP connection to the server cosm:
void sendDataCosm(float thisData) {
  // if there's a successful connection:
  if (client.connect("www.cosm.com", 80)) {
    Serial.println("connecting to Cosm...");
    // send the HTTP PUT request. 
    // fill in your feed address here:
    client.print("PUT /api/38973.csv HTTP/1.1\n");
    client.print("Host: www.cosm.com\n");
    // fill in your Pachube API key here:
    client.print("X-PachubeApiKey: -7w6tS_ZpHnFa-zNMIoDdAxZ8rX00EW16KjrMD18rwE\n");
    client.print("Content-Length: ");

    // calculate the length of the sensor reading in bytes:
    int thisLength = getLength(thisData);
    client.println(thisLength, DEC);

    // last pieces of the HTTP PUT request:
    client.print("Content-Type: text/csv\n");
    client.println("Connection: close\n");

    // here's the actual content of the PUT request:
    client.println(thisData, DEC);
    Serial.println("disconnecting from Cosm.");
    client.stop();
    
  } 
  else {
    // if you couldn't make a connection:
    Serial.println("Connection failed");
  }
}

//****************************************************************************************************************************************    
// this method makes a HTTP connection to the server marlenagui:
void sendDataEmoncms(float thisData) {
  // if there's a successful connection:
  if (client.connect("www.marlenagui.com", 80)) {
    Serial.println("connected to Marlenagui...");
    // send the HTTP PUT request. 
    client.print("PUT /Energies_Monitor/api/post?apikey=bc6e7c688d881750b70130f78308a546&json={TempHeatingOut:");
    client.print(thisData);
    client.println("} HTTP/1.0");
    client.println("Host: www.marlenagui.com");
    client.println();
    Serial.println("disconnecting from Marlenagui.");
    client.stop();
  } 
  else {
    // if you couldn't make a connection:
    Serial.println("Connection failed");
  }
}

//****************************************************************************************************************************************    
// This method calculates the number of digits in the sensor reading.  Since each digit of the ASCII decimal
// representation is a byte, the number of digits equals the number of bytes:
int getLength(int someValue) {
  // there's at least one byte:
  int digits = 1;
  // continually divide the value by ten, 
  // adding one to the digit count for each
  // time you divide, until you're at 0:
  int dividend = someValue /10;
  while (dividend > 0) {
    dividend = dividend /10;
    digits++;
  }
  // return the number of digits:
  return digits;
}


}

//****************************************************************************************************************************************    
// Invoked by interrupt0 once per rotation of the hall-effect sensor. Interrupt
// handlers should be kept as small as possible so they return quickly.
void pulseCounter()
{
  // Increment the pulse counter
  pulseCount++;
}
