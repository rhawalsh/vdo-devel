/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright Red Hat
 */

#ifndef TYPES_H
#define TYPES_H

#include <linux/compiler_attributes.h>
#include <linux/types.h>

#include "funnel-queue.h"

/* A size type in blocks. */
typedef u64 block_count_t;

/* The size of a block. */
typedef u16 block_size_t;

/* A counter for data_vios */
typedef u16 data_vio_count_t;

/* A height within a tree. */
typedef u8 height_t;

/* The logical block number as used by the consumer. */
typedef u64 logical_block_number_t;

/* The type of the nonce used to identify instances of VDO. */
typedef u64 nonce_t;

/* A size in pages. */
typedef u32 page_count_t;

/* A page number. */
typedef u32 page_number_t;

/*
 * The physical (well, less logical) block number at which the block is found on the underlying
 * device.
 */
typedef u64 physical_block_number_t;

/*
 * A release version number. These numbers are used to make the numbering space for component
 * versions independent across release branches.
 *
 * Really an enum, but we have to specify the size for encoding; see release_versions.h for the
 * enumeration values.
 */
typedef u32 release_version_number_t;

/* A count of tree roots. */
typedef u8 root_count_t;

/* A number of sectors. */
typedef u8 sector_count_t;

/* A sequence number. */
typedef u64 sequence_number_t;

/* The offset of a block within a slab. */
typedef u32 slab_block_number;

/* A size type in slabs. */
typedef u16 slab_count_t;

/* A slot in a bin or block map page. */
typedef u16 slot_number_t;

/* typedef thread_count_t - A thread counter. */
typedef u8 thread_count_t;

/* typedef thread_id_t - A thread ID, vdo threads are numbered sequentially from 0. */
typedef u8 thread_id_t;

/* A zone counter */
typedef u8 zone_count_t;

/* The following enums are persisted on storage, so the values must be preserved. */

/* The current operating mode of the VDO. */
enum vdo_state {
	VDO_DIRTY = 0,
	VDO_NEW = 1,
	VDO_CLEAN = 2,
	VDO_READ_ONLY_MODE = 3,
	VDO_FORCE_REBUILD = 4,
	VDO_RECOVERING = 5,
	VDO_REPLAYING = 6,
	VDO_REBUILD_FOR_UPGRADE = 7,

	/* Keep VDO_STATE_COUNT at the bottom. */
	VDO_STATE_COUNT
};

/**
 * vdo_state_requires_read_only_rebuild() - Check whether a vdo_state indicates
 * that a read-only rebuild is required.
 * @state: The vdo_state to check.
 *
 * Return: true if the state indicates a rebuild is required
 */
static inline bool __must_check vdo_state_requires_read_only_rebuild(enum vdo_state state)
{
	return ((state == VDO_FORCE_REBUILD) || (state == VDO_REBUILD_FOR_UPGRADE));
}

/**
 * vdo_state_requires_recovery() - Check whether a vdo state indicates that recovery is needed.
 * @state: The state to check.
 *
 * Return: true if the state indicates a recovery is required
 */
static inline bool __must_check vdo_state_requires_recovery(enum vdo_state state)
{
	return ((state == VDO_DIRTY) || (state == VDO_REPLAYING) || (state == VDO_RECOVERING));
}

/*
 * The current operation on a physical block (from the point of view of the recovery journal, slab
 * journals, and reference counts.
 */
enum journal_operation {
	VDO_JOURNAL_DATA_REMAPPING = 0,
	VDO_JOURNAL_BLOCK_MAP_REMAPPING = 1,
} __packed;

/* Partition IDs encoded in the volume layout in the super block. */
enum partition_id {
	VDO_BLOCK_MAP_PARTITION = 0,
	VDO_BLOCK_ALLOCATOR_PARTITION = 1,
	VDO_RECOVERY_JOURNAL_PARTITION = 2,
	VDO_SLAB_SUMMARY_PARTITION = 3,
#ifdef TEST_INTERNAL
	/* These are used in unit tests */
	VDO_TEST_PARTITION_1 = 1,
	VDO_TEST_PARTITION_2 = 2,
	VDO_TEST_PARTITION_3 = 3,
	VDO_TEST_PARTITION_4 = 4,
	VDO_TEST_PARTITION_5 = 5,
#endif /* TEST_INTERNAL */
} __packed;

