import json
from dataclasses import replace

import pytest

from tdx_ths_linker import CommandResult, EndpointState, InitResult, RuntimeState
from tdx_ths_linker.cli import (
    EXIT_OK,
    EXIT_REJECTED,
    EXIT_TIMEOUT,
    EXIT_UNAVAILABLE,
    EXIT_USAGE,
    main,
)


def endpoint(name, code="600000", available=True):
    return EndpointState(
        name=name,
        code=code,
        health="ready" if available else "unavailable",
        last_error="",
        updated_at=1,
        last_scan_at=1,
        last_resolved_offset=0,
        available=available,
        using_fallback=False,
        direct_read=True,
        pid=100,
    )


def runtime(tdx=None, ths=None):
    return RuntimeState(
        initialized=True,
        supported=True,
        runtime_health="ready",
        license_state="accepted",
        message="ready",
        tdx=tdx or endpoint("tdx"),
        ths=ths or endpoint("ths"),
    )


class FakeClient:
    def __init__(self, *, states=None, rejected=(), license_accepted=None):
        self.states = list(states or [runtime()])
        self.rejected = set(rejected)
        self.calls = []
        self.license_accepted = license_accepted

    def __enter__(self):
        return self

    def __exit__(self, *_):
        pass

    def initialize(self, accepted):
        self.calls.append(("initialize", accepted))
        return InitResult(True, "accepted", "ready", "ready")

    def get_state(self):
        self.calls.append(("state",))
        if len(self.states) > 1:
            return self.states.pop(0)
        return self.states[0]

    def set_code(self, target, code):
        self.calls.append(("set", target, code))
        accepted = target not in self.rejected
        return CommandResult(accepted, accepted, "queued" if accepted else "rejected", target, code, "")


def factory(client):
    return lambda **_: client


def output(capsys):
    return json.loads(capsys.readouterr().out)


def test_state_emits_one_json_envelope(capsys):
    client = FakeClient()
    code = main(["--json", "--accept-license", "state"], client_factory=factory(client))
    value = output(capsys)
    assert code == EXIT_OK
    assert value["schemaVersion"] == 1
    assert value["ok"] is True
    assert value["command"] == "state"
    assert value["data"]["tdx"]["code"] == "600000"


def test_state_waits_for_initial_scan(capsys):
    initializing = replace(runtime(), runtime_health="initializing")
    client = FakeClient(states=[initializing, runtime()])
    ticks = iter([0.0, 0.0])
    code = main(
        ["--json", "--accept-license", "state"],
        client_factory=factory(client),
        sleep=lambda _: None,
        monotonic=lambda: next(ticks, 0.1),
    )
    value = output(capsys)
    assert code == EXIT_OK
    assert value["data"]["runtime_health"] == "ready"
    assert client.calls.count(("state",)) == 2


def test_license_is_required_before_client_creation(capsys, monkeypatch):
    monkeypatch.delenv("TDX_THS_LINKER_LICENSE_ACCEPTED", raising=False)
    created = False

    def create(**_):
        nonlocal created
        created = True
        return FakeClient()

    code = main(["--json", "state"], client_factory=create)
    value = output(capsys)
    assert code == EXIT_USAGE
    assert created is False
    assert value["error"]["code"] == "license_not_accepted"


def test_invalid_code_is_rejected_before_client_creation(capsys):
    created = False

    def create(**_):
        nonlocal created
        created = True
        return FakeClient()

    code = main(
        ["--json", "--accept-license", "set", "abc", "--target", "both"],
        client_factory=create,
    )
    value = output(capsys)
    assert code == EXIT_USAGE
    assert created is False
    assert value["error"]["code"] == "invalid_usage"


def test_dual_set_waits_until_both_targets_match(capsys):
    initial = runtime()
    partial = runtime(tdx=replace(endpoint("tdx"), code="600825"))
    confirmed = runtime(
        tdx=replace(endpoint("tdx"), code="600825"),
        ths=replace(endpoint("ths"), code="600825"),
    )
    client = FakeClient(states=[initial, partial, confirmed])
    ticks = iter([0.0, 0.0, 0.1, 0.2, 0.3])
    code = main(
        ["--json", "--accept-license", "set", "600825", "--target", "both", "--wait"],
        client_factory=factory(client),
        sleep=lambda _: None,
        monotonic=lambda: next(ticks, 0.3),
    )
    value = output(capsys)
    assert code == EXIT_OK
    assert value["data"]["confirmed"] is True
    assert ("set", "tdx", "600825") in client.calls
    assert ("set", "ths", "600825") in client.calls


def test_unavailable_target_has_distinct_exit(capsys):
    client = FakeClient(states=[runtime(ths=endpoint("ths", available=False))])
    code = main(
        ["--json", "--accept-license", "set", "600825", "--target", "both"],
        client_factory=factory(client),
    )
    value = output(capsys)
    assert code == EXIT_UNAVAILABLE
    assert value["error"]["code"] == "target_unavailable"
    assert value["error"]["details"]["targets"] == ["ths"]


def test_rejected_command_has_distinct_exit(capsys):
    client = FakeClient(rejected={"ths"})
    code = main(
        ["--json", "--accept-license", "set", "600825", "--target", "both"],
        client_factory=factory(client),
    )
    value = output(capsys)
    assert code == EXIT_REJECTED
    assert value["error"]["details"]["targets"] == ["ths"]


def test_confirmation_timeout_includes_last_state(capsys):
    client = FakeClient(states=[runtime()])
    ticks = iter([0.0, 0.0, 0.2])
    code = main(
        [
            "--json",
            "--accept-license",
            "set",
            "600825",
            "--target",
            "tdx",
            "--wait",
            "--timeout",
            "0.1",
        ],
        client_factory=factory(client),
        sleep=lambda _: None,
        monotonic=lambda: next(ticks, 0.2),
    )
    value = output(capsys)
    assert code == EXIT_TIMEOUT
    assert value["error"]["code"] == "confirmation_timeout"
    assert value["error"]["details"]["state"]["tdx"]["code"] == "600000"
