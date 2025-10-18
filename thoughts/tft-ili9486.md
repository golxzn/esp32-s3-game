# Sorting things with TFT ili9486 out

## Overview

### Environment

 - Freenove esp32-s3 wroom;
 - 3.5" TFT LCD Shield (ili9486).

### There are 3 common ways to send bits to the TFT display

1. `GPIO` struct (`GPIO.out_w1tc` & `GPIO.out_w1ts`);
2. Dedicated GPIO;
3. SPI Master (MISO/MOSI or Octal mode).

### There are 2 problems I need to solve

1. Why TFT_eSPI works? It uses GPIO struct or MISO/MOSI SPI Controller;
2. Which way is the fastest?

It seems like the TFT_eSPI works just because they're initializing and
sendind data the right way.


## Digging into TFT_eSPI library solution

It doesn't really mater which protocol the library uses since it's designed
such way to switch between protocol using `User_Setup.h` file. Anyway, here's
the `User_Setup.h` changes I've made:

```hpp
// Tell the library to use 8 bit parallel mode (otherwise SPI is assumed)
#define TFT_PARALLEL_8_BIT

#define ILI9486_DRIVER

#define	TFT_RST 9 // Reset pin
#define	TFT_CS 10 // Chip select control pin
#define	TFT_DC 11 // Data Command control pin - must use a pin in the range 0-31 (LCD_RS on my display)
#define	TFT_WR 12 // Write strobe control pin - must use a pin in the range 0-31
#define	TFT_RD 13

#define	TFT_D2 4
#define	TFT_D3 5
#define	TFT_D4 6
#define	TFT_D5 7
#define	TFT_D6 15
#define	TFT_D7 16

#define	TFT_D0 17	 //	Must use pins in the range 0-31	for	the	data bus
#define	TFT_D1 18	 //	so a single	register write sets/clears all bits
```

### TFT_eSPI::init

Init function initialize the bus and display itself

#### Bus initialization

Nothing interesting, just:

```cpp
pinMode(TFT_CS, OUTPUT); digitalWrite(TFT_CS, HIGH); // Chip select high (inactive)
pinMode(TFT_WR, OUTPUT); digitalWrite(TFT_WR, HIGH); // Set write strobe high (inactive)
pinMode(TFT_DC, OUTPUT); digitalWrite(TFT_DC, HIGH); // Data/Command high = data mode
pinMode(TFT_RST, OUTPUT); digitalWrite(TFT_RST, HIGH); // Set high, do not share pin with another SPI device
pinMode(TFT_RD, OUTPUT); digitalWrite(TFT_RD, HIGH); // Set write strobe high (inactive)

// all the data pins
pinMode(TFT_D0-7, OUTPUT); digitalWrite(TFT_D0-7, HIGH);
```

Each of pin is set to `OUTPUT` mode and `HIGH` state only if it's active: `defined` and `>= 0`

There are a hack with masks caching, but it's unnecessary for us.

#### TFT Display reset & configuration

This is the reset sequence. Duplicated CS and DC seems like
just happened to be here by mistake. We'll check it

```cpp
pinMode(TFT_CS, OUTPUT); digitalWrite(TFT_CS, HIGH); // Chip select high (inactive)
pinMode(TFT_DC, OUTPUT); digitalWrite(TFT_DC, HIGH); // Data/Command high = data mode

// Reset sequence

writecommand(0x00/*NOP*/); // Put SPI bus in known state for TFT with CS tied low
digitalWrite(TFT_RST, HIGH);
delay(5);
digitalWrite(TFT_RST, LOW);
delay(20);
digitalWrite(TFT_RST, HIGH);
delay(150); // Wait for reset to complete
```

And, initialization sequence:

