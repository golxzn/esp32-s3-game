<div align="center">

# [`/** @todo: title */`]

### `[Road To Embedded]`

</div>

## About

This project was essentially created for a single purpose: I wanted to find
a job and got relocated to New Zealand. Since C++ is used more in embedded
there, I decided to make such a project, which will be both interesting for
me as a game (+engine) developer and them as a hiring companies.

> Spoiler: nobody cares. I had probably solve 1000+ leetcode or something.

Anyway, here we are! For videos and other interesting stuff, dive in my
[telegram channel](https://t.me/golxzn_channel). I shared there A LOT of
fun stuff like SIMD on this chip!

## Current progress

<div align="center">

[![Preview](https://img.youtube.com/vi/1tLMw9hM7Do/hqdefault.jpg)](https://youtu.be/1tLMw9hM7Do)

</div>

* Double-Buffered Rendering using TFT 3'5 ili9486 through [Dedicated GPIO][dedic-gpio] (I'm planning to migrate to Octal-SPI to reach 60 FPS);
* [SPIFF][spiff] file system;
* Multiple simultaneous [WAV file][wav-file] support using [LEDC][ledc] and [PWM][pwm].
* [DualSense®][dualsense] controller support over [HID over USB][hid-over-usb] (I did BT Classic, but then realize that esp32-s3 doesn't support it 😭);


<!-- LINKS -->

[dedic-gpio]: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/dedic_gpio.html
[spiff]: https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/storage/spiffs.html
[wav-file]: http://soundfile.sapp.org/doc/WaveFormat/
[ledc]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/ledc.html
[pwm]: https://en.wikipedia.org/wiki/Pulse-width_modulation
[dualsense]: https://www.playstation.com/en-us/accessories/dualsense-wireless-controller/
[hid-over-usb]: https://www.playstation.com/en-us/accessories/dualsense-wireless-controller/
