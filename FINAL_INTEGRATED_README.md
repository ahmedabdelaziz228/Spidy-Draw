# Final integrated result

This package merges:
- Muhammad's image processing and path cleanup
- Fathy's G-code generation and upload flow
- The rewritten motion-control firmware skeleton

## What is included
- `image_processor.py`: final vectorization pipeline
- `path_optimizer.py`: cleanup + ordering
- `web_handlers.py`: process / validate / preview / generate G-code
- `bridge_server.py`: lightweight local API + upload bridge to ESP32
- `web/index.html`: final UI for image -> paths -> G-code -> upload
- `src/web_server.cpp`: firmware updated with `/upload-text` endpoint

## Typical flow
1. Run `python bridge_server.py` on your laptop
2. Open `http://127.0.0.1:8080`
3. Process image and preview paths
4. Generate G-code
5. Send to ESP32 using its URL, for example `http://192.168.4.1`

## Important note
The ESP32 firmware is still the motion side only. Image processing runs on the laptop/browser side, which is the correct design for this project.