```cpp
digitalWrite(TFT_CS, LOW); // from begin_tft_write

// BEGIN OF INCLUDED ILI9486_Init.h

writecommand(0x01); // SW reset
delay(120);

writecommand(0x11); // Sleep out, also SW reset
delay(120);

writecommand(0x3A);
#if defined (TFT_PARALLEL_8_BIT) || defined (TFT_PARALLEL_16_BIT) || defined (RPI_DISPLAY_TYPE)
  writedata(0x55);           // 16-bit colour interface
#else
  writedata(0x66);           // 18-bit colour interface
#endif

writecommand(0xC0); //                          1100.0000 Power Control 1
writedata(0x0E);    //                          0001.0111   ... VRH1
writedata(0x0E);    //                          0001.0101   ... VRH2
writecommand(0xC1); //                          1100.0001 Power Control 2
writedata(0x41);    //                          0100.0001   . SAP BT
writedata(0x00);    //                          0000.0000   ..... VC
writecommand(0xC2); //                          1100.0010 Power Control 3
writedata(0x55);    //     nb. was 0x44         0101.0101   . DCA1 . DCA0

writecommand(0xC5);
writedata(0x00);
writedata(0x00);
writedata(0x00);
writedata(0x00);

writecommand(0xE0);
writedata(0x0F);
writedata(0x1F);
writedata(0x1C);
writedata(0x0C);
writedata(0x0F);
writedata(0x08);
writedata(0x48);
writedata(0x98);
writedata(0x37);
writedata(0x0A);
writedata(0x13);
writedata(0x04);
writedata(0x11);
writedata(0x0D);
writedata(0x00);

writecommand(0xE1);
writedata(0x0F);
writedata(0x32);
writedata(0x2E);
writedata(0x0B);
writedata(0x0D);
writedata(0x05);
writedata(0x47);
writedata(0x75);
writedata(0x37);
writedata(0x06);
writedata(0x10);
writedata(0x03);
writedata(0x24);
writedata(0x20);
writedata(0x00);

#if defined (TFT_PARALLEL_8_BIT) || defined (TFT_PARALLEL_16_BIT) || defined (RPI_DISPLAY_TYPE)
  writecommand(TFT_INVOFF);
#else
  writecommand(TFT_INVON);
#endif

writecommand(0x36);
writedata(0x48);

writecommand(0x29);                     // display on
delay(150);

// END OF INCLUDED ILI9486_Init.h

digitalWrite(TFT_CS, HIGH); // from end_tft_write
```

Then, they're setting the rotation (maybe it is necessary):

```cpp
digitalWrite(TFT_CS, LOW); // from begin_tft_write

// BEGIN OF INCLUDED TFT_Drivers/ILI9486_Rotation.h

#define TFT_MADCTL  0x36
#define TFT_MAD_MY  0x80
#define TFT_MAD_MX  0x40
#define TFT_MAD_MV  0x20
#define TFT_MAD_ML  0x10
#define TFT_MAD_BGR 0x08
#define TFT_MAD_MH  0x04
#define TFT_MAD_SS  0x02
#define TFT_MAD_GS  0x01
#define TFT_MAD_RGB 0x00

writecommand(TFT_MADCTL/* 0x36 */);
rotation = m % 8;
switch (rotation) {
 case 0: // Portrait
   writedata(TFT_MAD_BGR | TFT_MAD_MX);
   _width  = _init_width;
   _height = _init_height;
   break;
 case 1: // Landscape (Portrait + 90)
   writedata(TFT_MAD_BGR | TFT_MAD_MV);
   _width  = _init_height;
   _height = _init_width;
   break;
 case 2: // Inverter portrait
   writedata( TFT_MAD_BGR | TFT_MAD_MY);
   _width  = _init_width;
   _height = _init_height;
  break;
 case 3: // Inverted landscape
   writedata(TFT_MAD_BGR | TFT_MAD_MV | TFT_MAD_MX | TFT_MAD_MY);
   _width  = _init_height;
   _height = _init_width;
   break;
 case 4: // Portrait
   writedata(TFT_MAD_BGR | TFT_MAD_MX | TFT_MAD_MY);
   _width  = _init_width;
   _height = _init_height;
   break;
 case 5: // Landscape (Portrait + 90)
   writedata(TFT_MAD_BGR | TFT_MAD_MV | TFT_MAD_MX);
   _width  = _init_height;
   _height = _init_width;
   break;
 case 6: // Inverter portrait
   writedata( TFT_MAD_BGR);
   _width  = _init_width;
   _height = _init_height;
   break;
 case 7: // Inverted landscape
   writedata(TFT_MAD_BGR | TFT_MAD_MV | TFT_MAD_MY);
   _width  = _init_height;
   _height = _init_width;
   break;
}

// END OF INCLUDED TFT_Drivers/ILI9486_Rotation.h

delayMicroseconds(10);

digitalWrite(TFT_CS, HIGH); // from end_tft_write

addr_row = 0xFFFF;
addr_col = 0xFFFF;

// reset Viewport
```

