#!/usr/bin/env python3
"""Package the checked-out Mavernix revision with its actual Meson version."""
import argparse
from pathlib import Path
import re
import subprocess


def prepare(repo, topdir, tag=''):
    repo = repo.resolve()
    project = (repo / 'meson.build').read_text()
    match = re.search(r"\bversion\s*:\s*'([^']+)'", project)
    if not match or not re.fullmatch(r'\d+\.\d+\.\d+', match[1]):
        raise ValueError('Expected a stable x.y.z version in meson.build')
    version = match[1]
    if tag and tag != f'v{version}':
        raise ValueError(f'Tag {tag!r} does not match Meson version v{version}')
    for directory in ('BUILD', 'BUILDROOT', 'RPMS', 'SOURCES', 'SPECS', 'SRPMS'):
        (topdir / directory).mkdir(parents=True, exist_ok=True)
    spec = (repo / 'packaging/fedora/mavernix.spec').read_text()
    spec, count = re.subn(r'^Version:.*$', f'Version:        {version}', spec, flags=re.M)
    if count != 1:
        raise ValueError('Expected exactly one Version field in mavernix.spec')
    spec, count = re.subn(r'^Source0:.*$', 'Source0:        mavernix-%{version}.tar.gz', spec, flags=re.M)
    if count != 1:
        raise ValueError('Expected exactly one Source0 field in mavernix.spec')
    (topdir / 'SPECS/mavernix.spec').write_text(spec)
    archive = (topdir / f'SOURCES/mavernix-{version}.tar.gz').resolve()
    subprocess.run([
        'git', '-C', str(repo), 'archive', '--format=tar.gz',
        f'--prefix=mavernix-{version}/', f'--output={archive}', 'HEAD',
    ], check=True)
    print(f'Prepared Mavernix {version} from the checked-out commit')
    return version


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path.cwd())
    parser.add_argument('--topdir', type=Path, required=True)
    parser.add_argument('--tag', default='')
    args = parser.parse_args()
    try:
        prepare(args.repo, args.topdir, args.tag)
    except (ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'{error}\n')
