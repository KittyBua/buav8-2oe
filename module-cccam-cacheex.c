#define MODULE_LOG_PREFIX "cccam"

#include "globals.h"
#include "oscam-array.h"

#if defined(CS_CACHEEX) && defined(MODULE_CCCAM)

#include "module-cacheex.h"
#include "module-cccam-data.h"
#include "module-cccam-cacheex.h"
#include "oscam-cache.h"
#include "oscam-client.h"
#include "oscam-ecm.h"
#include "oscam-string.h"
#include "oscam-chk.h"
#include "oscam-reader.h"

#define CSP_HASH_SWAP(n) (((((uint32_t)(n) & 0xFF)) << 24) | \
						((((uint32_t)(n) & 0xFF00)) << 8) | \
						((((uint32_t)(n) & 0xFF0000)) >> 8) | \
						((((uint32_t)(n) & 0xFF000000)) >> 24))

extern int32_t cc_cli_connect(struct s_client *cl);
extern int32_t cc_cmd_send(struct s_client *cl, uint8_t *buf, int32_t len, cc_msg_type_t cmd);

void cc_cacheex_feature_trigger_in(struct s_client *cl, uint8_t *buf)
{
	int32_t feature = 0;
	int i = 0;
	uint8_t filter_count;
	uint8_t j;
	feature = buf[1] | (buf[0] << 8);

	switch(feature)
	{
		// set localgenerated only
		case 1:
			if(cl->typ == 'c' && cl->account->cacheex.mode == 2)
			{
				cl->account->cacheex.localgenerated_only = buf[4];
			}
			else if(cl->typ == 'p' && cl->reader->cacheex.mode == 3)
			{
				cl->reader->cacheex.localgenerated_only = buf[4];
			}
			break;
		// set localgenerated only caidtab
		case 2:
			filter_count = buf[i+4];
			i += 5;

			CAIDTAB *lgonly_ctab;
			memset(&lgonly_ctab, 0, sizeof(lgonly_ctab));
			
			if(cl->typ == 'c' && cl->account->cacheex.mode == 2)
			{
				lgonly_ctab = &cl->account->cacheex.localgenerated_only_caidtab;
			}
			else if(cl->typ == 'p' && cl->reader->cacheex.mode == 3)
			{
				lgonly_ctab = &cl->reader->cacheex.localgenerated_only_caidtab;
			}

			caidtab_clear(lgonly_ctab);

			for(j = 0; j < filter_count; j++)
			{
				CAIDTAB_DATA d;
				memset(&d, 0, sizeof(d));
				d.mask = 0xffff;
				d.caid = b2i(2, buf + i);
				i += 2;
				caidtab_add(lgonly_ctab, &d);
			}
			break;
		// set cacheex_ecm_filter - extended
		case 4:
			filter_count = buf[i+4];
			i += 5;

			CECSPVALUETAB *filter;
			memset(&filter, 0, sizeof(filter));
			
			if(cl->typ == 'c' && cl->account->cacheex.mode == 2 && cl->account->cacheex.allow_filter)
			{
				filter = &cl->account->cacheex.filter_caidtab;
			}
			else if(cl->typ == 'p' && cl->reader->cacheex.mode == 3 && cl->reader->cacheex.allow_filter)
			{
				filter = &cl->reader->cacheex.filter_caidtab;
			}

			cecspvaluetab_clear(filter);

			for(j = 0; j < filter_count; j++)
			{
				int32_t caid = -1, cmask = -1, provid = -1, srvid = -1;
				CECSPVALUETAB_DATA d;
				memset(&d, 0, sizeof(d));
				
				caid = b2i(2, buf + i);
				if(caid == 0xFFFF) caid = -1;
				i += 2;
				
				cmask = b2i(2, buf + i);
				if(cmask == 0xFFFF) cmask = -1;
				i += 2;

				provid = b2i(3, buf + i);
				if(provid == 0xFFFFFF) provid = -1;
				i += 3;

				srvid = b2i(2, buf + i);
				if(srvid == 0xFFFF) srvid = -1;
				i += 2;
				
				if(caid > 0)
				{
					d.caid = caid;
					d.cmask = cmask;
					d.prid = provid;
					d.srvid = srvid;
					cecspvaluetab_add(filter, &d);
				}
			}
			break;
		// no push after
		case 8: ;
			CAIDVALUETAB *ctab;
			memset(&ctab, 0, sizeof(ctab));

			if(cl->typ == 'c' && cl->account->cacheex.mode == 2)
				{
					ctab = &cl->account->cacheex.cacheex_nopushafter_tab;
				}
				else if(cl->typ == 'p' && cl->reader->cacheex.mode == 3)
				{
					ctab = &cl->reader->cacheex.cacheex_nopushafter_tab;
				}

			filter_count = buf[i+4];
			i += 5;

			caidvaluetab_clear(ctab);

			for(j = 0; j < filter_count; j++)
			{
				uint16_t caid = 0, value = 0;
				CAIDVALUETAB_DATA d;
				memset(&d, 0, sizeof(d));

				caid = b2i(2, buf + i);
				if(caid == 0xFFFF) caid = -1;
				i += 2;
				
				value = b2i(2, buf + i);
				if(value == 0xFFFF) value = -1;
				i += 2;
				
				if(caid > 0)
				{
					d.caid = caid;
					d.value = value;
					caidvaluetab_add(ctab, &d);
				}
			}
			break;
		// max_hop
		case 16:
			if(cl->typ == 'c' && cl->account->cacheex.mode == 2 && cl->account->cacheex.allow_maxhop)
			{
				cl->account->cacheex.maxhop = buf[4];
			}
			else if(cl->typ == 'p' && cl->reader->cacheex.mode == 3 && cl->reader->cacheex.allow_maxhop)
			{
				cl->reader->cacheex.maxhop = buf[4];
			}
			break;
		// aio-version
		case 32:
			if(cl->typ == 'c' && cl->account->cacheex.mode > 1)
			{
				char *ofs = (char *)buf + i + 4;
				cs_strncpy(cl->account->cacheex.aio_version, ofs, sizeof(cl->account->cacheex.aio_version));
			}
			else if(cl->typ == 'p' && cl->reader->cacheex.mode > 1)
			{
				char *ofs = (char *)buf + i + 4;
				cs_strncpy(cl->reader->cacheex.aio_version, ofs, sizeof(cl->reader->cacheex.aio_version));
			}
			break;
		default:
			return;
	}
}

