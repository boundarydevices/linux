/*
 * Driver for the TAS5805M Audio Amplifier
 *
 * Author: Andy Liu <andy-liu@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/regmap.h>

#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/initval.h>

#include "tas5805.h"

#define TAS5805M_DRV_NAME    "tas5805m"

#define TAS5805M_RATES	     (SNDRV_PCM_RATE_8000 | \
		       SNDRV_PCM_RATE_11025 | \
		       SNDRV_PCM_RATE_16000 | \
		       SNDRV_PCM_RATE_22050 | \
		       SNDRV_PCM_RATE_32000 | \
		       SNDRV_PCM_RATE_44100 | \
		       SNDRV_PCM_RATE_48000)
#define TAS5805M_FORMATS     (SNDRV_PCM_FMTBIT_S16_LE | \
			SNDRV_PCM_FMTBIT_S20_3LE |\
			SNDRV_PCM_FMTBIT_S24_LE | \
			SNDRV_PCM_FMTBIT_S32_LE)

#define TAS5805M_REG_00      (0x00)
#define TAS5805M_REG_03      (0x03)
#define TAS5805M_REG_24      (0x24)
#define TAS5805M_REG_25      (0x25)
#define TAS5805M_REG_26      (0x26)
#define TAS5805M_REG_27      (0x27)
#define TAS5805M_REG_28      (0x28)
#define TAS5805M_REG_29      (0x29)
#define TAS5805M_REG_2A      (0x2a)
#define TAS5805M_REG_2B      (0x2b)
#define TAS5805M_REG_35      (0x35)
#define TAS5805M_REG_7F      (0x7f)

#define TAS5805M_PAGE_00     (0x00)
#define TAS5805M_PAGE_2A     (0x2a)

#define TAS5805M_BOOK_00     (0x00)
#define TAS5805M_BOOK_8C     (0x8c)

#define TAS5805M_VOLUME_MAX  (158)
#define TAS5805M_VOLUME_MIN  (0)

const uint32_t tas5805m_volume[] = {
	0x0000001B,		//0, -110dB
	0x0000001E,		//1, -109dB
	0x00000021,		//2, -108dB
	0x00000025,		//3, -107dB
	0x0000002A,		//4, -106dB
	0x0000002F,		//5, -105dB
	0x00000035,		//6, -104dB
	0x0000003B,		//7, -103dB
	0x00000043,		//8, -102dB
	0x0000004B,		//9, -101dB
	0x00000054,		//10, -100dB
	0x0000005E,		//11, -99dB
	0x0000006A,		//12, -98dB
	0x00000076,		//13, -97dB
	0x00000085,		//14, -96dB
	0x00000095,		//15, -95dB
	0x000000A7,		//16, -94dB
	0x000000BC,		//17, -93dB
	0x000000D3,		//18, -92dB
	0x000000EC,		//19, -91dB
	0x00000109,		//20, -90dB
	0x0000012A,		//21, -89dB
	0x0000014E,		//22, -88dB
	0x00000177,		//23, -87dB
	0x000001A4,		//24, -86dB
	0x000001D8,		//25, -85dB
	0x00000211,		//26, -84dB
	0x00000252,		//27, -83dB
	0x0000029A,		//28, -82dB
	0x000002EC,		//29, -81dB
	0x00000347,		//30, -80dB
	0x000003AD,		//31, -79dB
	0x00000420,		//32, -78dB
	0x000004A1,		//33, -77dB
	0x00000532,		//34, -76dB
	0x000005D4,		//35, -75dB
	0x0000068A,		//36, -74dB
	0x00000756,		//37, -73dB
	0x0000083B,		//38, -72dB
	0x0000093C,		//39, -71dB
	0x00000A5D,		//40, -70dB
	0x00000BA0,		//41, -69dB
	0x00000D0C,		//42, -68dB
	0x00000EA3,		//43, -67dB
	0x0000106C,		//44, -66dB
	0x0000126D,		//45, -65dB
	0x000014AD,		//46, -64dB
	0x00001733,		//47, -63dB
	0x00001A07,		//48, -62dB
	0x00001D34,		//49, -61dB
	0x000020C5,		//50, -60dB
	0x000024C4,		//51, -59dB
	0x00002941,		//52, -58dB
	0x00002E49,		//53, -57dB
	0x000033EF,		//54, -56dB
	0x00003A45,		//55, -55dB
	0x00004161,		//56, -54dB
	0x0000495C,		//57, -53dB
	0x0000524F,		//58, -52dB
	0x00005C5A,		//59, -51dB
	0x0000679F,		//60, -50dB
	0x00007444,		//61, -49dB
	0x00008274,		//62, -48dB
	0x0000925F,		//63, -47dB
	0x0000A43B,		//64, -46dB
	0x0000B845,		//65, -45dB
	0x0000CEC1,		//66, -44dB
	0x0000E7FB,		//67, -43dB
	0x00010449,		//68, -42dB
	0x0001240C,		//69, -41dB
	0x000147AE,		//70, -40dB
	0x00016FAA,		//71, -39dB
	0x00019C86,		//72, -38dB
	0x0001CEDC,		//73, -37dB
	0x00020756,		//74, -36dB
	0x000246B5,		//75, -35dB
	0x00028DCF,		//76, -34dB
	0x0002DD96,		//77, -33dB
	0x00033718,		//78, -32dB
	0x00039B87,		//79, -31dB
	0x00040C37,		//80, -30dB
	0x00048AA7,		//81, -29dB
	0x00051884,		//82, -28dB
	0x0005B7B1,		//83, -27dB
	0x00066A4A,		//84, -26dB
	0x000732AE,		//85, -25dB
	0x00081385,		//86, -24dB
	0x00090FCC,		//87, -23dB
	0x000A2ADB,		//88, -22dB
	0x000B6873,		//89, -21dB
	0x000CCCCD,		//90, -20dB
	0x000E5CA1,		//91, -19dB
	0x00101D3F,		//92, -18dB
	0x0012149A,		//93, -17dB
	0x00144961,		//94, -16dB
	0x0016C311,		//95, -15dB
	0x00198A13,		//96, -14dB
	0x001CA7D7,		//97, -13dB
	0x002026F3,		//98, -12dB
	0x00241347,		//99, -11dB
	0x00287A27,		//100, -10dB
	0x002D6A86,		//101, -9dB
	0x0032F52D,		//102, -8dB
	0x00392CEE,		//103, -7dB
	0x004026E7,		//104, -6dB
	0x0047FACD,		//105, -5dB
	0x0050C336,		//106, -4dB
	0x005A9DF8,		//107, -3dB
	0x0065AC8C,		//108, -2dB
	0x00721483,		//109, -1dB
	0x00800000,		//110, 0dB
	0x008F9E4D,		//111, 1dB
	0x00A12478,		//112, 2dB
	0x00B4CE08,		//113, 3dB
	0x00CADDC8,		//114, 4dB
	0x00E39EA9,		//115, 5dB
	0x00FF64C1,		//116, 6dB
	0x011E8E6A,		//117, 7dB
	0x0141857F,		//118, 8dB
	0x0168C0C6,		//119, 9dB
	0x0194C584,		//120, 10dB
	0x01C62940,		//121, 11dB
	0x01FD93C2,		//122, 12dB
	0x023BC148,		//123, 13dB
	0x02818508,		//124, 14dB
	0x02CFCC01,		//125, 15dB
	0x0327A01A,		//126, 16dB
	0x038A2BAD,		//127, 17dB
	0x03F8BD7A,		//128, 18dB
	0x0474CD1B,		//129, 19dB
	0x05000000,		//130, 20dB
	0x059C2F02,		//131, 21dB
	0x064B6CAE,		//132, 22dB
	0x07100C4D,		//133, 23dB
	0x07ECA9CD,		//134, 24dB
	0x08E43299,		//135, 25dB
	0x09F9EF8E,		//136, 26dB
	0x0B319025,		//137, 27dB
	0x0C8F36F2,		//138, 28dB
	0x0E1787B8,		//139, 29dB
	0x0FCFB725,		//140, 30dB
	0x11BD9C84,		//141, 31dB
	0x13E7C594,		//142, 32dB
	0x16558CCB,		//143, 33dB
	0x190F3254,		//144, 34dB
	0x1C1DF80E,		//145, 35dB
	0x1F8C4107,		//146, 36dB
	0x2365B4BF,		//147, 37dB
	0x27B766C2,		//148, 38dB
	0x2C900313,		//149, 39dB
	0x32000000,		//150, 40dB
	0x3819D612,		//151, 41dB
	0x3EF23ECA,		//152, 42dB
	0x46A07B07,		//153, 43dB
	0x4F3EA203,		//154, 44dB
	0x58E9F9F9,		//155, 45dB
	0x63C35B8E,		//156, 46dB
	0x6FEFA16D,		//157, 47dB
	0x7D982575,		//158, 48dB
};

struct tas5805m_priv {
	struct regmap *regmap;

	struct mutex lock;

	int vol;
	int mute;
};

const struct regmap_config tas5805m_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

static int tas5805m_vol_info(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->access =
	    (SNDRV_CTL_ELEM_ACCESS_TLV_READ | SNDRV_CTL_ELEM_ACCESS_READWRITE);
	uinfo->count = 1;

	uinfo->value.integer.min = TAS5805M_VOLUME_MIN;
	uinfo->value.integer.max = TAS5805M_VOLUME_MAX;
	uinfo->value.integer.step = 1;

	return 0;
}

static int tas5805m_mute_info(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->access =
	    (SNDRV_CTL_ELEM_ACCESS_TLV_READ | SNDRV_CTL_ELEM_ACCESS_READWRITE);
	uinfo->count = 1;

	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	uinfo->value.integer.step = 1;

	return 0;
}

static int tas5805m_vol_locked_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5805m_priv *tas5805m = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&tas5805m->lock);
	ucontrol->value.integer.value[0] = tas5805m->vol;
	mutex_unlock(&tas5805m->lock);

	return 0;
}

static inline int get_volume_index(int vol)
{
	int index;

	index = vol;

	if (index < TAS5805M_VOLUME_MIN)
		index = TAS5805M_VOLUME_MIN;

	if (index > TAS5805M_VOLUME_MAX)
		index = TAS5805M_VOLUME_MAX;

	return index;
}

static void tas5805m_set_volume(struct snd_soc_codec *codec, int vol)
{
	unsigned int index;
	uint32_t volume_hex;
	uint8_t byte4;
	uint8_t byte3;
	uint8_t byte2;
	uint8_t byte1;

	index = get_volume_index(vol);
	volume_hex = tas5805m_volume[index];

	byte4 = ((volume_hex >> 24) & 0xFF);
	byte3 = ((volume_hex >> 16) & 0xFF);
	byte2 = ((volume_hex >> 8) & 0xFF);
	byte1 = ((volume_hex >> 0) & 0xFF);

	//w 58 00 00
	snd_soc_write(codec, TAS5805M_REG_00, TAS5805M_PAGE_00);
	//w 58 7f 8c
	snd_soc_write(codec, TAS5805M_REG_7F, TAS5805M_BOOK_8C);
	//w 58 00 2a
	snd_soc_write(codec, TAS5805M_REG_00, TAS5805M_PAGE_2A);
	//w 58 24 xx xx xx xx
	snd_soc_write(codec, TAS5805M_REG_24, byte4);
	snd_soc_write(codec, TAS5805M_REG_25, byte3);
	snd_soc_write(codec, TAS5805M_REG_26, byte2);
	snd_soc_write(codec, TAS5805M_REG_27, byte1);
	//w 58 28 xx xx xx xx
	snd_soc_write(codec, TAS5805M_REG_28, byte4);
	snd_soc_write(codec, TAS5805M_REG_29, byte3);
	snd_soc_write(codec, TAS5805M_REG_2A, byte2);
	snd_soc_write(codec, TAS5805M_REG_2B, byte1);
}

static int tas5805m_vol_locked_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5805m_priv *tas5805m = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&tas5805m->lock);

	tas5805m->vol = ucontrol->value.integer.value[0];
	tas5805m_set_volume(codec, tas5805m->vol);

	mutex_unlock(&tas5805m->lock);

	return 0;
}

static int tas5805m_mute(struct snd_soc_codec *codec, int mute)
{
	u8 reg03_value = 0;
	u8 reg35_value = 0;
//  struct snd_soc_codec *codec = dai->codec;

	if (mute) {
		//mute both left & right channels
		reg03_value = 0x0b;
		reg35_value = 0x00;
	} else {
		//unmute
		reg03_value = 0x03;
		reg35_value = 0x11;
	}

	snd_soc_write(codec, TAS5805M_REG_00, TAS5805M_PAGE_00);
	snd_soc_write(codec, TAS5805M_REG_7F, TAS5805M_BOOK_00);
	snd_soc_write(codec, TAS5805M_REG_00, TAS5805M_PAGE_00);
	snd_soc_write(codec, TAS5805M_REG_03, reg03_value);
	snd_soc_write(codec, TAS5805M_REG_35, reg35_value);

	return 0;
}

static int tas5805m_mute_locked_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5805m_priv *tas5805m = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&tas5805m->lock);

	tas5805m->mute = ucontrol->value.integer.value[0];
	tas5805m_mute(codec, tas5805m->mute);

	mutex_unlock(&tas5805m->lock);

	return 0;
}

static int tas5805m_mute_locked_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5805m_priv *tas5805m = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&tas5805m->lock);
	ucontrol->value.integer.value[0] = tas5805m->mute;
	mutex_unlock(&tas5805m->lock);

	return 0;
}

static const struct snd_kcontrol_new tas5805m_vol_control[] = {
	{
	 .iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	 .name = "Master Playback Volume",
	 .info = tas5805m_vol_info,
	 .get = tas5805m_vol_locked_get,
	 .put = tas5805m_vol_locked_put,
	 },
	{
	 .iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	 .name = "Maser Volume Mute",
	 .info = tas5805m_mute_info,
	 .get = tas5805m_mute_locked_get,
	 .put = tas5805m_mute_locked_put,
	}
};

static int tas5805m_snd_probe(struct snd_soc_codec *codec)
{
	int ret;

	ret = snd_soc_add_codec_controls(codec, tas5805m_vol_control, 2);

	return ret;
}

static struct snd_soc_codec_driver soc_codec_tas5805m = {
	.probe = tas5805m_snd_probe,
};

static const struct snd_soc_dai_ops tas5805m_dai_ops = {
	//.digital_mute = tas5805m_mute,
};

static struct snd_soc_dai_driver tas5805m_dai = {
	.name = "tas5805m-amplifier",
	.playback = {
		     .stream_name = "Playback",
		     .channels_min = 2,
		     .channels_max = 8,
		     .rates = TAS5805M_RATES,
		     .formats = TAS5805M_FORMATS,
		     },
	.ops = &tas5805m_dai_ops,
};

static int tas5805m_probe(struct device *dev, struct regmap *regmap)
{
	struct tas5805m_priv *tas5805m;
	int ret;

	tas5805m = devm_kzalloc(dev, sizeof(struct tas5805m_priv), GFP_KERNEL);
	if (!tas5805m)
		return -ENOMEM;

	dev_set_drvdata(dev, tas5805m);
	tas5805m->regmap = regmap;
	tas5805m->vol = 100;	//100, -10dB

	mutex_init(&tas5805m->lock);

	ret =
	    regmap_register_patch(regmap, tas5805m_init_sequence,
				  ARRAY_SIZE(tas5805m_init_sequence));
	if (ret != 0) {
		dev_err(dev, "Failed to initialize TAS5805M: %d\n", ret);
		goto err;

	}

	ret =
	    snd_soc_register_codec(dev, &soc_codec_tas5805m, &tas5805m_dai, 1);
	if (ret != 0) {
		dev_err(dev, "Failed to register CODEC: %d\n", ret);
		goto err;
	}

	return 0;

err:
	return ret;

}

static int tas5805m_i2c_probe(struct i2c_client *i2c,
			      const struct i2c_device_id *id)
{
	struct regmap *regmap;
	struct regmap_config config = tas5805m_regmap;

	regmap = devm_regmap_init_i2c(i2c, &config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	return tas5805m_probe(&i2c->dev, regmap);
}

static int tas5805m_remove(struct device *dev)
{
	snd_soc_unregister_codec(dev);

	return 0;
}

static int tas5805m_i2c_remove(struct i2c_client *i2c)
{
	tas5805m_remove(&i2c->dev);

	return 0;
}

static const struct i2c_device_id tas5805m_i2c_id[] = {
	{"tas5805",},
	{}
};

MODULE_DEVICE_TABLE(i2c, tas5805m_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id tas5805m_of_match[] = {
	{.compatible = "ti,tas5805",},
	{}
};

MODULE_DEVICE_TABLE(of, tas5805m_of_match);
#endif

static struct i2c_driver tas5805m_i2c_driver = {
	.probe = tas5805m_i2c_probe,
	.remove = tas5805m_i2c_remove,
	.id_table = tas5805m_i2c_id,
	.driver = {
		   .name = TAS5805M_DRV_NAME,
		   .of_match_table = tas5805m_of_match,
		   },
};

module_i2c_driver(tas5805m_i2c_driver);

MODULE_AUTHOR("Andy Liu <andy-liu@ti.com>");
MODULE_DESCRIPTION("TAS5805M Audio Amplifier Driver");
MODULE_LICENSE("GPL v2");
