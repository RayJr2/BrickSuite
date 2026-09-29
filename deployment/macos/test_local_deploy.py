"""Local adapter gates; bundle/signature/archive gates live in test_release_tools."""
import argparse
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from local_deploy import deploy, publish, source_tree, validate_source_versions


class LocalDeploy(unittest.TestCase):
    def test_debug_and_non_native_rejected_before_packaging(self):
        args = argparse.Namespace(config='Debug')
        with self.assertRaisesRegex(RuntimeError, 'Release'):
            deploy(args)
        args.config = 'Release'
        with patch('local_deploy.platform.system', return_value='Darwin'), \
             patch('local_deploy.platform.machine', return_value='x86_64'):
            with self.assertRaisesRegex(RuntimeError, 'native ARM64'):
                deploy(args)

    def test_publish_preserves_unmanaged_and_restores_on_failure(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            destination = root/'deploy'
            destination.mkdir()
            (destination/'existing.zip').write_text('previous')
            with self.assertRaisesRegex(RuntimeError, 'unmanaged'):
                publish(root/'absent', destination)
            (destination/'local-deploy.json').write_text(json.dumps({'owner': 'bricksuite-macos-deploy'}))
            with self.assertRaises(FileNotFoundError):
                publish(root/'absent', destination)
            self.assertEqual((destination/'existing.zip').read_text(), 'previous')
            staged = root/'new'
            staged.mkdir()
            (staged/'new.zip').write_text('validated')
            publish(staged, destination)
            self.assertEqual((destination/'new.zip').read_text(), 'validated')
            self.assertFalse(list(root.glob('.deploy-previous-*')))

    def test_missing_or_mismatched_notice_sources_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            with self.assertRaisesRegex(RuntimeError, 'Set QT_SOURCE'):
                source_tree('', [root], ['missing'], 'QT_SOURCE')
            for module in ('qtbase', 'qtsvg', 'qtimageformats', 'qtwebsockets'):
                (root/module).mkdir()
                (root/module/'.cmake.conf').write_text('set(QT_REPO_MODULE_VERSION "6.10.3")')
            (root/'VERSION.dat').write_text('MAJOR=3\nMINOR=6\nPATCH=4\n')
            validate_source_versions(root, root, '6.10.3', '3.6.4')
            with self.assertRaisesRegex(RuntimeError, 'OpenSSL'):
                validate_source_versions(root, root, '6.10.3', '3.6.3')
            (root/'qtsvg/.cmake.conf').write_text('set(QT_REPO_MODULE_VERSION "6.9.0")')
            with self.assertRaisesRegex(RuntimeError, 'qtsvg'):
                validate_source_versions(root, root, '6.10.3', '3.6.4')


if __name__ == '__main__':
    unittest.main()
