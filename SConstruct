# Based on scons file from cryptacular at https://bitbucket.org/dholth/cryptacular/
# Build with `scons` or `pip wheel .` (or any other pep517-compliant builder)

import os
import sys

import enscons
import enscons.cpyext
import pybind11
import pytoml as toml
import setuptools_scm


def get_metadata():
    metadata = dict(toml.load(open("pyproject.toml")))["tool"]["enscons"]

    # use setuptools_scm to get the version number from git tags
    version = setuptools_scm.get_version(local_scheme="no-local-version")

    metadata["version"] = version

    # add an extra piece of metadata so we can examine wheels to confirm
    # what exact git hash they were built on

    metadata["scm_version"] = setuptools_scm.get_version()
    return metadata


# This seems to be responsible for changing
# the second cp37 to abi3 in the name
# "chiabip158-0.13-cp37-cp37-macosx_10_15_x86_64.whl"
# Seems like "abi3" means a subset of the python interface
# that is guaranteed to work back to Python 3.2.
# See also https://docs.python.org/3/c-api/stable.html

# we have a choice of get_abi3_tag, get_binary_tag, get_universal_tag here
# get_abi3_tag doesn't work probably because pybind11 uses some API that's
# not supported in abi3

full_tag = enscons.get_binary_tag()

MSVC_VERSION = None
SHLIBSUFFIX = None
TARGET_ARCH = None  # only set for win32

# we need to set stuff up differently for Windows because it's funky
if sys.platform == "win32":
    import distutils.msvccompiler

    MSVC_VERSION = str(distutils.msvccompiler.get_build_version())  # it is a float
    SHLIBSUFFIX = ".pyd"
    TARGET_ARCH = "x86_64" if sys.maxsize.bit_length() == 63 else "x86"

# get path to "pybind11.h"
PYBIND11_PATH = pybind11.get_include()

# Split splits the string into paths
OTHER_HEADERS = Split("src src/include")

EXTRA_CPPPATH = [PYBIND11_PATH] + OTHER_HEADERS


env = Environment(
    tools=["default", "packaging", enscons.generate, enscons.cpyext.generate],
    PACKAGE_METADATA=get_metadata(),
    WHEEL_TAG=full_tag,
    MSVC_VERSION=MSVC_VERSION,
    TARGET_ARCH=TARGET_ARCH,
)
env["CPPPATH"].extend(EXTRA_CPPPATH)


# seems to be related to abi3 (see c-api comment above)

use_py_limited = "abi3" in full_tag

ext_filename = enscons.cpyext.extension_filename("chiavdf", abi3=use_py_limited)


SOURCE_DIST_FILES = [Glob("src/*", "src/python_bindings/*")]

# workaround of an enscons bug
# in some cases, such as "chiabip158-0.13.dev3" if the base name
# of the source distribution already looked like it had a suffix,
# the TarBuilder would not add the ".tar.gz" suffix. This
# hack fixes it. It needs to go after the enscons.generate is added
# to tools, but before the SDist is created

enscons.pytar.TarBuilder.ensure_suffix = True

# Add automatic source files, plus any other needed files.
sdist_source = SOURCE_DIST_FILES + [
    "PKG-INFO",
    "setup.py",
    "pyproject.toml",
    "SConstruct",
    "README.md",
]

# build source distribution with "scons dist"
sdist = env.SDist(source=sdist_source)
env.Alias("sdist", sdist)


EXT_SOURCE = [
    "src/python_bindings/fastvdf.cpp"
]  # _ for _ in Flatten(SOURCE_DIST_FILES) if str(_).endswith(".cpp")]

# for abi3 we need -DPy_LIMITED_API=0x03030000 to modify the Python.h headers

PARSE_FLAGS = "-DPy_LIMITED_API=0x03030000" if use_py_limited else ""

CPPFLAGS = ["-std=c++1z", "-O3"]

if sys.platform == "darwin":
    CPPFLAGS.append("-mmacosx-version-min=10.14")
    CPPFLAGS.append("-D CHIAOSX=1")

if sys.platform == "win32":
    CPPFLAGS.append("/EHsc")

LIBPATH = ["/usr/local/lib"]

extension = env.SharedLibrary(
    # we need to pass the PATH environment variable through from the shell so we
    # can find the g++ compiler on the docker image provided by cibuildwheel
    ENV={"PATH": os.environ["PATH"]},
    target=ext_filename,
    source=EXT_SOURCE,
    LIBPREFIX="",
    LIBS=["gmp", "gmpxx"],
    LIBPATH=["/usr/local/lib"],
    SHLIBSUFFIX=SHLIBSUFFIX,
    CPPFLAGS=CPPFLAGS,
    parse_flags=PARSE_FLAGS,
    # if you have some flags you know you want passed to the compiler but don't
    # know what they refer to in scons, you can use "parse_flags"
    # and it will reverse-engineer them into the scons env
    # if it doesn't understand how to do so (like "-std=c++11"), it will ignore them
)

compile_asm = env.Program(
    "src/compile_asm.cpp", LIBS=["pthread", "gmp"], CPPFLAGS=CPPFLAGS
)


def generate_actions(source, target, env, for_signature):
    modifer = "avx2" if "avx2" in str(target[0]) else ""
    return "%s %s > %s" % (source[0], modifer, target[0])


bld = env.Builder(generator=generate_actions)
env["BUILDERS"]["AsmCompiled"] = bld


asm_compiled = env.AsmCompiled("asm_compiled.s", compile_asm)
asm_compiled_avx2 = env.AsmCompiled("asm_compiled_avx2.s", compile_asm)

asm_lib = env.Library("asm_compiled_lib", [asm_compiled, asm_compiled_avx2])

VDF_CLIENT_SOURCE = ["src/vdf_client.cpp", asm_lib]
vdf_client = env.Program(
    VDF_CLIENT_SOURCE,
    LIBS=["gmp", "gmpxx", "boost_system", "pthread"],
    LIBPATH=LIBPATH,
    CPPFLAGS=CPPFLAGS,
)
env.Alias("vdf_client", vdf_client)

VDF_BENCH_SOURCE = ["src/vdf_bench.cpp", asm_lib]
vdf_bench = env.Program(
    VDF_BENCH_SOURCE,
    LIBS=["gmp", "gmpxx", "boost_system", "pthread"],
    LIBPATH=LIBPATH,
    CPPFLAGS=CPPFLAGS,
)
env.Alias("vdf_bench", vdf_bench)


# Only *.py is included automatically by setup2toml.
# Add extra 'purelib' files or package_data here.
py_source = []

binaries = []

vdf_client_2 = env.Install(".", vdf_client)
vdf_bench_2 = env.Install(".", vdf_bench)

if os.getenv("BUILD_VDF_CLIENT", "Y") == "Y":
    binaries.append(vdf_client_2)
if os.getenv("BUILD_VDF_BENCH", "N") == "Y":
    binaries.append(vdf_bench_2)


platlib = env.Whl("platlib", py_source + [extension] + binaries, root="")
wheel = env.WhlFile(source=platlib)

# "scons develop" for use with "pip install -e"

develop = env.Command("#DEVELOP", enscons.egg_info_targets(env), enscons.develop)
env.Alias("develop", develop)

# by default, build the wheel and the sdist
env.Default(wheel, sdist)
