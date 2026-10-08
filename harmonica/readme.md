#Harmonics testing:
Here we seek to use an HP/ Agilent 33220A function generator to control a 
speaker, and use that to stimulate the ADXL357BZ sensor in various ways. 
The function Generator is programable, with pyvisa. 

# Tests:

The main tests are displacement, and frequency response. 
when the sensor MCU reports the frequency of the function gen,
for a full spectrum of freqencies, Ill assert that the frequncy response 
code is functional. 

Then we can do things like resonance testing. 

## displacement:
I guess I could measure the speaker cone to crosscheck physical displacement.


# Programing guide:
### Waveform selection

Use `FUNC` to select the waveform while retaining the current frequency,
amplitude, and DC offset settings.

| Waveform | Python command |
|---|---|
| Sine | `fg.write("FUNC SIN")` |
| Square | `fg.write("FUNC SQU")` |
| Ramp / sawtooth | `fg.write("FUNC RAMP")` |
| Pulse | `fg.write("FUNC PULS")` |
| Gaussian noise | `fg.write("FUNC NOIS")` |
| DC | `fg.write("FUNC DC")` |
| Selected arbitrary waveform | `fg.write("FUNC USER")` |

For a triangle wave, select `RAMP` and set symmetry to 50%:

```python
fg.write("FUNC RAMP")
fg.write("FUNC:RAMP:SYMM 50")
```
