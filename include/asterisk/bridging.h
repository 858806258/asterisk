/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2007 - 2009, 2013 Digium, Inc.
 *
 * Richard Mudgett <rmudgett@digium.com>
 * Joshua Colp <jcolp@digium.com>
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*!
 * \file
 * \brief Channel Bridging API
 *
 * \author Richard Mudgett <rmudgett@digium.com>
 * \author Joshua Colp <jcolp@digium.com>
 * \ref AstBridging
 *
 * See Also:
 * \arg \ref AstCREDITS
 */

/*!
 * \page AstBridging Channel Bridging API
 *
 * The purpose of this API is to provide an easy and flexible way to bridge
 * channels of different technologies with different features.
 *
 * Bridging technologies provide the mechanism that do the actual handling
 * of frames between channels. They provide capability information, codec information,
 * and preference value to assist the bridging core in choosing a bridging technology when
 * creating a bridge. Different bridges may use different bridging technologies based on needs
 * but once chosen they all operate under the same premise; they receive frames and send frames.
 *
 * Bridges are a combination of bridging technology, channels, and features. A
 * developer creates a new bridge based on what they are currently expecting to do
 * with it or what they will do with it in the future. The bridging core determines what
 * available bridging technology will best fit the requirements and creates a new bridge.
 * Once created, channels can be added to the bridge in a blocking or non-blocking fashion.
 *
 * Features are such things as channel muting or DTMF based features such as attended transfer,
 * blind transfer, and hangup. Feature information must be set at the most granular level, on
 * the channel. While you can use features on a global scope the presence of a feature structure
 * on the channel will override the global scope. An example would be having the bridge muted
 * at global scope and attended transfer enabled on a channel. Since the channel itself is not muted
 * it would be able to speak.
 *
 * Feature hooks allow a developer to tell the bridging core that when a DTMF string
 * is received from a channel a callback should be called in their application. For
 * example, a conference bridge application may want to provide an IVR to control various
 * settings on the conference bridge. This can be accomplished by attaching a feature hook
 * that calls an IVR function when a DTMF string is entered.
 *
 */

#ifndef _ASTERISK_BRIDGING_H
#define _ASTERISK_BRIDGING_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include "asterisk/bridging_features.h"
#include "asterisk/bridging_roles.h"
#include "asterisk/dsp.h"
#include "asterisk/uuid.h"

/*! \brief Capabilities for a bridge technology */
enum ast_bridge_capability {
	/*! Bridge technology can service calls on hold */
	AST_BRIDGE_CAPABILITY_HOLDING = (1 << 0),
	/*! Bridge waits for channel to answer.  Passes early media. */
	AST_BRIDGE_CAPABILITY_EARLY = (1 << 1),
	/*! Bridge should natively bridge two channels if possible */
	AST_BRIDGE_CAPABILITY_NATIVE = (1 << 2),
	/*! Bridge is only capable of mixing 2 channels */
	AST_BRIDGE_CAPABILITY_1TO1MIX = (1 << 3),
	/*! Bridge is capable of mixing 2 or more channels */
	AST_BRIDGE_CAPABILITY_MULTIMIX = (1 << 4),
};

/*! \brief State information about a bridged channel */
enum ast_bridge_channel_state {
	/*! Waiting for a signal (Channel in the bridge) */
	AST_BRIDGE_CHANNEL_STATE_WAIT = 0,
	/*! Bridged channel was forced out and should be hung up (Bridge may dissolve.) */
	AST_BRIDGE_CHANNEL_STATE_END,
	/*! Bridged channel was forced out and should be hung up */
	AST_BRIDGE_CHANNEL_STATE_HANGUP,
};

enum ast_bridge_channel_thread_state {
	/*! Bridge channel thread is idle/waiting. */
	AST_BRIDGE_CHANNEL_THREAD_IDLE,
	/*! Bridge channel thread is writing a normal/simple frame. */
	AST_BRIDGE_CHANNEL_THREAD_SIMPLE,
	/*! Bridge channel thread is processing a frame. */
	AST_BRIDGE_CHANNEL_THREAD_FRAME,
};

struct ast_bridge_technology;
struct ast_bridge;

/*!
 * \brief Structure specific to bridge technologies capable of
 * performing talking optimizations.
 */
struct ast_bridge_tech_optimizations {
	/*! The amount of time in ms that talking must be detected before
	 *  the dsp determines that talking has occurred */
	unsigned int talking_threshold;
	/*! The amount of time in ms that silence must be detected before
	 *  the dsp determines that talking has stopped */
	unsigned int silence_threshold;
	/*! Whether or not the bridging technology should drop audio
	 *  detected as silence from the mix. */
	unsigned int drop_silence:1;
};

/*!
 * \brief Structure that contains information regarding a channel in a bridge
 */
