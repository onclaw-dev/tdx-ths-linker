from pathlib import Path

from setuptools import Distribution, setup
from setuptools.command.build_py import build_py
from setuptools.command.bdist_wheel import bdist_wheel


class WindowsDistribution(Distribution):
    def has_ext_modules(self):
        return True


class WindowsWheel(bdist_wheel):
    def get_tag(self):
        return ("py3", "none", "win_amd64")


class CleanBuildPy(build_py):
    def run(self):
        stale_installer = (
            Path(self.build_lib)
            / "tdx_ths_linker"
            / "agent"
            / "win-x64"
            / "onclaw-tdx-ths-setup-win-x64.exe"
        )
        stale_installer.unlink(missing_ok=True)
        super().run()


setup(
    distclass=WindowsDistribution,
    cmdclass={"bdist_wheel": WindowsWheel, "build_py": CleanBuildPy},
    zip_safe=False,
)