void cc_cacheex_feature_trigger(struct s_client *cl, int32_t feature, uint8_t mode)
{
	// size: (feature-bitfield & mask: 2) + payload-size: 2 + feature-payload :x
	uint16_t size = 2 + 2;
	int i = 0;
	uint8_t j;
	uint8_t payload[MAX_ECM_SIZE-size]; 
	memset(payload, 0, sizeof(payload));

	switch(feature)
	{
		// set localgenerated only
		case 1:
			// bitfield
			i2b_buf(2, feature, payload + i);
			i += 2;
			// payload-size
			i2b_buf(2, 1, payload + i);
			i += 2;

			size += 1;

			// set payload
			if(mode == 2)
			{
				if(cl->reader->cacheex.localgenerated_only_in)
					payload[i] = cl->reader->cacheex.localgenerated_only_in;
				else
					payload[i] = cfg.cacheex_localgenerated_only_in;
				i += 1;
			}
			else if(mode == 3)
			{
				if(cl->account->cacheex.localgenerated_only_in)
					payload[i] = cl->account->cacheex.localgenerated_only_in;
				else
					payload[i] = cfg.cacheex_localgenerated_only_in;
				i += 1;
			}
			
			break;
		// set localgenerated only caidtab
		case 2: ;
			CAIDTAB *lgonly_ctab;
			if(mode == 2)
			{
				lgonly_ctab = &cl->reader->cacheex.localgenerated_only_in_caidtab;
				if(!lgonly_ctab->ctnum)
					lgonly_ctab = &cfg.cacheex_localgenerated_only_in_caidtab;
			}
			else if(mode == 3)
			{
				lgonly_ctab = &cl->account->cacheex.localgenerated_only_in_caidtab;
				if(!lgonly_ctab->ctnum)
					lgonly_ctab = &cfg.cacheex_localgenerated_only_in_caidtab;
			}
			else
			{
				return;
			}

			size += (lgonly_ctab->ctnum * 2 + 1);

			// bitfield
			i2b_buf(2, feature, payload + i);
			i += 2;
			// payload-size
			if((lgonly_ctab->ctnum * 2 + 1) > (int)sizeof(payload))
			{
				cs_log_dbg(D_CACHEEX, "ERROR: to much localgenerated only caidtab-entries (max. 255)");
				return;
			}
			i2b_buf(2, (lgonly_ctab->ctnum * 2 + 1), payload + i); // n * caid + ctnum
			i += 2;
			// set payload
			if(lgonly_ctab->ctnum > 255)
			{
				cs_log_dbg(D_CACHEEX, "ERROR: to much localgenerated only caidtab-entries (max. 255)");
				return;
			}
			payload[i] = lgonly_ctab->ctnum;
			i += 1;

			for(j = 0; j < lgonly_ctab->ctnum; j++)
			{
				CAIDTAB_DATA *d = &lgonly_ctab->ctdata[j];
				if(d->caid)
				{
					i2b_buf(2, d->caid, payload + i);
					i += 2;
				}
				else
				{
					continue;
				}
			}
			break;
		// cacchex_ecm_filter extendend
		case 4: ;
			CECSPVALUETAB *filter;
			if(mode == 2)
			{
				filter = &cl->reader->cacheex.filter_caidtab;
				// if not set, use global settings
				if(cl->reader->cacheex.filter_caidtab.cevnum == 0 && cfg.cacheex_filter_caidtab.cevnum > 0)
					filter = &cfg.cacheex_filter_caidtab;
				// if aio, use global aio settings
				if(cl->reader->cacheex.filter_caidtab.cevnum == 0 && cfg.cacheex_filter_caidtab_aio.cevnum > 0 && cl->cacheex_aio_checked && (cl->reader->cacheex.feature_bitfield & 4))
					filter = &cfg.cacheex_filter_caidtab_aio;
			}
			else if(mode == 3)
			{
				filter = &cl->account->cacheex.filter_caidtab;
				// if not set, use global settings
				if(cl->account->cacheex.filter_caidtab.cevnum == 0 && cfg.cacheex_filter_caidtab.cevnum > 0)
					filter = &cfg.cacheex_filter_caidtab;
				// if aio, use global aio settings
				if(cl->account->cacheex.filter_caidtab.cevnum == 0 && cfg.cacheex_filter_caidtab_aio.cevnum > 0 && cl->cacheex_aio_checked && (cl->account->cacheex.feature_bitfield & 4))
					filter = &cfg.cacheex_filter_caidtab_aio;
			}
			else
			{
				return;
			}

			size += (filter->cevnum * 9 + 1);

			// bitfield
			i2b_buf(2, feature, payload + i);
			i += 2;
			// payload-size
			if((filter->cevnum * 9 + 1) > (int)sizeof(payload))
			{
				cs_log_dbg(D_CACHEEX, "ERROR: to much cacheex_ecm_filter-entries (max. 63), only 30 default cccam-filters sent");
				return;
			}
			i2b_buf(2, (filter->cevnum * 9 + 1), payload + i); // n * (caid2,mask2,provid3,srvid2) + ctnum1
			i += 2;
			// set payload
			payload[i] = filter->cevnum;
			i += 1;

			for(j = 0; j < filter->cevnum; j++)
			{
				CECSPVALUETAB_DATA *d = &filter->cevdata[j];
				if(d->caid)
				{
					i2b_buf(2, d->caid, payload + i);
					i += 2;
				}
				if(d->cmask)
				{
					i2b_buf(2, d->cmask, payload + i);
				}
				i += 2;

				if(d->prid)
				{
					i2b_buf(3, d->prid, payload + i);
				}
				i += 3;

				if(d->srvid)
				{
					i2b_buf(2, d->srvid, payload + i);
				}
				i += 2;
			}
			break;
		// no push after
		case 8: ;
			CAIDVALUETAB *ctab;
			if(mode == 2)
			{
				ctab = &cl->reader->cacheex.cacheex_nopushafter_tab;
				if(!ctab->cvnum)
					ctab = &cfg.cacheex_nopushafter_tab;
			}
			else if(mode == 3)
			{
				ctab = &cl->account->cacheex.cacheex_nopushafter_tab;
				if(!ctab->cvnum)
					ctab = &cfg.cacheex_nopushafter_tab;
			}
			else
			{
				return;
			}

			size += (ctab->cvnum * 4 + 1);

			// bitfield
			i2b_buf(2, feature, payload + i);
			i += 2;
			// payload-size
			if((ctab->cvnum * 4 + 1) > (int)sizeof(payload))
			{
				cs_log_dbg(D_CACHEEX, "ERROR: to much no push after caidtvalueab-entries (max. 255)");
				return;
			}
			i2b_buf(2, (ctab->cvnum * 4 + 1), payload + i); // n * (caid2+value2) + cvnum
			i += 2;
			// set payload
			if(ctab->cvnum > 255)
			{
				cs_log_dbg(D_CACHEEX, "ERROR: to much no push after caidtvalueab-entries (max. 255)");
				return;
			}
			payload[i] = ctab->cvnum;
			i += 1;

			for(j = 0; j < ctab->cvnum; j++)
			{
				CAIDVALUETAB_DATA *d = &ctab->cvdata[j];
				if(d->caid)
				{
					i2b_buf(2, d->caid, payload + i);
					i += 2;
					i2b_buf(2, d->value, payload + i);
					i += 2;
				}
				else
				{
					continue;
				}
			}
			break;
		// max hop
		case 16:
			// bitfield
			i2b_buf(2, feature, payload + i);
			i += 2;
			// payload-size
			i2b_buf(2, 1, payload + i);
			i += 2;

			size += 1;

			// set payload
			if(mode == 2)
			{
				if(cl->reader->cacheex.maxhop)
					payload[i] = cl->reader->cacheex.maxhop;
				else
					payload[i] = 0;
				i += 1;
			}
			else if(mode == 3)
			{
				if(cl->account->cacheex.maxhop)
					payload[i] = cl->account->cacheex.maxhop;
				else
					payload[i] = 0;
				i += 1;
			}
			break;
		// aio-version
		case 32: ;
			uint8_t token[12];

			// bitfield
			i2b_buf(2, feature, payload + i);
			i += 2;
			// payload-size
			i2b_buf(2, sizeof(token), payload + i);
			i += 2;
			
			size +=sizeof(token);
			// set payload
			
			snprintf((char *)token, sizeof(token), "%s", CS_AIO_VERSION);
			uint8_t *ofs = payload + i;
			memcpy(ofs, token, sizeof(token));
			break;
		default:
			return;
	}
	uint8_t buf[size];
	memset(buf, 0, sizeof(buf));
	memcpy(buf, payload, size);

	cc_cmd_send(cl, payload, size, MSG_CACHE_FEATURE_TRIGGER);
}

