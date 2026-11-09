import os
import time

import pytest

from tdx_ths_linker import Client


@pytest.mark.skipif(os.environ.get("ONCLAW_LIVE_TEST") != "1", reason="live desktop test is opt-in")
def test_live_dual_end_read_write():
    target = os.environ.get("ONCLAW_LIVE_CODE", "600825")
    with Client(license_accepted=True) as client:
        state = None
        for _ in range(30):
            state = client.get_state()
            if state.tdx.available and state.ths.available:
                break
            time.sleep(0.35)
        assert state is not None and state.tdx.available and state.ths.available
        assert client.set_code("tdx", target).accepted
        assert client.set_code("ths", target).accepted
        for _ in range(30):
            state = client.get_state()
            if state.tdx.code == target and state.ths.code == target:
                break
            time.sleep(0.35)
        assert state.tdx.code == target
        assert state.ths.code == target
