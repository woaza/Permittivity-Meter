# Permittivity Meter ReDesign – TODO List  
**ECE.23.D** | **Version 1.1** | **05-Nov-2025**

---
## Todos vom meetig am 27.11. Messalgorithmus und mc

Algorithmus:
Die untere varicap soll mit eingestellt werden bis ein minimum gefunden wird, die variacap braucht einfach eine gleichspannung angelegt und dann soll am ausgang mit unserem adc das signal gelesen werden.
Die oberen auf irgendwas lassen und zuerst die untere (D1 (p zu c20)) einstellen. Die Untere muss so eingestellt werden dass die richtige frequenz im spektrum für das minimum erreicht wurde. Danach kann man dann mit der anderen Varicap das minimum verbessern.

Sonstiges: 
Der GPIO Pin am µC wird wahrscheinlich nicht den 50Ohm eingang von der Schaltung mit dem rechtecksignal treiben können. Deswegen werden wir einen OPV nach dem GPIO Pin schalten müssen, dann können wir auch die verstärkung einstellen falls wir das brauchen.






## Deadline 07.11.2025
- [O] choosing an External Crystal

  **Part:** 449-LFXTAL058284BULK
  **BOM line:** X3
  **Link: https://www.mouser.at/ProductDetail/IQD/LFXTAL058284Bulk?qs=sGAEpiMZZMsBj6bBr9Q9af1kE%252BXo19x3mGiGn1Dh61%2FyhX7eZvTWqw%3D%3D**

- [ ] Shield Pinout – “Permittivity Shield v1.0”

  **Goal:** Plug-on Arduino shield using **outside peripheral pins**

- [ ] Power Architecture – “Mountain-Proof”  
  **Requirements:**   
  - –40 … +85 °C  
  - Power via Battrie or Bowerbank
  - Calculate runtime untill rechare is needed (can be done afterwards).

  **Power regulation:**
  - How will the power will be divided to the diffrent active component.
  - what kind of Boot/Buck will be needed and where will they be placed?