void cc_cacheex_feature_request_save(struct s_client *cl, uint8_t *buf)
{
	int32_t field = b2i(2, buf);
	
	if(cl->typ == 'c' && cl->account->cacheex.mode == 2)
	{
		cl->account->cacheex.feature_bitfield = field;
		// flag 32 => aio-version
		if(cl->account->cacheex.feature_bitfield & 32)
		{
			cc_cacheex_feature_trigger(cl, 32, 2);
		}
	}

	if(cl->typ == 'p' && cl->reader->cacheex.mode == 3)
	{
		cl->reader->cacheex.feature_bitfield = field;
		// flag 32 => aio-version
		if(cl->reader->cacheex.feature_bitfield & 32)
		{
			cc_cacheex_feature_trigger(cl, 32, 3);
		}
	}

	if(cl->typ == 'c' && cl->account->cacheex.mode == 3)
	{
		struct s_auth *acc = cl->account;
		if(acc)
		{
			acc->cacheex.feature_bitfield = field;
			// process feature-specific actions based on feature_bitfield received
			
			// flag 1 => set localgenerated only flag
			if(acc->cacheex.feature_bitfield & 1)
			{
				cc_cacheex_feature_trigger(cl, 1, 3);
			}
			// flag 2 => set localgenerated only caids flag
			if(acc->cacheex.feature_bitfield & 2)
			{
				cc_cacheex_feature_trigger(cl, 2, 3);
			}
			// flag 4 => set cacheex_ecm_filter (extended)
			if(acc->cacheex.feature_bitfield & 4)
			{
				cc_cacheex_feature_trigger(cl, 4, 3);
			}
			// flag 8 => np push after caids
			if(acc->cacheex.feature_bitfield & 8)
			{
				cc_cacheex_feature_trigger(cl, 8, 3);
			}
			// flag 16 => maxhop
			if(acc->cacheex.feature_bitfield & 16)
			{
				cc_cacheex_feature_trigger(cl, 16, 3);
			}
			// flag 32 => aio-version
			if(acc->cacheex.feature_bitfield & 32)
			{
				cc_cacheex_feature_trigger(cl, 32, 3);
			}
		}
		else
		{
			cs_log_dbg(D_CACHEEX, "feature_bitfield save failed - cl, %s", username(cl));
		}
	}
	else if(cl->typ == 'p' && cl->reader->cacheex.mode == 2)
	{
		struct s_reader *rdr = cl->reader;
		if(rdr)
		{
			rdr->cacheex.feature_bitfield = field;
			// process feature-specific actions

			// flag 1 => set localgenerated_only; cause of rdr->cacheex.localgenerated_only_in is set
			if(rdr->cacheex.feature_bitfield & 1)
			{
				cc_cacheex_feature_trigger(cl, 1, 2);
			}
			
			// flag 2 => set localgenerated_only_caidtab; cause of rdr->cacheex.localgenerated_only_in_caidtab is set
			if(rdr->cacheex.feature_bitfield & 2)
			{
				cc_cacheex_feature_trigger(cl, 2, 2);
			}

			// flag 4 => set cacchex_ecm_filter extendend
			if(rdr->cacheex.feature_bitfield & 4)
			{
				cc_cacheex_feature_trigger(cl, 4, 2);
			}

			// flag 8 => np push after caids
			if(rdr->cacheex.feature_bitfield & 8)
			{
				cc_cacheex_feature_trigger(cl, 8, 2);
			}
			// flag 16 => maxhop
			if(rdr->cacheex.feature_bitfield & 16)
			{
				cc_cacheex_feature_trigger(cl, 16, 2);
			}
			// flag 32 => aio-version
			if(rdr->cacheex.feature_bitfield & 32)
			{
				cc_cacheex_feature_trigger(cl, 32, 2);
			}
		}
		else
		{
			cs_log_dbg(D_CACHEEX, "feature_bitfield save failed - rdr, %s", username(cl));
		}
	}
}

