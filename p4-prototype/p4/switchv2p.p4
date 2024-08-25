#include <tna.p4>

#include "switchv2p_headers.p4"
#include "switchv2p_parser.p4"
#include "switchv2p_ingress_egress.p4"

Pipeline(SwitchV2PIngressParser(),
         SwitchV2PIngress(),
         SwitchV2PIngressDeparser(),
         SwitchV2PEgressParser(),
         SwitchV2PEgress(),
         SwitchV2PEgressDeparser()) pipe;

Switch(pipe) main;
