/*
 * Alessandro Rubini for CERN, 2011 -- public domain
 */
#include <ppsi/ppsi.h>
#include <ppsi/diag.h>

/*
 * This is the state machine code. i.e. the extension-independent
 * function that runs the machine. Errors are managed and reported
 * here (based on the diag module). The returned value is the time
 * in milliseconds to wait before reentering the state machine.
 * the normal protocol. If an extended protocol is used, the table used
 * is that of the extension, otherwise the one in state-table-default.c
 */

int pp_state_machine(struct pp_instance *ppi, uint8_t *packet, int plen)
{
	struct pp_state_table_item *ip;
	int state, err = 0;

	if (plen && plen < PP_HEADER_LENGTH)
		err = -1;
	if (plen >= PP_HEADER_LENGTH) {
		PP_VPRINTF("RECV %02d %d.%09d %s\n", plen,
			(int)ppi->last_rcv_time.seconds,
			(int)ppi->last_rcv_time.nanoseconds,
			pp_msg_names[packet[0] & 0x0f]);
		err = msg_unpack_header(ppi, packet);
	}
	if (err)
		return ppi->next_delay;

	state = ppi->state;

	/* a linear search is affordable up to a few dozen items */
	for (ip = pp_state_table; ip->state != PPS_END_OF_TABLE; ip++) {
		if (ip->state != state)
			continue;
		/* found: handle this state */
		ppi->next_state = state;
		ppi->next_delay = 0;
		if (pp_diag_verbosity)
			pp_diag_fsm(ppi, ip->name, STATE_ENTER, plen);
		err = ip->f1(ppi, packet, plen);
		if (err)
			pp_diag_error(ppi, err);
		if (pp_diag_verbosity)
			pp_diag_fsm(ppi, ip->name, STATE_LEAVE, 0 /* unused */);

		ppi->is_new_state = 0;

		/* done: if new state mark it, and enter it now (0 ms) */
		if (ppi->state != ppi->next_state) {
			ppi->state = ppi->next_state;
			ppi->is_new_state = 1;
			return 0;
		}
		return ppi->next_delay;
	}
	/* Unknwon state, can't happen */
	pp_diag_error_str2(ppi, "Unknown state in FSM", "");
	return 10000; /* No way out. Repeat message every 10s */
}
