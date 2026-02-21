#!/usr/bin/env python3
import importlib
import sys


def main():
    try:
        m = importlib.import_module('aetherion')
        print('imported', getattr(m, '__name__', repr(m)))
    except Exception as e:
        print('Import failed:', e)
        sys.exit(2)


if __name__ == '__main__':
    main()
