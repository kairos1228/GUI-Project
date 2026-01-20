# AI Agent Instructions for test-PDM-to-I2S Codebase

## Project Overview

**PSoC Edge MCU: PDM to I2S** is a multi-core embedded audio application demonstrating PDM (Pulse-Density Modulation) microphone input to I2S (Inter-IC Sound) audio codec output. The project uses a **dual-CPU three-project architecture** on PSoC Edge E84 MCU.

### Architecture Overview

```
Boot Flow:
ROM → Secure Enclave (SE) → CM33 Secure (proj_cm33_s)
                         → CM33 Non-Secure (proj_cm33_ns) [Main app]
                         → CM55 CPU (proj_cm55) [Put to DeepSleep]

Audio Pipeline:
Microphone → PDM/PCM Hardware Block (CM33_ns) 
           → SRAM Buffer (recorded_data[])
           → I2S Hardware Block (CM33_ns)
           → TLV320DAC3100 Codec (via I2C)
           → Speaker/Headphone
```

## Key Architecture Concepts

### Multi-Project Structure
- **proj_cm33_s**: Secure processing environment. Performs minimal secure configuration and passes control to non-secure. Rarely modified.
- **proj_cm33_ns**: Main application. Initializes clocks, GPIO, enables CM55 core, implements PDM-to-I2S audio recording/playback.
- **proj_cm55**: Minimal stub. Initialized by CM33_ns then enters DeepSleep. Future expansion point.

**Build behavior**: Top-level `Makefile` uses `MTB_TYPE=APPLICATION` to build all three projects sequentially. See [Makefile](Makefile) and [common.mk](common.mk).

### Hardware Peripherals (CM33_ns context)
- **PDM/PCM Block**: Converts digital microphone signal to 16-bit PCM at configurable sampling rates (16 kHz default). Generates interrupts on 32-sample FIFO threshold.
- **I2S Block**: Provides MCLK output and transmits PCM data to audio codec. Configurable word length (16-bit) and sample rate.
- **TLV320DAC3100 Codec**: External audio codec controlled via I2C. Auto-configures dividers for target sampling rates via `mtb_tlv320dac3100` library.
- **GPIO/UART**: Debug output via retarget-io (115200 baud). User button (CYBSP_USER_BTN) triggers record mode.

### Data Flow & Buffers
- **recorded_data[]**: Fixed-size SRAM buffer (BUFFER_SIZE) containing stereo PCM samples (LEFT_CH=channel 2, RIGHT_CH=channel 3).
- **Recording workflow**: Button press → PDM interrupt writes to buffer → Button release → I2S playback from buffer.
- **Sampling rate**: 16 kHz (SAMPLE_RATE_HZ). MCLK set to 2048000 Hz for codec compatibility.
- **Word length**: 16-bit stereo (NUM_CHANNELS=2).

## Build System & Commands

### Build Tool: ModusToolbox Make System
- **Toolchain**: GCC_ARM (default). Support for ARM, IAR, LLVM_ARM also available (see [common.mk](common.mk) TOOLCHAIN variable).
- **Target**: APP_KIT_PSE84_EVAL_EPC2 (default PSoC Edge E84 evaluation kit).
- **Config**: Debug (default). Release available via `CONFIG=Release`.

### Common Build Tasks (from VS Code)

| Task | Command | Purpose |
|------|---------|---------|
| **Build:proj_cm33_ns** | `make build_proj TOOLCHAIN=GCC_ARM CONFIG=Debug` | Rebuild CM33 non-secure project |
| **Build & Program:proj_cm33_ns** | `make program_proj ...` | Build and program all three projects to device |
| **Quick Program:proj_cm33_ns** | `make qprogram_proj` | Program without rebuild (flashes existing binaries) |
| **Rebuild:proj_cm33_ns** | Sequential clean + build | Full rebuild |
| **Build:proj_cm55** | Equivalent to CM33_ns but for CM55 project | CM55-specific build |

