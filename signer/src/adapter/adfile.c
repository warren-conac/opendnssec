/*
 * $Id$
 *
 * Copyright (c) 2009 NLNet Labs. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/**
 * File Adapters.
 *
 */

#include "config.h"
#include "adapter/adapi.h"
#include "adapter/adfile.h"
#include "adapter/adutil.h"
#include "shared/allocator.h"
#include "shared/duration.h"
#include "shared/file.h"
#include "shared/log.h"
#include "shared/status.h"
#include "shared/util.h"
#include "signer/zone.h"

#include <ldns/ldns.h>
#include <stdio.h>
#include <stdlib.h>

static const char* adapter_str = "adapter";
static ods_status adfile_read_file(FILE* fd, zone_type* zone, int include);


/**
 * Create a new file adapter.
 *
 */
adfile_type*
adfile_create(allocator_type* allocator, const char* filename)
{
    adfile_type* ad = NULL;

    if (!allocator) {
        ods_log_error("[%s] cannot create file adapter: no allocator",
            adapter_str);
        return NULL;
    }
    ods_log_assert(allocator);

    ad = (adfile_type*) allocator_alloc(allocator, sizeof(adfile_type));
    if (!ad) {
        ods_log_error("[%s] cannot create file adapter: allocator failed",
            adapter_str);
        return NULL;
    }

    ad->filename = allocator_strdup(allocator, filename);
    return ad;
}


/**
 * Compare adapters.
 *
 */
int
adfile_compare(adfile_type* f1, adfile_type* f2)
{
    if (!f1 && !f2) {
        return 0;
    } else if (!f1) {
        return -1;
    } else if (!f2) {
        return 1;
    }
    return ods_strcmp(f1->filename, f2->filename);
}


/**
 * Read the next RR from zone file.
 *
 */
static ldns_rr*
adfile_read_rr(FILE* fd, zone_type* zone, char* line, ldns_rdf** orig,
    ldns_rdf** prev, uint32_t* ttl, ldns_status* status, unsigned int* l)
{
    ldns_rr* rr = NULL;
    ldns_rdf* tmp = NULL;
    FILE* fd_include = NULL;
    int len = 0;
    ods_status s = ODS_STATUS_OK;
    uint32_t new_ttl = 0;
    const char *endptr;  /* unused */
    int offset = 0;

adfile_read_line:
    if (ttl) {
        new_ttl = *ttl;
    }

    len = adfile_read_line(fd, line, l);
    adfile_rtrim(line, &len);

    if (len >= 0) {
        switch (line[0]) {
            /* directive */
            case '$':
                if (strncmp(line, "$ORIGIN", 7) == 0 && isspace(line[7])) {
                    /* copy from ldns */
                    if (*orig) {
                        ldns_rdf_deep_free(*orig);
                        *orig = NULL;
                    }
                    offset = 8;
                    while (isspace(line[offset])) {
                        offset++;
                    }
                    tmp = ldns_rdf_new_frm_str(LDNS_RDF_TYPE_DNAME,
                        line + offset);
                    if (!tmp) {
                        /* could not parse what next to $ORIGIN */
                        *status = LDNS_STATUS_SYNTAX_DNAME_ERR;
                        return NULL;
                    }
                    *orig = tmp;
                    /* end copy from ldns */
                    goto adfile_read_line; /* perhaps next line is rr */
                    break;
                } else if (strncmp(line, "$TTL", 4) == 0 &&
                    isspace(line[4])) {
                    /* override default ttl */
                    offset = 5;
                    while (isspace(line[offset])) {
                        offset++;
                    }
                    if (ttl) {
                        *ttl = ldns_str2period(line + offset, &endptr);
                        new_ttl = *ttl;
                    }
                    goto adfile_read_line; /* perhaps next line is rr */
                    break;
                } else if (strncmp(line, "$INCLUDE", 8) == 0 &&
                    isspace(line[8])) {
                    /* dive into this file */
                    offset = 9;
                    while (isspace(line[offset])) {
                        offset++;
                    }
                    fd_include = ods_fopen(line + offset, NULL, "r");
                    if (fd_include) {
                        s = adfile_read_file(fd_include, zone, 1);
                        ods_fclose(fd_include);
                    } else {
                        ods_log_error("[%s] unable to open include file %s",
                            adapter_str, (line+offset));
                        *status = LDNS_STATUS_SYNTAX_ERR;
                        return NULL;
                    }
                    if (s != ODS_STATUS_OK) {
                        *status = s;
                        ods_log_error("[%s] error in include file %s",
                            adapter_str, (line+offset));
                        return NULL;
                    }
                    /* restore current ttl */
                    *ttl = new_ttl;
                    goto adfile_read_line; /* perhaps next line is rr */
                    break;
                }
                goto adfile_read_rr; /* this can be an owner name */
                break;
            /* comments, empty lines */
            case ';':
            case '\n':
                goto adfile_read_line; /* perhaps next line is rr */
                break;
            /* let's hope its a RR */
            default:
adfile_read_rr:
                if (adfile_whitespace_line(line, len)) {
                    goto adfile_read_line; /* perhaps next line is rr */
                    break;
                }

                *status = ldns_rr_new_frm_str(&rr, line, new_ttl, *orig, prev);
                if (*status == LDNS_STATUS_OK) {
                    ldns_rr2canonical(rr); /* TODO: canonicalize or not? */
                    return rr;
                } else if (*status == LDNS_STATUS_SYNTAX_EMPTY) {
                    if (rr) {
                        ldns_rr_free(rr);
                        rr = NULL;
                    }
                    *status = LDNS_STATUS_OK;
                    goto adfile_read_line; /* perhaps next line is rr */
                    break;
                } else {
                    ods_log_error("[%s] error parsing RR at line %i (%s): %s",
                        adapter_str, l&&*l?*l:0,
                        ldns_get_errorstr_by_id(*status), line);
                    while (len >= 0) {
                        len = adfile_read_line(fd, line, l);
                    }
                    if (rr) {
                        ldns_rr_free(rr);
                        rr = NULL;
                    }
                    return NULL;
                }
                break;
        }
    }

    /* -1, EOF */
    *status = LDNS_STATUS_OK;
    return NULL;
}