void cc_cacheex_feature_request_reply(struct s_client *cl)
{
	int32_t size = 2;
	uint8_t rbuf[size];

 	i2b_buf(2, CACHEEX_FEATURES, rbuf);
	cc_cmd_send(cl, rbuf, size, MSG_CACHE_FEATURE_EXCHANGE_REPLY);
}

void cc_cacheex_feature_request(struct s_client *cl)
{
	int32_t size = 2;
	uint8_t rbuf[2];
	i2b_buf(2, CACHEEX_FEATURES, rbuf);
	cc_cmd_send(cl, rbuf, size, MSG_CACHE_FEATURE_EXCHANGE);
}

void cc_cacheex_filter_out(struct s_client *cl)
{
	struct s_reader *rdr = (cl->typ == 'c') ? NULL : cl->reader;
	int i = 0, j;
	CECSPVALUETAB *filter;
	int32_t size = 482; // minimal size, keep it <= 512 for max UDP packet size without fragmentation
	uint8_t buf[482];
	memset(buf, 0, sizeof(buf));

	if(rdr && rdr->cacheex.mode == 2) // mode == 2 send filters from rdr
	{
		filter = &rdr->cacheex.filter_caidtab;
		// if not set, use global settings
		if(rdr->cacheex.filter_caidtab.cevnum == 0 && cfg.cacheex_filter_caidtab.cevnum > 0)
			filter = &cfg.cacheex_filter_caidtab;
	}
	else if(cl->typ == 'c' && cl->account && cl->account->cacheex.mode == 3) // mode == 3 send filters from acc
	{
		filter = &cl->account->cacheex.filter_caidtab;
		// if not set, use global settings
		if(cl->account->cacheex.filter_caidtab.cevnum == 0 && cfg.cacheex_filter_caidtab.cevnum > 0)
			filter = &cfg.cacheex_filter_caidtab;
	}
	else
	{
		return;
	}

	i2b_buf(2, filter->cevnum, buf + i);
	i += 2;

	int32_t max_filters = 30;
	for(j = 0; j < max_filters; j++)
	{
		if(filter->cevnum > j)
		{
			CECSPVALUETAB_DATA *d = &filter->cevdata[j];
			i2b_buf(4, d->caid, buf + i);
		}
		i += 4;
	}

	for(j = 0; j < max_filters; j++)
	{
		if(filter->cevnum > j)
		{
			CECSPVALUETAB_DATA *d = &filter->cevdata[j];
			i2b_buf(4, d->cmask, buf + i);
		}
		i += 4;
	}

	for(j = 0; j < max_filters; j++)
	{
		if(filter->cevnum > j)
		{
			CECSPVALUETAB_DATA *d = &filter->cevdata[j];
			i2b_buf(4, d->prid, buf + i);
		}
		i += 4;
	}

	for(j = 0; j < max_filters; j++)
	{
		if(filter->cevnum > j)
		{
			CECSPVALUETAB_DATA *d = &filter->cevdata[j];
			i2b_buf(4, d->srvid, buf + i);
		}
		i += 4;
	}

	cs_log_dbg(D_CACHEEX, "cacheex: sending push filter request to %s", username(cl));
	cc_cmd_send(cl, buf, size, MSG_CACHE_FILTER);
}

