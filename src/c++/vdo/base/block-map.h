/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright Red Hat
 */

#ifndef BLOCK_MAP_H
#define BLOCK_MAP_H

#include "admin-state.h"
#include "completion.h"
#include "dirty-lists.h"
#include "header.h"
#include "int-map.h"
#include "statistics.h"
#include "types.h"
#include "vdo-layout.h"
#include "vdo-component-states.h"
#include "vdo-page-cache.h"
#include "vio.h"

enum {
	BLOCK_MAP_VIO_POOL_SIZE = 64,
};

/* Used to indicate that the page holding the location of a tree root has been "loaded". */
extern const physical_block_number_t VDO_INVALID_PBN;

struct tree_page {
	struct waiter waiter;

	/* Dirty list entry */
	struct list_head entry;

	/* If dirty, the tree zone flush generation in which it was last dirtied. */
	u8 generation;

	/* Whether this page is an interior tree page being written out. */
	bool writing;

	/* If writing, the tree zone flush generation of the copy being written. */
	u8 writing_generation;

	/*
	 * Sequence number of the earliest recovery journal block containing uncommitted updates to
	 * this page
	 */
	sequence_number_t recovery_lock;

	/* The value of recovery_lock when the this page last started writing */
	sequence_number_t writing_recovery_lock;

	char page_buffer[VDO_BLOCK_SIZE];
};

struct block_map_zone {
	zone_count_t zone_number;
	thread_id_t thread_id;
	struct admin_state state;
	struct block_map *block_map;
	struct read_only_notifier *read_only_notifier;
	struct vdo_page_cache *page_cache;
	/* Dirty tree pages, by era*/
	struct dirty_lists *dirty_lists;
	data_vio_count_t active_lookups;
	struct int_map *loading_pages;
	struct vio_pool *vio_pool;
	/* The tree page which has issued or will be issuing a flush */
	struct tree_page *flusher;
	struct wait_queue flush_waiters;
	/* The generation after the most recent flush */
	u8 generation;
	u8 oldest_generation;
	/* The counts of dirty pages in each generation */
	u32 dirty_page_counts[256];
};

struct block_map {
	struct action_manager *action_manager;
	/* The absolute PBN of the first root of the tree part of the block map */
	physical_block_number_t root_origin;
	block_count_t root_count;

	/* The era point we are currently distributing to the zones */
	sequence_number_t current_era_point;
	/* The next era point */
	sequence_number_t pending_era_point;

	/* The number of entries in block map */
	block_count_t entry_count;
	nonce_t nonce;
	struct recovery_journal *journal;

	/* The trees for finding block map pages */
	struct forest *forest;
	/* The expanded trees awaiting growth */
	struct forest *next_forest;
	/* The number of entries after growth */
	block_count_t next_entry_count;

	zone_count_t zone_count;
	struct block_map_zone zones[];
};

/**
 * typedef vdo_entry_callback - A function to be called for each allocated PBN when traversing the
 *                              forest.
 * @pbn: A PBN of a tree node.
 * @completion: The parent completion of the traversal.
 *
 * Return: VDO_SUCCESS or an error.
 */
typedef int vdo_entry_callback(physical_block_number_t pbn, struct vdo_completion *completion);


static inline struct block_map_page * __must_check
vdo_as_block_map_page(struct tree_page *tree_page)
{
	return (struct block_map_page *) tree_page->page_buffer;
}

bool vdo_copy_valid_page(char *buffer, nonce_t nonce,
			 physical_block_number_t pbn,
			 struct block_map_page *page);

void vdo_block_map_check_for_drain_complete(struct block_map_zone *zone);

void vdo_find_block_map_slot(struct data_vio *data_vio);

physical_block_number_t
vdo_find_block_map_page_pbn(struct block_map *map, page_number_t page_number);

void vdo_write_tree_page(struct tree_page *page, struct block_map_zone *zone);

#ifdef INTERNAL
bool in_cyclic_range(u16 lower, u16 value, u16 upper, u16 modulus);
#endif /* INTERNAL */

struct forest;

struct tree_page * __must_check
vdo_get_tree_page_by_index(struct forest *forest,
			   root_count_t root_index,
			   height_t height,
			   page_number_t page_index);

int __must_check vdo_make_forest(struct block_map *map, block_count_t entries);

void vdo_free_forest(struct forest *forest);

void vdo_abandon_forest(struct block_map *map);

void vdo_replace_forest(struct block_map *map);

void vdo_traverse_forest(struct block_map *map,
			 vdo_entry_callback *callback,
			 struct vdo_completion *parent);

int __must_check vdo_decode_block_map(struct block_map_state_2_0 state,
				      block_count_t logical_blocks,
				      const struct thread_config *thread_config,
				      struct vdo *vdo,
				      struct read_only_notifier *read_only_notifier,
				      struct recovery_journal *journal,
				      nonce_t nonce,
				      page_count_t cache_size,
				      block_count_t maximum_age,
				      struct block_map **map_ptr);

void vdo_drain_block_map(struct block_map *map,
			 const struct admin_state_code *operation,
			 struct vdo_completion *parent);

void vdo_resume_block_map(struct block_map *map, struct vdo_completion *parent);

int __must_check
vdo_prepare_to_grow_block_map(struct block_map *map, block_count_t new_logical_blocks);

void vdo_grow_block_map(struct block_map *map, struct vdo_completion *parent);

void vdo_abandon_block_map_growth(struct block_map *map);

void vdo_free_block_map(struct block_map *map);

struct block_map_state_2_0 __must_check
vdo_record_block_map(const struct block_map *map);

void vdo_initialize_block_map_from_journal(struct block_map *map,
					   struct recovery_journal *journal);

zone_count_t vdo_compute_logical_zone(struct data_vio *data_vio);

void vdo_advance_block_map_era(struct block_map *map, sequence_number_t recovery_block_number);

void vdo_update_block_map_page(struct block_map_page *page,
			       struct data_vio *data_vio,
			       physical_block_number_t pbn,
			       enum block_mapping_state mapping_state,
			       sequence_number_t *recovery_lock);

void vdo_get_mapped_block(struct data_vio *data_vio);

void vdo_put_mapped_block(struct data_vio *data_vio);

struct block_map_statistics __must_check vdo_get_block_map_statistics(struct block_map *map);

#endif /* BLOCK_MAP_H */
