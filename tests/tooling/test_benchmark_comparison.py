#!/usr/bin/env python3
"""Verify baseline comparison rejects incompatible measurements and catches drift."""
import sys
sys.dont_write_bytecode = True
import importlib.util
from pathlib import Path
import unittest
import tempfile

spec = importlib.util.spec_from_file_location('benchmark', Path(__file__).parents[2] / 'tools/benchmark/run_benchmark.py')
benchmark = importlib.util.module_from_spec(spec)
spec.loader.exec_module(benchmark)

class ComparisonTests(unittest.TestCase):
    def report(self, seconds):
        return {'schema':'kyna.benchmark/v1','platform':'test','architecture':'arm64',
                'build_configuration':'Release','mode':'run',
                'workloads':[{'name':'calls','source_sha256':'abc','kyna':{'median_seconds':seconds}}]}
    def test_regression(self):
        result = benchmark.compare_baseline(self.report(1.25), self.report(1), 20)
        self.assertTrue(result[0]['regressed'])
        self.assertEqual(result[0]['change_percent'], 25)
    def test_improvement(self):
        self.assertFalse(benchmark.compare_baseline(self.report(.8), self.report(1), 20)[0]['regressed'])
    def test_platform_mismatch(self):
        baseline = self.report(1); baseline['architecture'] = 'x86_64'
        with self.assertRaises(ValueError): benchmark.compare_baseline(self.report(1), baseline, 20)
    def test_changed_input(self):
        baseline = self.report(1); baseline['workloads'][0]['source_sha256'] = 'changed'
        with self.assertRaises(ValueError): benchmark.compare_baseline(self.report(1), baseline, 20)
    def test_invalid_timing(self):
        for seconds in (0, -1, float("nan"), float("inf")):
            with self.assertRaises(ValueError): benchmark.compare_baseline(self.report(seconds), self.report(1), 20)
            with self.assertRaises(ValueError): benchmark.compare_baseline(self.report(1), self.report(seconds), 20)
    def test_changed_dependency(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'entry.kyna'
            dependency = Path(directory) / 'types.ky'
            source.write_text('import type { T } from "./types.ky";')
            dependency.write_text('export type T = int;')
            before = benchmark.workload_fingerprint(source, None)
            dependency.write_text('export type T = str;')
            self.assertNotEqual(before, benchmark.workload_fingerprint(source, None))
    def test_changed_output(self):
        baseline = self.report(1); baseline['workloads'][0]['output_sha256'] = 'changed'
        with self.assertRaises(ValueError): benchmark.compare_baseline(self.report(1), baseline, 20)
    def test_missing_workload(self):
        baseline = self.report(1); baseline['workloads'] = []
        with self.assertRaises(ValueError): benchmark.compare_baseline(self.report(1), baseline, 20)

if __name__ == '__main__': unittest.main()