struct ast_bridge_channel {
/* BUGBUG cond is only here because of external party suspend/unsuspend support. */
	/*! Condition, used if we want to wake up a thread waiting on the bridged channel */
	ast_cond_t cond;
	/*! Current bridged channel state */
	enum ast_bridge_channel_state state;
	/*! Asterisk channel participating in the bridge */
	struct ast_channel *chan;
	/*! Asterisk channel we are swapping with (if swapping) */
	struct ast_channel *swap;
	/*!
	 * \brief Bridge this channel is participating in
	 *
	 * \note The bridge pointer cannot change while the bridge or
	 * bridge_channel is locked.
	 */
	struct ast_bridge *bridge;
	/*!
	 * \brief Bridge class private channel data.
	 *
	 * \note This information is added when the channel is pushed
	 * into the bridge and removed when it is pulled from the
	 * bridge.
	 */
	void *bridge_pvt;
	/*!
	 * \brief Private information unique to the bridge technology.
	 *
	 * \note This information is added when the channel joins the
	 * bridge's technology and removed when it leaves the bridge's
	 * technology.
	 */
	void *tech_pvt;
	/*! Thread handling the bridged channel (Needed by ast_bridge_depart) */
	pthread_t thread;
	/* v-- These flags change while the bridge is locked or before the channel is in the bridge. */
	/*! TRUE if the channel is in a bridge. */
	unsigned int in_bridge:1;
	/*! TRUE if the channel just joined the bridge. */
	unsigned int just_joined:1;
	/*! TRUE if the channel is suspended from the bridge. */
	unsigned int suspended:1;
	/*! TRUE if the channel must wait for an ast_bridge_depart to reclaim the channel. */
	unsigned int depart_wait:1;
	/* ^-- These flags change while the bridge is locked or before the channel is in the bridge. */
	/*! Features structure for features that are specific to this channel */
	struct ast_bridge_features *features;
	/*! Technology optimization parameters used by bridging technologies capable of
	 *  optimizing based upon talk detection. */
	struct ast_bridge_tech_optimizations tech_args;
	/*! Copy of read format used by chan before join */
	struct ast_format read_format;
	/*! Copy of write format used by chan before join */
	struct ast_format write_format;
	/*! Call ID associated with bridge channel */
	struct ast_callid *callid;
	/*! A clone of the roles living on chan when the bridge channel joins the bridge. This may require some opacification */
	struct bridge_roles_datastore *bridge_roles;
	/*! Linked list information */
	AST_LIST_ENTRY(ast_bridge_channel) entry;
	/*! Queue of outgoing frames to the channel. */
	AST_LIST_HEAD_NOLOCK(, ast_frame) wr_queue;
	/*! Pipe to alert thread when frames are put into the wr_queue. */
	int alert_pipe[2];
	/*! TRUE if the bridge channel thread is waiting on channels (needs to be atomically settable) */
	int waiting;
	/*!
	 * \brief The bridge channel thread activity.
	 *
	 * \details Used by local channel optimization to determine if
	 * the thread is in an acceptable state to optimize.
	 *
	 * \note Needs to be atomically settable.
	 */
	enum ast_bridge_channel_thread_state activity;
};

enum ast_bridge_action_type {
	/*! Bridged channel is to detect a feature hook */
	AST_BRIDGE_ACTION_FEATURE,
	/*! Bridged channel is to act on an interval hook */
	AST_BRIDGE_ACTION_INTERVAL,
	/*! Bridged channel is to send a DTMF stream out */
	AST_BRIDGE_ACTION_DTMF_STREAM,
	/*! Bridged channel is to indicate talking start */
	AST_BRIDGE_ACTION_TALKING_START,
	/*! Bridged channel is to indicate talking stop */
	AST_BRIDGE_ACTION_TALKING_STOP,
	/*! Bridge channel is to play the indicated sound file. */
	AST_BRIDGE_ACTION_PLAY_FILE,
	/*! Bridge channel is to run the indicated application. */
	AST_BRIDGE_ACTION_RUN_APP,

	/*
	 * Bridge actions put after this comment must never be put onto
	 * the bridge_channel wr_queue because they have other resources
	 * that must be freed.
	 */

	/*! Bridge reconfiguration deferred technology destruction. */
	AST_BRIDGE_ACTION_DEFERRED_TECH_DESTROY = 1000,
	/*! Bridge deferred dissolving. */
	AST_BRIDGE_ACTION_DEFERRED_DISSOLVING,
};

enum ast_bridge_video_mode_type {
	/*! Video is not allowed in the bridge */
	AST_BRIDGE_VIDEO_MODE_NONE = 0,
	/*! A single user is picked as the only distributed of video across the bridge */
	AST_BRIDGE_VIDEO_MODE_SINGLE_SRC,
	/*! A single user's video feed is distributed to all bridge channels, but
	 *  that feed is automatically picked based on who is talking the most. */
	AST_BRIDGE_VIDEO_MODE_TALKER_SRC,
};

/*! This is used for both SINGLE_SRC mode to set what channel
 *  should be the current single video feed */
struct ast_bridge_video_single_src_data {
	/*! Only accept video coming from this channel */
	struct ast_channel *chan_vsrc;
};

/*! This is used for both SINGLE_SRC_TALKER mode to set what channel
 *  should be the current single video feed */
struct ast_bridge_video_talker_src_data {
	/*! Only accept video coming from this channel */
	struct ast_channel *chan_vsrc;
	int average_talking_energy;

	/*! Current talker see's this person */
	struct ast_channel *chan_old_vsrc;
};

struct ast_bridge_video_mode {
	enum ast_bridge_video_mode_type mode;
	/* Add data for all the video modes here. */
	union {
		struct ast_bridge_video_single_src_data single_src_data;
		struct ast_bridge_video_talker_src_data talker_src_data;
	} mode_data;
};

/*!
 * \brief Destroy the bridge.
 *
 * \param self Bridge to operate upon.
 *
 * \return Nothing
 */
typedef void (*ast_bridge_destructor_fn)(struct ast_bridge *self);

/*!
 * \brief The bridge is being dissolved.
 *
 * \param self Bridge to operate upon.
 *
 * \details
 * The bridge is being dissolved.  Remove any external
 * references to the bridge so it can be destroyed.
 *
 * \note On entry, self must NOT be locked.
 *
 * \return Nothing
 */
typedef void (*ast_bridge_dissolving_fn)(struct ast_bridge *self);

/*!
 * \brief Can this channel be pushed into the bridge.
 *
 * \param self Bridge to operate upon.
 * \param bridge_channel Bridge channel wanting to push.
 * \param swap Bridge channel to swap places with if not NULL.
 *
 * \note On entry, self is already locked.
 *
 * \retval TRUE if can push this channel into the bridge.
 */
typedef int (*ast_bridge_can_push_channel_fn)(struct ast_bridge *self, struct ast_bridge_channel *bridge_channel, struct ast_bridge_channel *swap);

