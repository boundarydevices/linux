/*
 * drivers/amlogic/ddr_tool/ddr_port_desc.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/aml_ddr_bandwidth.h>
#include <linux/io.h>
#include <linux/slab.h>

/*
 * NOTE:
 * Port "DEVICE" is total name for small bandwidth device.
 * There are many small bandwidth devices such as nand/arb/parser
 * connected to dmc under port "device", for better configure of
 * these devices, re-number them with start ID of 32
 *
 * EXAMPLE:
 *
 *            DMC CONTROLLER
 *                   |
 *   ---------------------------------
 *   |    |    | ..... |       | ... |
 *  arm  mali vpu    device   vdec  hevc
 *                     |
 *         ------------------------
 *         |    |     | ... |     |
 *       emmc  ge2d  usb  audio  spicc
 */
static struct ddr_port_desc ddr_port_desc_m8b[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "MALI0"         },
	{ .port_id =  2, .port_name = "MALI1"         },
	{ .port_id =  3, .port_name = "HEVC"          },
	{ .port_id =  8, .port_name = "VPU1"          },
	{ .port_id =  9, .port_name = "VPU2"          },
	{ .port_id = 10, .port_name = "VPU3"          },
	{ .port_id = 11, .port_name = "VDEC"          },
	{ .port_id = 12, .port_name = "HCODEC"        },
	{ .port_id = 14, .port_name = "AUDIO"         },
	{ .port_id = 15, .port_name = "DEVICE"        },
	/* start of each device */
	{ .port_id = 33, .port_name = "NAND"          },
	{ .port_id = 34, .port_name = "BLKMV"         },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 36, .port_name = "USB0"          },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "GE2D SOURCE1"  },
	{ .port_id = 39, .port_name = "GE2D SOURCE2"  },
	{ .port_id = 40, .port_name = "GE2D DEST"     },
	{ .port_id = 41, .port_name = "PASER"         },
	{ .port_id = 42, .port_name = "SANA"          },
	{ .port_id = 43, .port_name = "SDIO2"         },
	{ .port_id = 44, .port_name = "SPICC"         },
	{ .port_id = 45, .port_name = "ETHERNET"      }
};

static struct ddr_port_desc ddr_port_desc_gxbb[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "MALI0"         },
	{ .port_id =  2, .port_name = "MALI1"         },
	{ .port_id =  3, .port_name = "HDCP"          },
	{ .port_id =  4, .port_name = "HEVC"          },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id =  8, .port_name = "VPU READ1"     },
	{ .port_id =  9, .port_name = "VPU READ2"     },
	{ .port_id = 10, .port_name = "VPU READ3"     },
	{ .port_id = 11, .port_name = "VPU WRITE1"    },
	{ .port_id = 12, .port_name = "VPU WRITE2"    },
	{ .port_id = 13, .port_name = "VDEC"          },
	{ .port_id = 14, .port_name = "HCODEC"        },
	{ .port_id = 15, .port_name = "GE2D"          },
	/* start of each device */
	{ .port_id = 32, .port_name = "SD_EMMC_A"     },
	{ .port_id = 33, .port_name = "USB0"          },
	{ .port_id = 34, .port_name = "BLKMV"         },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 36, .port_name = "SD_EMMC_B"     },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "AUDIO OUT"     },
	{ .port_id = 39, .port_name = "AUDIO IN"      },
	{ .port_id = 40, .port_name = "AIU"           },
	{ .port_id = 41, .port_name = "PASER"         },
	{ .port_id = 42, .port_name = "AO CPU"        },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC"         },
	{ .port_id = 45, .port_name = "ETHERNET"      },
	{ .port_id = 46, .port_name = "SANA"          }
};