/* Metadata types for the vdo. */
enum vdo_metadata_type {
	VDO_METADATA_RECOVERY_JOURNAL = 1,
	VDO_METADATA_SLAB_JOURNAL = 2,
	VDO_METADATA_RECOVERY_JOURNAL_2 = 3,
} __packed;

/* A position in the block map where a block map entry is stored. */
struct block_map_slot {
	physical_block_number_t pbn;
	slot_number_t slot;
};

/*
 * Four bits of each five-byte block map entry contain a mapping state value used to distinguish
 * unmapped or trimmed logical blocks (which are treated as mapped to the zero block) from entries
 * that have been mapped to a physical block, including the zero block.
 *
 * FIXME: these should maybe be defines.
 */
enum block_mapping_state {
	VDO_MAPPING_STATE_UNMAPPED = 0, /* Must be zero to be the default value */
	VDO_MAPPING_STATE_UNCOMPRESSED = 1, /* A normal (uncompressed) block */
	VDO_MAPPING_STATE_COMPRESSED_BASE = 2, /* Compressed in slot 0 */
	VDO_MAPPING_STATE_COMPRESSED_MAX = 15, /* Compressed in slot 13 */
};

enum {
	VDO_MAX_COMPRESSION_SLOTS =
		(VDO_MAPPING_STATE_COMPRESSED_MAX - VDO_MAPPING_STATE_COMPRESSED_BASE + 1),
};


struct data_location {
	physical_block_number_t pbn;
	enum block_mapping_state state;
};

/* The configuration of a single slab derived from the configured block size and slab size. */
struct slab_config {
	/* total number of blocks in the slab */
	block_count_t slab_blocks;
	/* number of blocks available for data */
	block_count_t data_blocks;
	/* number of blocks for reference counts */
	block_count_t reference_count_blocks;
	/* number of blocks for the slab journal */
	block_count_t slab_journal_blocks;
	/*
	 * Number of blocks after which the slab journal starts pushing out a reference_block for
	 * each new entry it receives.
	 */
	block_count_t slab_journal_flushing_threshold;
	/*
	 * Number of blocks after which the slab journal pushes out all reference_blocks and makes
	 * all vios wait.
	 */
	block_count_t slab_journal_blocking_threshold;
	/* Number of blocks after which the slab must be scrubbed before coming online. */
	block_count_t slab_journal_scrubbing_threshold;
} __packed;

#if defined(__KERNEL__) || defined(INTERNAL)
enum vdo_completion_type {
	/* Keep VDO_UNSET_COMPLETION_TYPE at the top. */
	VDO_UNSET_COMPLETION_TYPE,
	VDO_ACTION_COMPLETION,
	VDO_ADMIN_COMPLETION,
	VDO_BLOCK_ALLOCATOR_COMPLETION,
	VDO_BLOCK_MAP_RECOVERY_COMPLETION,
	VDO_DATA_VIO_POOL_COMPLETION,
	VDO_DECREMENT_COMPLETION,
	VDO_FLUSH_COMPLETION,
	VDO_FLUSH_NOTIFICATION_COMPLETION,
	VDO_GENERATION_FLUSHED_COMPLETION,
	VDO_HASH_ZONE_COMPLETION,
	VDO_HASH_ZONES_COMPLETION,
	VDO_LOCK_COUNTER_COMPLETION,
	VDO_PAGE_COMPLETION,
	VDO_READ_ONLY_MODE_COMPLETION,
	VDO_READ_ONLY_REBUILD_COMPLETION,
	VDO_RECOVERY_COMPLETION,
	VDO_SLAB_SCRUBBER_COMPLETION,
	VDO_SUB_TASK_COMPLETION,
	VDO_SYNC_COMPLETION,
	VIO_COMPLETION,
#ifndef __KERNEL__
	/*
	 * Keep this block in sorted order. If you add or remove an entry, be sure to update the
	 * corresponding list in completion.c.
	 */
	VDO_TEST_COMPLETION, /* each unit test may define its own */
	VDO_WRAPPING_COMPLETION,
#endif /* not __KERNEL__ */
} __packed;