/*!
 * \brief Push this channel into the bridge.
 *
 * \param self Bridge to operate upon.
 * \param bridge_channel Bridge channel to push.
 * \param swap Bridge channel to swap places with if not NULL.
 *
 * \details
 * Setup any channel hooks controlled by the bridge.  Allocate
 * bridge_channel->bridge_pvt and initialize any resources put
 * in bridge_channel->bridge_pvt if needed.  If there is a swap
 * channel, use it as a guide to setting up the bridge_channel.
 *
 * \note On entry, self is already locked.
 *
 * \retval 0 on success
 * \retval -1 on failure
 */
typedef int (*ast_bridge_push_channel_fn)(struct ast_bridge *self, struct ast_bridge_channel *bridge_channel, struct ast_bridge_channel *swap);

/*!
 * \brief Pull this channel from the bridge.
 *
 * \param self Bridge to operate upon.
 * \param bridge_channel Bridge channel to pull.
 *
 * \details
 * Remove any channel hooks controlled by the bridge.  Release
 * any resources held by bridge_channel->bridge_pvt and release
 * bridge_channel->bridge_pvt.
 *
 * \note On entry, self is already locked.
 *
 * \return Nothing
 */
typedef void (*ast_bridge_pull_channel_fn)(struct ast_bridge *self, struct ast_bridge_channel *bridge_channel);

/*!
 * \brief Notify the bridge that this channel was just masqueraded.
 *
 * \param self Bridge to operate upon.
 * \param bridge_channel Bridge channel that was masqueraded.
 *
 * \details
 * A masquerade just happened to this channel.  The bridge needs
 * to re-evaluate this a channel in the bridge.
 *
 * \note On entry, self is already locked.
 *
 * \return Nothing
 */
typedef void (*ast_bridge_notify_masquerade_fn)(struct ast_bridge *self, struct ast_bridge_channel *bridge_channel);

/*!
 * \brief Bridge virtual methods table definition.
 *
 * \note Any changes to this struct must be reflected in
 * ast_bridge_alloc() validity checking.
 */
struct ast_bridge_methods {
	/*! Bridge class name for log messages. */
	const char *name;
	/*! Destroy the bridge. */
	ast_bridge_destructor_fn destroy;
	/*! The bridge is being dissolved.  Remove any references to the bridge. */
	ast_bridge_dissolving_fn dissolving;
	/*! TRUE if can push the bridge channel into the bridge. */
	ast_bridge_can_push_channel_fn can_push;
	/*! Push the bridge channel into the bridge. */
	ast_bridge_push_channel_fn push;
	/*! Pull the bridge channel from the bridge. */
	ast_bridge_pull_channel_fn pull;
	/*! Notify the bridge of a masquerade with the channel. */
	ast_bridge_notify_masquerade_fn notify_masquerade;
};

/*!
 * \brief Structure that contains information about a bridge
 */
struct ast_bridge {
	/*! Bridge virtual method table. */
	const struct ast_bridge_methods *v_table;
	/*! Immutable bridge UUID. */
	char uniqueid[AST_UUID_STR_LEN];
	/*! Bridge technology that is handling the bridge */
	struct ast_bridge_technology *technology;
	/*! Private information unique to the bridge technology */
	void *tech_pvt;
	/*! Call ID associated with the bridge */
	struct ast_callid *callid;
	/*! Linked list of channels participating in the bridge */
	AST_LIST_HEAD_NOLOCK(, ast_bridge_channel) channels;
	/*! Queue of actions to perform on the bridge. */
	AST_LIST_HEAD_NOLOCK(, ast_frame) action_queue;
	/*! The video mode this bridge is using */
	struct ast_bridge_video_mode video_mode;
	/*! Bridge flags to tweak behavior */
	struct ast_flags feature_flags;
	/*! Number of channels participating in the bridge */
	unsigned int num_channels;
	/*! Number of active channels in the bridge. */
	unsigned int num_active;
	/*!
	 * \brief Count of the active temporary requests to inhibit bridge merges.
	 * Zero if merges are allowed.
	 *
	 * \note Temporary as in try again in a moment.
	 */
	unsigned int inhibit_merge;
	/*! The internal sample rate this bridge is mixed at when multiple channels are being mixed.
	 *  If this value is 0, the bridge technology may auto adjust the internal mixing rate. */
	unsigned int internal_sample_rate;
	/*! The mixing interval indicates how quickly the bridges internal mixing should occur
	 * for bridge technologies that mix audio. When set to 0, the bridge tech must choose a
	 * default interval for itself. */
	unsigned int internal_mixing_interval;
	/*! TRUE if the bridge was reconfigured. */
	unsigned int reconfigured:1;
	/*! TRUE if the bridge has been dissolved.  Any channel that now tries to join is immediately ejected. */
	unsigned int dissolved:1;
};

/*!
 * \brief Register the new bridge with the system.
 * \since 12.0.0
 *
 * \param bridge What to register. (Tolerates a NULL pointer)
 *
 * \code
 * struct ast_bridge *ast_bridge_basic_new(uint32_t capabilities, int flags, uint32 dtmf_features)
 * {
 *     void *bridge;
 *
 *     bridge = ast_bridge_alloc(sizeof(struct ast_bridge_basic), &ast_bridge_basic_v_table);
 *     bridge = ast_bridge_base_init(bridge, capabilities, flags);
 *     bridge = ast_bridge_basic_init(bridge, dtmf_features);
 *     bridge = ast_bridge_register(bridge);
 *     return bridge;
 * }
 * \endcode
 *
 * \note This must be done after a bridge constructor has
 * completed setting up the new bridge but before it returns.
 *
 * \note After a bridge is registered, the bridge must be
 * explicitly destroyed by ast_bridge_destroy() to get rid of
 * the bridge.
 *
 * \retval bridge on success.
 * \retval NULL on error.
 */
struct ast_bridge *ast_bridge_register(struct ast_bridge *bridge);

