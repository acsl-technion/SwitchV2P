import bfrt_grpc.client as gc


class CheckMisdelivered():
    def __init__(self, bfrt_info):
        self.table_target = gc.Target(device_id=0, pipe_id=0xffff)
        self.table = bfrt_info.table_get("pipe.SwitchV2PIngress.cache.check_misdelivered")

        self.table.info.key_field_annotation_add("hdr.ipv4_outter.src_addr", "ipv4")
        self.table.entry_del(self.table_target)

    def add_entry(self, ipv4_addr):
        self.table.entry_add(
            self.table_target,
            [self.table.make_key([gc.KeyTuple('hdr.ipv4_outter.src_addr', ipv4_addr)])],
            [self.table.make_data([gc.DataTuple('misdelivered', True)], 'SwitchV2PIngress.cache.set_misdelivered')])
