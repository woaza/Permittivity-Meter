# Permittivity Meter ReDesign – TODO List

**ECE.23.D** | **Version 1.2** | **01-Dec-2025**

---

## Priority Tasks (Firmware & Hardware)

### Hardware / Electronics

- [ ] **PWM Output Voltage**: The GPIO pin outputs 3.3V, but the circuit requires a **1V peak**.
  - **Action**: Add a voltage divider or OpAmp buffer with attenuation after the GPIO pin (PA9).
  - **Note**: The GPIO cannot drive the 50 Ohm input directly. An OpAmp buffer is required anyway (as noted in meeting notes).
- [ ] **Shield Pinout**: Define "Permittivity Shield v1.0" using outside peripheral pins.
- [ ] **Power Architecture**:
  - [ ] Design for -40 to +85 °C.
  - [ ] Battery/Powerbank support.
  - [ ] Runtime calculation.
  - [ ] Regulator selection (Buck/Boost) and placement.

### Firmware / Algorithms

- [ ] **Peak Detection Algorithm**:
  - [ ] **Step 1 (Coarse)**: Set the lower varicap (D1) to a fixed voltage. Sweep the upper varicap to find the rough frequency range.
  - [ ] **Step 2 (Fine)**: Adjust the lower varicap to fine-tune the minimum.
  - [ ] **Input**: Read the signal via ADC.
- [ ] **Driver Verification**: (Done)
  - [x] ADC (122.5kHz)
  - [x] PWM (20MHz)
  - [x] DAC

---

## Meeting Notes (27.11.2025)

### Algorithm Details

- The lower varicap (D1, parallel to C20) should be adjusted first to find the minimum.
- The varicap needs a DC voltage.
- The output signal is read by the ADC.
- **Process**: Leave the upper varicap fixed -> Adjust lower varicap to find frequency -> Optimize with the other varicap.

### Hardware Notes

- **GPIO Drive Strength**: The microcontroller GPIO cannot drive the 50 Ohm input directly.
- **Solution**: Place an OpAmp (OPV) after the GPIO pin. This allows for impedance matching and gain adjustment (attenuation to 1V).

### Raw Notes

Algorithmus: Die untere varicap soll mit eingestellt werden bis ein minimum gefunden wird, die variacap braucht einfach eine gleichspannung angelegt und dann soll am ausgang mit unserem adc das signal gelesen werden. Die oberen auf irgendwas lassen und zuerst die untere (D1 (p zu c20)) einstellen. Die Untere muss so eingestellt werden dass die richtige frequenz im spektrum für das minimum erreicht wurde. Danach kann man dann mit der anderen Varicap das minimum verbessern.

Sonstiges: Der GPIO Pin am µC wird wahrscheinlich nicht den 50Ohm eingang von der Schaltung mit dem rechtecksignal treiben können. Deswegen werden wir einen OPV nach dem GPIO Pin schalten müssen, dann können wir auch die verstärkung einstellen falls wir das brauchen.

---

## Deadlines & Archive

### Deadline 07.11.2025

- [x] Choosing an External Crystal
  - **Part**: 449-LFXTAL058284BULK
  - **BOM line**: X3
  - **Link**: [Mouser](https://www.mouser.at/ProductDetail/IQD/LFXTAL058284Bulk?qs=sGAEpiMZZMsBj6bBr9Q9af1kE%252BXo19x3mGiGn1Dh61%2FyhX7eZvTWqw%3D%3D)
