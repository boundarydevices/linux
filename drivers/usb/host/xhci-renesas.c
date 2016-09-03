/*
 * xHCI host controller firmware loader for Renesas uPD720201/uPD720202
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/usb/phy.h>
#include <asm/unaligned.h>

#include "xhci.h"

/* Renesas uPD720201/uPD720202 firmware loader */

static const struct renesas_fw_entry {
	const char *firmware_name;
	u16 device;
	u8 revision;
	u16 expected_version;
} renesas_fw_table[] = {
	/*
	 * Only the uPD720201K8-711-BAC-A or uPD720202K8-711-BAA-A
	 * are listed in R19UH0078EJ0500 Rev.5.00 as devices which
	 * need the software loader.
	 *
	 * PP2U/ReleaseNote_USB3-201-202-FW.txt:
	 *
	 * Note: This firmware is for the following devices.
	 *	- uPD720201 ES 2.0 sample whose revision ID is 2.
	 *	- uPD720201 ES 2.1 sample & CS sample & Mass product, ID is 3.
	 *	- uPD720202 ES 2.0 sample & CS sample & Mass product, ID is 2.
	 */
	{ "K2013080.mem", 0x0014, 0x02, 0x2013 },
	{ "K2013080.mem", 0x0014, 0x03, 0x2013 },
	{ "K2013080.mem", 0x0015, 0x02, 0x2013 },
};

static const struct renesas_fw_entry *renesas_needs_fw_dl(struct pci_dev *dev)
{
	const struct renesas_fw_entry *entry;
	size_t i;

	/* This loader will only work with a RENESAS device. */
	if (!(dev->vendor == PCI_VENDOR_ID_RENESAS))
		return NULL;

	for (i = 0; i < ARRAY_SIZE(renesas_fw_table); i++) {
		entry = &renesas_fw_table[i];
		if (entry->device == dev->device &&
			entry->revision == dev->revision)
			return entry;
	}

	return NULL;
}

static int renesas_fw_download_image(struct pci_dev *dev,
					const u32 *fw,
					size_t step)
{
	size_t i;
	int err;
	u8 fw_status;
	bool data0_or_data1;

	/*
	 * The hardware does alternate between two 32-bit pages.
	 * (This is because each row of the firmware is 8 bytes).
	 *
	 * for even steps we use DATA0, for odd steps DATA1.
	 */
	data0_or_data1 = (step & 1) == 1;

	/* step+1. Read "Set DATAX" and confirm it is cleared. */
	for (i = 0; i < 10000; i++) {
		err = pci_read_config_byte(dev, 0xF5, &fw_status);
		if (err)
			return pcibios_err_to_errno(err);
		if (!(fw_status & BIT(data0_or_data1)))
			break;

		udelay(1);
	}
	if (i == 10000)
		return -ETIMEDOUT;

	/*
	 * step+2. Write FW data to "DATAX".
	 * "LSB is left" => force little endian
	 */
	err = pci_write_config_dword(dev, data0_or_data1 ? 0xFC : 0xF8,
					(__force u32) cpu_to_le32(fw[step]));
	if (err)
		return pcibios_err_to_errno(err);

	/* step+3. Set "Set DATAX". */
	err = pci_write_config_byte(dev, 0xF5, BIT(data0_or_data1));
	if (err)
		return pcibios_err_to_errno(err);

	return 0;
}