/*!
 * \internal
 * \brief Allocate the bridge class object memory.
 * \since 12.0.0
 *
 * \param size Size of the bridge class structure to allocate.
 * \param v_table Bridge class virtual method table.
 *
 * \retval bridge on success.
 * \retval NULL on error.
 */
struct ast_bridge *ast_bridge_alloc(size_t size, const struct ast_bridge_methods *v_table);

/*! \brief Bridge base class virtual method table. */
extern struct ast_bridge_methods ast_bridge_base_v_table;

/*!
 * \brief Initialize the base class of the bridge.
 *
 * \param self Bridge to operate upon. (Tolerates a NULL pointer)
 * \param capabilities The capabilities that we require to be used on the bridge
 * \param flags Flags that will alter the behavior of the bridge
 *
 * \retval self on success
 * \retval NULL on failure, self is already destroyed
 *
 * Example usage:
 *
 * \code
 * struct ast_bridge *bridge;
 * bridge = ast_bridge_alloc(sizeof(*bridge), &ast_bridge_base_v_table);
 * bridge = ast_bridge_base_init(bridge, AST_BRIDGE_CAPABILITY_1TO1MIX, AST_BRIDGE_FLAG_DISSOLVE_HANGUP);
 * \endcode
 *
 * This creates a no frills two party bridge that will be
 * destroyed once one of the channels hangs up.
 */
struct ast_bridge *ast_bridge_base_init(struct ast_bridge *self, uint32_t capabilities, unsigned int flags);

/*!
 * \brief Create a new base class bridge
 *
 * \param capabilities The capabilities that we require to be used on the bridge
 * \param flags Flags that will alter the behavior of the bridge
 *
 * \retval a pointer to a new bridge on success
 * \retval NULL on failure
 *
 * Example usage:
 *
 * \code
 * struct ast_bridge *bridge;
 * bridge = ast_bridge_base_new(AST_BRIDGE_CAPABILITY_1TO1MIX, AST_BRIDGE_FLAG_DISSOLVE_HANGUP);
 * \endcode
 *
 * This creates a no frills two party bridge that will be
 * destroyed once one of the channels hangs up.
 */
struct ast_bridge *ast_bridge_base_new(uint32_t capabilities, unsigned int flags);

/*!
 * \brief Try locking the bridge.
 *
 * \param bridge Bridge to try locking
 *
 * \retval 0 on success.
 * \retval non-zero on error.
 */
#define ast_bridge_trylock(bridge)	_ast_bridge_trylock(bridge, __FILE__, __PRETTY_FUNCTION__, __LINE__, #bridge)
static inline int _ast_bridge_trylock(struct ast_bridge *bridge, const char *file, const char *function, int line, const char *var)
{
	return __ao2_trylock(bridge, AO2_LOCK_REQ_MUTEX, file, function, line, var);
}

/*!
 * \brief Lock the bridge.
 *
 * \param bridge Bridge to lock
 *
 * \return Nothing
 */
#define ast_bridge_lock(bridge)	_ast_bridge_lock(bridge, __FILE__, __PRETTY_FUNCTION__, __LINE__, #bridge)
static inline void _ast_bridge_lock(struct ast_bridge *bridge, const char *file, const char *function, int line, const char *var)
{
	__ao2_lock(bridge, AO2_LOCK_REQ_MUTEX, file, function, line, var);
}

/*!
 * \brief Unlock the bridge.
 *
 * \param bridge Bridge to unlock
 *
 * \return Nothing
 */
#define ast_bridge_unlock(bridge)	_ast_bridge_unlock(bridge, __FILE__, __PRETTY_FUNCTION__, __LINE__, #bridge)
static inline void _ast_bridge_unlock(struct ast_bridge *bridge, const char *file, const char *function, int line, const char *var)
{
	__ao2_unlock(bridge, file, function, line, var);
}

/*!
 * \brief See if it is possible to create a bridge
 *
 * \param capabilities The capabilities that the bridge will use
 *
 * \retval 1 if possible
 * \retval 0 if not possible
 *
 * Example usage:
 *
 * \code
 * int possible = ast_bridge_check(AST_BRIDGE_CAPABILITY_1TO1MIX);
 * \endcode
 *
 * This sees if it is possible to create a bridge capable of bridging two channels
 * together.
 */
int ast_bridge_check(uint32_t capabilities);

/*!
 * \brief Destroy a bridge
 *
 * \param bridge Bridge to destroy
 *
 * \retval 0 on success
 * \retval -1 on failure
 *
 * Example usage:
 *
 * \code
 * ast_bridge_destroy(bridge);
 * \endcode
 *
 * This destroys a bridge that was previously created.
 */
int ast_bridge_destroy(struct ast_bridge *bridge);

/*!
 * \brief Notify bridging that this channel was just masqueraded.
 * \since 12.0.0
 *
 * \param chan Channel just involved in a masquerade
 *
 * \return Nothing
 */
void ast_bridge_notify_masquerade(struct ast_channel *chan);

/*!
 * \brief Join (blocking) a channel to a bridge
 *
 * \param bridge Bridge to join
 * \param chan Channel to join
 * \param swap Channel to swap out if swapping
 * \param features Bridge features structure
 * \param tech_args Optional Bridging tech optimization parameters for this channel.
 * \param pass_reference TRUE if the bridge reference is being passed by the caller.
 *
 * \retval state that channel exited the bridge with
 *
 * Example usage:
 *
 * \code
 * ast_bridge_join(bridge, chan, NULL, NULL, NULL, 0);
 * \endcode
 *
 * This adds a channel pointed to by the chan pointer to the bridge pointed to by
 * the bridge pointer. This function will not return until the channel has been
 * removed from the bridge, swapped out for another channel, or has hung up.
 *
 * If this channel will be replacing another channel the other channel can be specified
 * in the swap parameter. The other channel will be thrown out of the bridge in an
 * atomic fashion.
 *
 * If channel specific features are enabled a pointer to the features structure
 * can be specified in the features parameter.
 */
