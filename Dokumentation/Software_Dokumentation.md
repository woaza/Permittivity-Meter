# Software-Dokumentation – Permittivity Meter V2

## 1. Einleitung

### 1.1 Zweck des Dokuments

### 1.2 Geltungsbereich

### 1.3 Zielgruppe

### 1.4 Referenzen und verwandte Dokumente

### 1.5 Abkürzungen und Begriffe

---

## 2. Systemübersicht

### 2.1 Projektbeschreibung

### 2.2 Architekturübersicht (Layer-Diagramm)

### 2.3 Zielhardware (STM32L476RG / NUCLEO-L476RG)

### 2.4 Softwareschichten im Überblick

---

## 3. HAL Board Layer (`hl/hal_board.c`)

### 3.1 Zweck und Verantwortlichkeit

### 3.2 Schnittstelle zum darunterliegenden HAL-Treiber

### 3.3 Bereitgestellte Funktionen

#### 3.3.1 LED-Steuerung (`HalBoard_LED_Set`, `HalBoard_LED_Get`, `HalBoard_LED_Toggle`)

#### 3.3.2 Button-Abfrage (`HalBoard_BTN_Read`)

#### 3.3.3 DAC-Steuerung (`HalBoard_DAC_Set`, `HalBoard_DAC_SetRaw`)

#### 3.3.4 ADC-Abfrage (`HalBoard_ADC_Read`, `HalBoard_ADC_ReadRaw`)

#### 3.3.5 PWM-Steuerung (`HalBoard_PWM_Start`, `HalBoard_PWM_Stop`, `HalBoard_PWM_SetFreq`, `HalBoard_PWM_SetDuty`)

#### 3.3.6 Gain-Steuerung (`HalBoard_GAIN_Set`, `HalBoard_GAIN_Get`)

#### 3.3.7 NINA-Modul-Steuerung (`HalBoard_NINA_Reset`, `HalBoard_NINA_Stop`)

#### 3.3.8 LCD-Steuerung (`HalBoard_LCD_SetLine`)

#### 3.3.9 Initialisierung (`HalBoard_Init`)

### 3.4 Manual-Mode-Konzept (Handbetrieb)

### 3.5 CMD:HAL:\*-Befehlsrouting

### 3.6 Push-Style ACK Frames (`STAT:HW:*`)

---

## 4. Mock Board Layer (`mocks/mock_board.c`)

### 4.1 Zweck und Einsatzbereich

### 4.2 RF-Antwort-Modell (Parabolisches Dip-Modell)

#### 4.2.1 Mathematische Formel

#### 4.2.2 Konfigurierbare Parameter (Resonanz, Rauschen, Basis, Krümmung)

### 4.3 Bereitgestellte Funktionen

#### 4.3.1 `MockBoard_Init`

#### 4.3.2 `MockBoard_SetDAC`

#### 4.3.3 `MockBoard_ReadAmplitude`

#### 4.3.4 `MockBoard_SetResonance` / `MockBoard_SetNoise` / `MockBoard_SetBase` / `MockBoard_SetFail`

### 4.4 CMD:MOCK:\*-Befehle

### 4.5 Umschaltung Mock ↔ reale Hardware

### 4.6 Einschränkungen und bekannte Limitierungen

---

## 5. BSP Layer – Board Support Package

### 5.1 Überblick und Zweck

### 5.2 BSP RF (`bsp_rf.c` / `bsp_rf.h`)

#### 5.2.1 Zweck (Abstraktion des RF-Frontends)

#### 5.2.2 Bereitgestellte Funktionen (`BSP_RF_Init`, `BSP_RF_SetFreqVoltage`, `BSP_RF_SetQVoltage`, `BSP_RF_ReadAmplitude`)

#### 5.2.3 Switch-Point: Mock vs. reale Hardware

#### 5.2.4 Geplante Erweiterungen (DMA Buffer Capture)

### 5.3 BSP UI (`bsp_ui.c` / `bsp_ui.h`)

