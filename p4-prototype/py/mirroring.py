import bfrt_grpc.client as gc


class Mirroring():
    def __init__(self, bfrt_info):
        self.table = bfrt_info.table_get("$mirror.cfg")
        self.table_target = gc.Target(device_id=0, pipe_id=0xffff)
        self.table.entry_del(self.table_target)

    def add_entry(self, session_id, max_pkt_len, egress_port):
        self.table.entry_add(
            self.table_target,
            [self.table.make_key([gc.KeyTuple('$sid', session_id)])],
            [self.table.make_data([gc.DataTuple('$direction', str_val='BOTH'),
                                   gc.DataTuple('$session_enable', bool_val=True),
                                   gc.DataTuple('$ucast_egress_port_valid', bool_val=True),
                                   gc.DataTuple('$ucast_egress_port', egress_port),
                                   gc.DataTuple('$max_pkt_len', max_pkt_len)], '$normal')]
        )