enum ast_bridge_channel_state ast_bridge_join(struct ast_bridge *bridge,
	struct ast_channel *chan,
	struct ast_channel *swap,
	struct ast_bridge_features *features,
	struct ast_bridge_tech_optimizations *tech_args,
	int pass_reference);

/*!
 * \brief Impart (non-blocking) a channel onto a bridge
 *
 * \param bridge Bridge to impart on
 * \param chan Channel to impart
 * \param swap Channel to swap out if swapping.  NULL if not swapping.
 * \param features Bridge features structure.
 * \param independent TRUE if caller does not want to reclaim the channel using ast_bridge_depart().
 *
 * \note The features parameter must be NULL or obtained by
 * ast_bridge_features_new().  You must not dereference features
 * after calling even if the call fails.
 *
 * \retval 0 on success
 * \retval -1 on failure
 *
 * Example usage:
 *
 * \code
 * ast_bridge_impart(bridge, chan, NULL, NULL, 0);
 * \endcode
 *
 * \details
 * This adds a channel pointed to by the chan pointer to the
 * bridge pointed to by the bridge pointer.  This function will
 * return immediately and will not wait until the channel is no
 * longer part of the bridge.
 *
 * If this channel will be replacing another channel the other
 * channel can be specified in the swap parameter.  The other
 * channel will be thrown out of the bridge in an atomic
 * fashion.
 *
 * If channel specific features are enabled, a pointer to the
 * features structure can be specified in the features
 * parameter.
 *
 * \note If you impart a channel as not independent you MUST
 * ast_bridge_depart() the channel.  The bridge channel thread
 * is created join-able.  The implication is that the channel is
 * special and will not behave like a normal channel.
 *
 * \note If you impart a channel as independent you must not
 * ast_bridge_depart() the channel.  The bridge channel thread
 * is created non-join-able.  The channel must be treated as if
 * it were placed into the bridge by ast_bridge_join().
 * Channels placed into a bridge by ast_bridge_join() are
 * removed by a third party using ast_bridge_remove().
 */
int ast_bridge_impart(struct ast_bridge *bridge, struct ast_channel *chan, struct ast_channel *swap, struct ast_bridge_features *features, int independent);

/*!
 * \brief Depart a channel from a bridge
 *
 * \param chan Channel to depart
 *
 * \retval 0 on success
 * \retval -1 on failure
 *
 * Example usage:
 *
 * \code
 * ast_bridge_depart(chan);
 * \endcode
 *
 * This removes the channel pointed to by the chan pointer from any bridge
 * it may be in and gives control to the calling thread.
 * This does not hang up the channel.
 *
 * \note This API call can only be used on channels that were added to the bridge
 *       using the ast_bridge_impart API call with the independent flag FALSE.
 */
int ast_bridge_depart(struct ast_channel *chan);

/*!
 * \brief Remove a channel from a bridge
 *
 * \param bridge Bridge that the channel is to be removed from
 * \param chan Channel to remove
 *
 * \retval 0 on success
 * \retval -1 on failure
 *
 * Example usage:
 *
 * \code
 * ast_bridge_remove(bridge, chan);
 * \endcode
 *
 * This removes the channel pointed to by the chan pointer from the bridge
 * pointed to by the bridge pointer and requests that it be hung up. Control
 * over the channel will NOT be given to the calling thread.
 *
 * \note This API call can be used on channels that were added to the bridge
 *       using both ast_bridge_join and ast_bridge_impart.
 */
int ast_bridge_remove(struct ast_bridge *bridge, struct ast_channel *chan);

/*!
 * \brief Merge two bridges together
 *
 * \param dst_bridge Destination bridge of merge.
 * \param src_bridge Source bridge of merge.
 *
 * \retval 0 on success
 * \retval -1 on failure
 *
 * Example usage:
 *
 * \code
 * ast_bridge_merge(dst_bridge, src_bridge);
 * \endcode
 *
 * This merges the bridge pointed to by src_bridge into the bridge
 * pointed to by dst_bridge.  In reality all of the channels in
 * src_bridge are moved to dst_bridge.
 *
 * \note The source bridge has no active channels in it when
 * this operation is completed.  The caller should explicitly
 * call ast_bridge_destroy().
 */
int ast_bridge_merge(struct ast_bridge *dst_bridge, struct ast_bridge *src_bridge);

/*!
 * \brief Adjust the bridge merge inhibit request count.
 * \since 12.0.0
 *
 * \param bridge What to operate on.
 * \param request Inhibit request increment.
 *     (Positive to add requests.  Negative to remove requests.)
 *
 * \return Nothing
 */
void ast_bridge_merge_inhibit(struct ast_bridge *bridge, int request);

/*!
 * \brief Adjust the bridge_channel's bridge merge inhibit request count.
 * \since 12.0.0
 *
 * \param bridge_channel What to operate on.
 * \param request Inhibit request increment.
 *     (Positive to add requests.  Negative to remove requests.)
 *
 * \note This API call is meant for internal bridging operations.
 *
 * \retval bridge adjusted merge inhibit with reference count.
 */
struct ast_bridge *ast_bridge_channel_merge_inhibit(struct ast_bridge_channel *bridge_channel, int request);

/*!
 * \brief Suspend a channel temporarily from a bridge
 *
 * \param bridge Bridge to suspend the channel from
 * \param chan Channel to suspend
 *
 * \retval 0 on success
 * \retval -1 on failure
 *
 * Example usage:
 *
 * \code
 * ast_bridge_suspend(bridge, chan);
 * \endcode
 *
 * This suspends the channel pointed to by chan from the bridge pointed to by bridge temporarily.
 * Control of the channel is given to the calling thread. This differs from ast_bridge_depart as
 * the channel will not be removed from the bridge.
 *
 * \note This API call can be used on channels that were added to the bridge
 *       using both ast_bridge_join and ast_bridge_impart.
 */