static struct ddr_port_desc ddr_port_desc_gxl[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "MALI0"         },
	{ .port_id =  2, .port_name = "MALI1"         },
	{ .port_id =  3, .port_name = "HDCP"          },
	{ .port_id =  4, .port_name = "HEVC"          },
	{ .port_id =  5, .port_name = "TEST"          },
	{ .port_id =  6, .port_name = "USB3.0"        },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id =  8, .port_name = "VPU READ1"     },
	{ .port_id =  9, .port_name = "VPU READ2"     },
	{ .port_id = 10, .port_name = "VPU READ3"     },
	{ .port_id = 11, .port_name = "VPU WRITE1"    },
	{ .port_id = 12, .port_name = "VPU WRITE2"    },
	{ .port_id = 13, .port_name = "VDEC"          },
	{ .port_id = 14, .port_name = "HCODEC"        },
	{ .port_id = 15, .port_name = "GE2D"          },
	/* start of each device */
	{ .port_id = 32, .port_name = "SD_EMMC_A"     },
	{ .port_id = 33, .port_name = "USB0"          },
	{ .port_id = 34, .port_name = "DMA"           },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 36, .port_name = "SD_EMMC_B"     },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "AUDIO OUT"     },
	{ .port_id = 39, .port_name = "AUDIO IN"      },
	{ .port_id = 40, .port_name = "AIU"           },
	{ .port_id = 41, .port_name = "PASER"         },
	{ .port_id = 42, .port_name = "AO CPU"        },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC"         },
	{ .port_id = 45, .port_name = "ETHERNET"      },
	{ .port_id = 46, .port_name = "SANA"          }
};

static struct ddr_port_desc ddr_port_desc_gxm[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "MALI"          },
	{ .port_id =  2, .port_name = "H265ENC"       },
	{ .port_id =  3, .port_name = "HDCP"          },
	{ .port_id =  4, .port_name = "HEVC"          },
	{ .port_id =  5, .port_name = "TEST"          },
	{ .port_id =  6, .port_name = "USB3.0"        },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id =  8, .port_name = "VPU READ1"     },
	{ .port_id =  9, .port_name = "VPU READ2"     },
	{ .port_id = 10, .port_name = "VPU READ3"     },
	{ .port_id = 11, .port_name = "VPU WRITE1"    },
	{ .port_id = 12, .port_name = "VPU WRITE2"    },
	{ .port_id = 13, .port_name = "VDEC"          },
	{ .port_id = 14, .port_name = "HCODEC"        },
	{ .port_id = 15, .port_name = "GE2D"          },
	/* start of each device */
	{ .port_id = 32, .port_name = "SD_EMMC_A"     },
	{ .port_id = 33, .port_name = "USB0"          },
	{ .port_id = 34, .port_name = "DMA"           },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 36, .port_name = "SD_EMMC_B"     },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "AUDIO OUT"     },
	{ .port_id = 39, .port_name = "AUDIO IN"      },
	{ .port_id = 40, .port_name = "AIU"           },
	{ .port_id = 41, .port_name = "PASER"         },
	{ .port_id = 42, .port_name = "AO CPU"        },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC"         },
	{ .port_id = 45, .port_name = "ETHERNET"      },
	{ .port_id = 46, .port_name = "SANA"          }
};

static struct ddr_port_desc ddr_port_desc_gxlx[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "MALI0"         },
	{ .port_id =  2, .port_name = "MALI1"         },
	{ .port_id =  3, .port_name = "HDCP"          },
	{ .port_id =  4, .port_name = "HEVC"          },
	{ .port_id =  5, .port_name = "H265ENC/TEST"  },
	{ .port_id =  6, .port_name = "USB3.0"        },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id =  8, .port_name = "VPU READ1"     },
	{ .port_id =  9, .port_name = "VPU READ2"     },
	{ .port_id = 10, .port_name = "VPU READ3"     },
	{ .port_id = 11, .port_name = "VPU WRITE1"    },
	{ .port_id = 12, .port_name = "VPU WRITE2"    },
	{ .port_id = 13, .port_name = "VDEC"          },
	{ .port_id = 14, .port_name = "HCODEC"        },
	{ .port_id = 15, .port_name = "GE2D"          },
	/* start of each device */
	{ .port_id = 32, .port_name = "SD_EMMC_A"     },
	{ .port_id = 33, .port_name = "USB0"          },
	{ .port_id = 34, .port_name = "DMA"           },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 36, .port_name = "SD_EMMC_B"     },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "AUDIO OUT"     },
	{ .port_id = 39, .port_name = "AUDIO IN"      },
	{ .port_id = 40, .port_name = "AIU"           },
	{ .port_id = 41, .port_name = "PASER"         },
	{ .port_id = 42, .port_name = "AO CPU"        },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC"         },
	{ .port_id = 45, .port_name = "ETHERNET"      },
	{ .port_id = 46, .port_name = "SANA"          }
};