That's whole the initialization process.

### TFT_eSPI::fillScreen or how this library send data

The `fillScreen` is actually a `fillRect(0, 0, _width, _height, color)`:

The `fillRect` has some clipping code and the following writing sequence:

```cpp
begin_tft_write();
setWindow(x, y, x + w - 1, y + h - 1);
pushBlock(color, w * h);
end_tft_write();
```

Which is:

```cpp
digitalWrite(TFT_CS, LOW); // from end_tft_write

#define tft_Write_8(C) \
    GPIO_CLR_REG = GPIO_OUT_CLR_MASK; \
    GPIO_SET_REG = set_mask((uint8_t)(C)); \
    WR_H

#define tft_Write_32C(C,D) \
    GPIO_CLR_REG = GPIO_OUT_CLR_MASK; GPIO_SET_REG = set_mask((uint8_t) ((C) >> 8)); WR_H; \
    GPIO_CLR_REG = GPIO_OUT_CLR_MASK; GPIO_SET_REG = set_mask((uint8_t) ((C) >> 0)); WR_H; \
    GPIO_CLR_REG = GPIO_OUT_CLR_MASK; GPIO_SET_REG = set_mask((uint8_t) ((D) >> 8)); WR_H; \
    GPIO_CLR_REG = GPIO_OUT_CLR_MASK; GPIO_SET_REG = set_mask((uint8_t) ((D) >> 0)); WR_H

// setWindow
GPIO.out_w1tc = (1 << TFT_DC); tft_Write_8(TFT_CASET);
GPIO.out_w1ts = (1 << TFT_DC); tft_Write_32C(x0, x1);
GPIO.out_w1tc = (1 << TFT_DC); tft_Write_8(TFT_PASET);
GPIO.out_w1ts = (1 << TFT_DC); tft_Write_32C(y0, y1);
GPIO.out_w1tc = (1 << TFT_DC); tft_Write_8(TFT_RAMWR);
GPIO.out_w1ts = (1 << TFT_DC);

// pushBlock(color, len = w * h)
if ((color >> 8) == (color & 0xFF)) {
    if (!len) return;
    tft_Write_16(color);
    while (--len) {
        WR_L; WR_H; WR_L; WR_H;
    }
} else {
    while (len--) {
        tft_Write_16(color);
    }
}

digitalWrite(TFT_CS, HIGH); // from end_tft_write
```

Holly shit what a mess. But the `pushBlock` is quite smart move. We don't need to
send color every time. Just set the pins and repeat "send" message till we've done.

Anyway, we need to unshittify the `tft_Write_8/16/32` macros. They're complete mess:

```cpp

#define MASK_OFFSET 0 // I won't use TFT_D0 >= 32 anyway.

#define GPIO_CLR_REG GPIO.out_w1tc
#define GPIO_SET_REG GPIO.out_w1ts

// Mask for the 8 data bits to set pin directions
#define GPIO_DIR_MASK (\
    1 << TFT_D0 - MASK_OFFSET | \
    1 << TFT_D1 - MASK_OFFSET | \
    1 << TFT_D2 - MASK_OFFSET | \
    1 << TFT_D3 - MASK_OFFSET | \
    1 << TFT_D4 - MASK_OFFSET | \
    1 << TFT_D5 - MASK_OFFSET | \
    1 << TFT_D6 - MASK_OFFSET | \
    1 << TFT_D7 - MASK_OFFSET \
)

#define GPIO_OUT_CLR_MASK (GPIO_DIR_MASK | (1 << TFT_WR))

// A lookup table is used to set the different bit patterns, this uses 1kByte of RAM
#define set_mask(C) xset_mask[C] // 63fps Sprite rendering test 33% faster, graphicstest only 1.8% faster than shifting in real time

// Real-time shifting alternative to above to save 1KByte RAM, 47 fps Sprite rendering test
/*#define set_mask(C) (((C)&0x80)>>7)<<TFT_D7 | (((C)&0x40)>>6)<<TFT_D6 | (((C)&0x20)>>5)<<TFT_D5 | (((C)&0x10)>>4)<<TFT_D4 | \
                      (((C)&0x08)>>3)<<TFT_D3 | (((C)&0x04)>>2)<<TFT_D2 | (((C)&0x02)>>1)<<TFT_D1 | (((C)&0x01)>>0)<<TFT_D0
//*/

#define WR_L GPIO.out_w1tc = (1 << TFT_WR)
#define WR_H GPIO.out_w1ts = (1 << TFT_WR)

#define tft_Write_8(C) \
    GPIO_CLR_REG = GPIO_OUT_CLR_MASK; \
    GPIO_SET_REG = set_mask((uint8_t)(C)); \
    WR_H
```


