import bfrt_grpc.bfruntime_pb2 as bfruntime_pb2
import bfrt_grpc.client as gc
import grpc
from enum import IntEnum

class MatchGw():

    def __init__(self, bfrt_info,
                 gw_addresses):
        self.table_target = gc.Target(device_id=0, pipe_id=0xffff)
        self.table = bfrt_info.table_get("pipe.SwitchV2PIngress.match_gw")
        self.table.info.key_field_annotation_add("hdr.ipv4_outter.dst_addr", "ipv4")
        self.action = "SwitchV2PIngress.set_to_gw"
        self.gw_addresses = gw_addresses
        self.table.entry_del(self.table_target)
    
    def add_entries(self):
        for gw_address in self.gw_addresses:
            self.table.entry_add(self.table_target,
                                 [self.table.make_key([gc.KeyTuple('hdr.ipv4_outter.dst_addr', gw_address)])],
                                 [self.table.make_data([gc.DataTuple('to_gw', True)], self.action)])
