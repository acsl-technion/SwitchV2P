import bfrt_grpc.client as gc


class L3Forwarding():
    def __init__(self, bfrt_info):
        self.table_target = gc.Target(device_id=0, pipe_id=0xffff)
        self.table = bfrt_info.table_get("pipe.SwitchV2PIngress.fw.l3_forwarding")

        self.table.info.key_field_annotation_add("hdr.ipv4_outter.dst_addr", "ipv4")
        self.table.entry_del(self.table_target)

    def add_entry(self, ipv4_addr, port):
        self.table.entry_add(
            self.table_target,
            [self.table.make_key([gc.KeyTuple('hdr.ipv4_outter.dst_addr', ipv4_addr)])],
            [self.table.make_data([gc.DataTuple('port', port)], 'SwitchV2PIngress.fw.l3_forward')])
