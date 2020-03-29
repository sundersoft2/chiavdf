from distutils.command.install import install
from distutils.command.build import build

from setuptools import Command


BUILD_HOOKS = []
INSTALL_HOOKS = []


def add_install_hook(hook):
    INSTALL_HOOKS.append(hook)


def add_build_hook(hook):
    BUILD_HOOKS.append(hook)


class HookCommand(Command):
    def __init__(self, dist):
        self.dist = dist
        Command.__init__(self, dist)

    def initialize_options(self, *args):
        self.install_dir = None
        self.build_dir = None

    def finalize_options(self):
        self.set_undefined_options('build', ('build_scripts', 'build_dir'))
        self.set_undefined_options('install',
                                   ('install_platlib', 'install_dir'),
                                   )

    def run(self):
        for _ in self.hooks:
            _(install_dir=self.install_dir, build_dir=self.build_dir)


class build_hook(HookCommand):
    hooks = BUILD_HOOKS


class install_hook(HookCommand):
    hooks = INSTALL_HOOKS


build.sub_commands.append(("build_hook", lambda x: True))
install.sub_commands.append(("install_hook", lambda x: True))


############################################


import os
import shutil
import pathlib
import subprocess

from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext


class CMakeExtension(Extension):
    def __init__(self, name, sourcedir=''):
        Extension.__init__(self, name, sources=['./'])
        self.sourcedir = os.path.abspath(sourcedir)


def copy_vdf_client(build_dir, install_dir):
    install_dir = pathlib.Path(install_dir)
    install_dir.mkdir(parents=True, exist_ok=True)
    print(f"copy vdf_client to {install_dir}")
    shutil.copy("vdf_client", install_dir)


def invoke_make(build_dir, install_dir):
    subprocess.check_call('make -f Makefile.vdf-client', shell=True)


def invoke_cmake(build_dir, install_dir):
    subprocess.check_call('cmake .', shell=True)
    subprocess.check_call('cmake --build .', shell=True)


def copy_lib(build_dir, install_dir):
    for _ in os.listdir("."):
        if _.startswith("chiavdf."):
            p = pathlib.Path(_)
            if p.is_file():
                print(f"copy {p} to {install_dir}")
                shutil.copy(p, install_dir)


if os.getenv("BUILD_VDF_CLIENT", "Y") == "Y":
    add_install_hook(copy_vdf_client)
    add_build_hook(invoke_make)

add_build_hook(invoke_cmake)
add_install_hook(copy_lib)


class NoopBuild(build_ext):
    def run(self):
        pass

setup(
    name='chiavdf',
    author='Florin Chirica',
    author_email='florin@chia.net',
    description='Chia vdf verification (wraps C++)',
    license='Apache License',
    python_requires='>=3.5',
    long_description=open('README.md').read(),
    ext_modules=[Extension('chiavdf', [])],
    cmdclass=dict(build_ext=NoopBuild, install_hook=install_hook, build_hook=build_hook),
    zip_safe=False,
)
