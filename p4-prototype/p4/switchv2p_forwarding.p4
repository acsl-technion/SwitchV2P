#ifndef __FORWARDING__
#define __FORWARDING__

control SwitchV2PForwarding(inout header_t hdr, 
                            inout ig_metadata_t ig_md, 
                            in ingress_intrinsic_metadata_t ig_intr_md, 
                            inout ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md,
                            inout ingress_intrinsic_metadata_for_tm_t ig_intr_tm_md)
{
    action l3_forward(PortId_t port) {
        ig_intr_tm_md.ucast_egress_port = port;
    }

    action drop(){
		ig_intr_dprsr_md.drop_ctl = 0x1;
	}


    table l3_forwarding {
        key = {
            hdr.ipv4_outter.dst_addr : exact;
        }
        actions = {
            @defaultonly drop;
            l3_forward;
        }
        const default_action = drop();
    }


    apply {
        l3_forwarding.apply();
    }
}

#endif
