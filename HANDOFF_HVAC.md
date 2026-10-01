# HANDOFF: Проект HVAC_CleanRoom

**Актуализация:** 2026-09-20 (по фактическому состоянию репозитория).

## 1. Суть проекта
Распределенная система климат-контроля чистых помещений: Главный ПЛК, локомотивы (концентраторы), хаб дисплеев, вагоны (модули I/O), панели оператора. Связь — CAN/FDCAN.

## 2. Аппаратная часть (Analog_Modul_AI — вагон)
*   **МК:** STM32G0B1RBT6 (не F042 — устаревшая запись).
*   **CAN:** FDCAN2, 50 кбит/с, пины PC2 (RX), PC3 (TX).
*   **АЦП:** ADS1220 по SPI1 — PA5 SCK, PA6 MISO, PA7 MOSI; CS = PA4 (`STM_CS`); DRDY = PC4 (`STM_DRDY`, EXTI). SPI: 8 bit, CPHA = 2 Edge.
*   **Питание / поле:** 24 VDC, шина с CAN_H/CAN_L (детали в Docs / схемах Modul_AI).
*   **Статус:** проект CubeMX + HAL-каркас есть; драйвер ADS1220 и прикладной протокол CAN — впереди.

## 3. Остальные узлы (факт по репозиторию)
| Узел | МК | Статус |
|------|-----|--------|
| Firmware_MainPLC | STM32H723ZGT6 | Есть: FreeRTOS, ETH+LwIP, FDCAN1–3, USB CDC, RS485-задачи |
| Firmware_Locomotive | STM32G0B1RBT6 | Каркас: FDCAN1+FDCAN2 @ 50 кбит/с |
| Firmware_HUB_Displays | STM32G0B1RBT6 | Каркас: FDCAN1+FDCAN2 @ 50 кбит/с |
| Firmware_Displays (TFT5) | STM32H723 | Legacy-панель SSD1963 |
| Analog_Modul_AO / DI / DO | — | Папки пустые |
| Shared_Libs | — | Пусто |
| Web_Interface | — | Пусто |

**Docs:** схемы HUB/AI/Main, `MainPLC_Hardware_Spec.md`, 3D (Modul HUB Display, Modul_AI, Modul_Lokomotiv), даташиты TFT 4.3" и 10.1", сметы.

## 4. Структура (Monorepo)
`Docs/`, `Shared_Libs/`, `Firmware_Wagons/`, `Firmware_Locomotive/`, `Firmware_HUB_Displays/`, `Firmware_MainPLC/`, `Firmware_Displays/`, `Web_Interface/`.

CubeIDE: Hierarchical Presentation.

## 5. Следующие шаги
1. **`Shared_Libs/hvac_can.h`** — общий формат кадров (подключён в include paths Loco / AI / HUB / Displays). Сделано: кадр 0x301 «измерение AI» (влажность и температура ×0.01, байт статуса: обрыв/перегруз каналов, нет ответа АЦП, «нет связи» от локомотива; счётчик). Путь: AI CAN2 → Loco CAN1 → Loco CAN2 → HUB CAN2 → HUB CAN1 → дисплей CAN1. Дальше — адресация и типы модулей.
2. **Analog_Modul_AI** — драйвер ADS1220 (SPI + DRDY), публикация измерений по CAN.
3. **Выровнять битрейт MainPLC FDCAN** с полевой сетью 50 кбит/с (сейчас в IOC номинал выглядит незавершённым).
4. Custom CAN Bootloader для вагонов/локомотивов.
5. Заполнить AO / DI / DO и Web_Interface по мере готовности железа.
