#ifndef __CACHE__
#define __CACHE__

control Cache(inout header_t hdr,
              inout ig_metadata_t ig_md,
              in ingress_intrinsic_metadata_from_parser_t ig_intr_prsr_md,
              inout ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md)
{
    Hash<index_t>(HashAlgorithm_t.CRC32) dst_index_hash;
    Hash<index_t>(HashAlgorithm_t.CRC32) src_index_hash;
    Random<rand_t>() rand;

    // Assumption: 0 and 1 are invalid values.
    Register<key_pair_t, index_t> (CACHE_SIZE, {1, 0}) keys;
    RegisterAction<key_pair_t, index_t, bool>(keys) check_key = {
        void apply(inout key_pair_t val, out bool rv) {
            if (val.key == ig_md.switchv2p_md.key) {
                val.access_bit = 1;
                rv = true;
            } else {
                val.access_bit = 0;
                rv = false;
            }
        }
    };
    RegisterAction<key_pair_t, index_t, ipv4_addr_t>(keys) set_key = {
        void apply(inout key_pair_t val, out ipv4_addr_t rv) {
            rv = val.key;
            val.key = ig_md.switchv2p_md.key;
            val.access_bit = 0;
        }
    };
    RegisterAction<key_pair_t, index_t, ipv4_addr_t>(keys) set_key_if_not_active = {
        void apply(inout key_pair_t val, out ipv4_addr_t rv) {
            if (val.access_bit == 0) {
                rv = val.key;
                val.key = ig_md.switchv2p_md.key;
            } else {
                rv = 0;
            }
        }
    };
    RegisterAction<key_pair_t, index_t, bool>(keys) clear_key = {
        void apply(inout key_pair_t val) {
            if (val.key == ig_md.switchv2p_md.key) {
                val.key = 1;
                val.access_bit = 0;
            }
        }
    };


    Register<ipv4_addr_t, index_t> (CACHE_SIZE) vals;
    RegisterAction<ipv4_addr_t, index_t, ipv4_addr_t>(vals) get_val = {
        void apply(inout ipv4_addr_t val, out ipv4_addr_t rv) {
            rv = val;
        }
    };
    RegisterAction<ipv4_addr_t, index_t, ipv4_addr_t>(vals) set_val = {
        void apply(inout ipv4_addr_t val, out ipv4_addr_t rv) {
            rv = val;
            val = ig_md.switchv2p_md.val;
        }
    };

    action add_hit_tag() {
        hdr.switchv2p.switch_id = ig_md.switchv2p_md.switch_id;
    }

    action add_eviction_tag(ipv4_addr_t val) {
        hdr.switchv2p.type = EVICTION_TAG_TYPE;
        hdr.switchv2p.key = hdr.ipv4_inner.dst_addr;
        hdr.switchv2p.val = val;
    }

    action add_invalidation_tag() {
        hdr.switchv2p.type = INVALIDATION_TAG_TYPE;
        hdr.switchv2p.key = hdr.ipv4_inner.dst_addr;
    }

    action set_misdelivered(bool misdelivered) {
        ig_md.switchv2p_md.misdelivered = misdelivered;
    }

    table check_misdelivered {
        key = {
            hdr.ipv4_outter.src_addr: exact;
        }
        actions = {
            set_misdelivered;
        }
        default_action = set_misdelivered(false);
        const size = 64;
    }

    Register<timestamp_t, switch_id_t>(SWITCH_COUNT) bf;
    RegisterParam<timestamp_t>(8) retransmit_timeout;
    RegisterAction<timestamp_t, switch_id_t, bool>(bf) check_timeout = {
        void apply(inout timestamp_t val, out bool rv) {
            if (ig_intr_prsr_md.global_tstamp[47:16] > val) {
                val = ig_intr_prsr_md.global_tstamp[47:16] + retransmit_timeout.read();
                rv = true;
            } else {
                rv = false;
            }
        }
    };

    action set_mirror_session(MirrorId_t session) {
        ig_md.switchv2p_md.mirror_session = session;
    }

    table learning_mirror_session {
        key = {
            hdr.ipv4_outter.src_addr: exact;
        }
        actions = {
            set_mirror_session;
        }
        const default_action = set_mirror_session(2);
    }
    table invalidation_mirror_session {
        key = {
            hdr.switchv2p.switch_id: exact;
        }
        actions = {
            set_mirror_session;
        }
        const default_action = set_mirror_session(1);
        const size = 256;
    }

    apply {
        index_t idx = dst_index_hash.get({ig_md.switchv2p_md.key});
        if (!ig_md.switchv2p_md.to_gw && ig_md.switchv2p_md.switch_type == switch_type_t.TOR && 
            (hdr.switchv2p.type == NORMAL_TYPE || hdr.switchv2p.type == EVICTION_TAG_TYPE)) {
            idx = src_index_hash.get({hdr.ipv4_inner.src_addr});
            ig_md.switchv2p_md.key = hdr.ipv4_inner.src_addr;
            ig_md.switchv2p_md.val = hdr.ipv4_outter.src_addr;
        }
        
        check_misdelivered.apply();
        if (hdr.switchv2p.type == LEARNING_TYPE) {
            set_key.execute(idx);
            set_val.execute(idx);
        } else if (hdr.switchv2p.type == INVALIDATION_TYPE || hdr.switchv2p.type == INVALIDATION_TAG_TYPE) {
            clear_key.execute(idx);
        } else if (ig_md.switchv2p_md.to_gw) {
            if (ig_md.switchv2p_md.switch_type == switch_type_t.TOR && ig_md.switchv2p_md.misdelivered) {
                bool generate = check_timeout.execute(hdr.switchv2p.switch_id);
                if (generate) {
                    ig_intr_dprsr_md.mirror_type = MIRROR_TYPE_INVALIDATION;
                    ig_md.switchv2p_md.mirror_header_type = INVALIDATION_TYPE;
                    invalidation_mirror_session.apply();
                }
                
                add_invalidation_tag();
            } else {
                bool in_cache = check_key.execute(idx);
                if (in_cache) {
                    if (ig_md.switchv2p_md.switch_type == switch_type_t.SPINE) {
                        ipv4_addr_t val = get_val.execute(idx);
                        add_eviction_tag(val);
                    } else {
                        hdr.ipv4_outter.dst_addr = get_val.execute(idx);
                        add_hit_tag();
                    }
                } 
            }
        } else if (ig_md.switchv2p_md.switch_type == switch_type_t.TOR) {
            set_key.execute(idx);
            set_val.execute(idx);
        } else if ((ig_md.switchv2p_md.switch_type == switch_type_t.CORE && hdr.switchv2p.type == EVICTION_TAG_TYPE) ||
                   (ig_md.switchv2p_md.switch_type == switch_type_t.SPINE || ig_md.switchv2p_md.switch_type == switch_type_t.GW_SPINE)) {
            ipv4_addr_t prev_ipv4 = set_key_if_not_active.execute(idx);
            if (prev_ipv4 != 0) {
                hdr.switchv2p.key = prev_ipv4;
                hdr.switchv2p.val = set_val.execute(idx);
                if (hdr.switchv2p.key == 1) {
                    hdr.switchv2p.type = NORMAL_TYPE;
                } else {
                    hdr.switchv2p.type = EVICTION_TAG_TYPE;
                }
            }
        } else if (ig_md.switchv2p_md.switch_type == switch_type_t.GW_TOR) {
            if (rand.get() <= RAND_THRESHOLD) {
                ig_intr_dprsr_md.mirror_type = MIRROR_TYPE_LEARNING;
                ig_md.switchv2p_md.mirror_header_type = LEARNING_TYPE;
                learning_mirror_session.apply();
            }
            hdr.switchv2p.key = set_key.execute(idx);
            hdr.switchv2p.val = set_val.execute(idx);
            if (hdr.switchv2p.key == 1) {
                hdr.switchv2p.type = NORMAL_TYPE;
            } else {
                hdr.switchv2p.type = EVICTION_TAG_TYPE;
            }
        }
    }

}

#endif
