import ctypes

from tdx_ths_linker._native import (
    _CommandResultV1,
    _EndpointStateV1,
    _InitResultV1,
    _RuntimeStateV1,
)


def test_all_abi_structures_start_with_size():
    for structure in (_EndpointStateV1, _RuntimeStateV1, _InitResultV1, _CommandResultV1):
        assert structure._fields_[0] == ("struct_size", ctypes.c_uint32)
        assert ctypes.sizeof(structure) > ctypes.sizeof(ctypes.c_uint32)
