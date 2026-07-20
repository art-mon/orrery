"""
Bit-packed QR code payload for the ESP32 firmware.

Uses standard (non-micro) QR — Micro QR isn't reliably scanned by phone
cameras. Output is row-major bits, MSB-first inside each byte, base64
encoded so it fits cleanly in JSON.
"""
import base64
import segno


def _pack_bits(matrix) -> bytes:
    bits = [1 if cell else 0 for row in matrix for cell in row]
    out = bytearray()
    for i in range(0, len(bits), 8):
        byte = 0
        for j in range(8):
            if i + j < len(bits) and bits[i + j]:
                byte |= 1 << (7 - j)
        out.append(byte)
    return bytes(out)


def generate(data: str) -> dict:
    """Build a standard QR (error correction M) and return module bits."""
    q = segno.make(data, error="m", micro=False)
    matrix = q.matrix
    size = len(matrix)
    return {
        "data": data,
        "size": size,
        "version": q.version,
        "modules_b64": base64.b64encode(_pack_bits(matrix)).decode("ascii"),
    }