static int renesas_fw_verify(struct pci_dev *dev,
				const void *fw_data,
				size_t length)
{
	const struct renesas_fw_entry *entry = renesas_needs_fw_dl(dev);
	u16 fw_version_pointer;
	u16 fw_version;

	if (!entry)
		return -EINVAL;

	/*
	 * The Firmware's Data Format is describe in
	 * "6.3 Data Format" R19UH0078EJ0500 Rev.5.00 page 124
	 */

	/* "Each row is 8 bytes". => firmware size must be a multiple of 8. */
	if (length % 8 != 0) {
		dev_err(&dev->dev, "firmware size is not a multipe of 8.");
		return -EINVAL;
	}

	/*
	 * The bootrom chips of the big brother have sizes up to 64k.
	 * This is as big as the firmware can get.
	 */
	if (length < 0x1000 || length >= 0x10000) {
		dev_err(&dev->dev, "firmware is size %zd is not between 4k and 64k.",
			length);
		return -EINVAL;
	}

	/* The First 2 bytes are fixed value (55aa). "LSB on Left" */
	if (get_unaligned_le16(fw_data) != 0x55aa) {
		dev_err(&dev->dev, "no valid firmware header found.");
		return -EINVAL;
	}

	/* verify the firmware version position and print it. */
	fw_version_pointer = get_unaligned_le16(fw_data + 4);
	if (fw_version_pointer + 2 >= length) {
		dev_err(&dev->dev, "firmware version pointer is outside of the firmware image.");
		return -EINVAL;
	}

	fw_version = get_unaligned_le16(fw_data + fw_version_pointer);
	dev_dbg(&dev->dev, "got firmware version: %02x.", fw_version);

	if (fw_version != entry->expected_version) {
		dev_err(&dev->dev, "firmware version mismatch, expected version: %02x.",
			 entry->expected_version);
		return -EINVAL;
	}

	return 0;
}

static int renesas_fw_check_running(struct pci_dev *pdev)
{
	int err;
	u8 fw_state;

	/*
	 * Test if the device is actually needing the firmware. As most
	 * BIOSes will initialize the device for us. If the device is
	 * initialized.
	 */
	err = pci_read_config_byte(pdev, 0xF4, &fw_state);
	if (err)
		return pcibios_err_to_errno(err);

	/*
	 * Check if "FW Download Lock" is locked. If it is and the FW is
	 * ready we can simply continue. If the FW is not ready, we have
	 * to give up.
	 */
	if (fw_state & BIT(1)) {
		dev_dbg(&pdev->dev, "FW Download Lock is engaged.");

		if (fw_state & BIT(4))
			return 0;

		dev_err(&pdev->dev, "FW Download Lock is set and FW is not ready. Giving Up.");
		return -EIO;
	}

	/*
	 * Check if "FW Download Enable" is set. If someone (us?) tampered
	 * with it and it can't be resetted, we have to give up too... and
	 * ask for a forgiveness and a reboot.
	 */
	if (fw_state & BIT(0)) {
		dev_err(&pdev->dev, "FW Download Enable is stale. Giving Up (poweroff/reboot needed).");
		return -EIO;
	}

	/* Otherwise, Check the "Result Code" Bits (6:4) and act accordingly */
	switch ((fw_state & 0x70)) {
	case 0: /* No result yet */
		dev_dbg(&pdev->dev, "FW is not ready/loaded yet.");

		/* tell the caller, that this device needs the firmware. */
		return 1;

	case BIT(4): /* Success, device should be working. */
		dev_dbg(&pdev->dev, "FW is ready.");
		return 0;

	case BIT(5): /* Error State */
		dev_err(&pdev->dev, "hardware is in an error state. Giving up (poweroff/reboot needed).");
		return -ENODEV;

	default: /* All other states are marked as "Reserved states" */
		dev_err(&pdev->dev, "hardware is in an invalid state %x. Giving up (poweroff/reboot needed).",
			(fw_state & 0x70) >> 4);
		return -EINVAL;
	}
}

