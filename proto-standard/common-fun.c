/*
 * Aurelio Colosimo for CERN, 2011 -- GNU LGPL v2.1 or later
 * Based on PTPd project v. 2.1.0 (see AUTHORS for details)
 */
#include <ppsi/ppsi.h>
#include "common-fun.h"

static void *__align_pointer(void *p)
{
	unsigned long ip, align = 0;

	ip = (unsigned long)p;
	if (ip & 3)
		align = 4 - (ip & 3);
	return p + align;
}

void pp_prepare_pointers(struct pp_instance *ppi)
{
	ppi->tx_ptp = __align_pointer(pp_get_payload(ppi, ppi->tx_buffer));
	ppi->rx_ptp = __align_pointer(pp_get_payload(ppi, ppi->rx_buffer));

	/* Now that ptp payload is aligned, get back the header */
	ppi->tx_frame = pp_get_header(ppi, ppi->tx_ptp);
	ppi->rx_frame = pp_get_header(ppi, ppi->rx_ptp);

	if (0) { /* enable to verify... it works for me though */
		pp_printf("%p -> %p %p\n",
			  ppi->tx_buffer, ppi->tx_frame, ppi->tx_ptp);
		pp_printf("%p -> %p %p\n",
			  ppi->rx_buffer, ppi->rx_frame, ppi->rx_ptp);
	}
}

/* Called by listening, passive, slave, uncalibrated */
int st_com_execute_slave(struct pp_instance *ppi)
{
	int ret = 0;

	if (pp_hooks.execute_slave)
		ret = pp_hooks.execute_slave(ppi);
	if (ret == 1) /* done: just return */
		return 0;
	if (ret < 0)
		return ret;

	if (pp_timeout_z(ppi, PP_TO_ANN_RECEIPT)) {
		ppi->number_foreign_records = 0;
		ppi->foreign_record_i = 0;
		if (!DSDEF(ppi)->slaveOnly &&
			DSDEF(ppi)->clockQuality.clockClass != 255) {
			m1(ppi);
			ppi->next_state = PPS_MASTER;
		} else {
			ppi->next_state = PPS_LISTENING;
			pp_timeout_restart_annrec(ppi);
		}
	}
	return 0;
}

/* Called by this file, basically when an announce is got, all states */
static void st_com_add_foreign(struct pp_instance *ppi, unsigned char *buf)
{
	int i;
	MsgHeader *hdr = &ppi->received_ptp_header;

	/* Check if foreign master is already known */
	for (i = 0; i < ppi->number_foreign_records; i++) {
		if (!memcmp(&hdr->sourcePortIdentity,
			    &ppi->frgn_master[i].port_id,
			    sizeof(hdr->sourcePortIdentity))) {
			/* already in Foreign master data set, update info */
			msg_copy_header(&ppi->frgn_master[i].hdr, hdr);
			msg_unpack_announce(buf, &ppi->frgn_master[i].ann);
			return;
		}
	}

	/* New foreign master */
	if (ppi->number_foreign_records < PP_NR_FOREIGN_RECORDS)
		ppi->number_foreign_records++;

	/* FIXME: replace the worst */
	i = ppi->foreign_record_i;

	/* Copy new foreign master data set from announce message */
	memcpy(&ppi->frgn_master[i].port_id,
	       &hdr->sourcePortIdentity, sizeof(hdr->sourcePortIdentity));

	/*
	 * header and announce field of each Foreign Master are
	 * usefull to run Best Master Clock Algorithm
	 */
	msg_copy_header(&ppi->frgn_master[i].hdr, hdr);
	msg_unpack_announce(buf, &ppi->frgn_master[i].ann);

	PP_VPRINTF("New foreign Master added\n");

	ppi->foreign_record_i = (ppi->foreign_record_i+1) %
		PP_NR_FOREIGN_RECORDS;
}


