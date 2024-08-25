typedef bit<48> mac_addr_t;
typedef bit<16> ether_type_t;
typedef bit<32> ipv4_addr_t;
typedef bit<8>  ip_protocol_t;
typedef bit<13> index_t;
typedef bit<10>  rand_t;
typedef bit<8>   switch_id_t;
typedef bit<8>   type_t;
typedef bit<32> timestamp_t;
typedef bit<8> packet_type_t;
typedef bit<3> mirror_type_t;

const ether_type_t ETHERTYPE_IPV4 = 16w0x0800;
const ip_protocol_t IP_PROTOCOLS_SWITCHV2P = 146;
const packet_type_t NORMAL_TYPE = 0;
const packet_type_t INVALIDATION_TAG_TYPE = 1;
const packet_type_t EVICTION_TAG_TYPE = 2;
const packet_type_t INVALIDATION_TYPE = 3;
const packet_type_t LEARNING_TYPE = 4;
const mirror_type_t MIRROR_TYPE_LEARNING = 1;
const mirror_type_t MIRROR_TYPE_INVALIDATION = 2;


const int CACHE_SIZE = 8192;
const int RAND_THRESHOLD = 5;
const int SWITCH_COUNT = 80;


header ethernet_h {
	mac_addr_t dst_addr;
	mac_addr_t src_addr;
	ether_type_t ether_type;
}

header ipv4_h {
	bit<4> version;
	bit<4> ihl;
	bit<8> diffserv;
	bit<16> total_len;
	bit<16> identification;
	bit<3> flags;
	bit<13> frag_offset;
	bit<8> ttl;
	ip_protocol_t protocol;
	bit<16> hdr_checksum;
	ipv4_addr_t src_addr;
	ipv4_addr_t dst_addr;
}

struct key_pair_t {
	ipv4_addr_t key;
	ipv4_addr_t access_bit;
}

enum bit<3> switch_type_t {
	TOR = 0,
	GW_TOR = 1,
	SPINE = 2,
	GW_SPINE = 3,
	CORE = 4,
};

header switchv2p_h {
	type_t type;
	switch_id_t switch_id;
	ipv4_addr_t key;
	ipv4_addr_t val;
}

header mirror_h {
	packet_type_t type;
	ipv4_addr_t val;
}

struct header_t {
	mirror_h mirror;
    ethernet_h ethernet;
    ipv4_h ipv4_outter;
	ipv4_h ipv4_inner;
	switchv2p_h switchv2p;
}


@flexible
header switchv2p_md_h {
	switch_type_t switch_type;
	switch_id_t switch_id;
	ipv4_addr_t key;
	ipv4_addr_t val;
	MirrorId_t mirror_session;
	packet_type_t mirror_header_type;
	bool to_gw;
	bool in_cache;
	bool misdelivered;
}

const switchv2p_md_h switchv2p_md_initializer = {
	switch_type_t.TOR, 0, 0, 0, 0, 0, false, false, false,
};


struct ig_metadata_t {
	switchv2p_md_h switchv2p_md;
}

struct eg_metadata_t {
}

const ig_metadata_t ingress_metadata_initializer = { switchv2p_md_initializer };