void cc_cacheex_filter_in(struct s_client *cl, uint8_t *buf)
{
	struct s_reader *rdr = (cl->typ == 'c') ? NULL : cl->reader;
	int i = 0, j;
	int32_t caid, cmask, provid, srvid;
	CECSPVALUETAB *filter;

	// mode == 2 write filters to acc
	if(cl->typ == 'c' && cl->account && cl->account->cacheex.mode == 2 && cl->account->cacheex.allow_filter == 1)
	{
		filter = &cl->account->cacheex.filter_caidtab;
	}
	else if(rdr && rdr->cacheex.mode == 3 && rdr->cacheex.allow_filter == 1) // mode == 3 write filters to rdr
	{
		filter = &rdr->cacheex.filter_caidtab;
	}
	else
	{
		return;
	}

	cecspvaluetab_clear(filter);
	i += 2;

	int32_t max_filters = 30;
	for(j = 0; j < max_filters; j++)
	{
		caid = b2i(4, buf + i);
		if(caid > 0)
		{
			CECSPVALUETAB_DATA d;
			memset(&d, 0, sizeof(d));
			d.caid = b2i(4, buf + i);
			cecspvaluetab_add(filter, &d);
		}
		i += 4;
	}

	for(j = 0; j < max_filters; j++)
	{
		cmask = b2i(4, buf + i);
		if(j < filter->cevnum)
		{
			CECSPVALUETAB_DATA *d = &filter->cevdata[j];
			d->cmask = cmask;
		}
		i += 4;
	}

	for(j = 0; j < max_filters; j++)
	{
		provid = b2i(4, buf + i);
		if(j < filter->cevnum)
		{
			CECSPVALUETAB_DATA *d = &filter->cevdata[j];
			d->prid = provid;
		}
		i += 4;
	}

	for(j = 0; j < max_filters; j++)
	{
		srvid = b2i(4, buf + i);
		if(j < filter->cevnum)
		{
			CECSPVALUETAB_DATA *d = &filter->cevdata[j];
			d->srvid = srvid;
		}
		i += 4;
	}

	cs_log_dbg(D_CACHEEX, "cacheex: received push filter request from %s", username(cl));
}

