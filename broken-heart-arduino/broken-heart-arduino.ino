#include <Arduino.h>
#include "Button.h"

#define BUTTON_PIN 4

class HeartButton : public smm::Button {
	public:
	HeartButton(int buttonPin) : Button(buttonPin) {}

	void onPress() {
		Serial.println("pressed!");
	}

	void onRelease() {
		Serial.println("released!");
	}
} button(BUTTON_PIN);


void setup() {
	Serial.begin(115200);
}

void loop() {
	button.update();
}
