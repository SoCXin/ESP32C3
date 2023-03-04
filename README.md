# [ESP32-C3](https://doc.soc.xin/espressif/ESP32-C3)

* [Espressif](https://www.espressif.com/): [RISC-V](https://github.com/SoCXin/RISC-V)
* [L2R2](https://github.com/SoCXin/Level): 160 MHz (2.55 CoreMark/MHz) , [￥6.2 (QFN32)](https://item.szlcsc.com/3013220.html)

[![Build Status](https://github.com/SoCXin/ESP32C3/workflows/check/badge.svg)](https://github.com/SoCXin/ESP32C3/actions/workflows/check.yml)
[![Build Status](https://github.com/SoCXin/ESP32C3/workflows/build/badge.svg)](https://github.com/SoCXin/ESP32C3/actions/workflows/build.yml)

## [简介](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_cn.pdf)

[ESP32-C3](https://www.espressif.com/zh-hans/products/socs/ESP32-C3) 160MHz自研RISC-V单核, 内置 flash 的子型号 ESP32-C3F, ESP32-C3-MINI-1 模组尺寸小巧 (13×16.6 mm)，可支持最高工作温度 105℃。

ESP32-C3-WROOM-02 模组也支持 105℃ 的最高工作温度，它与 ESP-WROOM-02D 和 ESP-WROOM-02 模组引脚兼容，方便用户升级到 ESP32-C3。

[![sites](docs/ESP32-C3.png)](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_cn.pdf)

### 关键特性

* 160 MHz RISC-V , [407.22 CoreMark](https://www.eembc.org/coremark/scores.php)
* 400KB SRAM (TCM) + 384KB ROM + 4MB SPI Flash
* 2.4 GHz Wi-Fi4 & BLE 5.0
* 2 × UART
* QFN32(5*5)

## [xin资源](https://github.com/SoCXin)

* [参考资源](src/)
* [相关文档](docs/)
* [典型应用](project/)

## [选型建议](https://github.com/SoCXin)

[ESP32C3](https://github.com/SoCXin/ESP32C3) 成本对标 ESP8266,可满足各类常见的物联网产品功能需求。沿用乐鑫成熟的物联网开发框架[ESP-IDF](https://github.com/espressif/esp-idf)。

竞品[BL602](https://github.com/SoCXin/BL602) 拥有更高的主频和更丰富的外设，但Wi-Fi通信带宽相对更弱(72.2Mbps)。而在开源生态上，BL602逐渐被认可。