#### 5.3.1 Zweck (Button- und LED-Verwaltung)

#### 5.3.2 Bereitgestellte Funktionen

#### 5.3.3 Button-Logik und Entprellung

#### 5.3.4 LED-Zustandsverwaltung

### 5.4 BSP LCD (`bsp_lcd.c` / `bsp_lcd.h`)

#### 5.4.1 Zweck (LCD-Pufferverwaltung)

#### 5.4.2 Bereitgestellte Funktionen

#### 5.4.3 I2C-Anbindung und Zeilenpuffer

---

## 6. Applikationsschicht – Finite State Machine (`fsm_main.c`)

### 6.1 Überblick und Verantwortlichkeit

### 6.2 Zustandsdiagramm

### 6.3 Zustände im Detail

#### 6.3.1 `STATE_INIT`

#### 6.3.2 `STATE_IDLE`

#### 6.3.3 `STATE_CALIBRATION`

#### 6.3.4 `STATE_MEASURE_SEARCH`

#### 6.3.5 `STATE_MANUAL_OPERATION`

#### 6.3.6 `STATE_ERROR`

#### 6.3.7 `STATE_CALCULATION` (geplant)

### 6.4 Events und Event-Queue

#### 6.4.1 FSM-Event-Typen

#### 6.4.2 Event-Quellen (Button, BT-Manager)

#### 6.4.3 Event-Verarbeitung und Priorisierung

### 6.5 Zustandsübergänge

### 6.6 Fehlerbehandlung und Recovery

---

## 7. Messlogik (`rf_measure.c`)

### 7.1 Überblick und Verantwortlichkeit

### 7.2 Kalibrierung (Air Calibration)

#### 7.2.1 Ablauf (`RF_PerformAirCalibration`)

#### 7.2.2 Coarse Sweep

#### 7.2.3 Fine Sweep

#### 7.2.4 Parabolische Interpolation

#### 7.2.5 Ergebnis und Speicherung

### 7.3 Messung (Snow Measurement)

#### 7.3.1 Ablauf (`RF_PerformSnowMeasurement`)

#### 7.3.2 Suchbereich relativ zur Kalibrierung

#### 7.3.3 Ergebnisberechnung

### 7.4 Sampling-Funktion (`sample_at`)

### 7.5 Konfigurierbare Parameter (Sweep-Bereich, Schrittweiten)

### 7.6 Fehlerbehandlung (ungültige Messungen)

---

## 8. RF Trace (`rf_trace.c`)

### 8.1 Zweck (Sweep-Daten-Aufzeichnung)

### 8.2 Bereitgestellte Funktionen

### 8.3 Datenformat und Ausgabe (`DAT:TRACE:*`)

### 8.4 Nutzung zur Diagnose

---

## 9. Mathematisches Modell (`math_model.c`)

### 9.1 Zweck und Verantwortlichkeit

### 9.2 Permittivitätsberechnung

### 9.3 Signalverarbeitung (Undersampling / Bandpass Sampling)

#### 9.3.1 Prinzip des Undersamplings

#### 9.3.2 Alias-Frequenz-Berechnung

#### 9.3.3 DFT/Goertzel-Algorithmus (geplant)

### 9.4 Fehlerbetrachtung und Genauigkeit

---

## 10. Debug-Logging (`debug_log.c`)

### 10.1 Zweck (interner Ringpuffer für Diagnose)

### 10.2 Bereitgestellte Funktionen

### 10.3 Log-Domänen und Filterung

### 10.4 Ausgabe über `CMD:LOG`

---

## 11. Kommunikationsschicht – Protokoll und Transport

### 11.1 Überblick (ASCII-Protokoll)

### 11.2 BT-Manager / Protokoll-Parser (`bt_manager.c`)

#### 11.2.1 Zweck und Verantwortlichkeit

#### 11.2.2 Protokollstruktur (`CMD:*`, `STAT:*`, `DAT:*`)

#### 11.2.3 Befehlsverarbeitung (`BT_ProcessIncoming`)