struct vdo_completion;

/**
 * typedef vdo_action - An asynchronous VDO operation.
 * @completion: The completion of the operation.
 */
typedef void vdo_action(struct vdo_completion *completion);

enum vdo_completion_priority {
	BIO_ACK_Q_ACK_PRIORITY = 0,
	BIO_ACK_Q_MAX_PRIORITY = 0,
	BIO_Q_COMPRESSED_DATA_PRIORITY = 0,
	BIO_Q_DATA_PRIORITY = 0,
	BIO_Q_FLUSH_PRIORITY = 2,
	BIO_Q_HIGH_PRIORITY = 2,
	BIO_Q_METADATA_PRIORITY = 1,
	BIO_Q_VERIFY_PRIORITY = 1,
	BIO_Q_MAX_PRIORITY = 2,
	CPU_Q_COMPLETE_VIO_PRIORITY = 0,
	CPU_Q_COMPLETE_READ_PRIORITY = 0,
	CPU_Q_COMPRESS_BLOCK_PRIORITY = 0,
	CPU_Q_EVENT_REPORTER_PRIORITY = 0,
	CPU_Q_HASH_BLOCK_PRIORITY = 0,
	CPU_Q_MAX_PRIORITY = 0,
	UDS_Q_PRIORITY = 0,
	UDS_Q_MAX_PRIORITY = 0,
	VDO_DEFAULT_Q_COMPLETION_PRIORITY = 1,
	VDO_DEFAULT_Q_FLUSH_PRIORITY = 2,
	VDO_DEFAULT_Q_MAP_BIO_PRIORITY = 0,
	VDO_DEFAULT_Q_SYNC_PRIORITY = 2,
	VDO_DEFAULT_Q_VIO_CALLBACK_PRIORITY = 1,
	VDO_DEFAULT_Q_MAX_PRIORITY = 2,
	/* The maximum allowable priority */
	VDO_WORK_Q_MAX_PRIORITY = 2,
	/* A value which must be out of range for a valid priority */
	VDO_WORK_Q_DEFAULT_PRIORITY = VDO_WORK_Q_MAX_PRIORITY + 1,
};

struct vdo_completion {
	/* The type of completion this is */
	enum vdo_completion_type type;

	/*
	 * <code>true</code> once the processing of the operation is complete. This flag should not
	 * be used by waiters external to the VDO base as it is used to gate calling the callback.
	 */
	bool complete;

	/*
	 * If true, queue this completion on the next callback invocation, even if it is already
	 * running on the correct thread.
	 */
	bool requeue;

	/* The ID of the thread which should run the next callback */
	thread_id_t callback_thread_id;

	/* The result of the operation */
	int result;

	/* The VDO on which this completion operates */
	struct vdo *vdo;

	/* The callback which will be called once the operation is complete */
	vdo_action *callback;

	/* Callback which, if set, will be called if an error result is set */
	vdo_action *error_handler;

	/* The parent object, if any, that spawned this completion */
	void *parent;

	/* Entry link for lock-free work queue */
	struct funnel_queue_entry work_queue_entry_link;
	enum vdo_completion_priority priority;
	struct vdo_work_queue *my_queue;
	u64 enqueue_time;
};

struct block_allocator;
struct data_vio;
struct vdo;
#endif /* __KERNEL__ or INTERNAL */
struct vdo_config;
#if defined(__KERNEL__) || defined(INTERNAL)
struct vio;
#endif /* __KERNEL__ or INTERNAL */

#endif /* TYPES_H */
