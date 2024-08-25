import bfrt_grpc.bfruntime_pb2 as bfruntime_pb2
import bfrt_grpc.client as gc
import grpc
from enum import IntEnum

class SwitchType(IntEnum):
	TOR = 0
	GW_TOR = 1
	SPINE = 2
	GW_SPINE = 3
	CORE = 4

class SwitchConfig():

    def __init__(self, bfrt_info,
                 switch_id,
                 switch_type):
        self.table_target = gc.Target(device_id=0, pipe_id=0xffff)
        self.table = bfrt_info.table_get("pipe.SwitchV2PIngress.switch_config")
        self.action = "SwitchV2PIngress.set_config"
        self.switch_id = switch_id
        self.switch_type = switch_type
    
    def add_default_entries(self):
        self.table.default_entry_set(self.table_target, 
                                     self.table.make_data([gc.DataTuple('switch_type', self.switch_type), 
                                                           gc.DataTuple('switch_id', self.switch_id)], self.action))
