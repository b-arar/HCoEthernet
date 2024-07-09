#include <pcap/pcap.h>
#include <pcap/bpf.h>
#include <pcap/dlt.h>


void print_filter_program(struct bpf_program *prog) {
    struct bpf_insn* insn = prog->bf_insns;
    printf("{\n");
    for (int i = 0; i < prog->bf_len; i++) {
        printf("\t{0x%02x, %d, %d, 0x%08x}%c\n", insn->code, insn->jt, insn->jf, insn->k, i == prog->bf_len - 1 ? ' ' : ',');
        insn++;
    }
    printf("}\n");
}

int compile_filter(const char * filter_string, struct bpf_program* prog) {
    char errbuf[PCAP_ERRBUF_SIZE];
    int status = 0;

    pcap_t *handler = pcap_open_dead(DLT_EN10MB, 1518);

    status = pcap_compile(handler, prog, filter_string, 1, PCAP_NETMASK_UNKNOWN);
    if (status != 0) {
        printf("\nError: %s\n", pcap_geterr(handler));
        return status;
    }
    return 0;
}

