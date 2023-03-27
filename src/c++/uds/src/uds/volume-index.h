/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright Red Hat
 */

#ifndef VOLUMEINDEX_H
#define VOLUMEINDEX_H 1

#include "config.h"
#include "delta-index.h"
#include "uds.h"
#include "uds-threads.h"

/*
 * The volume index is the primary top-level index for UDS. It contains records which map a record
 * name to the chapter where a record with that name is stored. This mapping can definitively say
 * when no record exists. However, because we only use a sebset of the name for this index, it
 * cannot definitively say that a record for the entry does exist. It can only say that if a record
 * exists, it will be in a particular chapter. The request can then be dispatched to that chapter
 * for further processing.
 *
 * If the volume_index_record does not actually match the record name, the index can store a more
 * specific collision record to disambiguate the new entry from the existing one. Index entries are
 * managed with volume_index_record structures.
 */

#ifdef TEST_INTERNAL
extern unsigned int min_volume_index_delta_lists;

#endif /* TEST_INTERNAL */
struct volume_index_stats {
	/* Nanoseconds spent rebalancing */
	ktime_t rebalance_time;
	/* Number of memory rebalances */
	int rebalance_count;
	/* The number of records in the index */
	long record_count;
	/* The number of collision records */
	long collision_count;
	/* The number of records removed */
	long discard_count;
	/* The number of UDS_OVERFLOWs detected */
	long overflow_count;
	/* The number of delta lists */
	unsigned int num_lists;
	/* Number of early flushes */
	long early_flushes;
};

struct volume_sub_index_zone {
	u64 virtual_chapter_low;
	u64 virtual_chapter_high;
	long num_early_flushes;
} __aligned(L1_CACHE_BYTES);

struct volume_sub_index {
	/* The delta index */
	struct delta_index delta_index;
	/* The first chapter to be flushed in each zone */
	u64 *flush_chapters;
	/* The zones */
	struct volume_sub_index_zone *zones;
	/* The volume nonce */
	u64 volume_nonce;
	/* Expected size of a chapter (per zone) */
	u64 chapter_zone_bits;
	/* Maximum size of the index (per zone) */
	u64 max_zone_bits;
	/* The number of bits in address mask */
	unsigned int address_bits;
	/* Mask to get address within delta list */
	unsigned int address_mask;
	/* The number of bits in chapter number */
	unsigned int chapter_bits;
	/* The largest storable chapter number */
	unsigned int chapter_mask;
	/* The number of chapters used */
	unsigned int num_chapters;
	/* The number of delta lists */
	unsigned int num_delta_lists;
	/* The number of zones */
	unsigned int num_zones;
	/* The amount of memory allocated */
	u64 memory_size;
};

struct volume_index_zone {
	/* Protects the sampled index in this zone */
	struct mutex hook_mutex;
} __aligned(L1_CACHE_BYTES);

struct volume_index {
	unsigned int sparse_sample_rate;
	unsigned int num_zones;
	u64 memory_size;
	struct volume_sub_index vi_non_hook;
	struct volume_sub_index vi_hook;
	struct volume_index_zone *zones;
};

/*
 * The volume_index_record structure is used to facilitate processing of a record name. A client
 * first calls get_volume_index_record() to find the volume index record for a record name. The
 * fields of the record can then be examined to determine the state of the record.
 *
 * If is_found is false, then the index did not find an entry for the record name. Calling
 * put_volume_index_record() will insert a new entry for that name at the proper place.
 *
 * If is_found is true, then we did find an entry for the record name, and the virtual_chapter and
 * is_collision fields reflect the entry found. Subsequently, a call to
 * remove_volume_index_record() will remove the entry, a call to set_volume_index_record_chapter()
 * will update the existing entry, and a call to put_volume_index_record() will insert a new
 * collision record after the existing entry.
 */
struct volume_index_record {
	/* Public fields */

	/* Chapter where the record info is found */
	u64 virtual_chapter;
	/* This record is a collision */
	bool is_collision;
	/* This record is the requested record */
	bool is_found;

	/* Private fields */

	/* Zone that contains this name */
	unsigned int zone_number;
	/* The volume index */
	struct volume_sub_index *sub_index;
	/* Mutex for accessing this delta index entry in the hook index */
	struct mutex *mutex;
	/* The record name to which this record refers */
	const struct uds_record_name *name;
	/* The delta index entry for this record */
	struct delta_index_entry delta_entry;
};

int __must_check make_volume_index(const struct configuration *config,
				   u64 volume_nonce,
				   struct volume_index **volume_index);

void free_volume_index(struct volume_index *volume_index);

int __must_check compute_volume_index_save_blocks(const struct configuration *config,
						  size_t block_size,
						  u64 *block_count);

unsigned int __must_check
get_volume_index_zone(const struct volume_index *volume_index, const struct uds_record_name *name);

bool __must_check is_volume_index_sample(const struct volume_index *volume_index,
					 const struct uds_record_name *name);

/*
 * This function is only used to manage sparse cache membership. Most requests should use
 * get_volume_index_record() to look up index records instead.
 */
u64 __must_check lookup_volume_index_name(const struct volume_index *volume_index,
					  const struct uds_record_name *name);

int __must_check get_volume_index_record(struct volume_index *volume_index,
					 const struct uds_record_name *name,
					 struct volume_index_record *record);

int __must_check put_volume_index_record(struct volume_index_record *record, u64 virtual_chapter);

int __must_check remove_volume_index_record(struct volume_index_record *record);

int __must_check
set_volume_index_record_chapter(struct volume_index_record *record, u64 virtual_chapter);

void set_volume_index_open_chapter(struct volume_index *volume_index, u64 virtual_chapter);

void set_volume_index_zone_open_chapter(struct volume_index *volume_index,
					unsigned int zone_number,
					u64 virtual_chapter);

int __must_check load_volume_index(struct volume_index *volume_index,
				   struct buffered_reader **readers,
				   unsigned int num_readers);

int __must_check save_volume_index(struct volume_index *volume_index,
				   struct buffered_writer **writers,
				   unsigned int num_writers);

void get_volume_index_stats(const struct volume_index *volume_index,
			    struct volume_index_stats *stats);

#ifdef TEST_INTERNAL
size_t get_volume_index_memory_used(const struct volume_index *volume_index);

void get_volume_index_separate_stats(const struct volume_index *volume_index,
				     struct volume_index_stats *dense,
				     struct volume_index_stats *sparse);

#endif /* TEST_INTERNAL */
#endif /* VOLUMEINDEX_H */