/**
 * Read zone file.
 *
 */
static ods_status
adfile_read_file(FILE* fd, zone_type* zone, int include)
{
    ods_status result = LDNS_STATUS_OK;
    uint32_t soa_min = 0;
    ldns_rr* rr = NULL;
    ldns_rdf* prev = NULL;
    ldns_rdf* orig = NULL;
    ldns_status status = LDNS_STATUS_OK;
    char line[SE_ADFILE_MAXLINE];
    unsigned int line_update_interval = 100000;
    unsigned int line_update = line_update_interval;
    unsigned int l = 0;
    uint32_t serial = 0;

    ods_log_assert(fd);
    ods_log_assert(zone);

    if (!zone->signconf) {
        ods_log_error("[%s] read file failed, no signconf", adapter_str);
        return ODS_STATUS_ASSERT_ERR;
    }
    ods_log_assert(zone->signconf);

    if (!include) {
        rr = adfile_lookup_soa_rr(fd);
        /* default ttl */
        if (zone->signconf->soa_min) {
            soa_min = (uint32_t) duration2time(zone->signconf->soa_min);
        } else if (rr) {
            /**
             * default TTL: taking the SOA MINIMUM is in conflict with RFC2308
             * luckily, <SOA><Minimum> is a required field in conf.xml
             *
             */
            soa_min = ldns_rdf2native_int32(ldns_rr_rdf(rr,
                SE_SOA_RDATA_MINIMUM));
        }
        zone->zonedata->default_ttl = soa_min;
        /* serial */
        if (rr) {
            serial =
                ldns_rdf2native_int32(ldns_rr_rdf(rr, SE_SOA_RDATA_SERIAL));
            if (ods_strcmp(zone->signconf->soa_serial, "keep") == 0) {
                if (serial <= zone->zonedata->outbound_serial) {
                    ods_log_error("[%s] read file failed, zone %s SOA SERIAL "
                    " is set to keep, but serial %u in input zone is lower "
                    " than current serial %u", adapter_str, zone->name, serial,
                    zone->zonedata->outbound_serial);
                    ldns_rr_free(rr);
                    return ODS_STATUS_CONFLICT_ERR;
                }
            }
        }
        ldns_rr_free(rr);

        rewind(fd);
    }

    /* $ORIGIN <zone name> */
    orig = ldns_rdf_clone(zone->dname);

    ods_log_debug("[%s] start reading them RRs...", adapter_str);

    /* read records */
    while ((rr = adfile_read_rr(fd, zone, line, &orig, &prev,
        &(zone->zonedata->default_ttl), &status, &l)) != NULL) {

        if (status != LDNS_STATUS_OK) {
            ods_log_error("[%s] error reading RR at line %i (%s): %s",
                adapter_str, l, ldns_get_errorstr_by_id(status), line);
            result = ODS_STATUS_ERR;
            break;
        }

        if (l > line_update) {
            ods_log_debug("[%s] ...at line %i: %s", adapter_str, l, line);
            line_update += line_update_interval;
        }

        /* filter out DNSSEC RRs (except DNSKEY) from the Input File Adapter */
        if (util_is_dnssec_rr(rr)) {
            ldns_rr_free(rr);
            rr = NULL;
            continue;
        }

        /* add to the zonedata */
        result = adapi_add_rr(zone, rr);
        if (result != ODS_STATUS_OK) {
            ods_log_error("[%s] error adding RR at line %i: %s",
                adapter_str, l, line);
            break;
        }
        zone->stats->sort_count += 1;
    }

    /* and done */
    if (orig) {
        ldns_rdf_deep_free(orig);
        orig = NULL;
    }
    if (prev) {
        ldns_rdf_deep_free(prev);
        prev = NULL;
    }

    if (result == ODS_STATUS_OK && status != LDNS_STATUS_OK) {
        ods_log_error("[%s] error reading RR at line %i (%s): %s",
            adapter_str, l, ldns_get_errorstr_by_id(status), line);
        result = ODS_STATUS_ERR;
    }

    /* reset the default ttl (directives only affect the zone file) */
    zone->zonedata->default_ttl = soa_min;

    return result;
}


