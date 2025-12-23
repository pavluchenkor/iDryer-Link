/**
 * @file root_ca.h
 * @brief Let's Encrypt Root CA Certificate (ISRG Root X1)
 *
 * Valid until: 2035-06-04
 * Source: https://letsencrypt.org/certificates/
 *
 * === ПРОЦЕСС СОЗДАНИЯ СЕРТИФИКАТА ===
 *
 * 1. ПОЛУЧЕНИЕ КОРНЕВОГО СЕРТИФИКАТА:
 *    - Скачан с официального источника Let's Encrypt
 *    - Команда: openssl s_client -connect letsencrypt.org:443 -showcerts
 *    - Или: wget https://letsencrypt.org/certificates/isrgrootx1.pem
 *
 * 2. ПРОВЕРКА СЕРТИФИКАТА:
 *    - Проверка срока действия: openssl x509 -in root_ca.pem -text -noout
 *    - Проверка отпечатка: openssl x509 -in root_ca.pem -fingerprint -noout
 *
 * 3. КОНВЕРТАЦИЯ В C-СТРОКУ:
 *    - Оригинальный PEM-формат преобразован в строку констант
 *    - Сохранена в PROGMEM для экономии ОЗУ микроконтроллера
 *    - Используется для проверки TLS-сертификатов MQTT-сервера
 *
 * 4. ИСПОЛЬЗОВАНИЕ:
 *    - ESP8266/ESP32 использует этот root CA для валидации сертификата mqtt.idryer.org
 *    - Проверяется цепочка сертификатов: mqtt.idryer.org → Intermediate CA → Root CA (здесь)
 *    - Без валидного root CA невозможно установить защищённое соединение
 *
 * 5. ПРОВЕРКА АКТУАЛЬНОСТИ:
 *    - Ежегодно проверять срок действия (истекает 2035-06-04)
 *    - Обновлять при наличии новых версий root CA
 */

#ifndef ROOT_CA_H
#define ROOT_CA_H

// Let's Encrypt ISRG Root X1 (для mqtt.idryer.org)
//
// ТЕХНИКА ПРЕОБРАЗОВАНИЯ PEM → C-СТРОКА:
// =========================================
// 1. Raw String Literal (R"(...)"):
//    - Синтаксис C++11, позволяет вставить текст без экранирования
//    - Все символы (включая переносы строк) интерпретируются буквально
//    - Не нужно экранировать кавычки и обратные слэши
//    - Альтернатива: копировать построчно с '\n' в конце каждой строки
//
// 2. PROGMEM (Program Memory):
//    - Макрос для ESP8266/ESP32, размещает константу во flash памяти
//    - Экономит оперативную память (ОЗУ) микроконтроллера
//    - Сертификат ~1.5 КБ может целиком хранится в flash (обычно 4 МБ)
//    - При чтении используются специальные функции: sprintf_P(), strcpy_P()
//
// 3. Почему именно так:
//    - Размер: полный PEM сертификат ~1500 байт
//    - Безопасность: сертификат не изменяется в runtime
//    - Компактность: PROGMEM примерно в 2-3 раза меньше, чем в ОЗУ
//    - Удобство: копировать текст как есть, без изменений
//
const char ROOT_CA_LETSENCRYPT[] PROGMEM = R"(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)";

#endif // ROOT_CA_H
