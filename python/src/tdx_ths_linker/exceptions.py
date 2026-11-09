class TdxThsLinkerError(RuntimeError):
    """Base error for tdx-ths-linker."""


class UnsupportedPlatformError(TdxThsLinkerError):
    """Raised when the native client is used outside Windows x64."""


class NativeLoadError(TdxThsLinkerError):
    """Raised when the bundled native DLL cannot be loaded."""


class AbiMismatchError(TdxThsLinkerError):
    """Raised when the Python wrapper and native DLL use different ABIs."""


class NativeCallError(TdxThsLinkerError):
    """Raised when a C ABI call fails before producing a domain result."""


class ClientClosedError(TdxThsLinkerError):
    """Raised when an operation is attempted after closing a client."""
