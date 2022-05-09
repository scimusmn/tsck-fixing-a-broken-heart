#pragma once

#ifdef UNIT_TEST
#include "../tests/FakeEEPROM.h"
#include "../tests/FakeWire.h"
#else
#include <EEPROM.h>
#include <Wire.h>
#endif

#include "../FixedSizeString.h"
#include "../LookupTable.h"
#include "Tag.h"

typedef unsigned char byte;

namespace smm {
    template<typename keyT, typename valT, unsigned int MAX_ENTRIES>
    class EEPROMLookupTable :
	public smm::LookupTable<keyT, valT, MAX_ENTRIES> {
    public:
	void load() {
	    int numEntries = EEPROM[0];
	    if (numEntries == 255)
		// assume previously unwritten memory cell
		numEntries = 0;

	    if (numEntries > MAX_ENTRIES)
		// only read up to MAX_ENTRIES entries
		numEntries = MAX_ENTRIES;
	    
	    for (int i=0; i<numEntries; i++)
		readEntry(i);
	    
	}
	
	void save() {
	    EEPROM.update(0, this->size());
	    for (int i=0; i<this->size(); i++)
		saveEntry(i);
	}
    private:
	void readEntry(int index) {
	    const int entrySize = sizeof(keyT) + sizeof(valT);
	    int address = 1 + (index * entrySize);

	    keyT key;
	    unsigned char *b = (unsigned char *) &key;
	    for (int i=0; i<sizeof(keyT); i++) {
		*b = EEPROM[address+i];
		b++;
	    }
	    valT value;
	    b = (unsigned char *) &value;
	    for (int i=0; i<sizeof(valT); i++) {
		*b = EEPROM[address+sizeof(keyT)+i];
		b++;
	    }
	    this->add(key, value);
	}

	void saveEntry(int index) {
	    const int entrySize = sizeof(keyT) + sizeof(valT);
	    int address = 1 + (index * entrySize);

	    keyT key = this->key(index);
	    unsigned char *b = (unsigned char *) &key;
	    for (int i=0; i<sizeof(keyT); i++)
		EEPROM.update(address+i, *(b+i));

	    valT value = this->value(index);
	    b = (unsigned char *) &value;
	    for (int i=0; i<sizeof(valT); i++)
		EEPROM.update(address+sizeof(keyT)+i, *(b+i));
	}
    };

    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    typedef void (*readHandler)(byte, RfidTag&);

    #define MAX_READ_ATTEMPTS 3
    

    template<unsigned int MAX_READERS,
	     unsigned int MAX_TAGS,
	     unsigned int MAX_CATEGORIES>
    class RfidController {
    public:
	void setup() {
	    tags.load();
	    numReaders = 0;
	    Wire.begin();
	}
	void update() {
	    for (int i=0; i<numReaders; i++)
		updateReader(readers[i]);
	}

	void addReader(byte address) {
	    readers[numReaders] = address;
	    numReaders++;
	}
	void onRead(byte category, readHandler handler) {
	    handlers.add(category, handler);
	}
	void onReadUnknown(readHandler handler) {
	    unknownTagHandler = handler;
	}
	void onReadFailure(void (*handler)(byte address)) {
	    readFailHandler = handler;
	}
	void teachTag(RfidTag tag, byte category) {
	    tags.add(tag, category);
	    tags.save();
	}

	void forgetLastTag() {
	    byte newSize = tags.size() - 1;
	    if (newSize < 0)
		newSize = 0;
	    EEPROM[0] = newSize;
	    tags.load();
	}

	size_t numKnownTags() {
	    return tags.size();
	}

	void dumpTags() {
	    for (int i=0; i<tags.size(); i++) {
		Serial.print(tags.key(i).toString().c_str());
		Serial.print(" -> ");
		Serial.println((char) tags.value(i));
	    }
	}

	void testReaders() {
	    RfidTag tag;
	    for (int i=0; i<numReaders; i++) {
		byte address = readers[i];
		bool ok = loadTag(address, tag);
		Serial.print("reader 0x");
		Serial.print(address, HEX);
		if (ok)
		    Serial.println(": OK");
		else {
		    Serial.println(": FAIL");
		    Wire.requestFrom(address, 6);
		    Serial.print("requested 6 bytes, received ");
		    Serial.println(Wire.available());
		    while (Wire.available())
			Serial.println(Wire.read(), HEX);
		}
	    }
	}
    private:
	EEPROMLookupTable<RfidTag, byte, MAX_TAGS> tags;
	LookupTable<byte, readHandler, MAX_CATEGORIES> handlers;
	readHandler unknownTagHandler;
	void (*readFailHandler)(byte);
	int numReaders;
	byte readers[MAX_READERS];

	bool loadTag(byte address, RfidTag& tag) {
	    Wire.requestFrom(address, 6);
	    bool tagIsEmpty = true;
	    for (int i=0; i<5; i++) {
		if (!(Wire.available()))
		    return false; // not enough address bytes
		tag.tagData[i] = Wire.read();
	    }

	    if (!(Wire.available()))
		return false; // no checksum byte
	    byte checksum = Wire.read();
	    if (tag.checksum() != checksum)
		return false; // bad checksum
	    return true;
	}

	bool tagIsEmpty(RfidTag& tag) {
	    if (tag.tagData[0] != 255)
		return false;
	    if (tag.tagData[1] != 255)
		return false;
	    if (tag.tagData[2] != 255)
		return false;
	    if (tag.tagData[3] != 255)
		return false;
	    if (tag.tagData[4] != 255)
		return false;
	    return true;	    
	}

	void clearTag(byte address) {
	    Wire.beginTransmission(address);
	    Wire.write(0x54); // CLEAR TAG command
	    Wire.endTransmission();
	}

	void updateReader(byte address) {
	    RfidTag tag;
	    for (int i=0; i<MAX_READ_ATTEMPTS; i++) {
		bool success = loadTag(address, tag);
		bool empty = tagIsEmpty(tag);
		if (success && !empty) {
		    processTag(address, tag);
		    break;
		}
		else if (!success && !empty) {
		    if (readFailHandler != nullptr)
		    readFailHandler(address);//*/
		}
			
	    }
	    // success or failure, clear the reader
	    clearTag(address);
	}

	void processTag(byte address, RfidTag& tag) {
	    byte *category = tags[tag];
	    if (category != nullptr) {
		readHandler *handler = handlers[*category];
		if (handler != nullptr)
		    (*handler)(address, tag);
	    }
	    else { // unknown tag
		if (unknownTagHandler != nullptr)
		    unknownTagHandler(address, tag);
	    }
	}
    };
}
