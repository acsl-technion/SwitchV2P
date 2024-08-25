parser SwitchV2PIngressParser(
    packet_in pkt,
    out header_t hdr,
    out ig_metadata_t ig_md,
    out ingress_intrinsic_metadata_t ig_intr_md) {

    state start {
        ig_md = ingress_metadata_initializer;
        pkt.extract(ig_intr_md);
        transition parse_port_metadata;
    }

    state parse_port_metadata {
        pkt.advance(PORT_METADATA_SIZE);
        transition parse_ethernet;
    }

    state parse_ethernet {
        pkt.extract(hdr.ethernet);
        transition select(hdr.ethernet.ether_type) {
            ETHERTYPE_IPV4 : parse_ipv4;
            default : reject;
        }
    }

    state parse_ipv4 {
        pkt.extract(hdr.ipv4_outter);
        pkt.extract(hdr.ipv4_inner);
        transition select(hdr.ipv4_inner.protocol) {
            IP_PROTOCOLS_SWITCHV2P : parse_switchv2p;
            default : reject;
        }
    }

    state parse_switchv2p {
        type_t switchv2p_type = pkt.lookahead<type_t>();
        transition select(switchv2p_type) {
            INVALIDATION_TYPE: parse_tagged;
            LEARNING_TYPE: parse_tagged;
            EVICTION_TAG_TYPE: parse_tagged;
            default: parse_normal;
        }
    }

    state parse_tagged { 
        pkt.extract(hdr.switchv2p);
        ig_md.switchv2p_md.setValid();
        ig_md.switchv2p_md.key = hdr.switchv2p.key;
        ig_md.switchv2p_md.val = hdr.switchv2p.val;
        transition accept;
    }

    state parse_normal {
        pkt.extract(hdr.switchv2p);
        ig_md.switchv2p_md.setValid();
        ig_md.switchv2p_md.key = hdr.ipv4_inner.dst_addr;
        ig_md.switchv2p_md.val = hdr.ipv4_outter.dst_addr;
        transition accept;
    }
}


control SwitchV2PIngressDeparser(
        packet_out pkt,
        inout header_t hdr,
        in ig_metadata_t ig_md,
        in ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md) {

    Checksum() ipv4_checksum;
    Mirror() ing_port_mirror;

    apply {
        if (hdr.ipv4_outter.isValid()) {
            hdr.ipv4_outter.hdr_checksum = ipv4_checksum.update({hdr.ipv4_outter.version,
                                                                 hdr.ipv4_outter.ihl,
                                                                 hdr.ipv4_outter.diffserv,
                                                                 hdr.ipv4_outter.total_len,
                                                                 hdr.ipv4_outter.identification,
                                                                 hdr.ipv4_outter.flags,
                                                                 hdr.ipv4_outter.frag_offset,
                                                                 hdr.ipv4_outter.ttl,
                                                                 hdr.ipv4_outter.protocol,
                                                                 hdr.ipv4_outter.src_addr,
                                                                 hdr.ipv4_outter.dst_addr
                                                                });
        }

        if (ig_intr_dprsr_md.mirror_type == MIRROR_TYPE_LEARNING) {
            ing_port_mirror.emit<mirror_h>(ig_md.switchv2p_md.mirror_session, {ig_md.switchv2p_md.mirror_header_type, hdr.ipv4_outter.dst_addr});
        } else if (ig_intr_dprsr_md.mirror_type == MIRROR_TYPE_INVALIDATION) {
            ing_port_mirror.emit<mirror_h>(ig_md.switchv2p_md.mirror_session, {ig_md.switchv2p_md.mirror_header_type, hdr.ipv4_outter.dst_addr});
        }
        pkt.emit(hdr);
    }
}


parser SwitchV2PEgressParser(
        packet_in pkt,
        out header_t hdr,
        out eg_metadata_t eg_md,
        out egress_intrinsic_metadata_t eg_intr_md) {
    
    state start {
        pkt.extract(eg_intr_md);
        pkt.extract(hdr.mirror);
        pkt.extract(hdr.ethernet);
        pkt.extract(hdr.ipv4_outter);
        pkt.extract(hdr.ipv4_inner);
        pkt.extract(hdr.switchv2p);

        transition accept;
    }
}

control SwitchV2PEgressDeparser(
        packet_out pkt,
        inout header_t hdr,
        in eg_metadata_t eg_md,
        in egress_intrinsic_metadata_for_deparser_t eg_intr_md_for_dprsr) {

    Checksum() ipv4_checksum;

    apply {
        if (hdr.ipv4_outter.isValid()) {
            hdr.ipv4_outter.hdr_checksum = ipv4_checksum.update({hdr.ipv4_outter.version,
                                                                 hdr.ipv4_outter.ihl,
                                                                 hdr.ipv4_outter.diffserv,
                                                                 hdr.ipv4_outter.total_len,
                                                                 hdr.ipv4_outter.identification,
                                                                 hdr.ipv4_outter.flags,
                                                                 hdr.ipv4_outter.frag_offset,
                                                                 hdr.ipv4_outter.ttl,
                                                                 hdr.ipv4_outter.protocol,
                                                                 hdr.ipv4_outter.src_addr,
                                                                 hdr.ipv4_outter.dst_addr
                                                                });
        }
        if (hdr.ipv4_inner.isValid()) {
            hdr.ipv4_inner.hdr_checksum = ipv4_checksum.update({hdr.ipv4_inner.version,
                                                                 hdr.ipv4_inner.ihl,
                                                                 hdr.ipv4_inner.diffserv,
                                                                 hdr.ipv4_inner.total_len,
                                                                 hdr.ipv4_inner.identification,
                                                                 hdr.ipv4_inner.flags,
                                                                 hdr.ipv4_inner.frag_offset,
                                                                 hdr.ipv4_inner.ttl,
                                                                 hdr.ipv4_inner.protocol,
                                                                 hdr.ipv4_inner.src_addr,
                                                                 hdr.ipv4_inner.dst_addr
                                                                });
        }
        pkt.emit(hdr);
    } 
}
