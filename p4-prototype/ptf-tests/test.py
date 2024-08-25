import ptf.testutils as testutils
from p4testutils.misc_utils import *
from bfruntime_client_base_tests import BfRuntimeTest
import bfrt_grpc.client as gc
from scapy.all import *
from switch_config import SwitchConfig, SwitchType
from match_gw import MatchGw
from l3_forwarding import L3Forwarding
from check_misdelivered import CheckMisdelivered
from mirroring import Mirroring
import ipaddress
import zlib

p4_program_name = "switchv2p"
client_id = 0
logger = get_logger()
swports = get_sw_ports()
cache_size = 8192
gw_address = '132.68.0.3'

class SwitchV2P(Packet):
    name = "SwitchV2P"
    fields_desc = [ByteField("type", 0), ByteField("switch_id", 0), IPField("key", 0), IPField("val", 0)]

def switchv2p_packet(physical_src='132.68.0.1', physical_dst='132.68.0.2', virtual_src='10.0.0.1', 
                         virtual_dst='10.0.0.2', type=0, switch_id=0, key='0.0.0.0', val='0.0.0.0', payload=0):
    switchv2p_packet = Ether() / IP(src=physical_src, dst=physical_dst) / IP(src=virtual_src, dst=virtual_dst ,proto=146) / \
        SwitchV2P(type=type, switch_id=switch_id, key=key, val=val) / Raw("x"*payload)
    del switchv2p_packet.chksum
    switchv2p_packet = switchv2p_packet.__class__(bytes(switchv2p_packet))
    return switchv2p_packet

def setup_env(bfrt_info, switch_id, switch_type):
    bind_layers(IP, SwitchV2P, proto=146)

    mirroring = Mirroring(bfrt_info)
    mirroring.add_entry(1, 69, swports[2])

    l3_forwarding = L3Forwarding(bfrt_info)
    l3_forwarding.add_entry('132.68.0.2', swports[0])
    l3_forwarding.add_entry(gw_address, swports[1])

    config = SwitchConfig(bfrt_info, switch_id, switch_type)
    config.add_default_entries()

    match_gw = MatchGw(bfrt_info, [gw_address])
    match_gw.add_entries()

    check_misdelivered = CheckMisdelivered(bfrt_info)
    check_misdelivered.add_entry('133.68.0.2')

    clear_registers(bfrt_info)
    set_retransmission_timeout(bfrt_info, 8)

def clear_registers(bfrt_info):
    target = gc.Target(device_id=0, pipe_id=0xffff)
    for reg_name in ['keys', 'vals', 'bf']:
        table_name = f'SwitchV2PIngress.cache.{reg_name}'
        table = bfrt_info.table_get(table_name)
        table.entry_del(target)


def verify_register(test, bfrt_info, reg_name, field, idx, exp):
    table_name = f'SwitchV2PIngress.cache.{reg_name}'
    table = bfrt_info.table_get(table_name)
    values = table.entry_get(gc.Target(device_id=0, pipe_id=0xffff), 
                             [table.make_key([gc.KeyTuple('$REGISTER_INDEX', idx)])], {"from_hw": True})
    data, _ = next(values)
    test.assertTrue(data.to_dict()[f'{table_name}.{field}'][0] == exp)

def get_cache_idx(key):
    return zlib.crc32(ipaddress.ip_address(key).packed) % cache_size

def verify_key_val(test, bfrt_info, key, val):
    idx = get_cache_idx(key)
    verify_register(test, bfrt_info, 'keys', 'key', idx, int(ipaddress.ip_address(key)))
    verify_register(test, bfrt_info, 'vals', 'f1', idx, int(ipaddress.ip_address(val)))

def verify_invalid(test, bfrt_info, key):
    idx = get_cache_idx(key)
    verify_register(test, bfrt_info, 'keys', 'key', idx, 1)

def verify_access_bit(test, bfrt_info, key, exp):
    verify_register(test, bfrt_info, 'keys', 'access_bit', get_cache_idx(key), exp)

