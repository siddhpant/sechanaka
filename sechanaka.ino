/*
 * Sechanaka (सेचनक) - Automatically water your plants!
 * Copyright (C) 2022  Siddh Raman Pant

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/* ------------------------------------------------------------------------- */

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <ESP32Time.h>

/* ------------------------------------------------------------------------- */

// Pin configurations:

#define BUZZER_PIN		33
#define MOISTURE_SENSOR_PIN	34  // A6
#define RELAY_PIN		17

/* ------------------------------------------------------------------------- */

// Calibrated values:

#define AIR_MOISTURE_LEVEL	3600
#define WATER_MOISTURE_LEVEL	1300

/* ------------------------------------------------------------------------- */

// Percentage cutoff values:

#define LOW_MOISTURE_CUTOFF	20
#define HIGH_MOISTURE_CUTOFF	85

/* ------------------------------------------------------------------------- */

// Time configurations:

#define TIMER_INTERVAL		1000    // 1 s = 1000 ms.
#define SOOKHA_LOOP_COUNT	20      // 20 timer intervals (= 20s here).
#define IST_OFFSET_SEC		19800   // +0530 in seconds.

/* ------------------------------------------------------------------------- */

// Blynk configurations (GitHub folks, don't worry the account is no more):

#define BLYNK_TEMPLATE_ID	"TMPLIYgePrrV"
#define BLYNK_DEVICE_NAME	"Quickstart Device"
#define BLYNK_AUTH_TOKEN	"ICi8iEIgV5jpuiKjJud6tIb6-kHwkKLU"

#define WIFI_SSID		"mitti"
#define WIFI_PASS		""

#define BLYNK_VAR_SWITCH_CTL	V0
#define BLYNK_VAR_SWITCH_LED	V1
#define BLYNK_VAR_UPTIME_SEC	V2
#define BLYNK_VAR_LAST_UPDATED	V3
#define BLYNK_VAR_MOISTURE_PC	V4
#define BLYNK_VAR_BUZZER	V5

/* ------------------------------------------------------------------------- */

// To retrive time:

ESP32Time rtc;


void ntp_update_time(void)
{
	configTime(IST_OFFSET_SEC, 0, "time.google.com");

	struct tm timeinfo;
	if (getLocalTime(&timeinfo))
		rtc.setTimeStruct(timeinfo);
}


String get_current_datetime(void)
{
	// Example: "Fri, Nov 04 2022, 19:11:30 IST"
	return rtc.getTime("%a, %b %d %Y, %T IST");
}

/* ------------------------------------------------------------------------- */

// Buzzer helper functions:

inline void start_buzzer(void)
{
	digitalWrite(BUZZER_PIN, HIGH);
	Blynk.virtualWrite(BLYNK_VAR_BUZZER, 1);
}


inline void stop_buzzer(bool blynk_write = true)
{
	digitalWrite(BUZZER_PIN, LOW);
	Blynk.virtualWrite(BLYNK_VAR_BUZZER, blynk_write);
}

/* ------------------------------------------------------------------------- */

// Motor helper functions:

inline void start_motor(void)
{
	digitalWrite(RELAY_PIN, HIGH);
}


inline void stop_motor(void)
{
	digitalWrite(RELAY_PIN, LOW);
}

/* ------------------------------------------------------------------------- */

/*
 * Sense moisture percentage, and based on it, turn the motor on or off.
 * If for more than SOOKHA_LOOP_COUNT iterations we are below the threshold
 * even after starting the motor, that means we are probably out of water.
 * We will turn on buzzer to indicate that.
 */

int moisture_reading = 0;
int moisture_percentage = 0;

unsigned int counter = 0;

void sinchaaii(void)
{
	moisture_reading = analogRead(MOISTURE_SENSOR_PIN);

	/*
	 * Map moisture_reading to percentage (i.e., between 0 to 100).
	 *
	 * map(x, in_min, in_max, out_min, out_max)
  	 *  = (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
	 */
	moisture_percentage = map(moisture_reading,
				  AIR_MOISTURE_LEVEL, WATER_MOISTURE_LEVEL,
				  0, 100);

	/*
	 * Start watering when moisture < 10%, and stop when moisture > 85%.
	 * We don't do anything in between, so the motor is running or is
	 * stopped when moisture is between the range [10, 85].
	 */
	if (moisture_percentage < LOW_MOISTURE_CUTOFF) {
		start_motor();

		/*
		 * Start buzzer if for too long water not given.
		 * We don't want to have it continuously make sound, so we
		 * will start and stop after SOOKHA_LOOP_COUNT interval.
		 */
		if ((++counter / SOOKHA_LOOP_COUNT) % 2) {
			start_buzzer();
		} else {
			stop_buzzer(false);
		}
	} else {
		if (moisture_percentage > HIGH_MOISTURE_CUTOFF) {
			stop_motor();
		}
		
		if (counter > 0) {
			counter = 0;
			stop_buzzer();
		}
	}
}

/* ------------------------------------------------------------------------- */

// Blynk event functions:

bool enabled_by_remote = true;


BLYNK_WRITE(BLYNK_VAR_SWITCH_CTL)
{
	// We use "switch control" to turn our system on or off.
	enabled_by_remote = param.asInt();

	// Turn "switch LED" on/off to convey acknowledgement.
	Blynk.virtualWrite(BLYNK_VAR_SWITCH_LED, enabled_by_remote * 255);
}


BLYNK_CONNECTED()
{
	// Sync the values for all datastreams that has sync enabled.
	// BLYNK_WRITE event will be triggered for the associated pins.
	Blynk.syncAll();
}

/* ------------------------------------------------------------------------- */

// Blynk setters:

inline void send_moisture_level(void)
{
	Blynk.virtualWrite(BLYNK_VAR_MOISTURE_PC, moisture_percentage);
}


inline void send_time(void)
{
	Blynk.virtualWrite(BLYNK_VAR_UPTIME_SEC, millis() / 1000);
	Blynk.virtualWrite(BLYNK_VAR_LAST_UPDATED, get_current_datetime());
}

/* ------------------------------------------------------------------------- */

// Timer which will execute a loop regularly:

BlynkTimer timer;


void timer_loop()
{
	if (enabled_by_remote) {
 		sinchaaii();
 		send_moisture_level();
 		send_time();
 	} else {
 		stop_motor();
 		stop_buzzer();
 	}
}

/* ------------------------------------------------------------------------- */

void setup()
{
	Serial.begin(9600);

	pinMode(MOISTURE_SENSOR_PIN, INPUT);

	pinMode(BUZZER_PIN, OUTPUT);
	digitalWrite(BUZZER_PIN, LOW);

	pinMode(RELAY_PIN, OUTPUT);
	digitalWrite(RELAY_PIN, LOW);
	
	Serial.println("\nAttempting to connect to Blynk...");

	Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS, "blynk.cloud", 80);

	Serial.println("Connected to Blynk.");
	
	timer.setInterval(TIMER_INTERVAL, timer_loop);

	ntp_update_time();

}

/* ------------------------------------------------------------------------- */

void loop()
{
	Blynk.run();
	timer.run();
}

/* ------------------------------------------------------------------------- */

// End of file.