static struct ddr_port_desc ddr_port_desc_g12a[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "MALI"          },
	{ .port_id =  2, .port_name = "PCIE"          },
	{ .port_id =  3, .port_name = "HDCP"          },
	{ .port_id =  4, .port_name = "HEVC FRONT"    },
	{ .port_id =  5, .port_name = "TEST"          },
	{ .port_id =  6, .port_name = "USB3.0"        },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id =  8, .port_name = "HEVC BACK"     },
	{ .port_id =  9, .port_name = "H265ENC"       },
	{ .port_id = 16, .port_name = "VPU READ1"     },
	{ .port_id = 17, .port_name = "VPU READ2"     },
	{ .port_id = 18, .port_name = "VPU READ3"     },
	{ .port_id = 19, .port_name = "VPU WRITE1"    },
	{ .port_id = 20, .port_name = "VPU WRITE2"    },
	{ .port_id = 21, .port_name = "VDEC"          },
	{ .port_id = 22, .port_name = "HCODEC"        },
	{ .port_id = 23, .port_name = "GE2D"          },
	/* start of each device */
	{ .port_id = 32, .port_name = "SPICC1"        },
	{ .port_id = 33, .port_name = "USB0"          },
	{ .port_id = 34, .port_name = "DMA"           },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 36, .port_name = "SD_EMMC_B"     },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "AUDIO"         },
	{ .port_id = 39, .port_name = "AIFIFO"        },
	{ .port_id = 41, .port_name = "PASER"         },
	{ .port_id = 42, .port_name = "AO CPU"        },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC2"        },
	{ .port_id = 45, .port_name = "ETHERNET"      },
	{ .port_id = 46, .port_name = "SANA"          }
};

static struct ddr_port_desc ddr_port_desc_g12b[] __initdata = {
	{ .port_id =  0, .port_name = "ARM-ADDR-E"    },
	{ .port_id =  1, .port_name = "MALI"          },
	{ .port_id =  2, .port_name = "PCIE"          },
	{ .port_id =  3, .port_name = "HDCP"          },
	{ .port_id =  4, .port_name = "HEVC FRONT"    },
	{ .port_id =  5, .port_name = "TEST"          },
	{ .port_id =  6, .port_name = "USB3.0"        },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id =  8, .port_name = "HEVC BACK"     },
	{ .port_id =  9, .port_name = "H265ENC"       },
	{ .port_id = 10, .port_name = "NNA"           },
	{ .port_id = 11, .port_name = "GDC"           },
	{ .port_id = 12, .port_name = "MIPI ISP"      },
	{ .port_id = 13, .port_name = "ARM-ADDR-F"    },
	{ .port_id = 16, .port_name = "VPU READ1"     },
	{ .port_id = 17, .port_name = "VPU READ2"     },
	{ .port_id = 18, .port_name = "VPU READ3"     },
	{ .port_id = 19, .port_name = "VPU WRITE1"    },
	{ .port_id = 20, .port_name = "VPU WRITE2"    },
	{ .port_id = 21, .port_name = "VDEC"          },
	{ .port_id = 22, .port_name = "HCODEC"        },
	{ .port_id = 23, .port_name = "GE2D"          },
	/* start of each device */
	{ .port_id = 32, .port_name = "SPICC1"        },
	{ .port_id = 33, .port_name = "USB0"          },
	{ .port_id = 34, .port_name = "DMA"           },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 36, .port_name = "SD_EMMC_B"     },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "AUDIO"         },
	{ .port_id = 39, .port_name = "AIFIFO"        },
	{ .port_id = 40, .port_name = "SD_EMMC_A"     },
	{ .port_id = 41, .port_name = "PASER"         },
	{ .port_id = 42, .port_name = "AO CPU"        },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC2"        },
	{ .port_id = 45, .port_name = "ETHERNET"      },
	{ .port_id = 46, .port_name = "SANA"          }
};