int ast_bridge_suspend(struct ast_bridge *bridge, struct ast_channel *chan);

/*!
 * \brief Unsuspend a channel from a bridge
 *
 * \param bridge Bridge to unsuspend the channel from
 * \param chan Channel to unsuspend
 *
 * \retval 0 on success
 * \retval -1 on failure
 *
 * Example usage:
 *
 * \code
 * ast_bridge_unsuspend(bridge, chan);
 * \endcode
 *
 * This unsuspends the channel pointed to by chan from the bridge pointed to by bridge.
 * The bridge will go back to handling the channel once this function returns.
 *
 * \note You must not mess with the channel once this function returns.
 *       Doing so may result in bad things happening.
 */
int ast_bridge_unsuspend(struct ast_bridge *bridge, struct ast_channel *chan);

/*!
 * \brief Check and optimize out the local channels between bridges.
 * \since 12.0.0
 *
 * \param chan Local channel writing a frame into the channel driver.
 * \param peer Other local channel in the pair.
 *
 * \note It is assumed that chan is locked.
 *
 * \retval 0 if local channels were not optimized out.
 * \retval non-zero if local channels were optimized out.
 */
int ast_bridge_local_optimized_out(struct ast_channel *chan, struct ast_channel *peer);

/*!
 * \brief Try locking the bridge_channel.
 *
 * \param bridge_channel What to try locking
 *
 * \retval 0 on success.
 * \retval non-zero on error.
 */
#define ast_bridge_channel_trylock(bridge_channel)	_ast_bridge_channel_trylock(bridge_channel, __FILE__, __PRETTY_FUNCTION__, __LINE__, #bridge_channel)
static inline int _ast_bridge_channel_trylock(struct ast_bridge_channel *bridge_channel, const char *file, const char *function, int line, const char *var)
{
	return __ao2_trylock(bridge_channel, AO2_LOCK_REQ_MUTEX, file, function, line, var);
}

/*!
 * \brief Lock the bridge_channel.
 *
 * \param bridge_channel What to lock
 *
 * \return Nothing
 */
#define ast_bridge_channel_lock(bridge_channel)	_ast_bridge_channel_lock(bridge_channel, __FILE__, __PRETTY_FUNCTION__, __LINE__, #bridge_channel)
static inline void _ast_bridge_channel_lock(struct ast_bridge_channel *bridge_channel, const char *file, const char *function, int line, const char *var)
{
	__ao2_lock(bridge_channel, AO2_LOCK_REQ_MUTEX, file, function, line, var);
}

/*!
 * \brief Unlock the bridge_channel.
 *
 * \param bridge_channel What to unlock
 *
 * \return Nothing
 */
#define ast_bridge_channel_unlock(bridge_channel)	_ast_bridge_channel_unlock(bridge_channel, __FILE__, __PRETTY_FUNCTION__, __LINE__, #bridge_channel)
static inline void _ast_bridge_channel_unlock(struct ast_bridge_channel *bridge_channel, const char *file, const char *function, int line, const char *var)
{
	__ao2_unlock(bridge_channel, file, function, line, var);
}

/*!
 * \brief Lock the bridge associated with the bridge channel.
 * \since 12.0.0
 *
 * \param bridge_channel Channel that wants to lock the bridge.
 *
 * \details
 * This is an upstream lock operation.  The defined locking
 * order is bridge then bridge_channel.
 *
 * \note On entry, neither the bridge nor bridge_channel is locked.
 *
 * \note The bridge_channel->bridge pointer changes because of a
 * bridge-merge/channel-move operation between bridges.
 *
 * \return Nothing
 */
void ast_bridge_channel_lock_bridge(struct ast_bridge_channel *bridge_channel);

/*!
 * \brief Set bridge channel state to leave bridge (if not leaving already) with no lock.
 *
 * \param bridge_channel Channel to change the state on
 * \param new_state The new state to place the channel into
 *
 * \note This API call is only meant to be used within the
 * bridging module and hook callbacks to request the channel
 * exit the bridge.
 *
 * \note This function assumes the bridge_channel is locked.
 */
void ast_bridge_change_state_nolock(struct ast_bridge_channel *bridge_channel, enum ast_bridge_channel_state new_state);

/*!
 * \brief Set bridge channel state to leave bridge (if not leaving already).
 *
 * \param bridge_channel Channel to change the state on
 * \param new_state The new state to place the channel into
 *
 * Example usage:
 *
 * \code
 * ast_bridge_change_state(bridge_channel, AST_BRIDGE_CHANNEL_STATE_HANGUP);
 * \endcode
 *
 * This places the channel pointed to by bridge_channel into the
 * state AST_BRIDGE_CHANNEL_STATE_HANGUP if it was
 * AST_BRIDGE_CHANNEL_STATE_WAIT before.
 *
 * \note This API call is only meant to be used within the
 * bridging module and hook callbacks to request the channel
 * exit the bridge.
 */
void ast_bridge_change_state(struct ast_bridge_channel *bridge_channel, enum ast_bridge_channel_state new_state);

/*!
 * \brief Put an action onto the specified bridge.
 * \since 12.0.0
 *
 * \param bridge What to queue the action on.
 * \param action What to do.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 *
 * \note This API call is meant for internal bridging operations.
 * \note BUGBUG This may get moved.
 */
int ast_bridge_queue_action(struct ast_bridge *bridge, struct ast_frame *action);

/*!
 * \brief Write a frame to the specified bridge_channel.
 * \since 12.0.0
 *
 * \param bridge_channel Channel to queue the frame.
 * \param fr Frame to write.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 *
 * \note This API call is meant for internal bridging operations.
 * \note BUGBUG This may get moved.
 */
int ast_bridge_channel_queue_frame(struct ast_bridge_channel *bridge_channel, struct ast_frame *fr);

