/*
 *  Copyright (C) 2013, Analog Devices Inc.
 *	Author: Lars-Peter Clausen <lars@metafoo.de>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/dmaengine.h>
#include <linux/slab.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>

#include <sound/dmaengine_pcm.h>

struct dmaengine_pcm {
	struct dma_chan *chan[SNDRV_PCM_STREAM_CAPTURE + 1];
	const struct snd_dmaengine_pcm_config *config;
	struct snd_soc_platform platform;
	unsigned int flags;
};

static struct dmaengine_pcm *soc_platform_to_pcm(struct snd_soc_platform *p)
{
	return container_of(p, struct dmaengine_pcm, platform);
}

/**
 * snd_dmaengine_pcm_prepare_slave_config() - Generic prepare_slave_config callback
 * @substream: PCM substream
 * @params: hw_params
 * @slave_config: DMA slave config to prepare
 *
 * This function can be used as a generic prepare_slave_config callback for
 * platforms which make use of the snd_dmaengine_dai_dma_data struct for their
 * DAI DMA data. Internally the function will first call
 * snd_hwparams_to_dma_slave_config to fill in the slave config based on the
 * hw_params, followed by snd_dmaengine_set_config_from_dai_data to fill in the
 * remaining fields based on the DAI DMA data.
 */
int snd_dmaengine_pcm_prepare_slave_config(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct dma_slave_config *slave_config)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_dmaengine_dai_dma_data *dma_data;
	int ret;

	dma_data = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);

	ret = snd_hwparams_to_dma_slave_config(substream, params, slave_config);
	if (ret)
		return ret;

	snd_dmaengine_pcm_set_config_from_dai_data(substream, dma_data,
		slave_config);

	return 0;
}
EXPORT_SYMBOL_GPL(snd_dmaengine_pcm_prepare_slave_config);

static int dmaengine_pcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct dmaengine_pcm *pcm = soc_platform_to_pcm(rtd->platform);
	struct dma_chan *chan = snd_dmaengine_pcm_get_chan(substream);
	struct dma_slave_config slave_config;
	int ret;

	if (pcm->config->prepare_slave_config) {
		ret = pcm->config->prepare_slave_config(substream, params,
				&slave_config);
		if (ret)
			return ret;

		ret = dmaengine_slave_config(chan, &slave_config);
		if (ret)
			return ret;
	}

	return snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(params));
}

static int dmaengine_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct dmaengine_pcm *pcm = soc_platform_to_pcm(rtd->platform);
	struct dma_chan *chan = pcm->chan[substream->stream];
	int ret;

	ret = snd_soc_set_runtime_hwparams(substream,
				pcm->config->pcm_hardware);
	if (ret)
		return ret;

	return snd_dmaengine_pcm_open(substream, chan);
}

static struct device *dmaengine_dma_dev(struct dmaengine_pcm *pcm,
	struct snd_pcm_substream *substream)
{
	if (!pcm->chan[substream->stream])
		return NULL;

	return pcm->chan[substream->stream]->device->dev;
}