static int32_t cc_cacheex_push_chk(struct s_client *cl, struct ecm_request_t *er)
{
	struct cc_data *cc = cl->cc;
	if(chk_is_null_nodeid(cc->peer_node_id,8))
	{
		cs_log_dbg(D_CACHEEX, "cacheex: NO peer_node_id got yet, skip!");
		return 0;
	}

	if(ll_count(er->csp_lastnodes) >= cacheex_maxhop(cl)) // check max 10 nodes to push
	{
		cs_log_dbg(D_CACHEEX, "cacheex: nodelist reached %d nodes, no push", cacheex_maxhop(cl));
		return 0;
	}

	uint8_t *remote_node = cc->peer_node_id;

	// search existing peer nodes
	LL_LOCKITER *li = ll_li_create(er->csp_lastnodes, 0);
	uint8_t *node;
	while((node = ll_li_next(li)))
	{
		cs_log_dbg(D_CACHEEX, "cacheex: check node %" PRIu64 "X == %" PRIu64 "X ?",
					cacheex_node_id(node), cacheex_node_id(remote_node));

		if(memcmp(node, remote_node, 8) == 0)
		{
			break;
		}
	}
	ll_li_destroy(li);

	// node found, so we got it from there, do not push
	if(node)
	{
		cs_log_dbg(D_CACHEEX, "cacheex: node %" PRIu64 "X found in list => skip push!", cacheex_node_id(node));
		return 0;
	}

	if(!cl->cc)
	{
		if(cl->reader && !cl->reader->tcp_connected)
		{
			cc_cli_connect(cl);
		}
	}

	if(!cc || !cl->udp_fd)
	{
		cs_log_dbg(D_CACHEEX, "cacheex: not connected %s -> no push", username(cl));
		return 0;
	}

	// check if cw is already pushed
	if(check_is_pushed(er->cw_cache, cl))
	{
		return 0;
	}

	return 1;
}

static int32_t cc_cacheex_push_out(struct s_client *cl, struct ecm_request_t *er)
{
	int8_t rc = (er->rc < E_NOTFOUND) ? E_FOUND : er->rc;

	if(rc != E_FOUND && rc != E_UNHANDLED)
	{
		return -1; // Maybe later we could support other rcs
	}

	if(cl->reader)
	{
		if(!cl->reader->tcp_connected)
		{
			cc_cli_connect(cl);
		}
	}

	struct cc_data *cc = cl->cc;
	if(!cc || !cl->udp_fd)
	{
		cs_log_dbg(D_CACHEEX, "cacheex: not connected %s -> no push", username(cl));
		return (-1);
	}

	uint32_t size = sizeof(er->ecmd5) + sizeof(er->csp_hash) + sizeof(er->cw) + sizeof(uint8_t) +
					(ll_count(er->csp_lastnodes) + 1) * 8 + sizeof(uint8_t);

	uint8_t *buf;
	if(!cs_malloc(&buf, size + 20)) // camd35_send() adds +20
	{
		return -1;
	}

	// build ecm message
	//buf[0] = er->caid >> 8;
	//buf[1] = er->caid & 0xff;
	//buf[2] = er->prid >> 24;
	//buf[3] = er->prid >> 16;
	//buf[4] = er->prid >> 8;
	//buf[5] = er->prid & 0xff;
	//buf[10] = er->srvid >> 8;
	//buf[11] = er->srvid & 0xff;
	buf[12] = (sizeof(er->ecmd5) + sizeof(er->csp_hash) + sizeof(er->cw)) & 0xff;
	buf[13] = (sizeof(er->ecmd5) + sizeof(er->csp_hash) + sizeof(er->cw)) >> 8;
	//buf[12] = 0;
	//buf[13] = 0;
	buf[14] = rc;

	i2b_buf(2, er->caid, buf + 0);
	i2b_buf(4, er->prid, buf + 2);
	i2b_buf(2, er->srvid, buf + 10);

	if(er->cwc_cycletime && er->cwc_next_cw_cycle < 2)
	{
		buf[18] = er->cwc_cycletime; // contains cwc stage3 cycletime

		if(er->cwc_next_cw_cycle == 1)
		{
			buf[18] = (buf[18] | 0x80); // set bit 8 to high
		}

		if(cl->typ == 'c' && cl->account && cl->account->cacheex.mode)
		{
			cl->account->cwc_info++;
		}
		else if((cl->typ == 'p' || cl->typ == 'r') && (cl->reader && cl->reader->cacheex.mode))
		{
			cl->cwc_info++;
		}

		cs_log_dbg(D_CWC, "CWC (CE) push to %s cycletime: %isek - nextcwcycle: CW%i for %04X@%06X:%04X",
					username(cl), er->cwc_cycletime, er->cwc_next_cw_cycle, er->caid, er->prid, er->srvid);
	}

	buf[19] = er->ecm[0] != 0x80 && er->ecm[0] != 0x81 ? 0 : er->ecm[0];

	uint8_t *ofs = buf + 20;

	// write oscam ecmd5
	memcpy(ofs, er->ecmd5, sizeof(er->ecmd5)); // 16
	ofs += sizeof(er->ecmd5);

	// write csp hashcode
	i2b_buf(4, CSP_HASH_SWAP(er->csp_hash), ofs);
	ofs += 4;

	// write cw
	memcpy(ofs, er->cw, sizeof(er->cw)); // 16
	ofs += sizeof(er->cw);

	// write node count
	*ofs = ll_count(er->csp_lastnodes) + 1;
	ofs++;

	// write own node
	memcpy(ofs, cc->node_id, 8);
	ofs += 8;

	// write other nodes
	LL_LOCKITER *li = ll_li_create(er->csp_lastnodes, 0);
	uint8_t *node;
	while((node = ll_li_next(li)))
	{
		memcpy(ofs, node, 8);
		ofs += 8;
	}
	ll_li_destroy(li);

	// add localgenerated cw-flag
	if(er->localgenerated)
	{
		*ofs = 1;
	}
	else
	{
		*ofs = 0xFF;
	}
	ofs += 1;

	int32_t res = cc_cmd_send(cl, buf, size + 20, MSG_CACHE_PUSH);
	if(res > 0) // cache-ex is pushing out, so no receive but last_g should be updated otherwise disconnect!
	{
		if(cl->reader)
		{
			cl->reader->last_s = cl->reader->last_g = time((time_t *)0); // correct
		}

		if(cl)
		{
			cl->last = time(NULL);
		}
	}

	NULLFREE(buf);
	return res;
}