/*!
 * \brief Used to queue an action frame onto a bridge channel and write an action frame into a bridge.
 * \since 12.0.0
 *
 * \param bridge_channel Which channel work with.
 * \param action Type of bridge action frame.
 * \param data Frame payload data to pass.
 * \param datalen Frame payload data length to pass.
 *
 * \return Nothing
 */
typedef void (*ast_bridge_channel_post_action_data)(struct ast_bridge_channel *bridge_channel, enum ast_bridge_action_type action, const void *data, size_t datalen);

/*!
 * \brief Queue an action frame onto the bridge channel with data.
 * \since 12.0.0
 *
 * \param bridge_channel Which channel to queue the frame onto.
 * \param action Type of bridge action frame.
 * \param data Frame payload data to pass.
 * \param datalen Frame payload data length to pass.
 *
 * \return Nothing
 */
void ast_bridge_channel_queue_action_data(struct ast_bridge_channel *bridge_channel, enum ast_bridge_action_type action, const void *data, size_t datalen);

/*!
 * \brief Write an action frame into the bridge with data.
 * \since 12.0.0
 *
 * \param bridge_channel Which channel is putting the frame into the bridge.
 * \param action Type of bridge action frame.
 * \param data Frame payload data to pass.
 * \param datalen Frame payload data length to pass.
 *
 * \return Nothing
 */
void ast_bridge_channel_write_action_data(struct ast_bridge_channel *bridge_channel, enum ast_bridge_action_type action, const void *data, size_t datalen);

/*!
 * \brief Queue a control frame onto the bridge channel with data.
 * \since 12.0.0
 *
 * \param bridge_channel Which channel to queue the frame onto.
 * \param control Type of control frame.
 * \param data Frame payload data to pass.
 * \param datalen Frame payload data length to pass.
 *
 * \return Nothing
 */
void ast_bridge_channel_queue_control_data(struct ast_bridge_channel *bridge_channel, enum ast_control_frame_type control, const void *data, size_t datalen);

/*!
 * \brief Write a control frame into the bridge with data.
 * \since 12.0.0
 *
 * \param bridge_channel Which channel is putting the frame into the bridge.
 * \param control Type of control frame.
 * \param data Frame payload data to pass.
 * \param datalen Frame payload data length to pass.
 *
 * \return Nothing
 */
void ast_bridge_channel_write_control_data(struct ast_bridge_channel *bridge_channel, enum ast_control_frame_type control, const void *data, size_t datalen);

/*!
 * \brief Run an application on the bridge channel.
 * \since 12.0.0
 *
 * \param bridge_channel Which channel to run the application on.
 * \param app_name Dialplan application name.
 * \param app_args Arguments for the application. (NULL tolerant)
 * \param moh_class MOH class to request bridge peers to hear while application is running.
 *     NULL if no MOH.
 *     Empty if default MOH class.
 *
 * \note This is intended to be called by bridge hooks.
 *
 * \return Nothing
 */
void ast_bridge_channel_run_app(struct ast_bridge_channel *bridge_channel, const char *app_name, const char *app_args, const char *moh_class);

/*!
 * \brief Write a bridge action run application frame into the bridge.
 * \since 12.0.0
 *
 * \param bridge_channel Which channel is putting the frame into the bridge
 * \param app_name Dialplan application name.
 * \param app_args Arguments for the application. (NULL or empty for no arguments)
 * \param moh_class MOH class to request bridge peers to hear while application is running.
 *     NULL if no MOH.
 *     Empty if default MOH class.
 *
 * \note This is intended to be called by bridge hooks.
 *
 * \return Nothing
 */
void ast_bridge_channel_write_app(struct ast_bridge_channel *bridge_channel, const char *app_name, const char *app_args, const char *moh_class);

/*!
 * \brief Queue a bridge action run application frame onto the bridge channel.
 * \since 12.0.0
 *
 * \param bridge_channel Which channel to put the frame onto
 * \param app_name Dialplan application name.
 * \param app_args Arguments for the application. (NULL or empty for no arguments)
 * \param moh_class MOH class to request bridge peers to hear while application is running.
 *     NULL if no MOH.
 *     Empty if default MOH class.
 *
 * \note This is intended to be called by bridge hooks.
 *
 * \return Nothing
 */
void ast_bridge_channel_queue_app(struct ast_bridge_channel *bridge_channel, const char *app_name, const char *app_args, const char *moh_class);

/*!
 * \brief Play a file on the bridge channel.
 * \since 12.0.0
 *
 * \param bridge_channel Which channel to play the file on
 * \param custom_play Call this function to play the playfile. (NULL if normal sound file to play)
 * \param playfile Sound filename to play.
 * \param moh_class MOH class to request bridge peers to hear while file is played.
 *     NULL if no MOH.
 *     Empty if default MOH class.
 *
 * \note This is intended to be called by bridge hooks.
 *
 * \return Nothing
 */
void ast_bridge_channel_playfile(struct ast_bridge_channel *bridge_channel, void (*custom_play)(const char *playfile), const char *playfile, const char *moh_class);

/*!
 * \brief Write a bridge action play file frame into the bridge.
 * \since 12.0.0
 *
 * \param bridge_channel Which channel is putting the frame into the bridge
 * \param custom_play Call this function to play the playfile. (NULL if normal sound file to play)
 * \param playfile Sound filename to play.
 * \param moh_class MOH class to request bridge peers to hear while file is played.
 *     NULL if no MOH.
 *     Empty if default MOH class.
 *
 * \note This is intended to be called by bridge hooks.
 *
 * \return Nothing
 */
void ast_bridge_channel_write_playfile(struct ast_bridge_channel *bridge_channel, void (*custom_play)(const char *playfile), const char *playfile, const char *moh_class);