static void dmaengine_pcm_free(struct snd_pcm *pcm)
{
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

static const char * const dmaengine_pcm_dma_channel_names[] = {
	[SNDRV_PCM_STREAM_PLAYBACK] = "tx",
	[SNDRV_PCM_STREAM_CAPTURE] = "rx",
};

struct dma_chan *audio_dma_chan[SPDIF_PLAYBACK+1];
struct snd_dma_buffer hdmiin_audio_dma_buffer;
struct dmaengine_hdmiin_audio_pcm_runtime_data *hdmiin_audio_prtd = NULL;

int snd_dmaengine_hdmiin_audio_pcm_open()
{
	pr_info("%s: %d\n", __func__, __LINE__);

	if (!hdmiin_audio_prtd)
		hdmiin_audio_prtd = kzalloc(sizeof(*hdmiin_audio_prtd), GFP_KERNEL);
	if (!hdmiin_audio_prtd)
		return -ENOMEM;

	hdmiin_audio_prtd->dma_chan_c = audio_dma_chan[I2S_CAPTURE];
	hdmiin_audio_prtd->dma_chan_b = audio_dma_chan[I2S_PLAYBACK];
	hdmiin_audio_prtd->dma_chan_b2 = audio_dma_chan[SPDIF_PLAYBACK];

	
	hdmiin_audio_prtd->dma_buffer_c = hdmiin_audio_dma_buffer;
	hdmiin_audio_prtd->dma_buffer_b = hdmiin_audio_dma_buffer;
	hdmiin_audio_prtd->dma_buffer_b2 = hdmiin_audio_dma_buffer;

	return 0;
}
EXPORT_SYMBOL_GPL(snd_dmaengine_hdmiin_audio_pcm_open);

extern struct dmaengine_hdmiin_audio_pcm_runtime_data *hdmiin_audio_prtd;
int dmaengine_hdmiin_audio_pcm_hw_params(int mode)
{
	struct dmaengine_hdmiin_audio_pcm_runtime_data *prtd = hdmiin_audio_prtd;
	struct dma_chan *chan_c = prtd->dma_chan_c;
	struct dma_chan *chan_b = prtd->dma_chan_b;
	struct dma_chan *chan_b2 = prtd->dma_chan_b2;
	struct dma_slave_config slave_config_c;
	struct dma_slave_config slave_config_b;
	struct dma_slave_config slave_config_b2;
	int ret;

	if (HDMIN_NORMAL_MODE == mode)
		snd_get_hdmiin_audio_pcm_slave_config(&slave_config_c, I2S_CAPTURE);
	snd_get_hdmiin_audio_pcm_slave_config(&slave_config_b, I2S_PLAYBACK);
	snd_get_hdmiin_audio_pcm_slave_config(&slave_config_b2, SPDIF_PLAYBACK);

	if (HDMIN_NORMAL_MODE == mode)
		ret = dmaengine_slave_config(chan_c, &slave_config_c);
	ret = dmaengine_slave_config(chan_b, &slave_config_b);
	ret = dmaengine_slave_config(chan_b2, &slave_config_b2);

	if (ret)
		return ret;
	pr_info("%s: %d done\n", __func__, __LINE__);
	return 0;
}


static int dmaengine_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct dmaengine_pcm *pcm = soc_platform_to_pcm(rtd->platform);
	const struct snd_dmaengine_pcm_config *config = pcm->config;
	struct snd_pcm_substream *substream;
	unsigned int i;
	int ret;

	for (i = SNDRV_PCM_STREAM_PLAYBACK; i <= SNDRV_PCM_STREAM_CAPTURE; i++) {
		substream = rtd->pcm->streams[i].substream;
		if (!substream)
			continue;

		if (!pcm->chan[i] && (pcm->flags & SND_DMAENGINE_PCM_FLAG_COMPAT)) {
			pcm->chan[i] = dma_request_slave_channel(rtd->platform->dev,
								 dmaengine_pcm_dma_channel_names[i]);
		}

		if (!pcm->chan[i]) {
			dev_err(rtd->platform->dev,
				"Missing dma channel for stream: %d\n", i);
			ret = -EINVAL;
			goto err_free;
		}

		ret = snd_pcm_lib_preallocate_pages(substream,
				SNDRV_DMA_TYPE_DEV,
				dmaengine_dma_dev(pcm, substream),
				config->prealloc_buffer_size,
				config->pcm_hardware->buffer_bytes_max);
		if (ret)
			goto err_free;

		if (strstr(dma_chan_name(pcm->chan[i]), I2S_CAPTURE_DMA_CHN)) {
			audio_dma_chan[I2S_CAPTURE] = pcm->chan[i];
			hdmiin_audio_dma_buffer.area = substream->dma_buffer.area;
			hdmiin_audio_dma_buffer.addr = substream->dma_buffer.addr;
		} else if (strstr(dma_chan_name(pcm->chan[i]), I2S_PLAYBACK_DMA_CHN)) {
			audio_dma_chan[I2S_PLAYBACK] = pcm->chan[i];
		} else if (strstr(dma_chan_name(pcm->chan[i]), SPDIF_PLAYBACK_DMA_CHN)) {
			audio_dma_chan[SPDIF_PLAYBACK] = pcm->chan[i];
		}
	}

	return 0;

err_free:
	dmaengine_pcm_free(rtd->pcm);
	return ret;
}

