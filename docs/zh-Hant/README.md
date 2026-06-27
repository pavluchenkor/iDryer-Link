# iDryer Link — 快速指南

Link 是 iDryer 的通訊模組。它連接到控制器的以太網連接埠，並通過該連接埠與控制板通訊（該連接埠用作電源和 UART 接頭，而不是網路連接埠）。Link 將乾燥機連接到網際網路，並將其與 [portal.idryer.org](https://portal.idryer.org/) 關聯。

![link1](../img/link2.png)
![link1](../img/link1.png)

## 如何連接到控制器
**關閉控制器電源**

**組裝 RJ45 連接線**，請參照
![RJ45](../img/RJ45.png)
以確保導線配對正確。重要提示：此處的 RJ45 僅是電源/UART 接頭，不要將其連接到網路交換機。

**將電線連接到 ESP32-C3 Super mini**，按照示意圖
![esp32superMini](../img/esp32superMini.png)

**根據電路板的類型和製造商，引腳 6 和 7 的位置可能會有所不同。**
請檢查製造商提供的電路板引腳配置。

```
UART_RX_PIN 6 (白藍)
UART_TX_PIN 7 (白綠)
```

**ESP32-C3 super mini 引腳配置**

![ESP32-C3 super mini 引腳配置](../img/ESP32-C3-Super-Mini-pinout-low.jpg)

**ESP32-C3 Zero (Waveshare) 引腳配置**

![ESP32-C3 super mini 引腳配置](../img/ESP32-C3-ZERO-Waveshare-pinout-low.jpg)

同樣地，參照引腳配置，您可以連接任何開發電路板。


**連接線配置**

 ![wiring](../img/wiring.png) 
<!-- 5) Подключите Link в Ethernet‑порт контроллера. После включения питания контроллер будет работать с Link как с внешним модемом. -->

## 如何使用網頁刷機器刷入
網頁刷機器位於 https://install.idryer.org/

- 將 Link 連接到電腦的 USB 連接埠。
- 開啟 [頁面](https://install.idryer.org/)，選擇裝置 **iDryer Link**。
- 選擇電路板：
   - `ESP32-C3 super-mini` — 系列模組的主要選項。
   - `ESP32-C3 DevKit` — 如果您使用除錯電路板。
- 點擊 **Connect**，選擇串行連接埠（通常是 `USB JTAG/serial` 或 `CH340`）。如果刷入未啟動，按住電路板上的 `BOOT` 並短暫按下 `RST`。
- 點擊 **Install**。刷機器將自動刷入所有必要的程式。
- 達到 100% 後，Improv 嚮導將開啟：輸入 Wi-Fi SSID 和密碼，等待狀態顯示「已連接」。
- 如果 Improv 嚮導未開啟，請斷開 USB 並重新連接，選擇 **Connect** 而不重新刷入。

## 連接到入口

- 按下 **Start Claim** 按鈕。將出現字串 `PIN:123456` — 這是 PIN，有效期約為 5 分鐘。
- 前往 https://portal.idryer.org → 「新增裝置」 → 輸入 PIN。綁定成功後，裝置將出現在列表中。
- 斷開 USB，並通過 RJ45 將 Link 連接到控制器。
- 打開 iDryer 電源
- LED 指示燈將以藍色「呼吸」顯示成功連接

## 應該得到的結果
- 控制器在供電後立即看到 Link。
- 網頁入口上的裝置在連接到 Wi-Fi 後的 1-2 分鐘內出現在線上。
- 如果狀態未出現：重新檢查 `../../img/RJ45.png` 的引腳配置、連接線壓接品質，以及刷入步驟中選擇的電路板是否正確，並檢查 iDryer 菜單中的連接埠配置。

## CAD

[下載機箱](../../CAD/link-case.stp)