/* Called by slave and uncalibrated */
int st_com_slave_handle_announce(struct pp_instance *ppi, unsigned char *buf,
				 int len)
{
	MsgHeader *hdr = &ppi->received_ptp_header;
	MsgAnnounce ann;

	if (len < PP_ANNOUNCE_LENGTH)
		return -1;

	if (ppi->is_from_cur_par) {
		msg_unpack_announce(buf, &ann);
		s1(ppi, hdr, &ann);
	} else {
		/* st_com_add_foreign takes care of announce unpacking */
		st_com_add_foreign(ppi, buf);
	}

	/*Reset Timer handling Announce receipt timeout*/
	pp_timeout_restart_annrec(ppi);

	if (pp_hooks.handle_announce)
		pp_hooks.handle_announce(ppi);

	ppi->next_state = bmc(ppi); /* got a new announce: run bmc */

	return 0;
}

/* Called by slave and uncalibrated */
int st_com_slave_handle_sync(struct pp_instance *ppi, unsigned char *buf,
			     int len)
{
	TimeInternal correction_field;
	MsgHeader *hdr = &ppi->received_ptp_header;
	MsgSync sync;

	if (len < PP_SYNC_LENGTH)
		return -1;

	if (!ppi->is_from_cur_par)
		return 0;

	ppi->t2 = ppi->last_rcv_time;

	if ((hdr->flagField[0] & PP_TWO_STEP_FLAG) != 0) {
		ppi->waiting_for_follow = TRUE;
		ppi->recv_sync_sequence_id = hdr->sequenceId;
		return 0;
	}
	msg_unpack_sync(buf, &sync);
	cField_to_TimeInternal(&correction_field,
			      ppi->received_ptp_header.correctionfield);

	display_TimeInternal("Correction field",
			     &correction_field);

	ppi->waiting_for_follow = FALSE;
	to_TimeInternal(&ppi->t1,
			&sync.originTimestamp);
	pp_update_offset(ppi, &correction_field);
	pp_update_clock(ppi);
	return 0;
}

/* Called by slave and uncalibrated */
int st_com_slave_handle_followup(struct pp_instance *ppi, unsigned char *buf,
				 int len)
{
	TimeInternal correction_field;
	MsgFollowUp follow;
	int ret = 0;

	MsgHeader *hdr = &ppi->received_ptp_header;

	if (len < PP_FOLLOW_UP_LENGTH)
		return -1;

	if (!ppi->is_from_cur_par) {
		pp_error("%s: SequenceID doesn't match last Sync message\n",
			__func__);
		return 0;
	}

	if (!ppi->waiting_for_follow) {
		pp_error("%s: Slave was not waiting a follow up message\n",
			__func__);
		return 0;
	}

	if (ppi->recv_sync_sequence_id != hdr->sequenceId) {
		pp_error("%s: Follow up message is not from current parent\n",
			__func__);
		return 0;
	}

	msg_unpack_follow_up(buf, &follow);
	ppi->waiting_for_follow = FALSE;
	to_TimeInternal(&ppi->t1, &follow.preciseOriginTimestamp);

	cField_to_TimeInternal(&correction_field,
			      ppi->received_ptp_header.correctionfield);

	/* Call the extension; it may do it all and ask to return */
	if (pp_hooks.handle_followup)
		ret = pp_hooks.handle_followup(ppi, &ppi->t1,
					       &correction_field);
	if (ret == 1)
		return 0;
	if (ret < 0)
		return ret;

	pp_update_offset(ppi, &correction_field);
	pp_update_clock(ppi);
	return 0;
}

/* Called by master, listenting, passive. */
int st_com_master_handle_announce(struct pp_instance *ppi, unsigned char *buf,
				  int len)
{
	if (len < PP_ANNOUNCE_LENGTH)
		return -1;

	PP_VPRINTF("Announce message from another foreign master\n");

	st_com_add_foreign(ppi, buf);
	ppi->next_state = bmc(ppi); /* got a new announce: run bmc */
	return 0;
}

/*
 * Called by master, listenting, passive.
 * FIXME: this must be implemented to support one-step masters
 */
int st_com_master_handle_sync(struct pp_instance *ppi, unsigned char *buf,
			      int len)
{
	/* No more used: follow up is sent right after the corresponding sync */
	return 0;
}
