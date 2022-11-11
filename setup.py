from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
import subprocess
import os
import sys
import multiprocessing

LLVM3_NAME = "clang+llvm-3.1-x86_64-linux-ubuntu_12.04"
LLVM3_URL = f"https://releases.llvm.org/3.1/{LLVM3_NAME}.tar.gz"
LLVM10_NAME = "clang+llvm-10.0.0-x86_64-linux-gnu-ubuntu-18.04"
LLVM10_URL = f"https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.0/{LLVM10_NAME}.tar.xz"


class CMakeExtension(Extension):
    def __init__(self, name, sourcedir=''):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)


class CMakeBuild(build_ext):
    def run(self):
        try:
            out = subprocess.check_output(['cmake', '--version'])
        except OSError:
            raise RuntimeError(
                "CMake must be installed to build the following extensions: " +
                ", ".join(e.name for e in self.extensions))

        for ext in self.extensions:
            self.build_extension(ext)

    def build_extension(self, ext):
        extdir = os.path.abspath(
            os.path.dirname(self.get_ext_fullpath(ext.name)))
        cmake_args = ['-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=' + extdir,
                      '-DPYTHON_EXECUTABLE=' + sys.executable]

        # llvm directory
        root_dir = os.path.dirname(__file__)
        llvm3_dir = os.path.join(root_dir, "llvm3")
        if not os.path.exists(llvm3_dir):
            subprocess.call(["wget", "-q", LLVM3_URL], cwd=root_dir)
            subprocess.call(["tar", "xzf", f"{LLVM3_NAME}.tar.gz"], cwd=root_dir)
            os.remove(f"{LLVM3_NAME}.tar.gz")
            subprocess.call(["mv", LLVM3_NAME, "llvm3"], cwd=root_dir)
        llvm10_dir = os.path.join(root_dir, "llvm10")
        if not os.path.exists(llvm10_dir):
            subprocess.call(["wget", "-q",LLVM10_URL], cwd=root_dir)
            subprocess.call(["tar", "xf", f"{LLVM10_NAME}.tar.xz"], cwd=root_dir)
            os.remove(f"{LLVM10_NAME}.tar.xz")
            subprocess.call(["mv", LLVM10_NAME, "llvm10"], cwd=root_dir)

        # test env variable to determine whether to build in debug
        if os.environ.get("DEBUG") is not None:
            cfg = 'Debug'
        else:
            cfg = 'Release'
        build_args = ['--config', cfg]
        env = os.environ.copy()

        cmake_args += ['-DCMAKE_BUILD_TYPE=' + cfg]

        cpu_count = max(2, multiprocessing.cpu_count() // 2)
        build_args += ['--', '-j{0}'.format(cpu_count)]

        python_path = sys.executable
        cmake_args += ['-DPYTHON_EXECUTABLE:FILEPATH=' + python_path]

        env['CXXFLAGS'] = '{} -DVERSION_INFO=\\"{}\\"'.format(
            env.get('CXXFLAGS', ''),
            self.distribution.get_version())
        if not os.path.exists(self.build_temp):
            os.makedirs(self.build_temp)
        subprocess.check_call(['cmake', ext.sourcedir] + cmake_args,
                              cwd=self.build_temp, env=env)

        subprocess.check_call(
            ['cmake', '--build', '.', "--target", "vitis", "vitis0", "vitis_rtl"] + build_args,
            cwd=self.build_temp)


current_directory = os.path.abspath(os.path.dirname(__file__))
with open(os.path.join(current_directory, 'README.rst')) as f:
    long_description = f.read()

setup(
    name='hgdb-vitis',
    version='0.0.1',
    author='Keyi Zhang',
    author_email='keyi@cs.stanford.edu',
    long_description=long_description,
    long_description_content_type='text/x-rst',
    url="https://github.com/Kuree/hgdb-vitis",
    scripts=["hgdb-vitis"],
    python_requires=">=3.6",
    cmdclass={"build_ext": CMakeBuild},
    ext_modules=[CMakeExtension('vitis')],
)
