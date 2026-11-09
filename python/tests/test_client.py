import pytest

from tdx_ths_linker import Client, ClientClosedError
from tdx_ths_linker.contracts import CommandResult, EndpointState, InitResult, RuntimeState


def endpoint(name: str) -> EndpointState:
    return EndpointState(name, "600825", "ready", "", 1, 1, 1, True, False, True, 1)


class FakeBinding:
    def __init__(self):
        self.created = 0
        self.destroyed = []

    def create(self):
        self.created += 1
        return 42

    def destroy(self, handle):
        self.destroyed.append(handle)

    def initialize(self, handle, accepted):
        return InitResult(accepted, "accepted" if accepted else "unaccepted", "ready", "ok")

    def get_state(self, handle):
        return RuntimeState(True, True, "ready", "accepted", "ok", endpoint("ths"), endpoint("tdx"))

    def set_code(self, handle, target, code):
        return CommandResult(True, True, "accepted", target, code, "command queued")


def test_context_manager_closes_exactly_once():
    binding = FakeBinding()
    with Client(True, _binding=binding) as client:
        assert client.get_state().tdx.code == "600825"
    client.close()
    assert binding.destroyed == [42]


def test_closed_client_rejects_operations():
    client = Client(_binding=FakeBinding())
    client.close()
    with pytest.raises(ClientClosedError):
        client.get_state()


@pytest.mark.parametrize("target,code", [("bad", "600825"), ("tdx", "123"), ("ths", "１２３４５６")])
def test_command_validation(target, code):
    with Client(_binding=FakeBinding()) as client:
        with pytest.raises(ValueError):
            client.set_code(target, code)
