import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('plt_guides', ROOT / 'tools/plt_guides.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class GuideTests(unittest.TestCase):
    def setUp(self):
        self.original = (ROOT / 'tests/fixtures/corteLa.plt').read_bytes()
        self.expected = (ROOT / 'tests/fixtures/corteLaGuias.plt').read_bytes()

    def test_client_example_matches_manual_conversion(self):
        self.assertEqual(module.convert(self.original).split(), self.expected.split())

    def test_existing_guides_unchanged(self):
        self.assertEqual(module.convert(self.expected), self.expected)

    def test_unrecognized_format_unchanged(self):
        self.assertEqual(module.convert(b'IN;PU0,0;PD100,100;'), b'IN;PU0,0;PD100,100;')

    def test_no_enclosing_rectangle_unchanged(self):
        altered = self.original.replace(b'D4813,40800', b'D2400,20400')
        self.assertEqual(module.convert(altered), altered)

    def test_ambiguous_duplicate_border_unchanged(self):
        start = self.original.index(b'U0,20400')
        end = self.original.index(b'U402,38399')
        altered = self.original[:end] + self.original[start:end] + self.original[end:]
        self.assertEqual(module.convert(altered), altered)

    def test_conversion_is_idempotent(self):
        once = module.convert(self.original)
        self.assertEqual(module.convert(once), once)


if __name__ == '__main__':
    unittest.main()
