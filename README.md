# kerong

Cross-platform C++17 client and native bindings for the
[Kerong NCU16L](https://www.kerong.com) electronic locker system,
communicating over a ZNE-100TL transparent TCP-to-RS485 base unit.

* Pure C++17 core - no Boost, no asio
* Native bindings for Python (`pybind11`) and Node.js (`node-addon-api`)
* Strict frame and Modulo-256 checksum validation
* Typed error hierarchy (`KerongTimeoutError`, `KerongChecksumError`, ...)
* Thread-safe client, RAII socket cleanup
* Linux, macOS, Windows

## Installation

```bash
# Python
pip install kerong

# Node.js (>= 18)
npm install kerong
```

## Usage

### Python

```python
import kerong

client = kerong.KerongClient()
client.connect("192.168.1.200", 5000, timeout_ms=1000)

client.unlock(board=0, lock=0)

state = client.get_state(board=0)
for i in range(16):
    print(f"lock {i+1:>2}: {'unlocked' if not state.is_locked(i) else 'locked  '}, "
          f"item={'yes' if state.is_item_detected(i) else 'no'}")
```

### Node.js

```javascript
const { KerongClient } = require("kerong");

(async () => {
    const client = new KerongClient();
    await client.connect("192.168.1.200", 5000, 1000);

    await client.unlock(0, 0);

    const state = await client.getState(0);
    for (let i = 0; i < 16; i++) {
        console.log(`lock ${(i + 1).toString().padStart(2, " ")}: ${
            state.isLocked(i) ? "locked  " : "unlocked"}, `
            + `item=${state.isItemDetected(i) ? "yes" : "no"}`);
    }

    client.disconnect();
})();
```

## Protocol

Each frame is delimited by `STX (0x02)` and `ETX (0x03)`, ending with a
Modulo-256 checksum of all preceding bytes.

| Command          | Code  | Direction       | Response         |
| ---------------- | ----- | --------------- | ---------------- |
| Unlock           | `0x31` | Host -> Board  | none             |
| Get State        | `0x30` | Host -> Board  | `0x35` (9 bytes) |

The state response layout is `STX ADDR 0x35 LOCKS_1_8 LOCKS_9_16 IR_1_8 IR_9_16 ETX SUM`,
where `LOCKS_*` bits are `0=locked, 1=unlocked` and `IR_*` bits are `0=empty, 1=item`.
See `include/kerong/kerong_client.hpp` for the full frame layout.

## Building from source

The project is a standard CMake (>= 3.15) project. Both bindings are optional.

```bash
cmake -S . -B build -DKERONG_BUILD_PYTHON=ON -DKERONG_BUILD_NODE=ON
cmake --build build -j

# Python (in a venv)
pip install .

# Node.js
cd node && npm install
```

## License

MIT
