// qmipdc: minimal QMI PDC (Persistent Device Configuration) client over QRTR.
//
//   qmipdc list                    list software (MCFG) configs and the selected one
//   qmipdc info <id>               config info (description/version) of a loaded config
//   qmipdc load <file.mbn> <id>    load an MBN into the modem under <id>
//   qmipdc select <id>             mark <id> as the selected software config
//   qmipdc activate                activate the selected config (modem restarts)
//
// Message ids follow libqmi's qmi-service-pdc.json.
#include <errno.h>
#include <linux/qrtr.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#ifndef AF_QIPCRTR
#define AF_QIPCRTR 42
#endif

#define PDC_SERVICE 0x24
#define MSG_REGISTER 0x0020
#define MSG_GET_SELECTED 0x0022
#define MSG_SET_SELECTED 0x0023
#define MSG_LIST 0x0024
#define MSG_LOAD 0x0026
#define MSG_ACTIVATE 0x0027
#define MSG_GET_INFO 0x0028
#define CONFIG_TYPE_SW 1
#define CHUNK 900

static int sock;
static struct sockaddr_qrtr srv;
static uint16_t txn = 1;
static uint32_t token = 0x1234;

static int lookup(void)
{
	struct sockaddr_qrtr sq;
	socklen_t sl = sizeof(sq);
	struct qrtr_ctrl_pkt pkt;

	if (getsockname(sock, (struct sockaddr *)&sq, &sl) < 0)
		return -1;
	memset(&pkt, 0, sizeof(pkt));
	pkt.cmd = QRTR_TYPE_NEW_LOOKUP;
	pkt.server.service = PDC_SERVICE;
	sq.sq_port = QRTR_PORT_CTRL;
	if (sendto(sock, &pkt, sizeof(pkt), 0, (struct sockaddr *)&sq, sizeof(sq)) < 0)
		return -1;
	for (;;) {
		struct pollfd p = { sock, POLLIN, 0 };
		if (poll(&p, 1, 3000) <= 0)
			return -1;
		struct sockaddr_qrtr from;
		socklen_t fl = sizeof(from);
		ssize_t n = recvfrom(sock, &pkt, sizeof(pkt), 0, (struct sockaddr *)&from, &fl);
		if (n < (ssize_t)sizeof(pkt) || pkt.cmd != QRTR_TYPE_NEW_SERVER)
			continue;
		if (!pkt.server.service && !pkt.server.node && !pkt.server.port)
			return -1; /* end of lookup */
		srv.sq_family = AF_QIPCRTR;
		srv.sq_node = pkt.server.node;
		srv.sq_port = pkt.server.port;
		printf("PDC service: node %u port %u\n", srv.sq_node, srv.sq_port);
		return 0;
	}
}

struct msg {
	uint8_t buf[2048];
	size_t len;
};

static void msg_init(struct msg *m, uint16_t id)
{
	m->buf[0] = 0; /* request */
	m->buf[1] = txn & 0xff;
	m->buf[2] = txn >> 8;
	m->buf[3] = id & 0xff;
	m->buf[4] = id >> 8;
	m->len = 7;
}

static void tlv(struct msg *m, uint8_t t, const void *v, uint16_t l)
{
	m->buf[m->len++] = t;
	m->buf[m->len++] = l & 0xff;
	m->buf[m->len++] = l >> 8;
	memcpy(m->buf + m->len, v, l);
	m->len += l;
}

static void tlv_u32(struct msg *m, uint8_t t, uint32_t v) { tlv(m, t, &v, 4); }

/* Returns the result code of the response; fills ind with the matching
 * indication when want_ind is set. */
