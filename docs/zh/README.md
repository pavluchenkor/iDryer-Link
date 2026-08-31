# iDryer Link — 快速指南

Link 是 iDryer 的通信模块。它连接到控制器的以太网端口，通过该端口与主板通信（该端口用作电源和 UART 连接器，不是网络接口）。Link 将烘干机连接到互联网并将其与 [portal.idryer.org](https://portal.idryer.org/) 关联。

![link1](../img/link2.png)
![link1](../img/link1.png)

## 如何连接到控制器
**关闭控制器电源**

**组装 RJ45 电缆**，参考
![RJ45](../img/RJ45.png)
确保线对不混乱。重要提示：此处 RJ45 仅是电源/UART 连接器，不要将其连接到网络交换机。

**将电线连接到 ESP32-C3 Super mini**，按照原理图
![esp32superMini](../img/esp32superMini.png)

**根据板的类型和制造商，引脚 6 和 7 的位置可能不同。**
检查制造商提供的板的引脚图。

```
UART_RX_PIN 6 (白蓝色)
UART_TX_PIN 7 (白绿色)
```

**ESP32-C3 super mini 引脚图**

![ESP32-C3 super mini 引脚图](../img/ESP32-C3-Super-Mini-pinout-low.jpg)

**ESP32-C3 Zero (Waveshare) 引脚图**

![ESP32-C3 super mini 引脚图](../img/ESP32-C3-ZERO-Waveshare-pinout-low.jpg)

同样，通过参考引脚图，可以连接任何开发板。


**电缆接线图**

 ![wiring](../img/wiring.png) 
<!-- 5) Подключите Link в Ethernet‑порт контроллера. После включения питания контроллер будет работать с Link как с внешним модемом. -->

## 如何通过网页烧录工具进行烧录
网页烧录工具位于 https://install.idryer.org/

- 将 Link 连接到计算机的 USB 端口。
- 打开[页面](https://install.idryer.org/)并选择设备 **iDryer Link**。
- 选择板卡：
   - `ESP32-C3 super-mini` — 生产模块的主要选项。
   - `ESP32-C3 DevKit` — 如果您使用的是开发板。
- 点击 **Connect**，选择串行端口（通常为 `USB JTAG/serial` 或 `CH340`）。如果烧录未启动，按住板上的 `BOOT` 并短按 `RST`。
- 点击 **Install**。烧录工具将自动烧录所有必要文件。
- 100% 完成后，Improv 向导将打开：输入 Wi-Fi SSID 和密码，等待状态显示 "Connected"。
- 如果 Improv 向导未打开，断开 USB 连接并重新连接，选择 **Connect** 但不重复烧录。

## 连接到门户

- 点击 **Start Claim** 按钮。将显示字符串 `PIN:12345678` — 这是 PIN，有效期 10 分钟。
- 前往 https://portal.idryer.org → "添加设备" → 输入 PIN。设备绑定成功后将显示在列表中。
- 断开 USB 连接并通过 RJ45 将 Link 连接到控制器。
- 打开 iDryer 电源
- LED 指示灯将以蓝色"呼吸"效果表示成功连接

## 预期结果
- 控制器在上电后立即识别 Link。
- 在网页门户中，设备连接到 Wi-Fi 后的 1-2 分钟内将显示为在线。
- 如果状态未显示：重新检查 `../../img/RJ45.png` 的接线，检查压接质量，验证烧录步骤中选择的板卡是否正确，以及 iDryer 菜单中的端口配置。

## CAD

[下载外壳](../../CAD/link-case.stp)
