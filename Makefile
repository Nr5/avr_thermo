TARGET1=thermometer_m328
TARGET2=esp01_receiver_m328
MCU=atmega328p

SOURCES1=thermometer_m328.c
SOURCES2=esp01_receiver_m328.c

PROGRAMMER=arduino
#auskommentieren für automatische Wahl
PORT=-P/dev/ttyACM0
BAUD=-B19200

#Ab hier nichts verändern
OBJECTS1=$(SOURCES1:.c=.o)
OBJECTS2=$(SOURCES2:.c=.o)
CFLAGS=-c -Os -funroll-loops

all: hex eeprom

hex: $(TARGET1).hex $(TARGET2).hex

eeprom: $(TARGET1)_eeprom.hex $(TARGET2)_eeprom.hex

$(TARGET1).hex: $(TARGET1).elf
	avr-objcopy -O ihex -j .data -j .text $(TARGET1).elf $(TARGET1).hex

$(TARGET1)_eeprom.hex: $(TARGET1).elf
	avr-objcopy -O ihex -j .eeprom --change-section-lma .eeprom=1 $(TARGET1).elf $(TARGET1)_eeprom.hex

$(TARGET1).elf: $(OBJECTS1)
	avr-gcc $(LDFLAGS) -mmcu=$(MCU) $(OBJECTS1) -o $(TARGET1).elf




$(TARGET2).hex: $(TARGET2).elf
	avr-objcopy -O ihex -j .data -j .text $(TARGET2).elf $(TARGET2).hex

$(TARGET2)_eeprom.hex: $(TARGET2).elf
	avr-objcopy -O ihex -j .eeprom --change-section-lma .eeprom=1 $(TARGET2).elf $(TARGET2)_eeprom.hex

$(TARGET2).elf: $(OBJECTS2)
	avr-gcc $(LDFLAGS) -mmcu=$(MCU) $(OBJECTS2) -o $(TARGET2).elf



.c.o:
	avr-gcc $(CFLAGS) -mmcu=$(MCU) $< -o $@

size:
	avr-size --mcu=$(MCU) -C $(TARGET1).elf; avr-size --mcu=$(MCU) -C $(TARGET2).elf;




program:
	avrdude -p$(MCU) $(PORT) $(BAUD) -c$(PROGRAMMER) -Uflash:w:$(TARGET1).hex:a



clean_tmp:
	rm -rf *.o
	rm -rf *.elf

clean:
	rm -rf *.o
	rm -rf *.elf
	rm -rf *.hex
