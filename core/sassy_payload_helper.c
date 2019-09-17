#include <sassy/sassy.h>
#include <sassy/payload_helper.h>


int get_proto_offset(char *cur)
{
	u8 protocol_id = *((u8 *) cur);
	u8 *con_opcode;
	u16 log_entries;

	switch(protocol_id) 
	{
		case SASSY_PROTO_ECHO:
			return SASSY_PROTO_ECHO_PAYLOAD_SIZE;
		case SASSY_PROTO_FD:
			return SASSY_PROTO_FD_PAYLOAD_SIZE;
		case SASSY_PROTO_CONSENSUS:

			// skip protocol id (u8) and get opcode
			con_opcode = (u8 *) (cur + 1);

			sassy_dbg("consensus opcode %hhu detected\n ", *con_opcode);

			// LEAD opcode has variable size
			if((enum le_opcode *) con_opcode == LEAD) {
				// skip protocol id (u8) and skip con_opcode (u8)
				log_entries = *(cur + 2);

				sassy_dbg("detected %hhu log command entries\n", log_entries);

				return SASSY_CON_METADATA_SZ + (log_entries * SASSY_CON_LOGCMD_SZ);
			} 
			
			return SASSY_PROTO_CON_PAYLOAD_SZ;
			
		default:
			sassy_error("unknown protocol ID \n");
			return -EINVAL;
	}

}
EXPORT_SYMBOL(get_proto_offset);



/* Returns a pointer to the <n>th protocol of <spay>
 * 
 * If less than n protocols are included, a NULL ptr is returned
 */
char *sassy_get_proto(struct sassy_payload *spay, int n) 
{
	int i;
	char *cur_proto;
	int proto_offset = 0;

	cur_proto = spay->proto_data;

	if(spay->protocols_included < n) {
		sassy_error("only %d protocols are included, requested %d",spay->protocols_included, n);
		return NULL;
	}

	// Iterate through existing protocols
	for (i = 0; i < n; i++) {
		cur_offset = *(u16 *)(cur_proto + 2);
		cur_proto = cur_proto + cur_offset;
	}

	return cur_proto;
}
EXPORT_SYMBOL(sassy_get_proto);


/* Protocol offsets and protocols_included must be correct before calling this method. 
 *
 * Sets protocol id and reserves space in the sassy payload,
 * if the required space is available.
 * 
 * returns a pointer to the start of that protocol payload memory area.
 */
char *sassy_reserve_proto(struct sassy_payload *spay, u16 proto_size, u16 proto_id)
{
	int i;
	char *cur_proto;
	int proto_offset = 0;

	u16 *pid, *poff;

	cur_proto = spay->proto_data;

	// Iterate through existing protocols
	for (i = 0; i < spay->protocols_included; i++) {
		cur_offset = *(u16 *)(cur_proto + 2);
		cur_proto = cur_proto + cur_offset;
		proto_offset += cur_offset;
	}

	if (proto_offset + proto_size > MAX_SASSY_PAYLOAD_BYTES) {
		sassy_error("Not enough space in sassy payload for protocol\n");
		return NULL;
	}

	pid = (u16 *) cur_proto;
	poff = (u16 *)(cur_proto + 2);

	*pid = proto_id;
	*poff = proto_size;

	return cur_proto;
}
EXPORT_SYMBOL(sassy_reserve_proto);


/* Must be called after the sassy packet has been emitted. 
 */
void invalidate_proto_data(struct sassy_payload *spay)
{
	spay->protocols_included = 0;
}
EXPORT_SYMBOL(invalidate_proto_data);