static struct ddr_port_desc ddr_port_desc_axg[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "PCIE-A"        },
	{ .port_id =  2, .port_name = "PCIE-B"        },
	{ .port_id =  3, .port_name = "GE2D"          },
	{ .port_id =  4, .port_name = "AUDIO"         },
	{ .port_id =  5, .port_name = "TEST"          },
	{ .port_id =  6, .port_name = "USB3.0"        },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id =  8, .port_name = "VPU READ0"     },
	/* start of each device */
	{ .port_id = 34, .port_name = "DMA"           },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 36, .port_name = "SD_EMMC_B"     },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 42, .port_name = "AO CPU"        },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC0"        },
	{ .port_id = 45, .port_name = "ETHERNET"      },
	{ .port_id = 46, .port_name = "SPICC1"        }
};

static struct ddr_port_desc ddr_port_desc_txl[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "MALI0"         },
	{ .port_id =  2, .port_name = "MALI1"         },
	{ .port_id =  3, .port_name = "HDCP"          },
	{ .port_id =  4, .port_name = "HEVC"          },
	{ .port_id =  5, .port_name = "TEST"          },
	{ .port_id =  6, .port_name = "USB3.0"        },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id =  8, .port_name = "VPU READ1"     },
	{ .port_id =  9, .port_name = "VPU READ2"     },
	{ .port_id = 10, .port_name = "VPU READ3"     },
	{ .port_id = 11, .port_name = "VPU WRITE1"    },
	{ .port_id = 12, .port_name = "VPU WRITE2"    },
	{ .port_id = 13, .port_name = "VDEC"          },
	{ .port_id = 14, .port_name = "HCODEC"        },
	{ .port_id = 15, .port_name = "GE2D"          },
	/* start of each device */
	{ .port_id = 32, .port_name = "SD_EMMC_A"     },
	{ .port_id = 33, .port_name = "USB0"          },
	{ .port_id = 34, .port_name = "DMA"           },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 36, .port_name = "SD_EMMC_B"     },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "AUDIO OUT"     },
	{ .port_id = 39, .port_name = "AUDIO IN"      },
	{ .port_id = 40, .port_name = "AIU"           },
	{ .port_id = 41, .port_name = "PASER"         },
	{ .port_id = 42, .port_name = "AO CPU"        },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC"         },
	{ .port_id = 45, .port_name = "ETHERNET"      },
	{ .port_id = 46, .port_name = "SANA"          },
	{ .port_id = 47, .port_name = "DEMOD"         }
};

static struct ddr_port_desc ddr_port_desc_txlx[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "MALI0"         },
	{ .port_id =  2, .port_name = "MALI1"         },
	{ .port_id =  3, .port_name = "HDCP"          },
	{ .port_id =  4, .port_name = "HEVC"          },
	{ .port_id =  5, .port_name = "TEST"          },
	{ .port_id =  6, .port_name = "USB3.0"        },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id =  8, .port_name = "VPU READ1"     },
	{ .port_id =  9, .port_name = "VPU READ2"     },
	{ .port_id = 10, .port_name = "VPU READ3"     },
	{ .port_id = 11, .port_name = "VPU WRITE1"    },
	{ .port_id = 12, .port_name = "VPU WRITE2"    },
	{ .port_id = 13, .port_name = "VDEC"          },
	{ .port_id = 14, .port_name = "HCODEC"        },
	{ .port_id = 15, .port_name = "GE2D"          },
	/* start of each device */
	{ .port_id = 32, .port_name = "SPICC1"        },
	{ .port_id = 33, .port_name = "USB0"          },
	{ .port_id = 34, .port_name = "DMA"           },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 36, .port_name = "SD_EMMC_B"     },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "AUDIO OUT"     },
	{ .port_id = 39, .port_name = "AUDIO IN"      },
	{ .port_id = 40, .port_name = "AIU"           },
	{ .port_id = 41, .port_name = "PASER"         },
	{ .port_id = 42, .port_name = "AO CPU"        },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC"         },
	{ .port_id = 45, .port_name = "ETHERNET"      },
	{ .port_id = 46, .port_name = "SANA"          },
	{ .port_id = 47, .port_name = "DEMOD"         }
};