#### 11.2.4 Befehlsrouting und Dispatch

#### 11.2.5 Antwortgenerierung (`BT_Send`, `BT_Printf`)

#### 11.2.6 Integration mit FSM (Event-Weiterleitung)

#### 11.2.7 Integration mit HAL Board (CMD:HAL:\*-Routing)

#### 11.2.8 Integration mit Mock Board (CMD:MOCK:\*-Routing)

### 11.3 USB CDC Bridge / USART2 Transport (`usb_cdc_bridge.c`)

#### 11.3.1 Zweck (Zeilenweise Empfangs-/Sendeschnittstelle)

#### 11.3.2 DMA Receive-to-Idle (bevorzugt)

#### 11.3.3 Interrupt-basierter RX (Fallback)

#### 11.3.4 Polling RX (Clock-Fallback)

#### 11.3.5 RX-Byte-Ringpuffer

#### 11.3.6 RX-Line-Queue (zeilenweise Verarbeitung)

#### 11.3.7 TX-Ausgabe

### 11.4 Bluetooth-Kommunikation (`bt_communication.c`) (geplant)

#### 11.4.1 UART4 / NINA-Modul

#### 11.4.2 Geplante Integration in den Protokoll-Parser

---

## 12. Hauptprogramm und Initialisierung (`main.c`)

### 12.1 Boot-Sequenz

### 12.2 Systemtakt-Konfiguration (HSE / MSI Fallback)

### 12.3 Peripherie-Initialisierung

### 12.4 Hauptschleife (Event-Loop)

### 12.5 Reset-Ursachen-Erkennung

---

## 13. Tests

### 13.1 Überblick der Teststrategie

### 13.2 Unit-Tests

#### 13.2.1 Test HAL DAC (`test_hal_dac.c`)

#### 13.2.2 Weitere Unit-Tests

### 13.3 Integrationstests

### 13.4 PC-basierte Lifecycle-Tests (`tools/`)

#### 13.4.1 PC CLI (`pc_cli.py`)

#### 13.4.2 Lifecycle Test Script

#### 13.4.3 PySimpleGUI Desktop-Tool

### 13.5 Mock-basiertes Testen

---

## 14. PC-Tools und externe Schnittstellen (`tools/`)

### 14.1 Überblick

### 14.2 PC CLI (`pc_cli.py`)

#### 14.2.1 Funktionalität

#### 14.2.2 Verwendung

### 14.3 GUI-Tool (PySimpleGUI)

#### 14.3.1 Funktionalität

#### 14.3.2 Verwendung

### 14.4 Test-Skripte

---

## 15. Befehlsreferenz (Kommandoprotokoll)

### 15.1 Allgemeine Konventionen (Zeilenende, Encoding)

### 15.2 Kontrollbefehle (`CMD:CONN`, `CMD:RESET`, `CMD:CAL`, `CMD:MEAS`, `CMD:BTN:*`)

### 15.3 Debug- und Status-Befehle (`CMD:LEDS`, `CMD:LCD`, `CMD:LOG`, `CMD:TRACE`)

### 15.4 Mock-Befehle (`CMD:MOCK:RF:*`)

### 15.5 HAL-Board-Befehle (`CMD:HAL:*`)

### 15.6 Manual-Mode-Befehle (`CMD:MANUAL:*`)

### 15.7 Antwortformate (`STAT:*`, `DAT:*`)

---

## 16. Konfiguration und Build

### 16.1 Projektstruktur (Verzeichnisse und Dateien)

### 16.2 Build-Umgebung und Toolchain

### 16.3 Compiler-Flags und Defines

### 16.4 Linker-Konfiguration

### 16.5 Abhängigkeiten (STM32 HAL, CMSIS)

---

## 17. Bekannte Einschränkungen und offene Punkte

### 17.1 Aktuelle Limitierungen

### 17.2 Geplante Erweiterungen

### 17.3 Offene To-Dos (siehe ToDos.md / Milestones.md)
