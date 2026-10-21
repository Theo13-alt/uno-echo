"""Render the shared SVG master into PNG and multi-resolution Windows ICO."""
from pathlib import Path
import struct
from PySide6.QtCore import QByteArray, QBuffer, QIODevice, Qt
from PySide6.QtGui import QGuiApplication, QImage, QPainter
from PySide6.QtSvg import QSvgRenderer

app = QGuiApplication([])
assets = Path(__file__).resolve().parents[1] / 'data' / 'echo'
renderer = QSvgRenderer(str(assets / 'echo.svg'))
frames = []
for size in (16, 24, 32, 48, 64, 128, 256):
    canvas = QImage(size, size, QImage.Format_ARGB32)
    canvas.fill(Qt.transparent)
    painter = QPainter(canvas)
    renderer.render(painter)
    painter.end()
    data = QByteArray()
    buffer = QBuffer(data)
    buffer.open(QIODevice.WriteOnly)
    canvas.save(buffer, 'PNG')
    frames.append((size, bytes(data)))
    if size == 256:
        canvas.save(str(assets / 'echo.png'))
offset = 6 + 16 * len(frames)
entries = []
for size, data in frames:
    entries.append(struct.pack('<BBBBHHII', size % 256, size % 256, 0, 0, 1, 32, len(data), offset))
    offset += len(data)
(assets / 'echo.ico').write_bytes(struct.pack('<HHH', 0, 1, len(frames)) + b''.join(entries) + b''.join(data for _, data in frames))
