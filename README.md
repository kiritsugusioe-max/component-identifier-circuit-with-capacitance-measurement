# component-identifier-circuit-with-capacitance-measurement
R/C/Open identification and capacitance measurement system

This repository contains my personal circuit project for identifying a resistor, capacitor, or open circuit connected to the measurement terminals.  
For capacitors, the system also calculates and displays the capacitance.

## Project Summary

The system uses a comparator-based measurement circuit, a Schmitt trigger inverter, and an ATmega328P microcontroller.  
The main goal of this project was to design and test a simple measurement system, evaluate the accuracy of the capacitance measurement, and analyse the sources of error.

## Main Results

- Resistors, capacitors, and open circuits were successfully identified.
- Capacitance was measured in the range of 1 nF to 10 nF.
- The measured capacitance error was approximately 0.97% to 3.5%.
- The error was analysed by separating it into circuit-level and system-level sources.

## Repository Contents

| Folder | Description |
|---|---|
| `report/` | Project report |
| `schematic/` | Circuit schematic |
| `code/` | ATmega328P program |
| `data/` | Measurement data |
| `videos/` | Demonstration video |
| `image/` | Photograph of the implemented circuit |

## Notes

This project was completed independently as a personal project.  
The focus was on circuit design, measurement, implementation, and error analysis.

## Usage Notice

This repository is publicly available for portfolio and internship application review only.  
All rights are reserved by the author. No permission is granted to copy, redistribute, modify, reproduce, or reuse any part of this repository without explicit written permission.
