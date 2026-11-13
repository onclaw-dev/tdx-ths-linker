class TdxThsLinkerError(Exception):
    pass


class UnsupportedPlatformError(TdxThsLinkerError):
    pass


class AgentLaunchError(TdxThsLinkerError):
    pass


class AgentConnectionError(TdxThsLinkerError):
    pass


class ClientClosedError(TdxThsLinkerError):
    pass
