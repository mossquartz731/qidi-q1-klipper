# Stock Q1 Pro timed pressure advance

| Boundary | Decision |
| --- | --- |
| Base | QIDITECH/klipper at Q1 submodule revision `5a41ce0a0e1164be6e9f27ebde8973ee0cd96252`; `q1-pro-stock` retains the reviewed Q1 line, upstream `main` is preserved |
| Source history | Timed-PA port and expired-history free are separate commits. Runtime changes only `extruder.py`, CFFI definitions and `kin_extruder.c` |
| Installation | Host-only Python/CFFI/native helper pair. Build with the printer's interpreter/compiler, retain original source **and compiled helper**, install with Klippy stopped, then verify readiness and loaded state. This source repo is not a remote installer |
| Hardware | No MCU flash, OS migration, probe replacement or screen replacement is required |
| Calibration | Code qualification is printer/software-level. PA measurements belong to a material/nozzle/temperature/flow/acceleration combination; they are not universal printer constants |
| Current state | Native Q1 qualification and installation were verified by the private operational owner. Physical PETG measurements and actual Orca adaptive-PA comparison remain pending; no physical benefit is claimed |
| Private boundary | Credentials, addresses, backups, raw device captures, print assets and material readings remain in their existing private owners |

## Upstream changes and traps

- [Timed PA c84d78f](https://github.com/Klipper3d/klipper/commit/c84d78f3f169bc5163d11b74837f9880b0b7dba4) moves PA changes onto the motion timeline. QIDI already supplies the lookahead and scan-window APIs; later motion-queuing architecture is not imported.
- [Leak fix af81d23](https://github.com/Klipper3d/klipper/commit/af81d23fec795ba64882d2206bc9cbc019e4501b) frees expired history. Pending values must remain until generated motion no longer needs them.
- Ordinary nonzero PA changes avoid queue flushes. Smoothing-window changes, including PA switching to/from zero, still flush. Keep smoothing fixed in an adaptive comparison.
- Python caller, CFFI signature, C helper and destructor are one ABI change. Never ship an unmatched compiled helper.
- Orca's native PA calibration modes bypass adaptive PA. Measure inputs there, then test a normal sliced object with the measured table. A checkbox or successful slice does not establish physical performance.

## Focused checks

```sh
gcc -Wall -Werror -O2 -fsanitize=address,undefined \
  -Iklippy/chelper test/host/pa_history_test.c \
  klippy/chelper/trapq.c -lm -o /tmp/q1-pa-history
/tmp/q1-pa-history
```

The actual implementation is checked against closed-form motion integrals,
PA-boundary smoothing, a large print clock, non-PA moves, zero/re-enabled windows,
queued bursts, deduplication, expired cleanup and full deallocation. GitHub CI
runs this on Linux without printer credentials or hardware. Klippy fixtures run
through the existing native `scripts/test_klippy.py` dictionary-based runner;
they are not a numerical step-stream oracle. Hardware acceptance remains a
measured-material comparison with human preview and physical inspection.

This is a focused GPL-3.0 derivative, not a new firmware distribution. Retain
upstream copyright and licence notices. A public upstream contribution can follow
physical acceptance; an unprinted coupon is not qualified.