static int transact(struct msg *m, uint16_t id, int want_ind, uint8_t *ind, size_t *ind_len)
{
	uint16_t l = m->len - 7;
	m->buf[5] = l & 0xff;
	m->buf[6] = l >> 8;
	if (sendto(sock, m->buf, m->len, 0, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
		perror("sendto");
		return -1;
	}
	int result = -1, got_resp = 0;
	for (;;) {
		struct pollfd p = { sock, POLLIN, 0 };
		if (poll(&p, 1, 15000) <= 0) {
			fprintf(stderr, "timeout waiting for 0x%04x\n", id);
			return -1;
		}
		uint8_t r[8192];
		ssize_t n = recv(sock, r, sizeof(r), 0);
		if (n < 7)
			continue;
		uint16_t rid = r[3] | r[4] << 8;
		if (rid != id)
			continue;
		if (r[0] == 2) { /* response */
			for (size_t o = 7; o + 3 <= (size_t)n;) {
				uint8_t t = r[o];
				uint16_t tl = r[o + 1] | r[o + 2] << 8;
				if (t == 0x02 && tl >= 4) {
					uint16_t res = r[o + 3] | r[o + 4] << 8;
					uint16_t err = r[o + 5] | r[o + 6] << 8;
					printf(" response 0x%04x: result %s, error 0x%x\n", id,
					       res ? "FAILURE" : "SUCCESS", err);
					result = res ? err : 0;
				}
				o += 3 + tl;
			}
			got_resp = 1;
			if (!want_ind || result)
				break;
		} else if (r[0] == 4 && want_ind) { /* indication */
			memcpy(ind, r, n);
			*ind_len = n;
			break;
		}
	}
	txn++;
	return got_resp ? result : -1;
}

static const uint8_t *find_tlv(const uint8_t *m, size_t n, uint8_t t, uint16_t *l)
{
	for (size_t o = 7; o + 3 <= n;) {
		uint16_t tl = m[o + 1] | m[o + 2] << 8;
		if (m[o] == t) {
			*l = tl;
			return m + o + 3;
		}
		o += 3 + tl;
	}
	return NULL;
}

static int ind_status(const uint8_t *ind, size_t n)
{
	uint16_t l;
	const uint8_t *v = find_tlv(ind, n, 0x01, &l);
	if (!v || l < 2)
		return -1;
	return v[0] | v[1] << 8;
}

static void reg(void)
{
	struct msg m;
	uint8_t on = 1;
	msg_init(&m, MSG_REGISTER);
	tlv(&m, 0x10, &on, 1);
	transact(&m, MSG_REGISTER, 0, NULL, NULL);
}

static int cmd_list(void)
{
	struct msg m;
	uint8_t ind[8192];
	size_t n;
	uint16_t l;
	const uint8_t *v;

	msg_init(&m, MSG_LIST);
	tlv_u32(&m, 0x10, token);
	tlv_u32(&m, 0x11, CONFIG_TYPE_SW);
	if (transact(&m, MSG_LIST, 1, ind, &n))
		return 1;
	printf(" list status %d\n", ind_status(ind, n));
	v = find_tlv(ind, n, 0x11, &l);
	if (v && l) {
		unsigned cnt = v[0], o = 1;
		printf(" %u software configs:\n", cnt);
		for (unsigned i = 0; i < cnt && o + 5 <= l; i++) {
			uint32_t type = v[o] | v[o + 1] << 8 | v[o + 2] << 16 | v[o + 3] << 24;
			unsigned idl = v[o + 4];
			printf("  type %u id '%.*s'\n", type, idl, (const char *)v + o + 5);
			o += 5 + idl;
		}
	}

	msg_init(&m, MSG_GET_SELECTED);
	tlv_u32(&m, 0x01, CONFIG_TYPE_SW);
	tlv_u32(&m, 0x10, token);
	if (transact(&m, MSG_GET_SELECTED, 1, ind, &n))
		return 1;
	printf(" get-selected status %d\n", ind_status(ind, n));
	v = find_tlv(ind, n, 0x11, &l);
	if (v && l)
		printf(" active  : '%.*s'\n", v[0], (const char *)v + 1);
	v = find_tlv(ind, n, 0x12, &l);
	if (v && l)
		printf(" pending : '%.*s'\n", v[0], (const char *)v + 1);
	return 0;
}

static void type_with_id(struct msg *m, uint8_t t, const char *id)
{
	uint8_t b[256];
	uint32_t type = CONFIG_TYPE_SW;
	size_t idl = strlen(id);
	memcpy(b, &type, 4);
	b[4] = idl;
	memcpy(b + 5, id, idl);
	tlv(m, t, b, 5 + idl);
}

static int cmd_info(const char *id)
{
	struct msg m;
	uint8_t ind[8192];
	size_t n;
	uint16_t l;
	const uint8_t *v;

	msg_init(&m, MSG_GET_INFO);
	type_with_id(&m, 0x01, id);
	tlv_u32(&m, 0x10, token);
	if (transact(&m, MSG_GET_INFO, 1, ind, &n))
		return 1;
	printf(" info status %d\n", ind_status(ind, n));
	if ((v = find_tlv(ind, n, 0x11, &l)) && l >= 4)
		printf(" size    : %u\n", v[0] | v[1] << 8 | v[2] << 16 | v[3] << 24);
	if ((v = find_tlv(ind, n, 0x12, &l)) && l)
		printf(" desc    : '%.*s'\n", v[0], (const char *)v + 1);
	if ((v = find_tlv(ind, n, 0x13, &l)) && l >= 4)
		printf(" version : 0x%08x\n", v[0] | v[1] << 8 | v[2] << 16 | v[3] << 24);
	return 0;
}

static int cmd_load(const char *path, const char *id)
{
	FILE *f = fopen(path, "rb");
	if (!f) {
		perror(path);
		return 1;
	}
	fseek(f, 0, SEEK_END);
	long total = ftell(f);
	fseek(f, 0, SEEK_SET);
	uint8_t *data = malloc(total);
	if (fread(data, 1, total, f) != (size_t)total) {
		fclose(f);
		return 1;
	}
	fclose(f);
	printf("loading %s (%ld bytes) as '%s'\n", path, total, id);

	size_t idl = strlen(id);
	for (long off = 0; off < total;) {
		uint16_t cl = total - off > CHUNK ? CHUNK : total - off;
		uint8_t b[16 + 128 + CHUNK];
		uint32_t type = CONFIG_TYPE_SW, tot = total;
		size_t o = 0;
		memcpy(b + o, &type, 4); o += 4;
		b[o++] = idl;
		memcpy(b + o, id, idl); o += idl;
		memcpy(b + o, &tot, 4); o += 4;
		b[o++] = cl & 0xff;
		b[o++] = cl >> 8;
		memcpy(b + o, data + off, cl); o += cl;

		struct msg m;
		uint8_t ind[8192];
		size_t n;
		msg_init(&m, MSG_LOAD);
		tlv(&m, 0x01, b, o);
		tlv_u32(&m, 0x10, token);
		if (transact(&m, MSG_LOAD, 1, ind, &n))
			return 1;
		int st = ind_status(ind, n);
		uint16_t l;
		const uint8_t *v = find_tlv(ind, n, 0x12, &l);
		uint32_t remaining = v && l >= 4 ? v[0] | v[1] << 8 | v[2] << 16 | v[3] << 24 : 0;
		off += cl;
		printf("  chunk @%ld status %d remaining %u\n", off, st, remaining);
		if (st)
			return 1;
	}
	free(data);
	return 0;
}

static int cmd_select(const char *id)
{
	struct msg m;
	uint8_t ind[8192];
	size_t n;
	msg_init(&m, MSG_SET_SELECTED);
	type_with_id(&m, 0x01, id);
	tlv_u32(&m, 0x10, token);
	if (transact(&m, MSG_SET_SELECTED, 1, ind, &n))
		return 1;
	printf(" set-selected status %d\n", ind_status(ind, n));
	return 0;
}

static int cmd_activate(void)
{
	struct msg m;
	uint8_t ind[8192];
	size_t n;
	msg_init(&m, MSG_ACTIVATE);
	tlv_u32(&m, 0x01, CONFIG_TYPE_SW);
	tlv_u32(&m, 0x10, token);
	if (transact(&m, MSG_ACTIVATE, 1, ind, &n))
		return 1;
	printf(" activate status %d (the modem restarts now)\n", ind_status(ind, n));
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s list | info <id> | load <file> <id> | select <id> | activate\n", argv[0]);
		return 2;
	}
	sock = socket(AF_QIPCRTR, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("socket(AF_QIPCRTR)");
		return 1;
	}
	if (lookup()) {
		fprintf(stderr, "PDC service not found\n");
		return 1;
	}
	reg();
	if (!strcmp(argv[1], "list"))
		return cmd_list();
	if (!strcmp(argv[1], "info") && argc > 2)
		return cmd_info(argv[2]);
	if (!strcmp(argv[1], "load") && argc > 3)
		return cmd_load(argv[2], argv[3]);
	if (!strcmp(argv[1], "select") && argc > 2)
		return cmd_select(argv[2]);
	if (!strcmp(argv[1], "activate"))
		return cmd_activate();
	fprintf(stderr, "bad command\n");
	return 2;
}
