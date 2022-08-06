/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright Red Hat
 */

#ifndef DEDUPE_H
#define DEDUPE_H

#include <linux/kobject.h>
#include <linux/list.h>

#include "uds.h"

#include "completion.h"
#include "kernel-types.h"
#include "statistics.h"
#include "types.h"
#include "wait-queue.h"

struct hash_lock;
struct hash_zone;
struct hash_zones;

struct pbn_lock * __must_check
vdo_get_duplicate_lock(struct data_vio *data_vio);

int __must_check vdo_acquire_hash_lock(struct data_vio *data_vio);

void vdo_enter_hash_lock(struct data_vio *data_vio);

void vdo_continue_hash_lock(struct data_vio *data_vio);

void vdo_continue_hash_lock_on_error(struct data_vio *data_vio);

void vdo_release_hash_lock(struct data_vio *data_vio);

void vdo_share_compressed_write_lock(struct data_vio *data_vio,
				     struct pbn_lock *pbn_lock);

int __must_check
vdo_make_hash_zones(struct vdo *vdo, struct hash_zones **zones_ptr);

void vdo_free_hash_zones(struct hash_zones *zones);

thread_id_t __must_check
vdo_get_hash_zone_thread_id(const struct hash_zone *zone);

void vdo_get_hash_zone_statistics(struct hash_zones *zones,
				  struct hash_lock_statistics *tally);

struct hash_zone * __must_check
vdo_select_hash_zone(struct hash_zones *zones,
		     const struct uds_chunk_name *name);

void vdo_dump_hash_zones(struct hash_zones *zones);

int __must_check
vdo_make_dedupe_index(struct vdo *vdo, struct dedupe_index **index_ptr);

void vdo_dump_dedupe_index(struct dedupe_index *index);

void vdo_free_dedupe_index(struct dedupe_index *index);

const char *vdo_get_dedupe_index_state_name(struct dedupe_index *index);

uint64_t vdo_get_dedupe_index_timeout_count(struct dedupe_index *index);

void vdo_get_dedupe_index_statistics(struct dedupe_index *index,
				     struct index_statistics *stats);

int vdo_message_dedupe_index(struct dedupe_index *index, const char *name);

void vdo_query_index(struct data_vio *data_vio,
		     enum uds_request_type operation);

int vdo_add_dedupe_index_sysfs(struct dedupe_index *index,
			       struct kobject *parent);

void vdo_start_dedupe_index(struct dedupe_index *index, bool create_flag);

void vdo_suspend_dedupe_index(struct dedupe_index *index, bool save_flag);

void vdo_resume_dedupe_index(struct dedupe_index *index,
			     struct device_config *config);

void vdo_finish_dedupe_index(struct dedupe_index *index);

/*
 * Interval (in milliseconds) from submission until switching to fast path and
 * skipping UDS.
 */
extern unsigned int vdo_dedupe_index_timeout_interval;

/*
 * Minimum time interval (in milliseconds) between timer invocations to
 * check for requests waiting for UDS that should now time out.
 */
extern unsigned int vdo_dedupe_index_min_timer_interval;

void vdo_set_dedupe_index_timeout_interval(unsigned int value);
void vdo_set_dedupe_index_min_timer_interval(unsigned int value);

bool data_vio_may_query_index(struct data_vio *data_vio);

#endif /* DEDUPE_H */
