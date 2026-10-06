import importlib.util
from pathlib import Path
import unittest
import io
import sys
import types
from unittest.mock import MagicMock, patch

spec = importlib.util.spec_from_file_location('wave_bridge', Path(__file__).resolve().parents[1] / 'bridge/wave_bridge.py')
bridge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bridge)


class ParserTests(unittest.TestCase):
    def test_current_wave_format_reaches_music_output(self):
        data = bridge.parse_wave_line('0.4231 1 0.9231')
        self.assertEqual(data, {'wave': 0.4231, 'domain': 1, 'kernel': 0.9231})
        self.assertEqual(bridge.to_music_vaked(data)['sample'], 0.4231)

    def test_current_raw_format_preserves_metadata(self):
        self.assertEqual(bridge.parse_raw_line('0.750 0.320 2 1 0.9231'),
            {'rssi': .75, 'density': .32, 'frame_type': 2, 'domain': 1, 'kernel': .9231})

    def test_legacy_formats(self):
        self.assertEqual(bridge.parse_wave_line('0.5'), {'wave': .5})
        self.assertEqual(bridge.parse_raw_line('0.75 0.32 2'),
            {'rssi': .75, 'density': .32, 'frame_type': 2})

    def test_invalid_field_counts_and_nonfinite_values(self):
        for text in ['1 0', '1 0 1 extra', 'nan', 'inf 1 0.5', '0.5 1 nan']:
            with self.subTest(text=text):self.assertIsNone(bridge.parse_wave_line(text))
        for text in ['1 2', '1 2 0 1', '1 2 0 1 0.5 extra', 'nan 1 0', '1 2 0 1 inf']:
            with self.subTest(text=text):self.assertIsNone(bridge.parse_raw_line(text))


class ReconnectTests(unittest.TestCase):
    def test_disconnect_discards_partial_record_and_resumes(self):
        class SerialException(Exception):
            pass
        first, second = MagicMock(), MagicMock()
        first.read.side_effect = [b'~ {"freq":', SerialException('disconnected')]
        second.read.side_effect = [b'~ {"wave":0.5}\n', KeyboardInterrupt()]
        fake_serial = types.ModuleType('serial')
        fake_serial.SerialException = SerialException
        output = io.StringIO()
        with patch.dict(sys.modules, {'serial': fake_serial}), \
             patch.object(bridge, 'open_serial', side_effect=[first, second]) as opened, \
             patch.object(bridge.time, 'sleep'), \
             patch.object(sys, 'argv', ['wave_bridge.py', 'synthetic', '--dump']), \
             patch.object(sys, 'stdout', output):
            bridge.main()
        self.assertEqual(opened.call_count, 2)
        first.close.assert_called_once()
        second.close.assert_called_once()
        second.write.assert_called_once_with(b'mode json\n')
        self.assertIn('"sample": 0.5', output.getvalue())
        self.assertIn('1 waves streamed', output.getvalue())


if __name__ == '__main__':unittest.main()
