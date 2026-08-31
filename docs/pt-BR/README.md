# iDryer Link — Guia Rápido

Link é um módulo de comunicação para iDryer. Ele se conecta à porta Ethernet do controlador e se comunica com a placa de controle (a porta é usada como conector de alimentação e UART, não como rede). Link coloca o desumidificador na internet e o vincula ao [portal.idryer.org](https://portal.idryer.org/).

![link1](../img/link2.png)
![link1](../img/link1.png)

## Como conectar ao controlador
**Desligue a alimentação do controlador**

**Prepare o cabo RJ45**, verifique
![RJ45](../img/RJ45.png)
para não confundir os pares. Importante: RJ45 aqui é apenas um conector de alimentação/UART, não o conecte a um switch de rede.

**Conecte os fios** ao ESP32-C3 Super mini de acordo com o esquema
![esp32superMini](../img/esp32superMini.png)

**Dependendo do tipo e fabricante da placa, o local dos pinos 6 e 7 pode variar.**
Verifique o pinout da sua placa fornecido pelo fabricante.

```
UART_RX_PIN 6 (branco-azul)
UART_TX_PIN 7 (branco-verde)
```

**Pinout ESP32-C3 super mini**

![Pinout ESP32-C3 super mini](../img/ESP32-C3-Super-Mini-pinout-low.jpg)

**Pinout ESP32-C3 Zero (Waveshare)**

![Pinout ESP32-C3 super mini](../img/ESP32-C3-ZERO-Waveshare-pinout-low.jpg)

Da mesma forma, usando o pinout como referência, você pode conectar qualquer placa de desenvolvimento.


**Fiação do cabo**

 ![wiring](../img/wiring.png) 
<!-- 5) Conecte Link à porta Ethernet do controlador. Após ligar a alimentação, o controlador funcionará com Link como um modem externo. -->

## Como programar via programador web
O programador web está em https://install.idryer.org/ 

- Conecte Link à porta USB do seu computador.
- Abra a [página](https://install.idryer.org/ ) e selecione o dispositivo **iDryer Link**.
- Selecione a placa:
   - `ESP32-C3 super-mini` — opção principal para módulos em série.
   - `ESP32-C3 DevKit` — se você tiver uma placa de desenvolvimento.
- Clique em **Connect**, selecione a porta serial (geralmente `USB JTAG/serial` ou `CH340`). Se a programação não iniciar, mantenha pressionado `BOOT` na placa e clique brevemente em `RST`.
- Clique em **Install**. O programador fará toda a programação necessária.
- Após 100%, o assistente Improv será aberto: digite o SSID e a senha do Wi-Fi, aguarde o status "Connected".
- Se o assistente Improv não abrir, desconecte a USB e reconecte, selecionando **Connect** sem reprogramar.

## Conexão ao portal

- Clique no botão **Start Claim**. Uma string `PIN:12345678` será exibida — este é o PIN, válido por 10 minutos.
- Acesse https://portal.idryer.org → "Adicionar dispositivo" → insira o PIN. Após vincular com sucesso, o dispositivo aparecerá na lista.
- Desconecte a USB e conecte Link ao controlador via RJ45.
- Ligue a alimentação do iDryer
- A indicação de conexão bem-sucedida será fornecida pela iluminação LED em "respiração" azul

## O que deve acontecer
- O controlador reconhece Link imediatamente após a alimentação ser ligada.
- No portal web, o dispositivo fica online dentro de 1-2 minutos após se conectar ao Wi-Fi.
- Se o status não aparecer: revise o pinout em `../../img/RJ45.png`, a qualidade da crimagem e a seleção correta da placa na etapa de programação e a configuração da porta no menu iDryer.

## CAD

[Baixar gabinete](../../CAD/link-case.stp)