def set_key_val(bfrt_info, key, val, access_bit=0):
    idx = get_cache_idx(key)
    table_name = 'SwitchV2PIngress.cache.keys'
    table = bfrt_info.table_get(table_name)
    target = gc.Target(device_id=0, pipe_id=0xffff)
    table.entry_add(target, [table.make_key([gc.KeyTuple('$REGISTER_INDEX', idx)])], 
                    [table.make_data([gc.DataTuple(f'{table_name}.key', int(ipaddress.ip_address(key))), 
                                      gc.DataTuple(f'{table_name}.access_bit', access_bit)])])
    table_name = 'SwitchV2PIngress.cache.vals'
    table = bfrt_info.table_get(table_name)
    table.entry_add(target, [table.make_key([gc.KeyTuple('$REGISTER_INDEX', idx)])], 
                    [table.make_data([gc.DataTuple(f'{table_name}.f1', int(ipaddress.ip_address(val)))])])

def set_retransmission_timeout(bfrt_info, timeout):
    timeout_param = bfrt_info.table_get("pipe.SwitchV2PIngress.cache.retransmit_timeout")
    target = gc.Target(device_id=0, pipe_id=0xffff)
    timeout_param.default_entry_set(target, timeout_param.make_data([gc.DataTuple("value", timeout)]))



class TestSourceLearning(BfRuntimeTest):        
    """@brief This test verifies that TORs learn source addresses.
    """

    def setUp(self):
        BfRuntimeTest.setUp(self, client_id, p4_program_name)

    def runTest(self):
        # Get bfrt_info and set it as part of the test
        bfrt_info = self.interface.bfrt_info_get(self.p4_name)
        setup_env(bfrt_info, 0, SwitchType.TOR)
        testutils.send_packet(self, swports[2], switchv2p_packet())
        testutils.verify_packet(self, switchv2p_packet(), swports[0])
        verify_key_val(self, bfrt_info, '10.0.0.1', '132.68.0.1')
        testutils.verify_no_other_packets(self)

class TestLookup(BfRuntimeTest):        
    """@brief This test verifies basic lookup.
    """

    def setUp(self):
        BfRuntimeTest.setUp(self, client_id, p4_program_name)

    def runTest(self):
        # Get bfrt_info and set it as part of the test
        bfrt_info = self.interface.bfrt_info_get(self.p4_name)
        setup_env(bfrt_info, 0, SwitchType.TOR)
        set_key_val(bfrt_info, '10.0.0.2', '132.68.0.2')
        testutils.send_packet(self, swports[2], switchv2p_packet(physical_dst=gw_address))
        testutils.verify_packet(self, switchv2p_packet(physical_dst='132.68.0.2'), swports[0])
        testutils.verify_no_other_packets(self)
        verify_access_bit(self, bfrt_info, '10.0.0.2', 1)

class TestAccessBit(BfRuntimeTest):        
    """@brief This test verifies basic lookup.
    """

    def setUp(self):
        BfRuntimeTest.setUp(self, client_id, p4_program_name)

    def runTest(self):
        # Get bfrt_info and set it as part of the test
        bfrt_info = self.interface.bfrt_info_get(self.p4_name)
        setup_env(bfrt_info, 0, SwitchType.TOR)
        set_key_val(bfrt_info, '10.0.0.2', '132.68.0.2')
        testutils.send_packet(self, swports[2], switchv2p_packet(physical_dst=gw_address))
        testutils.verify_packet(self, switchv2p_packet(physical_dst='132.68.0.2'), swports[0])
        testutils.verify_no_other_packets(self)
        verify_access_bit(self, bfrt_info, '10.0.0.2', 1)
        testutils.send_packet(self, swports[2], switchv2p_packet(virtual_dst='10.0.22.103', 
                                                                     physical_dst=gw_address))
        testutils.verify_packet(self, switchv2p_packet(virtual_dst='10.0.22.103', physical_dst=gw_address), 
                                swports[1])
        verify_access_bit(self, bfrt_info, '10.0.0.2', 0)


class TestMisdelivery(BfRuntimeTest): 
    """@brief This test verifies that TORs generate invalidation packets.
    """

    def setUp(self):
        BfRuntimeTest.setUp(self, client_id, p4_program_name)

    def runTest(self):
        # Get bfrt_info and set it as part of the test
        bfrt_info = self.interface.bfrt_info_get(self.p4_name)
        setup_env(bfrt_info, 0, SwitchType.TOR)
        testutils.send_packet(self, swports[2], switchv2p_packet(physical_src='133.68.0.2', 
                                                                     physical_dst=gw_address, switch_id=3, payload=30))
        testutils.verify_packet(self, switchv2p_packet(physical_src='133.68.0.2', physical_dst=gw_address, type=1, 
                                                           switch_id=3, key='10.0.0.2', payload=30), swports[1])
        testutils.verify_packet(self, switchv2p_packet(physical_src='133.68.0.2', physical_dst='0.0.0.0', type=3, 
                                                           switch_id=3, key='10.0.0.2'), swports[2])
        testutils.verify_no_other_packets(self)

