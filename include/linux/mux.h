/*
 * mux.h - definitions for the multiplexer interface
 *
 * Copyright (C) 2017 Axentia Technologies AB
 *
 * Author: Peter Rosin <peda@axentia.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _LINUX_MUX_H
#define _LINUX_MUX_H

#include <linux/device.h>
#include <linux/rwsem.h>

struct mux_chip;
struct mux_control;
struct platform_device;

/**
 * struct mux_control_ops -	Mux controller operations for a mux chip.
 * @set:			Set the state of the given mux controller.
 */
struct mux_control_ops {
	int (*set)(struct mux_control *mux, int state);
};

/* These defines match the constants from the dt-bindings. On purpose. */
#define MUX_IDLE_AS_IS      (-1)
#define MUX_IDLE_DISCONNECT (-2)

/**
 * struct mux_control -	Represents a mux controller.
 * @lock:		Protects the mux controller state.
 * @chip:		The mux chip that is handling this mux controller.
 * @states:		The number of mux controller states.
 * @cached_state:	The current mux controller state, or -1 if none.
 * @idle_state:		The mux controller state to use when inactive, or one
 *			of MUX_IDLE_AS_IS and MUX_IDLE_DISCONNECT.
 */
struct mux_control {
	struct rw_semaphore lock; /* protects the state of the mux */

	struct mux_chip *chip;

	unsigned int states;
	int cached_state;
	int idle_state;
};

/**
 * struct mux_chip -	Represents a chip holding mux controllers.
 * @controllers:	Number of mux controllers handled by the chip.
 * @mux:		Array of mux controllers that are handled.
 * @dev:		Device structure.
 * @id:			Used to identify the device internally.
 * @ops:		Mux controller operations.
 */
struct mux_chip {
	unsigned int controllers;
	struct mux_control *mux;
	struct device dev;
	int id;

	const struct mux_control_ops *ops;
};

#define to_mux_chip(x) container_of((x), struct mux_chip, dev)

/**
 * mux_chip_priv() - Get the extra memory reserved by mux_chip_alloc().
 * @mux_chip: The mux-chip to get the private memory from.
 *
 * Return: Pointer to the private memory reserved by the allocator.
 */
static inline void *mux_chip_priv(struct mux_chip *mux_chip)
{
	return &mux_chip->mux[mux_chip->controllers];
}

/**
 * mux_chip_alloc() - Allocate a mux-chip.
 * @dev: The parent device implementing the mux interface.
 * @controllers: The number of mux controllers to allocate for this chip.
 * @sizeof_priv: Size of extra memory area for private use by the caller.
 *
 * Return: A pointer to the new mux-chip, NULL on failure.
 */
struct mux_chip *mux_chip_alloc(struct device *dev,
				unsigned int controllers, size_t sizeof_priv);

/**
 * mux_chip_register() - Register a mux-chip, thus readying the controllers
 *			 for use.
 * @mux_chip: The mux-chip to register.
 *
 * Do not retry registration of the same mux-chip on failure. You should
 * instead put it away with mux_chip_free() and allocate a new one, if you
 * for some reason would like to retry registration.
 *
 * Return: Zero on success or a negative errno on error.
 */
int mux_chip_register(struct mux_chip *mux_chip);

/**
 * mux_chip_unregister() - Take the mux-chip off-line.
 * @mux_chip: The mux-chip to unregister.
 *
 * mux_chip_unregister() reverses the effects of mux_chip_register().
 * But not completely, you should not try to call mux_chip_register()
 * on a mux-chip that has been registered before.
 */
void mux_chip_unregister(struct mux_chip *mux_chip);

/**
 * mux_chip_free() - Free the mux-chip for good.
 * @mux_chip: The mux-chip to free.
 *
 * mux_chip_free() reverses the effects of mux_chip_alloc().
 */
void mux_chip_free(struct mux_chip *mux_chip);

/**
 * devm_mux_chip_alloc() - Resource-managed version of mux_chip_alloc().
 * @dev: The parent device implementing the mux interface.
 * @controllers: The number of mux controllers to allocate for this chip.
 * @sizeof_priv: Size of extra memory area for private use by the caller.
 *
 * See mux_chip_alloc() for more details.
 *
 * Return: A pointer to the new mux-chip, NULL on failure.
 */