/**
 * Read zone from input file adapter.
 *
 */
ods_status
adfile_read(struct zone_struct* zone, const char* filename)
{
    FILE* fd = NULL;
    zone_type* adzone = (zone_type*) zone;
    ods_status status = ODS_STATUS_OK;

    if (!adzone || !adzone->name) {
        ods_log_error("[%s] unable to read file: no zone (or no name given)",
            adapter_str);
        return ODS_STATUS_ASSERT_ERR;
    }
    ods_log_assert(adzone);
    ods_log_assert(adzone->name);

    if (!adzone->signconf) {
        ods_log_error("[%s] unable to read file: no signconf", adapter_str);
        return ODS_STATUS_ASSERT_ERR;
    }
    ods_log_assert(adzone->signconf);

    if (!filename) {
        ods_log_error("[%s] unable to read file: no filename given",
            adapter_str);
        return ODS_STATUS_ASSERT_ERR;
    }
    ods_log_assert(filename);

    ods_log_debug("[%s] read zone %s from file %s", adapter_str,
        adzone->name, filename);

    /* read the zonefile */
    fd = ods_fopen(filename, NULL, "r");
    if (fd) {
        status = adfile_read_file(fd, adzone, 0);
        ods_fclose(fd);
    } else {
        status = ODS_STATUS_FOPEN_ERR;
    }
    if (status != ODS_STATUS_OK) {
        ods_log_error("[%s] read zone %s from file %s failed: %s",
         adapter_str, adzone->name, filename, ods_status2str(status));
    } else {
        /* wipe out current records? */
        status = zonedata_diff(adzone->zonedata, adzone->signconf->keys);
    }
    return status;
}


/**
 * Write zone to output file adapter.
 *
 */
ods_status
adfile_write(struct zone_struct* zone, const char* filename)
{
    FILE* fd = NULL;
    zone_type* adzone = (zone_type*) zone;
    ods_status status = ODS_STATUS_OK;

    if (!adzone || !adzone->name) {
        ods_log_error("[%s] unable to write file: no zone (or no "
            "name given)", adapter_str);
        return ODS_STATUS_ASSERT_ERR;
    }
    ods_log_assert(adzone);
    ods_log_assert(adzone->name);

    if (!filename) {
        ods_log_error("[%s] unable to write file: no filename given",
            adapter_str);
        return ODS_STATUS_ERR;
    }
    ods_log_assert(filename);

    ods_log_debug("[%s] write zone %s to file %s", adapter_str,
        adzone->name, filename);

    fd = ods_fopen(filename, NULL, "w");
    if (fd) {
        status = zonedata_print(fd, adzone->zonedata);
        ods_fclose(fd);
    } else {
        status = ODS_STATUS_FOPEN_ERR;
    }

    return status;
}
