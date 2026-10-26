# Script de PlatformIO (pre-build): comprime web/index.html con gzip y lo
# genera como src/web_page.h para servirlo comprimido desde la flash.
import gzip
import os

Import("env")  # noqa: F821

root = env["PROJECT_DIR"]  # noqa: F821
src = os.path.join(root, "web", "index.html")
dst = os.path.join(root, "src", "web_page.h")

with open(src, "rb") as f:
    data = gzip.compress(f.read(), compresslevel=9, mtime=0)

lines = ",".join(str(b) for b in data)
content = (
    "// Generado por tools/embed_web.py a partir de web/index.html. No editar.\n"
    "#pragma once\n#include <Arduino.h>\n\n"
    f"static const size_t INDEX_HTML_GZ_LEN = {len(data)};\n"
    f"static const uint8_t INDEX_HTML_GZ[] PROGMEM = {{{lines}}};\n"
)

old = open(dst).read() if os.path.exists(dst) else None
if content != old:
    with open(dst, "w") as f:
        f.write(content)
    print(f"web/index.html -> src/web_page.h ({len(data)} bytes gzip)")