static const struct snd_pcm_ops dmaengine_pcm_ops = {
	.open		= dmaengine_pcm_open,
	.close		= snd_dmaengine_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= dmaengine_pcm_hw_params,
	.hw_free	= snd_pcm_lib_free_pages,
	.trigger	= snd_dmaengine_pcm_trigger,
	.pointer	= snd_dmaengine_pcm_pointer,
};

static const struct snd_soc_platform_driver dmaengine_pcm_platform = {
	.ops		= &dmaengine_pcm_ops,
	.pcm_new	= dmaengine_pcm_new,
	.pcm_free	= dmaengine_pcm_free,
	.probe_order	= SND_SOC_COMP_ORDER_LATE,
};

static const struct snd_pcm_ops dmaengine_no_residue_pcm_ops = {
	.open		= dmaengine_pcm_open,
	.close		= snd_dmaengine_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= dmaengine_pcm_hw_params,
	.hw_free	= snd_pcm_lib_free_pages,
	.trigger	= snd_dmaengine_pcm_trigger,
	.pointer	= snd_dmaengine_pcm_pointer_no_residue,
};

static const struct snd_soc_platform_driver dmaengine_no_residue_pcm_platform = {
	.ops		= &dmaengine_no_residue_pcm_ops,
	.pcm_new	= dmaengine_pcm_new,
	.pcm_free	= dmaengine_pcm_free,
	.probe_order	= SND_SOC_COMP_ORDER_LATE,
};

static void dmaengine_pcm_request_chan_of(struct dmaengine_pcm *pcm,
	struct device *dev)
{
	unsigned int i;

	if ((pcm->flags & SND_DMAENGINE_PCM_FLAG_NO_DT) || !dev->of_node)
		return;

	if (pcm->flags & SND_DMAENGINE_PCM_FLAG_HALF_DUPLEX) {
		pcm->chan[0] = dma_request_slave_channel(dev, "rx-tx");
		pcm->chan[1] = pcm->chan[0];
	} else {
		for (i = SNDRV_PCM_STREAM_PLAYBACK; i <= SNDRV_PCM_STREAM_CAPTURE; i++) {
			pcm->chan[i] = dma_request_slave_channel(dev,
					dmaengine_pcm_dma_channel_names[i]);

		}
	}
}

/**
 * snd_dmaengine_pcm_register - Register a dmaengine based PCM device
 * @dev: The parent device for the PCM device
 * @config: Platform specific PCM configuration
 * @flags: Platform specific quirks
 */
int snd_dmaengine_pcm_register(struct device *dev,
	const struct snd_dmaengine_pcm_config *config, unsigned int flags)
{
	struct dmaengine_pcm *pcm;

	pcm = kzalloc(sizeof(*pcm), GFP_KERNEL);
	if (!pcm)
		return -ENOMEM;

	pcm->config = config;
	pcm->flags = flags;

	dmaengine_pcm_request_chan_of(pcm, dev);

	if (flags & SND_DMAENGINE_PCM_FLAG_NO_RESIDUE)
		return snd_soc_add_platform(dev, &pcm->platform,
				&dmaengine_no_residue_pcm_platform);
	else
		return snd_soc_add_platform(dev, &pcm->platform,
				&dmaengine_pcm_platform);
}
EXPORT_SYMBOL_GPL(snd_dmaengine_pcm_register);

/**
 * snd_dmaengine_pcm_unregister - Removes a dmaengine based PCM device
 * @dev: Parent device the PCM was register with
 *
 * Removes a dmaengine based PCM device previously registered with
 * snd_dmaengine_pcm_register.
 */
void snd_dmaengine_pcm_unregister(struct device *dev)
{
	struct snd_soc_platform *platform;
	struct dmaengine_pcm *pcm;
	unsigned int i;

	platform = snd_soc_lookup_platform(dev);
	if (!platform)
		return;

	pcm = soc_platform_to_pcm(platform);

	for (i = SNDRV_PCM_STREAM_PLAYBACK; i <= SNDRV_PCM_STREAM_CAPTURE; i++) {
		if (pcm->chan[i]) {
			dma_release_channel(pcm->chan[i]);
			if (pcm->flags & SND_DMAENGINE_PCM_FLAG_HALF_DUPLEX)
				break;
		}
	}

	snd_soc_remove_platform(platform);
	kfree(pcm);
}
EXPORT_SYMBOL_GPL(snd_dmaengine_pcm_unregister);

MODULE_LICENSE("GPL");