class TestMisdeliveryTimeout(BfRuntimeTest): 
    """@brief This test verifies that the retranmission mechanism invalidation packets works correctly.
    """

    def setUp(self):
        BfRuntimeTest.setUp(self, client_id, p4_program_name)

    def runTest(self):
        # Get bfrt_info and set it as part of the test
        bfrt_info = self.interface.bfrt_info_get(self.p4_name)
        setup_env(bfrt_info, 0, SwitchType.TOR)
        set_retransmission_timeout(bfrt_info, 10000)
        def send_and_verify(with_generated, switch_id=3):
            testutils.send_packet(self, swports[2], switchv2p_packet(physical_src='133.68.0.2', 
                                                                     physical_dst=gw_address, switch_id=switch_id, payload=30))
            testutils.verify_packet(self, switchv2p_packet(physical_src='133.68.0.2', physical_dst=gw_address, type=1, 
                                                           switch_id=switch_id, key='10.0.0.2', payload=30), swports[1])
            if with_generated:
                testutils.verify_packet(self, switchv2p_packet(physical_src='133.68.0.2', physical_dst='0.0.0.0', type=3, 
                                                           switch_id=switch_id, key='10.0.0.2'), swports[2])
        
        
        send_and_verify(True)
        send_and_verify(False)
        testutils.verify_no_other_packets(self)
        time.sleep(2)
        send_and_verify(True)
        send_and_verify(True, 4)

class TestInvalidationTag(BfRuntimeTest): 
    """@brief This test verifies that switches correctly invalidate their cache when processing packets with invalidation tags.
    """

    def setUp(self):
        BfRuntimeTest.setUp(self, client_id, p4_program_name)

    def runTest(self):
        # Get bfrt_info and set it as part of the test
        bfrt_info = self.interface.bfrt_info_get(self.p4_name)
        setup_env(bfrt_info, 0, SwitchType.SPINE)
        set_key_val(bfrt_info, '10.0.0.2', '200.68.0.2')
        testutils.send_packet(self, swports[2], switchv2p_packet(physical_dst=gw_address, type=1))
        testutils.verify_packet(self, switchv2p_packet(physical_dst=gw_address, type=1), swports[1])
        verify_invalid(self, bfrt_info, '10.0.0.2')
        testutils.verify_no_other_packets(self)

class TestInvalidation(BfRuntimeTest): 
    """@brief This test verifies that switches correctly invalidate their cache when processing invalidation packets.
    """

    def setUp(self):
        BfRuntimeTest.setUp(self, client_id, p4_program_name)

    def runTest(self):
        # Get bfrt_info and set it as part of the test
        bfrt_info = self.interface.bfrt_info_get(self.p4_name)
        setup_env(bfrt_info, 0, SwitchType.CORE)
        set_key_val(bfrt_info, '10.0.0.2', '200.68.0.2')
        testutils.send_packet(self, swports[2], switchv2p_packet(physical_dst='15.0.0.1', type=3, key='10.0.0.2'))
        testutils.verify_no_other_packets(self)
        verify_invalid(self, bfrt_info, '10.0.0.2')

class TestEviction(BfRuntimeTest): 
    """@brief This test verifies that switches correctly spillover evicted entries.
    """

    def setUp(self):
        BfRuntimeTest.setUp(self, client_id, p4_program_name)

    def runTest(self):
        # Get bfrt_info and set it as part of the test
        bfrt_info = self.interface.bfrt_info_get(self.p4_name)
        setup_env(bfrt_info, 0, SwitchType.GW_TOR)
        set_key_val(bfrt_info, '10.0.22.103', '200.68.0.2')
        testutils.send_packet(self, swports[2], switchv2p_packet())
        testutils.verify_packet(self, switchv2p_packet(type=2, key='10.0.22.103', val='200.68.0.2'), swports[0])
        testutils.verify_no_other_packets(self)
        verify_key_val(self, bfrt_info, '10.0.0.2', '132.68.0.2')

