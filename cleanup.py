#!/usr/bin/env python3
import sys
import json
import argparse
import pathlib
import io
import re
import urllib.parse as urllib_parse
from typing import Dict, Any

class FileMap:
    def __init__(self, filepath):
        self._filepath = filepath
        with open(self._filepath, 'r') as file:
            self._content = file.read()
            self._file_map = list()
            for chr in self._content:
                if not self._file_map:
                    self._file_map.append(0)
                self._file_map[-1] += 1
                if chr == '\n':
                    self._file_map.append(0)
        
    @property
    def content(self) -> str:
        return self._content

    def get_offset_by_loc(self, line: int, column: int) -> int:
        if line <= 0 or line - 1 >= len(self._file_map):
            raise ValueError(f'Invalid line number {line} in file {self._filepath}')
        if column <= 0 or column - 1 >= self._file_map[line - 1]:
            raise ValueError(f'Invalid column number {column} in file {self._filepath} at line {line}')
        
        return column - 1 + sum(self._file_map[:line - 1])

class ErroneousSegmentRemoval:
    def __init__(self, *, assert_fn: str):
        self._assert_fn = assert_fn
        self._file_maps = dict()
        self._skipped_segments = dict()

    def load_sarif(self, sarif: Dict[Any, Any]):
        for run in sarif.get('runs', ()):
            for result in run.get('results', ()):
                if result['level'] == 'error':
                    for location in result.get('locations', ()):
                        if uri := location.get('physicalLocation', dict()).get('artifactLocation', dict()).get('uri'):
                            if region := location.get('physicalLocation', dict()).get('region'):
                                uri = urllib_parse.urlparse(uri)
                                if uri.scheme == 'file':
                                    filepath = pathlib.Path(uri.path).resolve()
                                    start_line = region['startLine']
                                    start_column = region['startColumn']
                                    file_map = self._get_file_map(filepath)
                                    offset = file_map.get_offset_by_loc(start_line, start_column)
                                    self._record_segment(filepath, offset)

    def process_file(self, filepath, out: io.TextIOBase):
        filepath = pathlib.Path(filepath).resolve()
        file_map = self._get_file_map(filepath)
        index = 0
        skip_segments = self._skipped_segments.get(filepath, dict())
        while index < len(file_map.content):
            if index in skip_segments:
                index = skip_segments[index] + 1
                continue
            out.write(file_map.content[index])
            index += 1

    def _get_file_map(self, filepath: pathlib.Path) -> FileMap:
        if file_map := self._file_maps.get(filepath):
            return file_map
        file_map = FileMap(filepath)
        self._file_maps[filepath] = file_map
        return file_map
    
    def _record_segment(self, filepath: pathlib.Path, offset: int):
        file_map = self._get_file_map(filepath)
        if file_map.content[offset:offset + len(self._assert_fn)] == self._assert_fn:
            paren_depth = 0
            quote = None
            end_offset = offset
            while end_offset < len(file_map.content):
                chr = file_map.content[end_offset]
                if quote is None:
                    if chr == '(':
                        paren_depth += 1
                    elif chr == ')':
                        paren_depth -= 1
                        if paren_depth < 0:
                            return
                        elif paren_depth == 0:
                            end_offset += 1
                            break
                    elif chr == '\'' or chr == '\"':
                        quote = chr
                else:
                    if chr == '\\':
                        end_offset += 1
                    elif chr == quote:
                        quote = None
                end_offset += 1
            if paren_depth == 0:
                if filepath not in self._skipped_segments:
                    self._skipped_segments[filepath] = dict()
                self._skipped_segments[filepath][offset] = max(self._skipped_segments[filepath].get(offset, -1), end_offset)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(prog=sys.argv[0], description='Assertion removal script')
    parser.add_argument('--sarif-json', type=str, required=True, help='Path to SARIF JSON file with compilation failures')
    parser.add_argument('--assert-fn', type=str, required=False, default='assert', help='Name of assert function')
    parser.add_argument('prog_filepath', type=str, help='Program source file path')
    
    args = parser.parse_args(sys.argv[1:])
    with open(args.sarif_json) as sarif_json_file:
        sarif_json = json.load(sarif_json_file)

    range_set = ErroneousSegmentRemoval(assert_fn=args.assert_fn)
    range_set.load_sarif(sarif_json)
    range_set.process_file(args.prog_filepath, sys.stdout)
