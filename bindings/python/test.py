"""
Example usage of the kerong Python bindings.

This is a demonstration only. By default it points at a ZNE-100TL base unit
on 192.168.1.200:5000; point it at your hardware (or a simulator) to run.
"""

import argparse
import sys
import time

import kerong


def main() -> int:
    parser = argparse.ArgumentParser(description="Kerong NCU16L demo")
    parser.add_argument("--host", default="192.168.1.200", help="ZNE-100TL IP")
    parser.add_argument("--port", type=int, default=5000, help="TCP port")
    parser.add_argument("--timeout", type=int, default=1000, help="Socket timeout (ms)")
    parser.add_argument("--board", type=int, default=0, help="Board address (0-15)")
    parser.add_argument("--lock", type=int, default=0, help="Lock index (0-15)")
    args = parser.parse_args()

    client = kerong.KerongClient()

    try:
        client.connect(args.host, args.port, args.timeout)
        print(f"Connected to {args.host}:{args.port}")
    except kerong.KerongTimeoutError as e:
        print(f"connect timed out: {e}", file=sys.stderr)
        return 1
    except kerong.KerongError as e:
        print(f"connect failed: {e}", file=sys.stderr)
        return 1

    try:
        print(f"unlock(board={args.board}, lock={args.lock}) -> sending")
        client.unlock(args.board, args.lock)
        time.sleep(0.1)

        state = client.get_state(args.board)
        print(f"board       = {state.board_address}")
        print(f"locks_1_8   = 0x{state.locks_1_8:02x}")
        print(f"locks_9_16  = 0x{state.locks_9_16:02x}")
        print(f"ir_1_8      = 0x{state.ir_1_8:02x}")
        print(f"ir_9_16     = 0x{state.ir_9_16:02x}")
        for i in range(16):
            lock_no = i + 1
            print(f"  lock {lock_no:>2}: "
                  f"{'locked' if state.is_locked(i) else 'unlocked'}, "
                  f"item={'yes' if state.is_item_detected(i) else 'no'}")
    except kerong.KerongTimeoutError as e:
        print(f"operation timed out: {e}", file=sys.stderr)
        return 2
    except kerong.KerongChecksumError as e:
        print(f"checksum error: {e}", file=sys.stderr)
        return 3
    except kerong.KerongInvalidFrameError as e:
        print(f"invalid frame: {e}", file=sys.stderr)
        return 4
    except kerong.KerongConnectionError as e:
        print(f"connection error: {e}", file=sys.stderr)
        return 5
    except kerong.KerongError as e:
        print(f"kerong error: {e}", file=sys.stderr)
        return 6
    finally:
        client.disconnect()
        print("disconnected")

    return 0


if __name__ == "__main__":
    sys.exit(main())
