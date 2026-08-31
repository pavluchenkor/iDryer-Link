# iDryer Link — クイックスタートガイド

Link は iDryer 用通信モジュールです。コントローラーの Ethernet ポートに接続し、制御基板と通信します（このポートはネットワークではなく、電源と UART の接続として使用されます）。Link はドライアーをインターネットに接続し、[portal.idryer.org](https://portal.idryer.org/) と連動させます。

![link1](../img/link2.png)
![link1](../img/link1.png)

## コントローラーへの接続方法

**コントローラーの電源を切ってください**

**RJ45 ケーブルを組み立ててください**。次の画像を参考にして、ペアを間違えないようにしてください。
![RJ45](../img/RJ45.png)

重要：ここでの RJ45 は単なる電源/UART コネクタであり、ネットワークスイッチには接続しないでください。

**配線図に従って ESP32-C3 Super mini に配線してください**
![esp32superMini](../img/esp32superMini.png)

**プレートの種類とメーカーによって、ピン 6 および 7 の配置が異なる場合があります。**
メーカーが提供するピンアウト資料で確認してください。

```
UART_RX_PIN 6 (白青)
UART_TX_PIN 7 (白緑)
```

**ESP32-C3 super mini のピンアウト**

![ESP32-C3 super mini のピンアウト](../img/ESP32-C3-Super-Mini-pinout-low.jpg)

**ESP32-C3 Zero (Waveshare) のピンアウト**

![ESP32-C3 super mini のピンアウト](../img/ESP32-C3-ZERO-Waveshare-pinout-low.jpg)

同様にピンアウトを参照して、任意の開発ボードを接続できます。


**ケーブルピンアウト**

 ![wiring](../img/wiring.png) 
<!-- 5) Link を コントローラーの Ethernet ポートに接続してください。コントローラーの電源投入後、コントローラーは Link を外部モデムとして動作させます。 -->

## Web フラッシャーを使用した書き込み

Web フラッシャーは https://install.idryer.org/ にあります。

- Link を USB ポートに接続します。
- [ページ](https://install.idryer.org/)を開き、**iDryer Link** デバイスを選択します。
- ボードを選択します：
   - `ESP32-C3 super-mini` — 量産モジュール用の標準オプション。
   - `ESP32-C3 DevKit` — デバッグボードを使用している場合。
- **Connect** をクリックし、シリアルポートを選択します（通常 `USB JTAG/serial` または `CH340`）。書き込みが開始しない場合は、ボードの `BOOT` を押しながら `RST` を短く押してください。
- **Install** をクリックします。フラッシャーが必要なすべてをプログラムします。
- 100% 後に Improv ウィザードが開きます：Wi-Fi SSID とパスワードを入力し、「Connected」ステータスを待ってください。
- Improv ウィザードが開かない場合は、USB を取り外して再度接続し、書き込みなしで **Connect** を選択してください。

## ポータルへの接続

- **Start Claim** ボタンをクリックします。`PIN:12345678` という文字列が表示されます。これは PIN で、有効期限は 10 分です。
- https://portal.idryer.org → 「デバイスの追加」→ PIN を入力します。デバイスが正常にバインドされた後、デバイスはリストに表示されます。
- USB を取り外し、RJ45 でコントローラーに Link を接続します。
- iDryer の電源を入れます。
- LED インジケーターが青で「呼吸」して、接続成功を通知します。

## 完成したら

- コントローラーは電源投入直後に Link を認識します。
- Web ポータルにデバイスが Wi-Fi 接続後 1 ～ 2 分以内にオンラインで表示されます。
- ステータスが表示されない場合：`../../img/RJ45.png` でピンアウトを再確認し、コネクタの圧着品質と書き込みステップでのボード選択の正確性、および iDryer メニューのポート構成を確認してください。

## CAD

[ケースをダウンロード](../../CAD/link-case.stp)