/*!
 * \brief Queue a bridge action play file frame onto the bridge channel.
 * \since 12.0.0
 *
 * \param bridge_channel Which channel to put the frame onto.
 * \param custom_play Call this function to play the playfile. (NULL if normal sound file to play)
 * \param playfile Sound filename to play.
 * \param moh_class MOH class to request bridge peers to hear while file is played.
 *     NULL if no MOH.
 *     Empty if default MOH class.
 *
 * \note This is intended to be called by bridge hooks.
 *
 * \return Nothing
 */
void ast_bridge_channel_queue_playfile(struct ast_bridge_channel *bridge_channel, void (*custom_play)(const char *playfile), const char *playfile, const char *moh_class);

/*!
 * \brief Restore the formats of a bridge channel's channel to how they were before bridge_channel_join
 * \since 12.0.0
 *
 * \param bridge_channel Channel to restore
 */
void ast_bridge_channel_restore_formats(struct ast_bridge_channel *bridge_channel);

/*!
 * \brief Adjust the internal mixing sample rate of a bridge
 * used during multimix mode.
 *
 * \param bridge Channel to change the sample rate on.
 * \param sample_rate the sample rate to change to. If a
 *        value of 0 is passed here, the bridge will be free to pick
 *        what ever sample rate it chooses.
 *
 */
void ast_bridge_set_internal_sample_rate(struct ast_bridge *bridge, unsigned int sample_rate);

/*!
 * \brief Adjust the internal mixing interval of a bridge used
 * during multimix mode.
 *
 * \param bridge Channel to change the sample rate on.
 * \param mixing_interval the sample rate to change to.  If 0 is set
 * the bridge tech is free to choose any mixing interval it uses by default.
 */
void ast_bridge_set_mixing_interval(struct ast_bridge *bridge, unsigned int mixing_interval);

/*!
 * \brief Set a bridge to feed a single video source to all participants.
 */
void ast_bridge_set_single_src_video_mode(struct ast_bridge *bridge, struct ast_channel *video_src_chan);

/*!
 * \brief Set the bridge to pick the strongest talker supporting
 * video as the single source video feed
 */
void ast_bridge_set_talker_src_video_mode(struct ast_bridge *bridge);

/*!
 * \brief Update information about talker energy for talker src video mode.
 */
void ast_bridge_update_talker_src_video_mode(struct ast_bridge *bridge, struct ast_channel *chan, int talker_energy, int is_keyfame);

/*!
 * \brief Returns the number of video sources currently active in the bridge
 */
int ast_bridge_number_video_src(struct ast_bridge *bridge);

/*!
 * \brief Determine if a channel is a video src for the bridge
 *
 * \retval 0 Not a current video source of the bridge.
 * \retval None 0, is a video source of the bridge, The number
 *         returned represents the priority this video stream has
 *         on the bridge where 1 is the highest priority.
 */
int ast_bridge_is_video_src(struct ast_bridge *bridge, struct ast_channel *chan);

/*!
 * \brief remove a channel as a source of video for the bridge.
 */
void ast_bridge_remove_video_src(struct ast_bridge *bridge, struct ast_channel *chan);

/*!
 * \brief Set channel to goto specific location after the bridge.
 * \since 12.0.0
 *
 * \param chan Channel to setup after bridge goto location.
 * \param context Context to goto after bridge.
 * \param exten Exten to goto after bridge.
 * \param priority Priority to goto after bridge.
 *
 * \details Add a channel datastore to setup the goto location
 * when the channel leaves the bridge and run a PBX from there.
 *
 * \return Nothing
 */
void ast_after_bridge_set_goto(struct ast_channel *chan, const char *context, const char *exten, int priority);

/*!
 * \brief Set channel to run the h exten after the bridge.
 * \since 12.0.0
 *
 * \param chan Channel to setup after bridge goto location.
 * \param context Context to goto after bridge.
 *
 * \details Add a channel datastore to setup the goto location
 * when the channel leaves the bridge and run a PBX from there.
 *
 * \return Nothing
 */
void ast_after_bridge_set_h(struct ast_channel *chan, const char *context);

/*!
 * \brief Set channel to go on in the dialplan after the bridge.
 * \since 12.0.0
 *
 * \param chan Channel to setup after bridge goto location.
 * \param context Current context of the caller channel.
 * \param exten Current exten of the caller channel.
 * \param priority Current priority of the caller channel
 * \param parseable_goto User specified goto string from dialplan.
 *
 * \details Add a channel datastore to setup the goto location
 * when the channel leaves the bridge and run a PBX from there.
 *
 * If parseable_goto then use the given context/exten/priority
 *   as the relative position for the parseable_goto.
 * Else goto the given context/exten/priority+1.
 *
 * \return Nothing
 */
void ast_after_bridge_set_go_on(struct ast_channel *chan, const char *context, const char *exten, int priority, const char *parseable_goto);

/*!
 * \brief Setup any after bridge goto location to begin execution.
 * \since 12.0.0
 *
 * \param chan Channel to setup after bridge goto location.
 *
 * \details Pull off any after bridge goto location datastore and
 * setup for dialplan execution there.
 *
 * \retval 0 on success.  The goto location is set for a PBX to run it.
 * \retval non-zero on error or no goto location.
 *
 * \note If the after bridge goto is set to run an h exten it is
 * run here immediately.
 */
int ast_after_bridge_goto_setup(struct ast_channel *chan);

/*!
 * \brief Run a PBX on any after bridge goto location.
 * \since 12.0.0
 *
 * \param chan Channel to execute after bridge goto location.
 *
 * \details Pull off any after bridge goto location datastore
 * and run a PBX at that location.
 *
 * \note On return, the chan pointer is no longer valid because
 * the channel has hung up.
 *
 * \return Nothing
 */
void ast_after_bridge_goto_run(struct ast_channel *chan);

/*!
 * \brief Discard channel after bridge goto location.
 * \since 12.0.0
 *
 * \param chan Channel to discard after bridge goto location.
 *
 * \return Nothing
 */
void ast_after_bridge_goto_discard(struct ast_channel *chan);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif	/* _ASTERISK_BRIDGING_H */
