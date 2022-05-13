/*
 * Dummy power supply driver for android
 *
 * Copyright (C) 2017 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/of.h>

#define DUMMY_POWER_NUM 2
#define POWER_SUPPLY_PROP_CAPACITY_VALUE 85
#define POWER_SUPPLY_PROP_VOLTAGE_NOW_VALUE 3600
#define POWER_SUPPLY_PROP_CURRENT_NOW_VALUE 400000
#define POWER_SUPPLY_PROP_CYCLE_COUNT_VALUE 32
#define POWER_SUPPLY_PROP_TEMP_VALUE 350
#define POWER_SUPPLY_PROP_CHARGE_FULL_VALUE 4000000
#define POWER_SUPPLY_PROP_CHARGE_COUNTER_VALUE 1900000
#define POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX_VALUE 500000
#define POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX_VALUE 5000000

struct dummy_usb_property{
    int online;
};
struct dummy_usb_property *usb_property;

struct dummy_battery_property{
    int status;
    int health;
    int present;
    int technology;
    int capacity;
    int voltage_now;
    int current_now;
    int cycle_count;
    int temp;
    int charge_full;
    int charge_counter;
    int constant_charge_current_max;
    int constant_charge_voltage_max;
    int online;
    struct i2c_adapter *i2c_parent;
    int i2c_batt_address;
    int i2c_batt_register;
    int i2c_batt_voltage_min;
    int i2c_batt_voltage_max;
    int i2c_batt_voltage_mult;
    int i2c_batt_voltage_div;
    int old_capacity;
};
struct dummy_battery_property *battery_property;

static enum power_supply_property dummy_battery_props[] = {
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_HEALTH,
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_TECHNOLOGY,
    POWER_SUPPLY_PROP_CAPACITY,
    POWER_SUPPLY_PROP_VOLTAGE_NOW,
    POWER_SUPPLY_PROP_CURRENT_NOW,
    POWER_SUPPLY_PROP_CYCLE_COUNT,
    POWER_SUPPLY_PROP_TEMP,
    POWER_SUPPLY_PROP_CHARGE_FULL,
    POWER_SUPPLY_PROP_CHARGE_COUNTER,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
    POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property dummy_usb_props[] = {
    POWER_SUPPLY_PROP_ONLINE,
};

static int dummy_usb_get_property(struct power_supply *psy,
                                  enum power_supply_property psp,
                                  union power_supply_propval *val)
{
    int ret = 0;
    switch (psp) {
        case POWER_SUPPLY_PROP_ONLINE:
            val->intval = usb_property->online;
            break;
        default:
            ret = -EINVAL;
            break;
    }
    return ret;
}

static int dummy_usb_set_property(struct power_supply *psy,
                                  enum power_supply_property psp,
                                  const union power_supply_propval *val)
{
    int ret = 0;
    switch (psp) {
        case POWER_SUPPLY_PROP_ONLINE:
            usb_property->online = val->intval;
            break;
        default:
            ret = -EINVAL;
            break;
    }
    return ret;
}

static int dummy_usb_property_is_writeable(struct power_supply *psy,
                                  enum power_supply_property psp)
{
    return psp == POWER_SUPPLY_PROP_ONLINE;
}

static int dummy_i2c_read_voltage(int *voltage)
{
    u8 buf[2];
    struct i2c_msg msgs[2];
    int ret;

    buf[0] = battery_property->i2c_batt_register & 0xFF;

    msgs[0].addr = battery_property->i2c_batt_address;
    msgs[0].flags = 0;
    msgs[0].len = 1;
    msgs[0].buf = &buf;

    msgs[1].addr = battery_property->i2c_batt_address;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = 2;
    msgs[1].buf = &buf;

    ret = i2c_transfer(battery_property->i2c_parent, msgs, 2);
    if (ret >= 0) {
        ret = 0;
        *voltage = (buf[1] | (buf[0] << 8)) & 0xFFF;
        *voltage *= battery_property->i2c_batt_voltage_mult;
        *voltage /= battery_property->i2c_batt_voltage_div;
    }

    return ret;
}

static int dummy_battery_get_property(struct power_supply *psy,
                                      enum power_supply_property psp,
                                      union power_supply_propval *val)
{
    int ret = 0;
    switch (psp) {
        case POWER_SUPPLY_PROP_STATUS:
            val->intval = battery_property->status;
            break;
        case POWER_SUPPLY_PROP_HEALTH:
            val->intval = POWER_SUPPLY_HEALTH_GOOD;
            break;
        case POWER_SUPPLY_PROP_PRESENT:
            val->intval = true;
            break;
        case POWER_SUPPLY_PROP_TECHNOLOGY:
            val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
            break;
        case POWER_SUPPLY_PROP_CAPACITY:
            val->intval = POWER_SUPPLY_PROP_CAPACITY_VALUE;
            if (battery_property->i2c_parent) {
                int voltage;

                ret = dummy_i2c_read_voltage(&voltage);
                if (!ret) {
                    int delta;
                    int delta_max = battery_property->i2c_batt_voltage_max -
                        battery_property->i2c_batt_voltage_min;

                    if (voltage > battery_property->i2c_batt_voltage_max)
                        voltage = battery_property->i2c_batt_voltage_max;
                    if (voltage < battery_property->i2c_batt_voltage_min)
                        voltage = battery_property->i2c_batt_voltage_min;

                    delta = voltage - battery_property->i2c_batt_voltage_min;

                    val->intval = delta * 100 / delta_max;
                }
                if (val->intval > battery_property->old_capacity) {
                    battery_property->old_capacity = val->intval;
                    battery_property->status = POWER_SUPPLY_STATUS_CHARGING;
                    usb_property->online = 1;
                } else if (val->intval < battery_property->old_capacity) {
                    battery_property->old_capacity = val->intval;
                    battery_property->status = POWER_SUPPLY_STATUS_DISCHARGING;
                    usb_property->online = 0;
                }
            }
            break;
        case POWER_SUPPLY_PROP_VOLTAGE_NOW:
            val->intval = POWER_SUPPLY_PROP_VOLTAGE_NOW_VALUE;
            if (battery_property->i2c_parent) {
                ret = dummy_i2c_read_voltage(&val->intval);
            }
            break;
        case POWER_SUPPLY_PROP_CURRENT_NOW:
            val->intval = POWER_SUPPLY_PROP_CURRENT_NOW_VALUE;
            break;
        case POWER_SUPPLY_PROP_CYCLE_COUNT:
            val->intval = POWER_SUPPLY_PROP_CYCLE_COUNT_VALUE;
            break;
        case POWER_SUPPLY_PROP_TEMP:
            val->intval = POWER_SUPPLY_PROP_TEMP_VALUE;
            break;
        case POWER_SUPPLY_PROP_CHARGE_FULL:
            val->intval = POWER_SUPPLY_PROP_CHARGE_FULL_VALUE;
            break;
        case POWER_SUPPLY_PROP_CHARGE_COUNTER:
            val->intval = POWER_SUPPLY_PROP_CHARGE_COUNTER_VALUE;
            break;
        case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
            val->intval = POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX_VALUE;
            break;
        case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
            val->intval = POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX_VALUE;
            break;
        case POWER_SUPPLY_PROP_ONLINE:
            val->intval = battery_property->online;
            break;
        default:
            ret = -EINVAL;
            break;
    }
    return ret;
}

static int dummy_battery_set_property(struct power_supply *psy,
                                  enum power_supply_property psp,
                                  const union power_supply_propval *val)
{
    int ret = 0;
    switch (psp) {
        case POWER_SUPPLY_PROP_ONLINE:
            battery_property->online = val->intval;
            break;
        default:
            ret = -EINVAL;
            break;
    }
    return ret;
}

static int dummy_battery_property_is_writeable(struct power_supply *psy,
                                  enum power_supply_property psp)
{
    return psp == POWER_SUPPLY_PROP_ONLINE;
}

static struct power_supply *dummy_power_supplies[DUMMY_POWER_NUM];
static const struct power_supply_desc dummy_power_desc[] = {
    {
        .properties = dummy_battery_props,
        .num_properties = ARRAY_SIZE(dummy_battery_props),
        .set_property = dummy_battery_set_property,
        .get_property = dummy_battery_get_property,
        .name = "battery",
        .property_is_writeable = dummy_battery_property_is_writeable,
        .type = POWER_SUPPLY_TYPE_BATTERY,
    },
    {
        .properties = dummy_usb_props,
        .num_properties = ARRAY_SIZE(dummy_usb_props),
        .set_property = dummy_usb_set_property,
        .get_property = dummy_usb_get_property,
        .name = "usb",
        .property_is_writeable = dummy_usb_property_is_writeable,
        .type = POWER_SUPPLY_TYPE_MAINS,
    }
};

static int __init dummy_power_init(void)
{
    struct device_node *np, *parent_np;
    struct power_supply_config psy_cfg = {};
    int i;
    int ret;

    usb_property = (struct dummy_usb_property*)kmalloc(sizeof(struct dummy_usb_property*),GFP_KERNEL);
    usb_property->online = 1;

    battery_property = (struct dummy_battery_property*)kmalloc(sizeof(struct dummy_battery_property*),GFP_KERNEL);
    battery_property->online = 1;
    battery_property->i2c_parent = NULL;
    battery_property->status = POWER_SUPPLY_STATUS_CHARGING;

    for (i = 0; i < ARRAY_SIZE(dummy_power_supplies); i++) {
        dummy_power_supplies[i] = power_supply_register(NULL,
                                                        &dummy_power_desc[i],
                                                        &psy_cfg);
        if (IS_ERR(dummy_power_supplies[i])) {
            ret = PTR_ERR(dummy_power_supplies[i]);
            goto failed;
        }
    }

    np = of_find_node_by_name(NULL, "dummy-battery");
    if (np == NULL)
        return 0;

    parent_np = of_parse_phandle(np, "i2c-parent", 0);
    if (parent_np == NULL) {
        of_node_put(np);
        return 0;
    }

    battery_property->i2c_parent = of_find_i2c_adapter_by_node(parent_np);
    of_node_put(parent_np);
    if (!battery_property->i2c_parent) {
        pr_info("%s: i2c entry present but not found!", __func__);
        of_node_put(np);
        return 0;
    }

    ret = of_property_read_u32(np, "i2c-batt-address",
                               &battery_property->i2c_batt_address);
    ret |= of_property_read_u32(np, "i2c-batt-register",
                                &battery_property->i2c_batt_register);
    ret |= of_property_read_u32(np, "i2c-batt-voltage-min",
                                &battery_property->i2c_batt_voltage_min);
    ret |= of_property_read_u32(np, "i2c-batt-voltage-max",
                                &battery_property->i2c_batt_voltage_max);
    ret |= of_property_read_u32(np, "i2c-batt-voltage-mult",
                                &battery_property->i2c_batt_voltage_mult);
    ret |= of_property_read_u32(np, "i2c-batt-voltage-div",
                                &battery_property->i2c_batt_voltage_div);
    if (ret) {
        pr_info("%s: one of the parameters is missing!", __func__);
        battery_property->i2c_parent = NULL;
    } else {
        /* a battery is present so we consider it discharging */
        usb_property->online = 0;
        battery_property->status = POWER_SUPPLY_STATUS_DISCHARGING;
        battery_property->old_capacity = 100;
    }

    of_node_put(np);

    return 0;
failed:
    while (--i >= 0)
        power_supply_unregister(dummy_power_supplies[i]);
    return ret;
}

static void __exit dummy_power_exit(void)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(dummy_power_supplies); i++) {
        power_supply_unregister(dummy_power_supplies[i]);
    }
    kfree(usb_property);
    kfree(battery_property);
}

module_init(dummy_power_init);
module_exit(dummy_power_exit);

MODULE_DESCRIPTION("Power supply driver for Android devices with no battery");
MODULE_AUTHOR("Dave Rim davidrim@google.com");
MODULE_LICENSE("GPL");
