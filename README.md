# HVAC CleanRoom Project

Распределенная система управления климатом (HVAC) для чистых помещений.

## Архитектура системы
Связь узлов — локальная шина CAN (FDCAN):
*   **Главный ПЛК (STM32H723ZGT6):** центральный узел, агрегация данных, ПИД, HMI (Ethernet / Modbus TCP / LwIP), двери и полевая связь (RS485), USB CDC, FreeRTOS. В CubeMX: FDCAN1–3, ETH RMII (LAN8742), USB OTG HS.
*   **Локомотивы (STM32G0B1RBT6):** концентраторы сцепок вагонов; два канала FDCAN (FDCAN1 + FDCAN2), 50 кбит/с.
*   **Хаб дисплеев (STM32G0B1RBT6):** мост к панелям оператора; два канала FDCAN, 50 кбит/с.
*   **Вагоны (модули I/O, STM32G0B1RBT6):** аналоговые и дискретные оконечные модули. Сейчас заполнен **Analog_Modul_AI** (ADS1220 по SPI1, FDCAN2). AO / DI / DO — заготовки папок.
*   **Панели оператора:** legacy-проект TFT5 SSD1963 (STM32H723); в Docs — даташиты TFT 4.3" и 10.1", 3D корпусов HUB / AI / Локомотив.

## Структура репозитория (Monorepo)
*   `Docs/` — ТЗ, схемы, спецификация Main PLC, 3D-модели, даташиты дисплеев, сметы.
*   `Shared_Libs/` — общие библиотеки (пока пусто; нужен `can_protocol.h`).
*   `Firmware_Wagons/` — Analog_Modul_AI (есть), Analog_Modul_AO / Digital_Modul_DI / Digital_Modul_DO (пусто).
*   `Firmware_Locomotive/` — прошивка концентратора (каркас CubeMX + FDCAN).
*   `Firmware_HUB_Displays/` — прошивка хаба дисплеев (каркас CubeMX + FDCAN).
*   `Firmware_MainPLC/` — прошивка Главного ПЛК.
*   `Firmware_Displays/` — панель TFT5 SSD1963 (`Display_TFT4.3`).

У каждого модуля две папки: `Application` — основная прошивка, `Bootloader` — загрузчик. Общее ядро загрузчика и протокол обновления — в `Shared_Libs/boot_core` и `Shared_Libs/fwupd`.
*   `Web_Interface/` — фронтенд/бэкенд (пока пусто).

## Особенности
*   Удаленное обновление прошивок вагонов и локомотивов через Custom CAN Bootloader (план).
*   Изоляция CAN на полевой стороне (трансиверы / гальваника по схемам).
*   Проекты STM32CubeIDE в иерархическом монорепозитории.