All tasks use modus-shell bash wrapper to ensure cross-platform compatibility. See workspace tasks in `.vscode/tasks.json`.

### Manual Build (Terminal)
```bash
cd /path/to/test-PDM-to-I2S
make build_proj TOOLCHAIN=GCC_ARM CONFIG=Debug                    # Build all three projects
make program_proj TOOLCHAIN=GCC_ARM CONFIG=Debug                  # Build and program to device
make qprogram_proj                                                  # Quick program (existing binaries)
make clean_proj                                                     # Clean all projects
```

For individual project builds:
```bash
cd proj_cm33_ns
make build_proj TOOLCHAIN=GCC_ARM CONFIG=Debug
```

### Build Artifacts
- **Compilation database**: `proj_cm33_ns/build/compile_commands.json` (for IntelliSense in VS Code)
- **Binary output**: `proj_cm33_ns/build/APP_KIT_PSE84_EVAL_EPC2/Debug/` (ELF files, hex files)
- **Dependency info**: `proj_cm33_ns/deps/assetlocks.json`, `proj_cm33_ns/libs/*.mtb` (ModusToolbox library manifests)

## Critical Developer Patterns

### Audio Module Architecture (proj_cm33_ns/source/)
Split into two autonomous modules with clear interfaces:

**app_pdm_pcm/** - PDM microphone recording
- `app_pdm_pcm_init()`: Initialize PDM/PCM hardware, register ISR
- `app_pdm_pcm_activate()`: Start recording to SRAM
- `app_pdm_pcm_deactivate()`: Stop recording
- **Data output**: Populates `recorded_data[]` array on ISR (interrupt-driven)

**app_i2s/** - I2S audio output (depends on app_pdm_pcm)
- `app_i2s_init()`: Initialize I2S block, configure TLV320DAC3100 codec via I2C
- `app_i2s_play_audio()`: Start playback from SRAM buffer
- **Data input**: Reads from `recorded_data[]` array populated by PDM module

**Integration pattern**: Main loop polls button flag and orchestrates these module functions. No direct inter-module communication except shared buffer.

### Interrupt Handling Convention
- **Button ISR** (`user_button_interrupt_handler`): Sets `button_flag` volatile flag only. No heavy processing in ISR.
- **PDM ISR** (`channel_config.irq_handler`): Copies FIFO data to `recorded_data[]`, updates `audio_data_ptr` pointer.
- **Main loop**: Polls `button_flag` and `audio_data_ptr` to detect state changes and orchestrate operations.

### Clock/Resource Configuration
- **Clocks**: BSP initialization (`cybsp_init()`) sets up system clocks, peripheral clock routes, and power domains automatically.
- **Pins**: All pin/clock mappings defined in PSoC Creator design file (BSP). Don't modify manually unless extending hardware.
- **CM55 Enable**: CM33_ns explicitly calls `Cy_SysEnableCM55()` after resource setup. CM55 then enters DeepSleep (safe low-power state).

### Key Constants & Naming
- **CYBSP_*** prefix: Board Support Package macros (GPIO, IRQ, clock IDs). Platform-agnostic abstraction layer.
- **Buffer sizing**: `BUFFER_SIZE` (samples per channel), `NUM_CHANNELS=2`, `PDM_HALF_FIFO_SIZE` (interrupt threshold).
- **Audio parameters**: `SAMPLE_RATE_HZ` (16 kHz), `MCLK_HZ` (2048000 Hz), `WORD_LEN` (16 bits).
- **TLV320DAC3100**: Codec-specific constants from `mtb_tlv320dac3100.h` (MCLK range, sample rate dividers).

## External Dependencies & Integration

### ModusToolbox Libraries (auto-fetched from mtb_shared/)
- **core-lib**: Core Cypress PDL (Peripheral Driver Library) - hardware register abstraction
- **mtb-dsl-pse8xxgp**: Device Support Library (DSL) - PSoC Edge E84 HAL and pin definitions
- **mtb-ipc**: IPC library (for future CM33-CM55 communication; currently unused)
- **mtb-srf**: Secure Runtime Framework (handles TrustZone security)
- **audio-codec-tlv320dac3100**: TLV320DAC3100 driver library (I2C codec control)
- **retarget-io**: Debug UART retargeting (printf → UART)

### Device Model (mtb-dsl-pse8xxgp)
- Provides `cybsp_init()` (BSP init), `CYBSP_*` macros (GPIO/IRQ definitions), and MCUBoot-compatible boot layout
- Pin/clock configuration pre-set in design files; changes require re-generation via ModusToolbox GUI

## Common Modification Patterns

### Adding a New Peripheral Module
1. Create `source/app_new_module/` directory with `app_new_module.c` and `app_new_module.h`
2. In header: extern function declarations (`app_new_module_init()`, etc.) and public data
3. Include in [proj_cm33_ns/main.c](proj_cm33_ns/main.c) and call `app_new_module_init()` in main
4. Rebuild: `cd proj_cm33_ns && make build_proj`

### Adjusting Audio Parameters (Sampling Rate, Buffer Size)
- **Sampling rate**: Edit `SAMPLE_RATE_HZ` in [app_i2s/app_i2s.h](proj_cm33_ns/source/app_i2s/app_i2s.h) and codec config in `app_i2s_init()`
- **Buffer size**: Edit `BUFFER_SIZE` in [app_pdm_pcm/app_pdm_pcm.h](proj_cm33_ns/source/app_pdm_pcm/app_pdm_pcm.h). Ensure SRAM capacity (check `*.map` file in build output)
- **MCLK frequency**: Codec-dependent. Verify TLV320DAC3100 datasheet compliance before changing.

### Debugging Output
- Enable debug UART via `retarget_io_init()` (already configured in CM33_ns main)
- Print via standard `printf()` → redirected to UART at 115200 baud
- Open serial terminal (Tera Term recommended) to COM port provided by KitProg3 USB bridge

## Testing & Validation

### Manual Testing Workflow
1. **Build & Program**: Run "Build & Program:proj_cm33_ns" task
2. **Connect hardware**: USB KitProg3 cable, select UART COM port in terminal emulator
3. **Record audio**: Press User Button 1, speak into microphone (up to 4 seconds), release button
4. **Verify playback**: Audio should play back through speaker/headphone jack
5. **Check console output**: UART terminal shows "PSOC Edge MCU: PDM to I2S Code Example" on startup

### Debugging
- **GDB debugging**: ModusToolbox provides OpenOCD config (`openocd.tcl`). Use VS Code debug launcher or `make debug` equivalent
- **ISR issues**: Add debug breakpoints in PDM/I2S ISRs. Check FIFO overflow flags in hardware registers
- **Buffer overflow**: Verify `BUFFER_SIZE` calculation against recording duration. Monitor `audio_data_ptr` index in debugger

## File Reference Guide

| Path | Purpose |
|------|---------|
| [Makefile](Makefile) | Top-level APPLICATION makefile (orchestrates 3 projects) |
| [common.mk](common.mk) | Shared settings (toolchain, target, config) |
| [common_app.mk](common_app.mk) | Application-level paths and variables |
| [proj_cm33_ns/main.c](proj_cm33_ns/main.c) | CM33 non-secure main loop, button ISR, module initialization |
| [proj_cm33_ns/source/app_pdm_pcm/](proj_cm33_ns/source/app_pdm_pcm/) | PDM/PCM recording module |
| [proj_cm33_ns/source/app_i2s/](proj_cm33_ns/source/app_i2s/) | I2S playback and codec config module |
| [docs/design_and_implementation.md](docs/design_and_implementation.md) | Detailed architecture and flow diagrams |
| [README.md](README.md) | User-facing operation guide and hardware setup |
| [openocd.tcl](openocd.tcl) | OpenOCD JTAG configuration for debugging |
| [configs/boot_with_extended_boot.json](configs/boot_with_extended_boot.json) | Boot image signing/merge config |

