================================================================
================================================================
================================================================

On Security, Trust, and Open Source

iDryer is entering its cloud era. With that comes not only new functionality, but also reasonable questions about security, trust, and how much control stays in the hands of the owner.

This is the first, but clearly not the last question of this kind.
https://discord.com/channels/1332280943465201724/1482642064977170542/1484676446424404008

So that I do not have to write the same answer over and over again, I will answer it once here and then simply link back to this text.

If the discussion starts from the premise of “what if the project author is a bad actor,” then within that model nothing can really be proven to anyone at all. In that case, suspicion should apply not only to iDryer, but also to Home Assistant, the router, the phone, the cameras, and every other node on the network. At that point, it is no longer a technical discussion about risks, but a question of baseline trust in any modern device, which in its extreme form easily turns into ordinary network paranoia.

So I suggest discussing not the author’s motives, but the architecture, the code, and the verifiable boundaries of access.

Yes, LINK knows the Wi-Fi password. Otherwise Wi-Fi simply would not work. In the current implementation, it is stored locally on the device and used to connect to your network. There is no separate transmission of Wi-Fi credentials to the cloud or to Home Assistant in the firmware.

Yes, if the device is connected to a local MQTT broker, it has whatever access is granted to it by the owner of that broker. And it is the broker owner who is responsible for defining the boundaries of that access within their own network. This is not something specific to iDryer, but a basic responsibility of anyone connecting third-party devices to their home automation setup.

LINK’s architecture was not originally designed for fully isolated local-only use, but for a real mainstream scenario: portal, claiming, authorization, app, support. For most users, cloud infrastructure in a product like this is an advantage, not a drawback. That is my view of the product, the user experience, and how this kind of system should work for the majority of owners. I do not consider it reasonable to break that model for someone’s private preferences.

The cloud is needed here not only for authorization, claiming, and remote access. It also supports tasks that are simply not reasonable to implement at the microcontroller level: a filament catalog, material and drying presets, a shared knowledge base, telemetry collection and processing, and preparation of data for writing RFID tags in multiple formats. This is not the kind of logic and data processing that makes sense to push onto an MCU. If all of that had to be done locally, it would require a very different hardware platform, closer to a Raspberry Pi or similar systems, with a different cost and a different level of complexity.

At the same time, the project is not built on blind trust in me. This is a private project, not a large team. That is exactly why the LINK source code will be opened after I bring it into proper condition: structure, comments, documentation, and general usability for other developers.

Yes, I use AI review and automated code-review tools. But that is not a substitute for open source and a critical view from the community. On the contrary, in a single-developer project external review is especially important, because one person can absolutely make mistakes or overlook things.

Some of the architectural decisions here were made from a simple assumption: physical access to the dryer belongs only to its owner. For a device of this class, that is a normal and deliberate model.

The bottom line is simple: the project is open, its architectural decisions are deliberate, its mainstream scenario is cloud-based, and for those who want a different level of control there will be source code and the ability to adapt the firmware to their own requirements, rather than an expectation that the entire product should be rebuilt around someone’s specific preferences.

The firmware performs its core functions perfectly well without Wi-Fi. Drying quality is not determined by network access. Whether to use the network features or not is entirely up to you.

In any case, this is the current solution that I am offering to the community for evaluation in terms of functionality, usability, and the overall direction of the product. It is not a final or permanently fixed design. What comes next will be shaped by real-world use, community feedback, and the direction in which the product itself evolves.


================================================================
================================================================
================================================================

О безопасности, доверии и открытом коде

iDryer вступает в эру облачных технологий. Это влечет за собой не только появление новых функций, но и вполне обоснованные вопросы о безопасности, доверии и том, насколько владелец сохраняет контроль над устройством.

Это первый, но явно не последний вопрос такого типа.
https://discord.com/channels/1332280943465201724/1482642064977170542/1484676446424404008

Чтобы не писать один и тот же ответ каждый раз, отвечу один раз здесь и дальше буду просто давать ссылку на этот текст.

Если рассуждать в логике «а что если автор проекта bad actor», то в такой модели нельзя доказать вообще ничего и никому. Тогда под подозрением должны быть не только iDryer, но и Home Assistant, роутер, телефон, камеры и любой другой узел в сети. Это уже не технический разговор о рисках, а вопрос базового доверия к любому современному устройству, который в крайней форме легко скатывается в обычную сетевую истерию.

Поэтому я предлагаю обсуждать не мотивы автора, а архитектуру, код и проверяемые границы доступа.

Да, LINK знает пароль от Wi‑Fi. Иначе Wi‑Fi просто не работает. По текущей реализации он хранится локально на устройстве и используется для подключения к вашей сети. Отдельной отправки учетных данных Wi‑Fi в облако или в Home Assistant в прошивке нет.

Да, если устройство подключено к локальному MQTT брокеру, оно имеет тот доступ, который ему выдал владелец этого брокера. И именно владелец брокера отвечает за границы этого доступа в своей сети. Это не особенность iDryer, а базовая ответственность любого, кто подключает сторонние устройства к своей домашней автоматизации.

Архитектура LINK изначально сделана не под полностью изолированное локальное использование, а под реальный массовый сценарий: портал, привязка, авторизация, приложение, поддержка. Для большинства пользователей облачная инфраструктура в таком продукте является плюсом, а не минусом. Это мое видение продукта, пользовательского опыта и того, как такая система должна работать для основной массы владельцев. Я не считаю правильным ломать эту модель ради чьих-то частных предпочтений.

Облако здесь нужно не только для авторизации, привязки и удалённого доступа. На нём также завязаны задачи, которые просто неразумно пытаться реализовать на уровне микроконтроллера: каталог филаментов, пресеты материалов и режимов сушки, коллективная база знаний, сбор и обработка телеметрии, подготовка данных для записи на RFID-метки в нескольких форматах. Это не тот объём логики и данных, который разумно перекладывать на MCU. Если пытаться делать всё это локально, речь пойдёт уже не о контроллере такого класса, а о совсем другой аппаратной платформе, уровня Raspberry Pi или аналогов, с другой стоимостью и другой сложностью.

При этом проект не строится на слепом доверии ко мне. Это частный проект, а не большая команда. Именно поэтому исходники LINK будут открыты после того, как я приведу их в нормальное состояние: структура, комментарии, документация, пригодность для работы другим разработчикам.

Да, я использую AI-ревью и автоматические инструменты проверки кода. Но это не замена открытому исходному коду и критическому взгляду сообщества. Наоборот: в частном проекте внешняя проверка особенно важна, потому что один человек вполне может ошибаться или что-то упустить.

Часть архитектурных решений здесь изначально принималась из простой предпосылки: физический доступ к сушилке есть только у её владельца. Для такого класса устройства это нормальная и осознанная модель.

Итог простой: проект открыт, архитектурные решения в нём осознанные, массовый сценарий у продукта облачный, а для тех, кому нужен другой уровень контроля, будет исходный код и возможность адаптировать прошивку под свои требования, а не ожидание того, что весь продукт будет перестроен под частный сценарий.

Прошивка отлично справляется со своими основными функциями и без Wi‑Fi. Качество сушки не определяется наличием доступа в сеть. Использовать сетевые функции или нет — это ваш выбор.

В любом случае, это текущее решение, которое я предлагаю сообществу для оценки функциональности, качества использования, удобства и общего вектора развития продукта. Это не окончательная и не «навсегда зафиксированная» конструкция. Дальше всё будет определяться реальным опытом, обратной связью сообщества и тем, куда в итоге повернёт кривая развития самого продукта.