Sooo, if we translate this ancient relicts into modern language, it'll be something like:

```cpp
__attribute__((always_inline))
inline uint32_t get_bit_at(const uint32_t data, const uint32_t id) {
    return (data >> id) & 0x01u;
}

__attribute__((always_inline))
static void tft_write(const uint8_t data) {
    const uint32_t enabled_pins_mask{ 0u
        | get_bit_at(data, 0) << TFT_D0
        | get_bit_at(data, 1) << TFT_D1
        | get_bit_at(data, 2) << TFT_D2
        | get_bit_at(data, 3) << TFT_D3
        | get_bit_at(data, 4) << TFT_D4
        | get_bit_at(data, 5) << TFT_D5
        | get_bit_at(data, 6) << TFT_D6
        | get_bit_at(data, 7) << TFT_D7
    };


    GPIO.out_w1tc = GPIO_DIR_MASK | (1 << TFT_WR); // Clear bus & write pin
    GPIO.out_w1ts = enabled_pins_mask; // Set the bus pins
    GPIO.out_w1ts = 1 << TFT_WR; // Set the Write pin. It's like a FIRE button
}
```

Well, It's super close to which I've tried first time. The `enabled_pins_mask`
is cached and could be obtained in `xset_mask`. That's another wise move from
the library author.

The last uncertain thing is the initialization sequence. What is `writecommand`
and `writedata`? So, let's see it:

```cpp

void TFT_eSPI::writecommand(uint8_t c) {
    digitalWrite(TFT_CS, LOW); // from begin_tft_write
    GPIO.out_w1tc = (1 << TFT_DC) // command mode
    tft_Write_8(c);
    GPIO.out_w1ts = (1 << TFT_DC) // data mode
    digitalWrite(TFT_CS, HIGH); // from begin_tft_write
}

void TFT_eSPI::writedata(uint8_t d) {
    digitalWrite(TFT_CS, LOW); // from begin_tft_write
    GPIO.out_w1ts = (1 << TFT_DC) // Play safe, but should already be in data mode
    tft_Write_8(d);
    GPIO.out_w1tc = (1 << TFT_CS) // Allow more hold time for low VDI rail
    digitalWrite(TFT_CS, HIGH); // from begin_tft_write
}
```

The only thing which is uncertain here for me is the clear of `TFT_CS` in the
`writedata` step. The comment makes thing more complicated anyway.

But, the information above is enough to make a demo.

## Current solution (temporary)

RN I'm using `Dedicated GPIO` to send bits, but it's too fast: it's about
201Kbps, which cause 20FPS, If I remove the limiters, Dedicated GPIO handles
226Kbps, which cause 23FPS, which isn't much better.

```cpp
[[gnu::always_inline]]
inline void display::send_bits8(const uint8_t data) noexcept {
	GPIO.out_w1tc = pins::DATA_PINS_MASK | (1 << pins::WRITE); // Clear bus & write pin

#if defined(GZN_TFT_USE_DEDICATED_GPIO)
	dedic_gpio_bundle_write(m_output_bus, 0xFF, data);
	asm volatile("nop\nnop\nnop\nnop"); // Prevent gliches
#else
	GPIO.out_w1ts = pins::make_mask(data); // Set the bus pins
#endif // defined(GZN_TFT_USE_DEDICATED_GPIO)

	GPIO.out_w1ts = 1 << pins::WRITE; // Set the Write pin. It's like a FIRE button
}
```

I discovered that this display could support up to 500Mbps speed through `MIPI
DSI` link, but my esp32-s3 doesn't support it. But there are few different
interfaces like 3/4 line SPI, MDDI, RGB, MCU8080, etc. And esp32-s3 has some
sort of bilt-in [tools to connect TFT display](tft-api-reference) and supports
RGB, MCU8080, and SPI protocols.





[tft-api-reference]: https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/peripherals/lcd/index.html

