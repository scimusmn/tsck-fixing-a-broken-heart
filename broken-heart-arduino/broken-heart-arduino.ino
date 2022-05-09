#include <Arduino.h>

#include "Button.h"
#include "SerialController.h"
#include "Rfid/Controller.h"
#include "Rfid/Tag.h"


#define BUTTON_PIN 4


smm::RfidController<1, 0, 0> rfid;
smm::SerialController<> serial;

class HeartButton : public smm::Button {
	public:
	HeartButton(int buttonPin) : Button(buttonPin) {}

	void onPress() {
		serial.send("button-pressed", 1);
	}
} button(BUTTON_PIN);


void onRead(byte address, smm::RfidTag& tag) {
	serial.send("read-tag", tag.toString().c_str());
}


void setup() {
	serial.setup();
	rfid.setup();
	rfid.addReader(0x50);
	rfid.onReadUnknown(onRead);
	rfid.onReadFailure([](byte addr) { serial.send("read-failure", addr); });

	serial.send("arduino-ready", 1);
}

void loop() {
	button.update();
	rfid.update();
}