static int renesas_fw_download(struct pci_dev *pdev,
	const struct firmware *fw, unsigned int retry_counter)
{
	const u32 *fw_data = (const u32 *) fw->data;
	size_t i;
	int err;
	u8 fw_status;

	/*
	 * For more information and the big picture: please look at the
	 * "Firmware Download Sequence" in "7.1 FW Download Interface"
	 * of R19UH0078EJ0500 Rev.5.00 page 131
	 *
	 * 0. Set "FW Download Enable" bit in the
	 * "FW Download Control & Status Register" at 0xF4
	 */
	err = pci_write_config_byte(pdev, 0xF4, BIT(0));
	if (err)
		return pcibios_err_to_errno(err);

	/* 1 - 10 follow one step after the other. */
	for (i = 0; i < fw->size / 4; i++) {
		err = renesas_fw_download_image(pdev, fw_data, i);
		if (err) {
			dev_err(&pdev->dev, "Firmware Download Step %zd failed at position %zd bytes with (%d).",
				 i, i * 4, err);
			return err;
		}
	}

	/*
	 * This sequence continues until the last data is written to
	 * "DATA0" or "DATA1". Naturally, we wait until "SET DATA0/1"
	 * is cleared by the hardware beforehand.
	 */
	for (i = 0; i < 10000; i++) {
		err = pci_read_config_byte(pdev, 0xF5, &fw_status);
		if (err)
			return pcibios_err_to_errno(err);
		if (!(fw_status & (BIT(0) | BIT(1))))
			break;

		udelay(1);
	}
	if (i == 10000)
		dev_warn(&pdev->dev, "Final Firmware Download step timed out.");

	/*
	 * 11. After finishing writing the last data of FW, the
	 * System Software must clear "FW Download Enable"
	 */
	err = pci_write_config_byte(pdev, 0xF4, 0);
	if (err)
		return pcibios_err_to_errno(err);

	/* 12. Read "Result Code" and confirm it is good. */
	for (i = 0; i < 10000; i++) {
		err = pci_read_config_byte(pdev, 0xF4, &fw_status);
		if (err)
			return pcibios_err_to_errno(err);
		if (fw_status & BIT(4))
			break;

		udelay(1);
	}
	if (i == 10000) {
		/* Timed out / Error - let's see if we can fix this */
		err = renesas_fw_check_running(pdev);
		switch (err) {
		case 0: /*
			 * we shouldn't end up here.
			 * maybe it took a little bit longer.
			 * But all should be well?
			 */
			break;

		case 1: /* (No result yet? - we can try to retry) */
			if (retry_counter < 10) {
				retry_counter++;
				dev_warn(&pdev->dev, "Retry Firmware download: %d try.",
						retry_counter);
				return renesas_fw_download(pdev, fw,
								retry_counter);
			}
			return -ETIMEDOUT;

		default:
			return err;
		}
	}
	/*
	 * Optional last step: Engage Firmware Lock
	 *
	 * err = pci_write_config_byte(pdev, 0xF4, BIT(2));
	 * if (err)
	 *	return pcibios_err_to_errno(err);
	 */

	return 0;
}

int renesas_check_if_fw_dl_is_needed(struct pci_dev *pdev)
{
	const struct renesas_fw_entry *entry = renesas_needs_fw_dl(pdev);
	const struct firmware *fw = NULL;
	int err;

	if (!entry)
		return 0;

	err = renesas_fw_check_running(pdev);
	/* Continue ahead, if the firmware is already running. */
	if (err <= 0)
		return err;

	err = request_firmware(&fw, entry->firmware_name, &pdev->dev);
	if (err) {
		dev_err(&pdev->dev, "firmware request for %s failed (%d).", entry->firmware_name, err);
		return err;
	}

	err = renesas_fw_verify(pdev, fw->data, fw->size);
	if (err)
		goto free_fw;

	err = renesas_fw_download(pdev, fw, 0);
	if (err) {
		dev_err(&pdev->dev, "firmware failed to download (%d).", err);
		goto free_fw;
	}

 free_fw:
	release_firmware(fw);
	return err;
}

EXPORT_SYMBOL_GPL(renesas_check_if_fw_dl_is_needed);

MODULE_DESCRIPTION("xHCI PCI Renesas firmware loader");
MODULE_LICENSE("GPL");