static struct ddr_port_desc ddr_port_desc_txhd[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "MALI0"         },
	{ .port_id =  2, .port_name = "MALI1"         },
	{ .port_id =  4, .port_name = "HEVC"          },
	{ .port_id =  5, .port_name = "TEST"          },
	{ .port_id =  6, .port_name = "USB3.0"        },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id =  8, .port_name = "VPU READ1"     },
	{ .port_id =  9, .port_name = "VPU READ2"     },
	{ .port_id = 10, .port_name = "VPU READ3"     },
	{ .port_id = 11, .port_name = "VPU WRITE1"    },
	{ .port_id = 12, .port_name = "VPU WRITE2"    },
	{ .port_id = 13, .port_name = "VDEC"          },
	{ .port_id = 14, .port_name = "HCODEC"        },
	{ .port_id = 15, .port_name = "GE2D"          },
	/* start of each device */
	{ .port_id = 33, .port_name = "USB0"          },
	{ .port_id = 34, .port_name = "DMA"           },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 36, .port_name = "SD_EMMC_B"     },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "AUDIO OUT"     },
	{ .port_id = 39, .port_name = "AUDIO IN"      },
	{ .port_id = 40, .port_name = "AIU"           },
	{ .port_id = 41, .port_name = "PASER"         },
	{ .port_id = 42, .port_name = "AO CPU"        },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC"         },
	{ .port_id = 45, .port_name = "ETHERNET"      },
	{ .port_id = 46, .port_name = "SANA"          },
	{ .port_id = 47, .port_name = "DEMOD"         }
};

static struct ddr_port_desc *chip_ddr_port;
static unsigned char chip_ddr_port_num;

int __init ddr_find_port_desc(int cpu_type, struct ddr_port_desc **desc)
{
	int desc_size = -EINVAL;

	if (chip_ddr_port) {
		*desc = chip_ddr_port;
		return chip_ddr_port_num;
	}

	switch (cpu_type) {
	case MESON_CPU_MAJOR_ID_M8B:
		*desc = ddr_port_desc_m8b;
		desc_size = ARRAY_SIZE(ddr_port_desc_m8b);
		break;

	case MESON_CPU_MAJOR_ID_GXBB:
		*desc = ddr_port_desc_gxbb;
		desc_size = ARRAY_SIZE(ddr_port_desc_gxbb);
		break;

	case MESON_CPU_MAJOR_ID_GXL:
		*desc = ddr_port_desc_gxl;
		desc_size = ARRAY_SIZE(ddr_port_desc_gxl);
		break;

	case MESON_CPU_MAJOR_ID_GXM:
		*desc = ddr_port_desc_gxm;
		desc_size = ARRAY_SIZE(ddr_port_desc_gxm);
		break;

	case MESON_CPU_MAJOR_ID_TXL:
		*desc = ddr_port_desc_txl;
		desc_size = ARRAY_SIZE(ddr_port_desc_txl);
		break;

	case MESON_CPU_MAJOR_ID_TXLX:
		*desc = ddr_port_desc_txlx;
		desc_size = ARRAY_SIZE(ddr_port_desc_txlx);
		break;

	case MESON_CPU_MAJOR_ID_AXG:
		*desc = ddr_port_desc_axg;
		desc_size = ARRAY_SIZE(ddr_port_desc_axg);
		break;

	case MESON_CPU_MAJOR_ID_GXLX:
		*desc = ddr_port_desc_gxlx;
		desc_size = ARRAY_SIZE(ddr_port_desc_gxlx);
		break;

	case MESON_CPU_MAJOR_ID_TXHD:
		*desc = ddr_port_desc_txhd;
		desc_size = ARRAY_SIZE(ddr_port_desc_txhd);
		break;

	case MESON_CPU_MAJOR_ID_G12A:
		*desc = ddr_port_desc_g12a;
		desc_size = ARRAY_SIZE(ddr_port_desc_g12a);
		break;

	case MESON_CPU_MAJOR_ID_G12B:
		*desc = ddr_port_desc_g12b;
		desc_size = ARRAY_SIZE(ddr_port_desc_g12b);
		break;

	default:
		return -EINVAL;
	}

	/* using once */
	chip_ddr_port = kcalloc(desc_size, sizeof(*chip_ddr_port), GFP_KERNEL);
	if (!chip_ddr_port)
		return -ENOMEM;
	memcpy(chip_ddr_port, *desc, sizeof(struct ddr_port_desc) * desc_size);
	chip_ddr_port_num = desc_size;
	*desc = chip_ddr_port;

	return desc_size;
}
