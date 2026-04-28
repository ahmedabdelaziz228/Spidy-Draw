import tempfile, os, cv2, numpy as np
from web_handlers import WebHandlers


def make_test_image():
    img = np.ones((240,240,3), dtype=np.uint8) * 255
    cv2.rectangle(img, (40,40), (200,180), (0,0,0), 2)
    cv2.circle(img, (120,120), 30, (0,0,0), 2)
    f = tempfile.NamedTemporaryFile(delete=False, suffix='.png')
    cv2.imwrite(f.name, img)
    f.close()
    return f.name


def run_smoke_test():
    h = WebHandlers()
    path = make_test_image()
    try:
        with open(path, 'rb') as fp:
            result = h.handle_process_image(fp.read(), 'test.png', {'invert': True, 'auto_threshold': True})
        assert result['success'], result
        preview = h.handle_preview()
        assert preview['success'], preview
        g = h.handle_generate_gcode({'safe_margin': 5.0, 'optimize': True, 'max_commands': 8000})
        assert g['success'], g
        assert g['data']['command_count'] > 5
        print('OK: final integration smoke test passed')
    finally:
        os.unlink(path)


if __name__ == '__main__':
    run_smoke_test()