class TestEvictionTag(BfRuntimeTest): 
    """@brief This test verifies that switches correctly learn evicted entries.
    """

    def setUp(self):
        BfRuntimeTest.setUp(self, client_id, p4_program_name)

    def runTest(self):
        # Get bfrt_info and set it as part of the test
        bfrt_info = self.interface.bfrt_info_get(self.p4_name)
        setup_env(bfrt_info, 0, SwitchType.GW_TOR)
        testutils.send_packet(self, swports[2], switchv2p_packet(type=2, key='10.0.0.1', val='200.68.0.1'))
        testutils.verify_packet(self, switchv2p_packet(key='0.0.0.1'), swports[0])
        testutils.verify_no_other_packets(self)
        verify_key_val(self, bfrt_info, '10.0.0.1', '200.68.0.1')
        verify_invalid(self, bfrt_info, '10.0.0.2')

class TestLearningPacket(BfRuntimeTest): 
    """@brief This test verifies that TORs correctly learn from learning packets.
    """

    def setUp(self):
        BfRuntimeTest.setUp(self, client_id, p4_program_name)

    def runTest(self):
        # Get bfrt_info and set it as part of the test
        bfrt_info = self.interface.bfrt_info_get(self.p4_name)
        setup_env(bfrt_info, 0, SwitchType.TOR)
        testutils.send_packet(self, swports[2], switchv2p_packet(physical_dst='15.0.0.1', type=4, 
                                                                     key='150.0.0.1', val='200.68.0.1'))
        testutils.verify_no_other_packets(self)
        verify_key_val(self, bfrt_info, '150.0.0.1', '200.68.0.1')
        verify_invalid(self, bfrt_info, '10.0.0.1')

class TestSpineActiveEntry(BfRuntimeTest): 
    """@brief This test verifies that Spines do not overwrite entries with set access bits.
    """

    def setUp(self):
        BfRuntimeTest.setUp(self, client_id, p4_program_name)

    def runTest(self):
        # Get bfrt_info and set it as part of the test
        bfrt_info = self.interface.bfrt_info_get(self.p4_name)
        setup_env(bfrt_info, 0, SwitchType.SPINE)
        set_key_val(bfrt_info, '10.0.22.103', '200.68.0.2', 1)
        testutils.send_packet(self, swports[2], switchv2p_packet())
        testutils.verify_packet(self, switchv2p_packet(), swports[0])
        testutils.verify_no_other_packets(self)
        verify_key_val(self, bfrt_info, '10.0.22.103', '200.68.0.2')

class TestSpineNonActiveEntry(BfRuntimeTest): 
    """@brief This test verifies that Spines overwrite entries with unset access bits.
    """

    def setUp(self):
        BfRuntimeTest.setUp(self, client_id, p4_program_name)

    def runTest(self):
        # Get bfrt_info and set it as part of the test
        bfrt_info = self.interface.bfrt_info_get(self.p4_name)
        setup_env(bfrt_info, 0, SwitchType.SPINE)
        set_key_val(bfrt_info, '10.0.22.103', '200.68.0.2')
        testutils.send_packet(self, swports[2], switchv2p_packet())
        testutils.verify_packet(self, switchv2p_packet(type=2, key='10.0.22.103', val='200.68.0.2'), swports[0])
        testutils.verify_no_other_packets(self)
        verify_key_val(self, bfrt_info, '10.0.0.2', '132.68.0.2')

class TestSpineLearning(BfRuntimeTest): 
    """@brief This test verifies that Spines correctly performs destination learning.
    """

    def setUp(self):
        BfRuntimeTest.setUp(self, client_id, p4_program_name)

    def runTest(self):
        # Get bfrt_info and set it as part of the test
        bfrt_info = self.interface.bfrt_info_get(self.p4_name)
        setup_env(bfrt_info, 0, SwitchType.SPINE)
        testutils.send_packet(self, swports[2], switchv2p_packet())
        testutils.verify_packet(self, switchv2p_packet(key='0.0.0.1'), swports[0])
        testutils.verify_no_other_packets(self)
        verify_key_val(self, bfrt_info, '10.0.0.2', '132.68.0.2')
