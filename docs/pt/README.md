# iDryer Link — guia rápido

Link é um módulo de comunicação para iDryer. Ele se conecta à porta Ethernet do controlador e se comunica com a placa de controle através dela (a porta é usada como um conector de energia e UART, não como rede). O Link coloca o secador na internet e o conecta ao [portal.idryer.org](https://portal.idryer.org/).

![link1](../img/link2.png)
![link1](../img/link1.png)

## Como conectar ao controlador
**Desligue a energia do controlador**

**Prepare o cabo RJ45**, consulte
![RJ45](../img/RJ45.png)
para não confundir os pares. Importante: o RJ45 aqui é apenas um conector de energia/UART, não o conecte a um switch de rede.

**Conecte os fios** ao ESP32-C3 Super mini de acordo com o esquema
![esp32superMini](../img/esp32superMini.png)

**Dependendo do tipo e fabricante da placa, a localização dos pinos 6 e 7 pode variar.**
Verifique no diagrama de pinos da sua placa fornecido pelo fabricante.

```
UART_RX_PIN 6 (azul e branco)
UART_TX_PIN 7 (verde e branco)
```

**Diagrama de pinos do ESP32-C3 super mini**

![Diagrama de pinos do ESP32-C3 super mini](../img/ESP32-C3-Super-Mini-pinout-low.jpg)

**Diagrama de pinos do ESP32-C3 Zero (Waveshare)**

![Diagrama de pinos do ESP32-C3 super mini](../img/ESP32-C3-ZERO-Waveshare-pinout-low.jpg)

Da mesma forma, consultando o diagrama de pinos, você pode conectar qualquer placa de desenvolvimento.


**Esquema de fiação do cabo**

 ![wiring](../img/wiring.png)
<!-- 5) Conecte o Link à porta Ethernet do controlador. Após ligar a energia, o controlador funcionará com o Link como um modem externo. -->

## Como fazer flash através do web flasher
O web flasher está em https://install.idryer.org/

- Conecte o Link à porta USB do seu computador.
- Abra a [página](https://install.idryer.org/) e selecione o dispositivo **iDryer Link**.
- Selecione a placa:
   - `ESP32-C3 super-mini` — a opção principal para módulos em série.
   - `ESP32-C3 DevKit` — se você tiver uma placa de desenvolvimento.
- Clique em **Connect**, selecione a porta serial (geralmente `USB JTAG/serial` ou `CH340`). Se o flash não iniciar, mantenha `BOOT` pressionado na placa e pressione `RST` rapidamente.
- Clique em **Install**. O flasher fará o flash de tudo automaticamente.
- Após 100%, o assistente Improv será aberto: insira SSID e senha do Wi-Fi, aguarde o status "Connected".
- Se o assistente Improv não abrir, desconecte o USB e repita a conexão, selecionando **Connect** sem fazer flash novamente.

## Conectar ao portal

- Clique no botão **Start Claim**. Uma string `PIN:123456` aparecerá — este é o PIN, válido por ~5 minutos.
- Vá para https://portal.idryer.org → "Adicionar dispositivo" → insira o PIN. Após a vinculação bem-sucedida, o dispositivo aparecerá na lista.
- Desconecte o USB e conecte o Link ao controlador via RJ45.
- Ligue a energia do iDryer
- A indicação LED informará sobre a conexão bem-sucedida com uma "respiração" azul

## O que deverá resultar
- O controlador vê o Link imediatamente após a energia ser ligada.
- No portal da web, o dispositivo aparecerá online em 1–2 minutos após a conexão ao Wi-Fi.
- Se o status não aparecer: revise novamente o esquema de fiação no `../../img/RJ45.png`, a qualidade da crimpagem e a seleção correta da placa na etapa de flash, e a configuração de portas no menu do iDryer.

## CAD

[Baixar o gabinete](../../CAD/link-case.stp)