struct mux_chip *devm_mux_chip_alloc(struct device *dev,
				     unsigned int controllers,
				     size_t sizeof_priv);

/**
 * devm_mux_chip_register() - Resource-managed version mux_chip_register().
 * @dev: The parent device implementing the mux interface.
 * @mux_chip: The mux-chip to register.
 *
 * See mux_chip_register() for more details.
 *
 * Return: Zero on success or a negative errno on error.
 */
int devm_mux_chip_register(struct device *dev, struct mux_chip *mux_chip);

/**
 * devm_mux_chip_unregister() - Resource-managed version mux_chip_unregister().
 * @dev: The device that originally registered the mux-chip.
 * @mux_chip: The mux-chip to unregister.
 *
 * See mux_chip_unregister() for more details.
 *
 * Note that you do not normally need to call this function.
 */
void devm_mux_chip_unregister(struct device *dev, struct mux_chip *mux_chip);

/**
 * devm_mux_chip_free() - Resource-managed version mux_chip_free().
 * @dev: The device that originally got the mux-chip.
 * @mux_chip: The mux-chip to free.
 *
 * See mux_chip_free() for more details.
 *
 * Note that you do not normally need to call this function.
 */
void devm_mux_chip_free(struct device *dev, struct mux_chip *mux_chip);

/**
 * mux_control_select() - Select the given multiplexer state.
 * @mux: The mux-control to request a change of state from.
 * @state: The new requested state.
 *
 * Make sure to call mux_control_deselect() when the operation is complete and
 * the mux-control is free for others to use, but do not call
 * mux_control_deselect() if mux_control_select() fails.
 *
 * Return: 0 if the requested state was already active, or 1 it the
 * mux-control state was changed to the requested state. Or a negative
 * errno on error.
 *
 * Note that the difference in return value of zero or one is of
 * questionable value; especially if the mux-control has several independent
 * consumers, which is something the consumers should perhaps not be making
 * assumptions about.
 */
int mux_control_select(struct mux_control *mux, int state);

/**
 * mux_control_deselect() - Deselect the previously selected multiplexer state.
 * @mux: The mux-control to deselect.
 *
 * Return: 0 on success and a negative errno on error. An error can only
 * occur if the mux has an idle state. Note that even if an error occurs, the
 * mux-control is unlocked and is thus free for the next access.
 */
int mux_control_deselect(struct mux_control *mux);

/**
 * mux_control_get_index() - Get the index of the given mux controller
 * @mux: The mux-control to get the index for.
 *
 * Return: The index of the mux controller within the mux chip the mux
 * controller is a part of.
 */
static inline unsigned int mux_control_get_index(struct mux_control *mux)
{
	return mux - mux->chip->mux;
}

/**
 * mux_control_get() - Get the mux-control for a device.
 * @dev: The device that needs a mux-control.
 * @mux_name: The name identifying the mux-control.
 *
 * Return: A pointer to the mux-control, or an ERR_PTR with a negative errno.
 */
struct mux_control *mux_control_get(struct device *dev, const char *mux_name);

/**
 * mux_control_put() - Put away the mux-control for good.
 * @mux: The mux-control to put away.
 *
 * mux_control_put() reverses the effects of mux_control_get().
 */
void mux_control_put(struct mux_control *mux);

/**
 * devm_mux_control_get() - Get the mux-control for a device, with resource
 *			    management.
 * @dev: The device that needs a mux-control.
 * @mux_name: The name identifying the mux-control.
 *
 * Return: Pointer to the mux-control, or an ERR_PTR with a negative errno.
 */
struct mux_control *devm_mux_control_get(struct device *dev,
					 const char *mux_name);

/**
 * devm_mux_control_put() - Resource-managed version mux_control_put().
 * @dev: The device that originally got the mux-control.
 * @mux: The mux-control to put away.
 *
 * Note that you do not normally need to call this function.
 */
void devm_mux_control_put(struct device *dev, struct mux_control *mux);

#endif /* _LINUX_MUX_H */