void cc_cacheex_push_in(struct s_client *cl, uint8_t *buf)
{
	struct cc_data *cc = cl->cc;
	ECM_REQUEST *er;

	if(!cc)
	{
		return;
	}

	if(cl->reader)
	{
		cl->reader->last_s = cl->reader->last_g = time((time_t *)0);
	}

	if(cl)
	{
		cl->last = time(NULL);
	}

	int8_t rc = buf[14];
	if(rc != E_FOUND && rc != E_UNHANDLED) // Maybe later we could support other rcs
	{
		return;
	}

	uint16_t size = buf[12] | (buf[13] << 8);
	if(size != sizeof(er->ecmd5) + sizeof(er->csp_hash) + sizeof(er->cw))
	{
		cs_log_dbg(D_CACHEEX, "cacheex: %s received old cash-push format! data ignored!", username(cl));
		return;
	}

	if(!(er = get_ecmtask()))
	{
		return;
	}

	er->caid = b2i(2, buf + 0);
	er->prid = b2i(4, buf + 2);
	er->srvid = b2i(2, buf + 10);
	er->ecm[0] = buf[19] != 0x80 && buf[19] != 0x81 ? 0 : buf[19]; // odd/even byte, usefull to send it over CSP and to check cw for swapping
	er->rc = rc;

	er->ecmlen = 0;

	if(buf[18])
	{
		if(buf[18] & (0x01 << 7))
		{
			er->cwc_cycletime = (buf[18] & 0x7F); // remove bit 8 to get cycletime
			er->cwc_next_cw_cycle = 1;
		}
		else
		{
			er->cwc_cycletime = buf[18];
			er->cwc_next_cw_cycle = 0;
		}
	}

	uint8_t *ofs = buf + 20;

	// Read ecmd5
	memcpy(er->ecmd5, ofs, sizeof(er->ecmd5)); // 16
	ofs += sizeof(er->ecmd5);

	if(!check_cacheex_filter(cl, er))
	{
		return;
	}

	// check cacheex_ecm_filter
	if(check_client(cl) && cl->typ == 'p' && cl->reader && cl->reader->cacheex.mode == 2
		&& 	(		(cl->reader->cacheex.filter_caidtab.cevnum > 0 && !chk_csp_ctab(er, &cl->reader->cacheex.filter_caidtab)) // reader cacheex_ecm_filter not matching if set
				|| 	(cl->reader->cacheex.filter_caidtab.cevnum == 0 && (cl->reader->cacheex.feature_bitfield & 4) && cfg.cacheex_filter_caidtab_aio.cevnum > 0 && !chk_csp_ctab(er, &cfg.cacheex_filter_caidtab_aio)) // global cacheex_ecm_filter_aio not matching if set
				|| 	(cl->reader->cacheex.filter_caidtab.cevnum == 0 && cfg.cacheex_filter_caidtab_aio.cevnum == 0 && cfg.cacheex_filter_caidtab.cevnum > 0 && !chk_csp_ctab(er, &cfg.cacheex_filter_caidtab)) // global cacheex_ecm_filter not matching if set
			)
	)
	{
		cs_log_dbg(D_CACHEEX, "cacheex: received cache not matching cacheex_ecm_filter => pushing filter again");
		cc_cacheex_filter_out(cl);	// get cache != cacheex_ecm_filter, send filter again - remote restarted
		if(cl->reader->cacheex.feature_bitfield & 4)
			cc_cacheex_feature_trigger(cl, 4, 2);
		return;
	}

	if(check_client(cl) && cl->typ == 'c' && cl->account && cl->account->cacheex.mode == 3
		&& 	(		(cl->account->cacheex.filter_caidtab.cevnum > 0 && !chk_csp_ctab(er, &cl->account->cacheex.filter_caidtab)) // account cacheex_ecm_filter not matching if set
				|| 	(cl->account->cacheex.filter_caidtab.cevnum == 0 && (cl->account->cacheex.feature_bitfield & 4) && cfg.cacheex_filter_caidtab_aio.cevnum > 0 && !chk_csp_ctab(er, &cfg.cacheex_filter_caidtab_aio)) // global cacheex_ecm_filter_aio not matching if set
				|| 	(cl->account->cacheex.filter_caidtab.cevnum == 0 && cfg.cacheex_filter_caidtab_aio.cevnum == 0 && cfg.cacheex_filter_caidtab.cevnum > 0 && !chk_csp_ctab(er, &cfg.cacheex_filter_caidtab)) // global cacheex_ecm_filter not matching if set
			)
	)
	{
		cs_log_dbg(D_CACHEEX, "cacheex: received cache not matching cacheex_ecm_filter => pushing filter again");
		cc_cacheex_filter_out(cl); // get cache != cacheex_ecm_filter, send filter again - remote restarted
		if(cl->account->cacheex.feature_bitfield & 4)
			cc_cacheex_feature_trigger(cl, 4, 3);
		return;
	}

	// Read csp_hash
	er->csp_hash = CSP_HASH_SWAP(b2i(4, ofs));
	ofs += 4;

	// Read cw
	memcpy(er->cw, ofs, sizeof(er->cw)); // 16
	ofs += sizeof(er->cw);

	// Read lastnode count
	uint8_t count = *ofs;
	ofs++;

	// check max nodes
	if(count > cacheex_maxhop(cl))
	{
		cs_log_dbg(D_CACHEEX, "cacheex: received %d nodes (max=%d), ignored! %s",
					(int32_t)count, cacheex_maxhop(cl), username(cl));

		NULLFREE(er);
		return;
	}

	cs_log_dbg(D_CACHEEX, "cacheex: received %d nodes %s", (int32_t)count, username(cl));

	// Read lastnodes
	uint8_t *data;
	if (er)
	{
		er->csp_lastnodes = ll_create("csp_lastnodes");
	}

	while(count)
	{
		if(!cs_malloc(&data, 8))
		{
			break;
		}

		memcpy(data, ofs, 8);
		ofs += 8;
		ll_append(er->csp_lastnodes, data);
		count--;

		cs_log_dbg(D_CACHEEX, "cacheex: received node %" PRIu64 "X %s", cacheex_node_id(data), username(cl));
	}

	if(b2i(1, ofs) == 1)
	{
		er->localgenerated = 1;
		cs_log_dbg(D_CACHEEX, "cacheex: received ECM with localgenerated flag %04X@%06X:%04X %s", er->caid, er->prid, er->srvid, username(cl));
	}
	// without localgenerated flag
	else 
	{
		if(	cfg.cacheex_localgenerated_only_in 
				|| (cl->typ == 'p' && cl->reader && cl->reader->cacheex.mode == 2
					&& (cl->reader->cacheex.localgenerated_only_in || chk_ctab_ex(er->caid, &cl->reader->cacheex.localgenerated_only_in_caidtab) || chk_ctab_ex(er->caid, &cfg.cacheex_localgenerated_only_in_caidtab)))
				|| (cl->typ == 'c' && cl->account && cl->account->cacheex.mode == 3
					&& (cl->account->cacheex.localgenerated_only_in || chk_ctab_ex(er->caid, &cl->account->cacheex.localgenerated_only_in_caidtab) || chk_ctab_ex(er->caid, &cfg.cacheex_localgenerated_only_in_caidtab)))
		)
		{
			cs_log_dbg(D_CACHEEX, "cacheex: drop ECM without localgenerated flag %04X@%06X:%04X %s", er->caid, er->prid, er->srvid, username(cl));
			free_push_in_ecm(er);
			return;
		}
	}

	// for compatibility: add peer node if no node received
	if(!ll_count(er->csp_lastnodes))
	{
		if(!cs_malloc(&data, 8))
		{
			return;
		}

		memcpy(data, cc->peer_node_id, 8);
		ll_append(er->csp_lastnodes, data);
		cs_log_dbg(D_CACHEEX, "cacheex: added missing remote node id %" PRIu64 "X", cacheex_node_id(data));
	}

	if (er->cwc_cycletime && er->cwc_next_cw_cycle < 2)
	{
		if(cl->typ == 'c' && cl->account && cl->account->cacheex.mode)
		{
			cl->account->cwc_info++;
		}
		else if((cl->typ == 'p' || cl->typ == 'r') && (cl->reader && cl->reader->cacheex.mode))
		{
			cl->cwc_info++;
		}

		cs_log_dbg(D_CWC, "CWC (CE) received from %s cycletime: %isek - nextcwcycle: CW%i for %04X@%06X:%04X",
					username(cl), er->cwc_cycletime, er->cwc_next_cw_cycle, er->caid, er->prid, er->srvid);
	}
	
	cacheex_add_to_cache(cl, er);
}

void cc_cacheex_module_init(struct s_module *ph)
{
	ph->c_cache_push = cc_cacheex_push_out;
	ph->c_cache_push_chk = cc_cacheex_push_chk;
}

#endif