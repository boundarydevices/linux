#include "fec_switch.h"

/* Transmitter timeout */
#define TX_TIMEOUT (2 * HZ)

#if defined(CONFIG_KSZ_SWITCH_EMBEDDED)
/* Need to predefine get_sysfs_data. */

#ifndef get_sysfs_data
struct ksz_port;

static void get_sysfs_data_(struct net_device *ndev,
	struct semaphore **proc_sem, struct ksz_port **port);

#define get_sysfs_data		get_sysfs_data_
#endif

#if defined(CONFIG_IBA_KSZ9897)
#include "../micrel/iba-ksz9897.c"
#elif defined(CONFIG_HAVE_KSZ9897)
#include "../micrel/spi-ksz9897.c"
#elif defined(CONFIG_HAVE_KSZ8795)
#include "../micrel/spi-ksz8795.c"
#elif defined(CONFIG_SMI_KSZ8895)
#include "../micrel/smi-ksz8895.c"
#elif defined(CONFIG_HAVE_KSZ8895)
#include "../micrel/spi-ksz8895.c"
#elif defined(CONFIG_SMI_KSZ8863)
#include "../micrel/smi-ksz8863.c"
#elif defined(CONFIG_HAVE_KSZ8863)
#include "../micrel/spi-ksz8863.c"
#elif defined(CONFIG_IBA_LAN937X)
#include "../microchip/iba-lan937x.c"
#elif defined(CONFIG_SMI_LAN937X)
#include "../microchip/smi-lan937x.c"
#elif defined(CONFIG_HAVE_LAN937X)
#include "../microchip/spi-lan937x.c"
#endif
#endif

#if !defined(get_sysfs_data) || defined(CONFIG_KSZ_SWITCH_EMBEDDED)
static void get_sysfs_data_(struct net_device *ndev,
	struct semaphore **proc_sem, struct ksz_port **port)
{
	struct fec_switch *fs = netdev_priv(ndev);
	struct sw_priv *hw_priv;

	hw_priv = fs->parent;
	*port = &fs->port;
	*proc_sem = &hw_priv->proc_sem;
}  /* get_sysfs_data */
#endif

#ifndef get_sysfs_data
#define get_sysfs_data		get_sysfs_data_
#endif

#if !defined(CONFIG_KSZ_SWITCH_EMBEDDED)

#if defined(CONFIG_HAVE_KSZ9897)
#include "../micrel/ksz_sw_sysfs_9897.c"
#elif defined(CONFIG_HAVE_KSZ8795)
#include "../micrel/ksz_sw_sysfs_8795.c"
#elif defined(CONFIG_HAVE_KSZ8895)
#include "../micrel/ksz_sw_sysfs_8895.c"
#elif defined(CONFIG_HAVE_KSZ8863)
#include "../micrel/ksz_sw_sysfs.c"
#elif defined(CONFIG_HAVE_KSZ8463)
#include "../micrel/ksz_sw_sysfs.c"
#elif defined(CONFIG_HAVE_LAN937X)
#include "../microchip/lan937x_sw_sysfs.c"
#endif

#ifdef CONFIG_1588_PTP
#ifdef CONFIG_HAVE_LAN937X
#include "../microchip/lan937x_ptp_sysfs.c"
#else
#include "../micrel/ksz_ptp_sysfs.c"
#endif
#endif
#ifdef CONFIG_KSZ_DLR
#include "../micrel/ksz_dlr_sysfs.c"
#endif
#endif

static int sw_device_seen;

#if !defined(CONFIG_KSZ_IBA_ONLY)
static struct ksz_sw *check_avail_switch(struct net_device *ndev, int id)
{
	int phy_mode;
	char phy_id[MII_BUS_ID_SIZE];
	char bus_id[MII_BUS_ID_SIZE];
	struct ksz_sw *sw = NULL;
	struct phy_device *phydev = NULL;

	/* Check whether MII switch exists. */
	phy_mode = PHY_INTERFACE_MODE_MII;
	snprintf(bus_id, MII_BUS_ID_SIZE, "sw.%d", id);
	snprintf(phy_id, MII_BUS_ID_SIZE, PHY_ID_FMT, bus_id, 0);
	phydev = phy_attach(ndev, phy_id, phy_mode);
	if (!IS_ERR(phydev)) {
		struct phy_priv *phydata = phydev->priv;

		sw = phydata->port->sw;

		/*
		 * In case multiple devices mode is used and this phydev is not
		 * attached again.
		 */
		if (sw)
			phydev->interface = sw->interface;
		phy_detach(phydev);
	}
	return sw;
}  /* check_avail_switch */

static int fec_enet_sw_chk(struct fec_switch *fs)
{
	struct ksz_sw *sw = fs->port.sw;

	if (!sw) {
		sw = check_avail_switch(fs->netdev, sw_device_seen);
		if (!sw)
			return -ENXIO;
		fs->port.sw = sw;
	}
	return 0;
}
#endif

#if defined(CONFIG_KSZ_IBA_ONLY) || defined(CONFIG_KSZ_SMI)
static int get_sw_irq(void)
{
	struct device *dev;
	int spi_bus;
	int spi_select;
	char name[20];

	spi_select = 0;
	for (spi_bus = 0; spi_bus < 2; spi_bus++) {
		sprintf(name, "spi%d.%d\n", spi_bus, spi_select);
		dev = bus_find_device_by_name(&spi_bus_type, NULL, name);
		if (dev && dev->of_node) {
			int irq = of_irq_get(dev->of_node, 0);

			return irq;
		}
	}
	return -1;
}  /* get_sw_irq */
#endif

static void stop_dev_queues(struct fec_switch *fs, struct ksz_sw *sw, struct net_device *hw_dev,
			    u16 qid)
{
	if (sw) {
		int dev_count = sw->dev_count + sw->dev_offset;
		struct netdev_queue *nq;
		struct net_device *ndev;
		int p;

		for (p = 0; p < dev_count; p++) {
			ndev = sw->netdev[p];
			if (!ndev || ndev == hw_dev)
				continue;
			if (netif_running(ndev) || ndev == fs->netdev) {
				nq = netdev_get_tx_queue(ndev, qid);
				netif_tx_stop_queue(nq);
			}
		}
	}
}  /* stop_dev_queues */

static void wake_dev_queues(struct ksz_sw *sw, struct net_device *hw_dev,
			    u16 qid)
{
	if (sw) {
		int dev_count = sw->dev_count + sw->dev_offset;
		struct netdev_queue *nq;
		struct net_device *ndev;
		int p;

		for (p = 0; p < dev_count; p++) {
			ndev = sw->netdev[p];
			if (!ndev || ndev == hw_dev)
				continue;
			if (netif_running(ndev)) {
				nq = netdev_get_tx_queue(ndev, qid);
				if (netif_tx_queue_stopped(nq))
					netif_tx_wake_queue(nq);
			}
		}
		wake_up_interruptible(&sw->queue);
	}
}  /* wake_dev_queues */

#if defined(CONFIG_HAVE_KSZ9897) || defined(CONFIG_HAVE_LAN937X)
static int priv_multi(void *ptr)
{
	struct fec_switch *fs = ptr;

	return (fs->multi & 1);
}  /* priv_multi */
#endif

static int priv_promisc(void *ptr)
{
	struct fec_switch *fs = ptr;

	return fs->promisc;
}  /* priv_promisc */

#if !defined(CONFIG_HAVE_KSZ9897) && !defined(CONFIG_HAVE_LAN937X)
static int priv_match_multi(void *ptr, u8 *data)
{
	struct netdev_hw_addr *ha;
	struct fec_switch *fs = ptr;
	int drop = true;

	netdev_for_each_mc_addr(ha, fs->netdev) {
		if (!memcmp(data, ha->addr, ETH_ALEN)) {
			drop = false;
			break;
		}
	}
	return drop;
}  /* priv_match_multi */
#endif

static struct net_device *sw_rx_proc(struct ksz_sw *sw, struct sk_buff *skb,
				     __u8 *data)
{
	struct net_device *ndev;
	struct fec_switch *fs;
	int len = skb->len;
	int rx_port = 0;
#if defined(CONFIG_KSZ_SWITCH) || defined(CONFIG_1588_PTP)
	int forward = 0;
	int tag = 0;
	void *ptr = NULL;
	void (*rx_tstamp)(void *ptr, struct sk_buff *skb) = NULL;
#endif
#ifdef CONFIG_1588_PTP
	struct ptp_info *ptp = &sw->ptp_hw;
	int ptp_tag = 0;
#endif
	struct net_device *parent_dev = NULL;
	struct sk_buff *parent_skb = NULL;

	ndev = sw->net_ops->rx_dev(sw, data, &len, &tag, &rx_port);
	if (!ndev) {
		dev_kfree_skb_any(skb);
		return NULL;
	}

	/* vlan_get_tag requires network device in socket buffer. */
	skb->dev = ndev;

	/* skb_put is already used. */
	if (len != skb->len) {
		int diff = skb->len - len;

		skb->len -= diff;
		skb->tail -= diff;
	}

	fs = netdev_priv(ndev);

	/* Internal packets handled by the switch. */
	if (!sw->net_ops->drv_rx(sw, skb, rx_port)) {
		ndev->stats.rx_packets++;
		ndev->stats.rx_bytes += skb->len;
		return NULL;
	}

	if (!sw->net_ops->match_pkt(sw, &ndev, (void **) &fs, priv_promisc,
#if defined(CONFIG_HAVE_KSZ9897) || defined(CONFIG_HAVE_LAN937X)
	    priv_multi,
#else
	    priv_match_multi,
#endif
	    skb, fs->common->hw_promisc)) {
		dev_kfree_skb_irq(skb);
		return NULL;
	}

#ifdef CONFIG_1588_PTP
	ptr = ptp;
	if (sw->features & PTP_HW) {
		if (ptp->ops->drop_pkt(ptp, skb, sw->vlan_id, &tag, &ptp_tag,
				       &forward)) {
			dev_kfree_skb_any(skb);
			return NULL;
		}
		if (ptp_tag)
			rx_tstamp = ptp->ops->get_rx_tstamp;
	}
#endif

	/* Need to forward to VLAN devices for PAE messages. */
	if (!forward) {
		struct ethhdr *eth = (struct ethhdr *) data;

		if (eth->h_proto == htons(0x888E))
			forward = FWD_VLAN_DEV | FWD_STP_DEV;
	}

	/* No VLAN port forwarding; need to send to parent. */
	if ((forward & FWD_VLAN_DEV) && !tag)
		forward &= ~FWD_VLAN_DEV;
	ndev = sw->net_ops->parent_rx(sw, ndev, skb, &forward, &parent_dev,
		&parent_skb);

	/* ndev may change. */
	if (ndev != skb->dev) {
		skb->dev = ndev;
	}

	sw->net_ops->port_vlan_rx(sw, ndev, parent_dev, skb,
		forward, tag, ptr, rx_tstamp);
	return ndev;
}  /* sw_rx_proc */

#ifdef CONFIG_KSZ_SMI
static int smi_read(struct mii_bus *bus, int phy_id, int regnum)
{
	return fec_enet_mdio_read(bus, phy_id, regnum);
}

static int smi_write(struct mii_bus *bus, int phy_id, int regnum, u16 val)
{
	return fec_enet_mdio_write(bus, phy_id, regnum, val);
}
#endif

static void priv_adjust_link(struct net_device *ndev)
{}

static u8 get_priv_state(struct net_device *ndev)
{
	struct fec_switch *fs = netdev_priv(ndev);

	return fs->state;
}  /* get_priv_state */

static void set_priv_state(struct net_device *ndev, u8 state)
{
	struct fec_switch *fs = netdev_priv(ndev);

	fs->state = state;
}  /* set_priv_state */

static struct ksz_port *get_priv_port(struct net_device *ndev)
{
	struct fec_switch *fs = netdev_priv(ndev);

	return &fs->port;
}  /* get_priv_port */

#if defined(CONFIG_HAVE_KSZ9897) || defined(CONFIG_HAVE_LAN937X)
static int get_net_ready(struct net_device *ndev)
{
	struct fec_switch *fs = netdev_priv(ndev);

	return fs->common->ready;
}  /* get_net_ready */
#endif

static void hw_set_multicast(struct fec_switch *fs, int multicast)
{
	void __iomem *hwp = fs->common->fep->hwp;
	unsigned int tmp;

	tmp = readl(hwp + FEC_R_CNTRL);
	tmp &= ~0x8;
	writel(tmp, hwp + FEC_R_CNTRL);

	if (multicast) {
		/* Catch all multicast addresses, so set the
		 * filter to all 1's
		 */
		writel(0xffffffff, hwp + FEC_GRP_HASH_TABLE_HIGH);
		writel(0xffffffff, hwp + FEC_GRP_HASH_TABLE_LOW);
	} else {
		writel(0, hwp + FEC_GRP_HASH_TABLE_HIGH);
		writel(0, hwp + FEC_GRP_HASH_TABLE_LOW);
	}
}  /* hw_set_multicast */

static void hw_set_promisc(struct fec_switch *fs, int promisc)
{
	void __iomem *hwp = fs->common->fep->hwp;
	unsigned int tmp;

	tmp = readl(hwp + FEC_R_CNTRL);
	if (promisc)
		tmp |= 0x8;
	else
		tmp &= ~0x8;
	writel(tmp, hwp + FEC_R_CNTRL);
}  /* hw_set_promisc */

static void dev_set_multicast(struct fec_switch *fs, int multicast)
{
	if (multicast != fs->multi) {
		struct fec_switch_common *fsc = fs->common;
		u8 hw_multi = fsc->hw_multi;

		if (multicast)
			++fsc->hw_multi;
		else
			--fsc->hw_multi;
		fs->multi = multicast;

		/* Turn on/off all multicast mode. */
		if (fsc->hw_multi <= 1 && hw_multi <= 1)
			hw_set_multicast(fs, fsc->hw_multi);
	}
}  /* dev_set_multicast */

static void dev_set_promisc(struct fec_switch *fs, int promisc)
{
	if (promisc != fs->promisc) {
		struct fec_switch_common *fsc = fs->common;
		u8 hw_promisc = fsc->hw_promisc;

		if (promisc)
			++fsc->hw_promisc;
		else
			--fsc->hw_promisc;
		fs->promisc = promisc;

		/* Turn on/off promiscuous mode. */
		if (fsc->hw_promisc <= 1 && hw_promisc <= 1)
			hw_set_promisc(fs, fsc->hw_promisc);
	}
}  /* dev_set_promisc */

static void promisc_reset_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct fec_switch *fs = container_of(dwork,
		struct fec_switch, promisc_reset);

	fs->promisc_addr = 0;
	dev_set_promisc(fs, fs->promisc_mode);
}  /* promisc_reset_work */

static void fec_enet_set_mac_addr(struct net_device *ndev)
{
	struct fec_switch *fs = netdev_priv(ndev);
	struct ksz_sw *sw = fs->port.sw;
	u8 hw_promisc = fs->common->hw_promisc;
	u8 promisc;

	promisc = sw->net_ops->set_mac_addr(sw, ndev, hw_promisc,
		fs->port.first_port);
	fs->promisc_addr = promisc ? 1 : 0;
	dev_set_promisc(fs, fs->promisc_addr || fs->promisc_mode);
	/* A hack to accept changed KSZ9897 IBA response. */
	if (2 == promisc)
		schedule_delayed_work(&fs->promisc_reset, 10);
}  /* fec_enet_set_mac_addr */

static void prep_sw_first(struct ksz_sw *sw, int *port_count,
	int *mib_port_count, int *dev_count, char *dev_name)
{
	*port_count = 1;
	*mib_port_count = 1;
	*dev_count = 1;
	dev_name[0] = '\0';
	sw->net_ops->get_state = get_priv_state;
	sw->net_ops->set_state = set_priv_state;
	sw->net_ops->get_priv_port = get_priv_port;
#if defined(CONFIG_HAVE_KSZ9897) || defined(CONFIG_HAVE_LAN937X)
	sw->net_ops->get_ready = get_net_ready;
#endif
	sw->net_ops->setup_special(sw, port_count, mib_port_count, dev_count);
	sw_update_csum(sw);
}  /* prep_sw_first */

static void prep_sw_dev(struct ksz_sw *sw, struct fec_switch *fs, int i,
	int port_count, int mib_port_count, char *dev_name)
{
#ifndef CONFIG_KSZ_NO_MDIO_BUS
	int phy_mode;
	char phy_id[MII_BUS_ID_SIZE];
	char bus_id[MII_BUS_ID_SIZE];
	struct phy_device *phydev;
#endif

	fs->phy_addr = sw->net_ops->setup_dev(sw, fs->netdev, dev_name,
		&fs->port, i, port_count, mib_port_count);

#ifndef CONFIG_KSZ_NO_MDIO_BUS
	phy_mode = fs->common->fep->phy_interface;
	snprintf(bus_id, MII_BUS_ID_SIZE, "sw.%d", sw->id);
	snprintf(phy_id, MII_BUS_ID_SIZE, PHY_ID_FMT, bus_id, fs->phy_addr);
	phydev = phy_attach(fs->netdev, phy_id, phy_mode);
	if (!IS_ERR(phydev)) {
		fs->netdev->phydev = phydev;
	}
#endif
}  /* prep_sw_dev */

static const struct net_device_ops ksz_fec_netdev_ops;
static const struct ethtool_ops ksz_fec_enet_ethtool_ops;

struct fec_switch *ksz_alloc_etherdev(struct fec_switch_common *fsc, int num_tx_qs, int num_rx_qs)
{
	struct fec_switch *fs;
	struct net_device *ndev = alloc_etherdev_mqs(sizeof(struct fec_switch),
				  num_tx_qs, num_rx_qs);

	if (!ndev)
		return NULL;

	fs = netdev_priv(ndev);
	fs->netdev = ndev;
	fs->common = fsc;
	ndev->netdev_ops = &ksz_fec_netdev_ops;
	ndev->ethtool_ops = &ksz_fec_enet_ethtool_ops;
	return fs;
}

struct net_device *ksz_alloc_etherdev_first(struct device *dev, int size,
		int num_tx_qs, int num_rx_qs)
{
	struct fec_switch_common *fsc;
	struct fec_enet_private *fep;
	struct fec_switch *fs;

	fsc = devm_kzalloc(dev, sizeof(struct fec_switch_common) + size, GFP_KERNEL);
	if (!fsc)
		return NULL;
	fep = (struct fec_enet_private *)(fsc + 1);
	spin_lock_init(&fsc->tx_lock);
	fsc->fep = fep;

	fs = ksz_alloc_etherdev(fsc, num_tx_qs, num_rx_qs);
	if (!fs)
		return NULL;
#if defined(CONFIG_KSZ_IBA_ONLY)
	fs->ndev->features &= ~NETIF_F_TSO;
#endif
	return fs->netdev;
}

static int fec_enet_sw_init(struct fec_switch *fs)
{
	struct fec_enet_private *fep;
	struct ksz_sw *sw;
	int err;
	int i;
	int port_count;
	int dev_count;
	int mib_port_count;
	char dev_label[IFNAMSIZ];
	struct net_device *ndev;
	struct net_device *main_dev;
	struct platform_device *pdev;
	netdev_features_t features;

	sw = fs->port.sw;

	/* This is the main private structure holding hardware information. */
	fep = fs->common->fep;
	fs->parent = sw->dev;
	main_dev = fep->netdev;
	pdev = fep->pdev;
	features = main_dev->features;

	prep_sw_first(sw, &port_count, &mib_port_count, &dev_count, dev_label);

	/* The main switch phydev will not be attached. */
	if (dev_count > 1) {
		struct phy_device *phydev = sw->phy[0];

		phydev->interface = fep->phy_interface;
	}

	/* Save the base device name. */
	strlcpy(dev_label, main_dev->name, IFNAMSIZ);

#ifndef CONFIG_KSZ_SMI
	if (sw->net_ops->setup_mdiobus)
		sw->net_ops->setup_mdiobus(sw, fep->mii_bus);
#endif
	prep_sw_dev(sw, fs, 0, port_count, mib_port_count, dev_label);

	/* Only the main one needs to set adjust_link for configuration. */
	if (main_dev->phydev->mdio.bus &&
	    !main_dev->phydev->adjust_link) {
		main_dev->phydev->adjust_link = fec_enet_adjust_link;

		fep->link = 0;
		fep->speed = 0;
		fep->full_duplex = 0;
	}

	INIT_DELAYED_WORK(&fs->promisc_reset, promisc_reset_work);

	for (i = 1; i < dev_count; i++) {
		fs = ksz_alloc_etherdev(fs->common, fep->num_tx_queues,
				fep->num_rx_queues);
		if (!fs)
			break;

		ndev = fs->netdev;
		ndev->phydev = &fs->dummy_phy;
		ndev->phydev->duplex = 1;
		ndev->phydev->speed = SPEED_1000;
		ndev->phydev->autoneg = 1;

		ndev->watchdog_timeo = TX_TIMEOUT;

		ndev->base_addr = main_dev->base_addr;
		memcpy(ndev->dev_addr, main_dev->dev_addr, ETH_ALEN);

		ndev->hw_features = main_dev->hw_features;
		ndev->features = features;

		SET_NETDEV_DEV(ndev, &pdev->dev);

		prep_sw_dev(sw, fs, i, port_count, mib_port_count, dev_label);
		if (ndev->phydev->mdio.bus)
			ndev->phydev->adjust_link = priv_adjust_link;

		err = register_netdev(ndev);
		if (err) {
			free_netdev(ndev);
			break;
		}

		netif_carrier_off(ndev);
	}

#if !defined(CONFIG_KSZ_IBA_ONLY)
	/*
	 * Adding sysfs support is optional for network device.  It is more
	 * convenient to locate eth0 more or less than spi<bus>.<select>,
	 * especially when the bus number is auto assigned which results in a
	 * very big number.
	 */
	err = init_sw_sysfs(sw, &fs->sysfs, &main_dev->dev);

#ifdef CONFIG_1588_PTP
	if (sw->features & PTP_HW)
		err = init_ptp_sysfs(&fs->ptp_sysfs, &main_dev->dev);
#endif
#ifdef CONFIG_KSZ_DLR
	if (sw->features & DLR_HW)
		err = init_dlr_sysfs(&main_dev->dev);
#endif
#endif

#if defined(CONFIG_KSZ_IBA_ONLY) && !defined(CONFIG_KSZ_NO_MDIO_BUS)
	if (main_dev->phydev->mdio.bus) {
		struct phy_device *phydev = main_dev->phydev;

		phy_attached_info(phydev);
	}
#endif
	sw_device_seen++;

	return 0;
}

#if defined(CONFIG_KSZ_IBA_ONLY)
static int not_ready;

/**
 * netdev_start_iba - Start using IBA for register access
 *
 * This routine starts using IBA for register access.
 */
static void netdev_start_iba(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct ksz_sw *sw = container_of(dwork, struct ksz_sw, set_ops);
	struct ksz_iba_info *iba = &sw->info->iba;
	struct net_device *ndev;
	struct fec_switch *fs;
	int rx_mode;

	if (2 != iba->use_iba)
		return;

	/* Communication is not ready if a cable connection is used. */
	if (sw->net_ops->get_ready && !sw->net_ops->get_ready(iba->dev)) {
		not_ready = true;
		schedule_delayed_work(&sw->set_ops, 1);
		return;
	}

	/* Need some time after link is established. */
	if (not_ready) {
		not_ready = false;
		schedule_delayed_work(&sw->set_ops, 10);
		return;
	}

	ndev = sw->netdev[0];
	fs = netdev_priv(ndev);

	sw->reg = &sw_iba_ops;
	iba->cnt = 0;
	if (ksz_probe_next(sw->dev)) {
		fs->parent = NULL;
		fs->port.sw = NULL;
		return;
	}

#ifdef CONFIG_1588_PTP
	sw->ptp_hw.reg = &ptp_iba_ops;
#endif

	fec_enet_sw_init(fs);

	fs->multi = false;
	fs->promisc = false;

	fs->common->hw_multi = 0;
	fs->common->hw_promisc = 0;

#if 1
	/* Clear MIB counters for debugging. */
	memset(&fs->common->fep->ethtool_stats, 0, fec_get_stat_size());
#endif
	rx_mode = sw->net_ops->open_dev(sw, ndev, ndev->dev_addr);
	dev_set_multicast(fs, (rx_mode & 1) ? 1 : 0);
	fs->promisc_mode = (rx_mode & 2) ? 1 : 0;
	dev_set_promisc(fs, fs->promisc_mode | fs->promisc_addr);
	sw->net_ops->open(sw);

	sw->net_ops->open_port(sw, ndev, &fs->port, &fs->state);
	fs->common->opened++;

	/* Signal IBA initialization is complete. */
	sw->info->iba.use_iba = 3;
}  /* netdev_start_iba */

static int create_sw_dev(struct net_device *ndev, struct fec_switch *fs)
{
	struct sw_priv *ks;
	struct ksz_sw *sw;

	/*
	 * Stop normal traffic from going out until the switch is
	 * configured to block looping frames.
	 */
	netif_carrier_off(ndev);

	ks = kzalloc(sizeof(struct sw_priv), GFP_KERNEL);
	ks->hw_dev = ndev;
	ks->dev = &ndev->dev;

	ks->irq = get_sw_irq();
	if (ks->irq == fs->netdev->phydev->irq)
		ks->irq = 0;

	intr_mode = 1;
	sw_device_present = 0;
	sw = &ks->sw;
	ksz_probe_prep(ks, ndev);

	sw->net_ops->get_state = get_priv_state;
	sw->net_ops->set_state = set_priv_state;
	sw->net_ops->get_priv_port = get_priv_port;
	sw->net_ops->get_ready = get_net_ready;
	sw->netdev[0] = ndev;
	sw->dev_count = 1;

	INIT_DELAYED_WORK(&sw->set_ops, netdev_start_iba);

	sw_set_dev(sw, ndev, ndev->dev_addr);

	fs->parent = sw->dev;
	fs->port.sw = sw;

	not_ready = false;

#ifdef DEBUG_MSG
	init_dbg();
#endif
	return 0;
}
#endif

static void fec_enet_sw_exit(struct fec_switch *fs)
{
	struct net_device *ndev = fs->netdev;
	struct ksz_sw *sw = fs->port.sw;
	int i;

#if !defined(CONFIG_KSZ_IBA_ONLY)
#ifdef CONFIG_KSZ_DLR
	if (sw->features & DLR_HW)
		exit_dlr_sysfs(&ndev->dev);
#endif
#ifdef CONFIG_1588_PTP
	if (sw->features & PTP_HW)
		exit_ptp_sysfs(&fs->ptp_sysfs, &ndev->dev);
#endif
	exit_sw_sysfs(sw, &fs->sysfs, &ndev->dev);
#endif
	for (i = 1; i < sw->dev_count + sw->dev_offset; i++) {
		ndev = sw->netdev[i];
		if (!ndev)
			continue;
		fs = netdev_priv(ndev);
		flush_work(&fs->port.link_update);
		unregister_netdev(ndev);
		if (ndev->phydev->mdio.bus)
			phy_detach(ndev->phydev);
		free_netdev(ndev);
	}
}

void ksz_drv_remove(struct net_device *ndev)
{
	struct fec_switch *fs = netdev_priv(ndev);
	struct ksz_sw *sw = fs->port.sw;

	if (sw) {
#if defined(CONFIG_KSZ_IBA_ONLY)

		/* Still under initialization in IBA-only mode. */
		if (2 == sw->info->iba.use_iba) {
			cancel_delayed_work_sync(&sw->set_ops);

			/* May not started yet. */
			if (2 == sw->info->iba.use_iba) {
				kfree(sw->dev);
				fs->port.sw = NULL;
				sw = NULL;
			}
		} else
#endif
		fec_enet_sw_exit(fs);
	}
	if (fs->common->fep->mii_bus) {
#ifdef CONFIG_KSZ_SMI
		smi_remove(fs->common->sw_pdev);
#endif
	}
	if (sw) {
		flush_work(&fs->port.link_update);
		sw->net_ops->leave_dev(sw);
		if (ndev->phydev) {
			if (ndev->phydev->mdio.bus)
				phy_detach(ndev->phydev);
			ndev->phydev = NULL;
		}
#if defined(CONFIG_KSZ_IBA_ONLY) && defined(DEBUG_MSG)
		exit_dbg();
#endif
	}
#ifdef CONFIG_KSZ8795_EMBEDDED
	ksz8795_exit();
#endif
#ifdef CONFIG_KSZ8895_EMBEDDED
	ksz8895_exit();
#endif
#ifdef CONFIG_KSZ9897_EMBEDDED
	ksz9897_exit();
#endif
#ifdef CONFIG_LAN937X_EMBEDDED
	lan937x_exit();
#endif
}

int ksz_switch_init(struct net_device *ndev)
{
	struct fec_switch *fs = netdev_priv(ndev);
	int ret;
#ifdef CONFIG_KSZ_SMI
	int irq = get_sw_irq();

	ret = smi_probe(&fs->common->sw_pdev, fs->common->fep->mii_bus, irq);
	if (ret)
		return ret;
#endif

#ifdef CONFIG_KSZ8795_EMBEDDED
	ksz8795_init();
#endif
#ifdef CONFIG_KSZ8895_EMBEDDED
	ksz8895_init();
#endif
#ifdef CONFIG_KSZ9897_EMBEDDED
	ksz9897_init();
#endif
#ifdef CONFIG_LAN937X_EMBEDDED
	lan937x_init();
#endif

#if !defined(CONFIG_KSZ_IBA_ONLY)
	ret = fec_enet_sw_chk(fs);
	if (!ret) {
		ndev->features &= ~NETIF_F_TSO;
		ndev->hw_features = ndev->features;
	}
#endif
	return 0;
}

void ksz_enet_sw_init(struct net_device *ndev)
{
#if !defined(CONFIG_KSZ_IBA_ONLY)
	struct fec_switch *fs = netdev_priv(ndev);

	if (fs->port.sw)
		fec_enet_sw_init(fs);
#endif
}

struct fec_enet_private *ksz_get_fep(struct net_device *ndev)
{
	struct fec_switch *fs = netdev_priv(ndev);

	return fs->common->fep;
}

struct phy_device *ksz_get_phydev(struct net_device *ndev)
{
	struct fec_switch *fs = netdev_priv(ndev);
	struct ksz_sw *sw = fs->port.sw;
	struct phy_device *phy_dev = NULL;

	if (sw) {
		phy_dev = ndev->phydev;
		if (!phy_dev) {
			netdev_err(ndev, "no phy\n");
			return ERR_PTR(-ENODEV);
		}
	}
	return phy_dev;
}

void ksz_check_ready(struct net_device *ndev)
{
	struct fec_switch *fs = netdev_priv(ndev);

	fs->common->ready = netif_running(ndev);
}

struct net_device *ksz_sw_rx_proc(struct sk_buff *skb, __u8 *data, struct net_device *ndev)
{
	struct fec_switch *fs = netdev_priv(ndev);
	struct ksz_sw *sw = fs->port.sw;

	if (sw)
		ndev = sw_rx_proc(sw, skb, data);
	return ndev;
}

unsigned long ksz_tx_lock(struct net_device *ndev)
{
	struct fec_switch *fs = netdev_priv(ndev);
	unsigned long flags;

	/* Need to block transmit when manipulating the
	 * transmit queue of all network devices.
	 */
	spin_lock_irqsave(&fs->common->tx_lock, flags);
	return flags;
}

void ksz_tx_wake_unlock(struct net_device *ndev, struct netdev_queue *nq,
		int qid, unsigned long flags)
{
	struct fec_switch *fs = netdev_priv(ndev);

	if (!netif_tx_queue_stopped(nq))
		wake_dev_queues(fs->port.sw, ndev, qid);
	spin_unlock_irqrestore(&fs->common->tx_lock, flags);
}

struct sk_buff * ksz_final_skb(struct sk_buff *skb, struct net_device *ndev)
{
	struct fec_switch *fs = netdev_priv(ndev);
	struct ksz_port *port = &fs->port;
	struct ksz_sw *sw = port->sw;

	if (sw) {
		skb = sw->net_ops->final_skb(sw, skb, ndev, port);
		if (!skb)
			return skb;
	}
	return skb;
}

struct sk_buff * ksz_add_tail(struct sk_buff *skb, struct net_device *ndev)
{
	struct fec_switch *fs = netdev_priv(ndev);
	struct ksz_port *port = &fs->port;
	struct ksz_sw *sw = port->sw;

	if (sw) {
		int len = skb->len;
		int header = 0;

		len = sw->net_ops->get_tx_len(sw, skb, port->first_port,
			&header);

		/* Hardware cannot generate checksum correctly for HSR frame. */
		if (skb->ip_summed && header > VLAN_HLEN) {
			struct sk_buff *nskb;

			nskb = netdev_alloc_skb_ip_align(NULL, len);
			if (nskb) {
				skb_copy_and_csum_dev(skb, nskb->data);
				skb->ip_summed = CHECKSUM_NONE;
				nskb->len = skb->len;

				nskb->dev = skb->dev;
				nskb->sk = skb->sk;
				nskb->protocol = skb->protocol;
				nskb->ip_summed = skb->ip_summed;
				nskb->csum = skb->csum;
				skb_shinfo(nskb)->tx_flags = skb_shinfo(skb)->tx_flags;
				skb_set_network_header(nskb, ETH_HLEN);
				skb_set_tail_pointer(nskb, nskb->len);
			}
			dev_kfree_skb_any(skb);
			skb = nskb;
		}
	}
	return skb;
}

static void ksz_wake_dev_queues(struct net_device *ndev)
{
	struct fec_switch *fs = netdev_priv(ndev);

	wake_dev_queues(fs->port.sw, fs->netdev, 0);
}

static void
fec_drv_shutdown(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct fec_switch *fs = netdev_priv(ndev);
	struct ksz_sw *sw = fs->port.sw;
	int i;
	int dev_count = 1;

	if (sw)
		dev_count = sw->dev_count + sw->dev_offset;

	/* Reverse order as the first network device may be needed. */
	for (i = dev_count - 1; i >= 0; i--) {
		if (sw) {
			ndev = sw->netdev[i];
			if (!ndev)
				continue;
		}
		if (netif_running(ndev)) {
			ndev->netdev_ops->ndo_stop(ndev);

			/* This call turns off the transmit queue. */
			netif_device_detach(ndev);
		}
	}
}

static int ksz_fec_enet_open(struct net_device *ndev)
{
	struct fec_switch *fs = netdev_priv(ndev);
	struct fec_switch_common *fsc = fs->common;
	struct fec_enet_private *fep = fsc->fep;
	struct net_device *pndev = ndev;
	struct ksz_sw *sw = fs->port.sw;
	int ret;

	if (sw) {
		fs->multi = false;
		fs->promisc = false;
		if (fsc->opened > 0) {
			netif_carrier_off(ndev);
			goto skip_hw;
		}

		/* Use first net device for accessing hardware. */
		ndev = fs->netdev;
	}

	ret = fec_enet_open_start(ndev, fep);

	if (ret)
		return ret;

	if (sw) {
		if (0 == fsc->opened) {
			int rx_mode = 0;

			/* Need to wait for adjust_link to start operation. */
			fsc->ready = false;
			fsc->hw_multi = 0;
			fsc->hw_promisc = 0;

#if 1
			/* Clear MIB counters for debugging. */
			memset(&fep->ethtool_stats, 0, fec_get_stat_size());
#endif
			rx_mode = sw->net_ops->open_dev(sw, ndev,
				ndev->dev_addr);
			dev_set_multicast(fs, (rx_mode & 1) ? 1 : 0);
			fs->promisc_mode = (rx_mode & 2) ? 1 : 0;
			dev_set_promisc(fs, fs->promisc_mode | fs->promisc_addr);
			sw->net_ops->open(sw);
		}
skip_hw:
		sw->net_ops->open_port(sw, pndev, &fs->port, &fs->state);
		fsc->opened++;
	}
	if (!sw)
		phy_start(ndev->phydev);
	netif_tx_start_all_queues(ndev);

	if (sw && fsc->opened > 1)
		return 0;
	fec_set_qos(ndev, fep);

#if defined(CONFIG_KSZ_IBA_ONLY)
	if (!sw)
		create_sw_dev(ndev, fs);
#endif
	return 0;
}

static int ksz_fec_enet_close(struct net_device *ndev)
{
	struct fec_switch *fs = netdev_priv(ndev);
	struct fec_switch_common *fsc = fs->common;
	struct fec_enet_private *fep = fsc->fep;
	struct net_device *pndev = ndev;
	struct ksz_sw *sw = fs->port.sw;

#if defined(CONFIG_KSZ_IBA_ONLY)
	if (sw) {

		/* Still under initialization in IBA-only mode. */
		if (2 == sw->info->iba.use_iba) {
			cancel_delayed_work_sync(&sw->set_ops);

			/* May not started yet. */
			if (2 == sw->info->iba.use_iba) {
				kfree(sw->dev);
				fs->port.sw = NULL;
				sw = NULL;
			}
		}
	}
#endif
	if (sw) {
		dev_set_multicast(fs, false);
		dev_set_promisc(fs, false);
		sw->net_ops->close_port(sw, pndev, &fs->port);
		fsc->opened--;
		if (fsc->opened > 0) {
			if (netif_device_present(ndev))
				netif_tx_disable(ndev);
			netif_carrier_off(ndev);
			return 0;
		}

		/* Use first net device for accessing hardware. */
		ndev = fep->netdev;
		if (!fsc->opened) {
			sw->net_ops->close(sw);
			sw->net_ops->stop(sw, true);
		}

#if defined(CONFIG_KSZ_IBA_ONLY)
		sw->net_ops->leave_dev(sw);
		ksz_remove(sw->dev);
		fs->port.sw = NULL;
		sw = NULL;
#endif
	}

	/* Reset ready indication. */
	fsc->ready = false;

	if (!(sw))
		phy_stop(ndev->phydev);

	if (netif_device_present(ndev)) {
		napi_disable(&fep->napi);
		netif_tx_disable(ndev);
		fec_stop(ndev);
	}

	if (!(sw)) {
		phy_disconnect(ndev->phydev);
		ndev->phydev = NULL;
	}
	return fec_enet_close_finish(ndev, fep);
}

static netdev_tx_t ksz_fec_enet_start_xmit(struct sk_buff *skb,
		struct net_device *ndev)
{
	unsigned short queue = skb_get_queue_mapping(skb);
	struct netdev_queue *nq = netdev_get_tx_queue(ndev, queue);
	struct fec_switch *fs = netdev_priv(ndev);
	struct fec_switch_common *fsc = fs->common;
	struct fec_enet_private *fep = fsc->fep;
	unsigned long flags;
	netdev_tx_t ret;

	/* May be called from switch driver. */
	if (netif_tx_queue_stopped(nq))
		return NETDEV_TX_BUSY;

	spin_lock_irqsave(&fsc->tx_lock, flags);

	ret = fec_enet_start_xmit(skb, ndev, fep, queue, nq);
	if (netif_tx_queue_stopped(nq))
		stop_dev_queues(fs, fs->port.sw, ndev, queue);

	spin_unlock_irqrestore(&fsc->tx_lock, flags);
	return ret;
}

u16 ksz_fec_enet_select_queue(struct net_device *ndev, struct sk_buff *skb,
			  void *accel_priv, select_queue_fallback_t fallback)
{
	struct fec_switch *fs = netdev_priv(ndev);
	struct fec_enet_private *fep = fs->common->fep;

	return fec_enet_select_queue(ndev, skb, accel_priv, fallback, fep);
}

static void ksz_set_multicast_list(struct net_device *ndev)
{
	struct fec_switch *fs = netdev_priv(ndev);
	struct fec_switch_common *fsc = fs->common;
	struct fec_enet_private *fep = fsc->fep;
	struct ksz_sw *sw = fs->port.sw;
	int multicast = ((ndev->flags & IFF_ALLMULTI) == IFF_ALLMULTI);
	bool do_hash = false;

	if (sw) {
		dev_set_promisc(fs, fs->promisc_mode | fs->promisc_addr |
			(((ndev->flags & IFF_PROMISC) == IFF_PROMISC) ? 1 : 0));
		if (!(ndev->flags & IFF_ALLMULTI)) {
			if (sw->dev_count > 1) {
				if ((ndev->flags & IFF_MULTICAST) &&
				    !netdev_mc_empty(ndev))
					sw->net_ops->set_multi(sw, ndev,
						&fs->port);
				multicast |= ((ndev->flags & IFF_MULTICAST) ==
					IFF_MULTICAST) << 1;
			} else if (!fsc->hw_multi && !netdev_mc_empty(ndev)) {
				do_hash = true;
			}
		}
		dev_set_multicast(fs, multicast);
		if (do_hash)
			goto multicast_hash;
		return;
	}
	if (check_promisc_allmulti(ndev, fep))
		return;
multicast_hash:
	set_multicast_list1(ndev, fep);
}

static void ksz_fec_timeout(struct net_device *ndev)
{
	struct fec_switch *fs = netdev_priv(ndev);

	return fec_timeout(ndev, fs->common->fep);
}

static int ksz_fec_set_mac_address(struct net_device *ndev, void *p)
{
	struct fec_switch *fs = netdev_priv(ndev);
	struct sockaddr *addr = p;

	if (addr) {
		if (!is_valid_ether_addr(addr->sa_data))
			return -EADDRNOTAVAIL;
		memcpy(ndev->dev_addr, addr->sa_data, ndev->addr_len);

		if ((fs->port.sw))
			fec_enet_set_mac_addr(ndev);
	}
	return fec_set_mac_address(ndev, fs->common->fep);
}

static int ksz_fec_enet_ioctl(struct net_device *ndev, struct ifreq *rq, int cmd)
{
	struct fec_switch *fs = netdev_priv(ndev);
	int result;
	struct ksz_sw *sw = fs->port.sw;
#ifdef CONFIG_1588_PTP
	struct ptp_info *ptp;
#endif

	result = -EOPNOTSUPP;
	switch (cmd) {
#ifdef CONFIG_1588_PTP
	case SIOCSHWTSTAMP:
		if (sw && (sw->features & PTP_HW)) {
			int i;
			int p;
			u16 ports;

			ports = 0;
			for (i = 0, p = fs->port.first_port;
			     i < fs->port.port_cnt; i++, p++)
				ports |= (1 << p);
			ptp = &sw->ptp_hw;
			result = ptp->ops->hwtstamp_ioctl(ptp, rq, ports);
		}
		break;
	case SIOCDEVPRIVATE + 15:
		if (sw && (sw->features & PTP_HW)) {
			ptp = &sw->ptp_hw;
			result = ptp->ops->dev_req(ptp, rq->ifr_data, NULL);
		}
		break;
#endif
#ifdef CONFIG_KSZ_MRP
	case SIOCDEVPRIVATE + 14:
		if (sw && (sw->features & MRP_SUPPORT)) {
			struct mrp_info *mrp = &sw->mrp;

			result = mrp->ops->dev_req(mrp, rq->ifr_data);
		}
		break;
#endif
	case SIOCDEVPRIVATE + 13:
		if (sw) {
			result = sw->ops->dev_req(sw, rq->ifr_data, NULL);
		}
		break;
	default:
		result = -EOPNOTSUPP;
	}
	if (result != -EOPNOTSUPP)
		return result;
	return fec_enet_ioctl(ndev, rq, cmd, fs->common->fep);
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void ksz_fec_poll_controller(struct net_device *ndev)
{
	struct fec_switch *fs = netdev_priv(ndev);
	struct fec_enet_private *fep = fs->common->fep;

	return fec_poll_controller(fep->netdev, fep);
}
#endif

static int ksz_fec_set_features(struct net_device *ndev,
		netdev_features_t features)
{
	struct fec_switch *fs = netdev_priv(ndev);

	return fec_set_features(ndev, features, fs->common->fep);
}

static int ksz_fec_enet_add_vid(struct net_device *ndev, __be16 proto, u16 vid)
{
	struct fec_switch *fs = netdev_priv(ndev);
	struct ksz_sw *sw = fs->port.sw;

	if (sw)
		sw->net_ops->add_vid(sw, vid);
	return 0;
}

static int ksz_fec_enet_kill_vid(struct net_device *ndev, __be16 proto, u16 vid)
{
	struct fec_switch *fs = netdev_priv(ndev);
	struct ksz_sw *sw = fs->port.sw;

	if (sw)
		sw->net_ops->kill_vid(sw, vid);
	return 0;
}

static const struct net_device_ops ksz_fec_netdev_ops = {
	.ndo_open		= ksz_fec_enet_open,
	.ndo_stop		= ksz_fec_enet_close,
	.ndo_start_xmit		= ksz_fec_enet_start_xmit,
	.ndo_select_queue       = ksz_fec_enet_select_queue,
	.ndo_set_rx_mode	= ksz_set_multicast_list,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_tx_timeout		= ksz_fec_timeout,
	.ndo_set_mac_address	= ksz_fec_set_mac_address,
	.ndo_do_ioctl		= ksz_fec_enet_ioctl,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= ksz_fec_poll_controller,
#endif
	.ndo_set_features	= ksz_fec_set_features,
	.ndo_vlan_rx_add_vid	= ksz_fec_enet_add_vid,
	.ndo_vlan_rx_kill_vid	= ksz_fec_enet_kill_vid,
};


static void ksz_fec_enet_get_drvinfo(struct net_device *ndev,
				 struct ethtool_drvinfo *info)
{
	struct fec_switch *fs = netdev_priv(ndev);

	fec_enet_get_drvinfo(ndev, info, fs->common->fep);
}

static int ksz_fec_enet_get_regs_len(struct net_device *ndev)
{
	struct fec_switch *fs = netdev_priv(ndev);

	return fec_enet_get_regs_len(ndev, fs->common->fep);
}

static void ksz_fec_enet_get_regs(struct net_device *ndev,
			      struct ethtool_regs *regs, void *regbuf)
{
	struct fec_switch *fs = netdev_priv(ndev);

	fec_enet_get_regs(ndev, regs, regbuf, fs->common->fep);
}

static int ksz_fec_enet_get_coalesce(struct net_device *ndev,
		struct ethtool_coalesce *ec)
{
	struct fec_switch *fs = netdev_priv(ndev);

	return fec_enet_get_coalesce(ndev, ec, fs->common->fep);
}

static int ksz_fec_enet_set_coalesce(struct net_device *ndev, struct ethtool_coalesce *ec)
{
	struct fec_switch *fs = netdev_priv(ndev);

	return fec_enet_set_coalesce(ndev, ec, fs->common->fep);
}

#if !defined(CONFIG_M5272)
static void ksz_fec_enet_get_pauseparam(struct net_device *ndev,
				    struct ethtool_pauseparam *pause)
{
	struct fec_switch *fs = netdev_priv(ndev);

	fec_enet_get_pauseparam(ndev, pause, fs->common->fep);
}

static int ksz_fec_enet_set_pauseparam(struct net_device *ndev,
				   struct ethtool_pauseparam *pause)
{
	struct fec_switch *fs = netdev_priv(ndev);

	return fec_enet_set_pauseparam(ndev, pause, fs->common->fep);
}

static void ksz_fec_enet_get_ethtool_stats(struct net_device *ndev,
				       struct ethtool_stats *stats, u64 *data)
{
	struct fec_switch *fs = netdev_priv(ndev);

	/* If current device is not running switch to the main device
	 * just to invoke fec_enet_update_ethtool_stats.
	 */
	if (!netif_running(ndev))
		ndev = fs->common->fep->netdev;

	fec_enet_get_ethtool_stats(ndev, stats, data, fs->common->fep);
}
#endif

static int ksz_fec_enet_get_ts_info(struct net_device *ndev,
				struct ethtool_ts_info *info)
{
	struct fec_switch *fs = netdev_priv(ndev);
#ifdef CONFIG_1588_PTP
	struct ksz_sw *sw = fs->port.sw;

	if (sw && (sw->features & PTP_HW)) {
		struct ptp_info *ptp = &sw->ptp_hw;

		return ptp->ops->get_ts_info(ptp, ndev, info);
	}
#endif
	return fec_enet_get_ts_info(ndev, info, fs->common->fep);
}

static int ksz_fec_enet_get_tunable(struct net_device *ndev,
				const struct ethtool_tunable *tuna,
				void *data)
{
	struct fec_switch *fs = netdev_priv(ndev);

	return fec_enet_get_tunable(ndev, tuna, data, fs->common->fep);
}

static int ksz_fec_enet_set_tunable(struct net_device *ndev,
				const struct ethtool_tunable *tuna,
				const void *data)
{
	struct fec_switch *fs = netdev_priv(ndev);

	return fec_enet_set_tunable(ndev, tuna, data, fs->common->fep);
}

static void ksz_fec_enet_get_wol(struct net_device *ndev,
		struct ethtool_wolinfo *wol)
{
	struct fec_switch *fs = netdev_priv(ndev);

	fec_enet_get_wol(ndev, wol, fs->common->fep);
}

static int ksz_fec_enet_set_wol(struct net_device *ndev,
		struct ethtool_wolinfo *wol)
{
	struct fec_switch *fs = netdev_priv(ndev);

	return fec_enet_set_wol(ndev, wol, fs->common->fep);
}

static int ksz_fec_enet_get_eee(struct net_device *ndev,
		struct ethtool_eee *edata)
{
	struct fec_switch *fs = netdev_priv(ndev);

	return fec_enet_get_eee(ndev, edata, fs->common->fep);
}

static int ksz_fec_enet_set_eee(struct net_device *ndev,
		struct ethtool_eee *edata)
{
	struct fec_switch *fs = netdev_priv(ndev);

	return fec_enet_set_eee(ndev, edata, fs->common->fep);
}

static const struct ethtool_ops ksz_fec_enet_ethtool_ops = {
	.get_drvinfo		= ksz_fec_enet_get_drvinfo,
	.get_regs_len		= ksz_fec_enet_get_regs_len,
	.get_regs		= ksz_fec_enet_get_regs,
	.nway_reset		= phy_ethtool_nway_reset,
	.get_link		= ethtool_op_get_link,
	.get_coalesce		= ksz_fec_enet_get_coalesce,
	.set_coalesce		= ksz_fec_enet_set_coalesce,
#ifndef CONFIG_M5272
	.get_pauseparam		= ksz_fec_enet_get_pauseparam,
	.set_pauseparam		= ksz_fec_enet_set_pauseparam,
	.get_strings		= fec_enet_get_strings,
	.get_ethtool_stats	= ksz_fec_enet_get_ethtool_stats,
	.get_sset_count		= fec_enet_get_sset_count,
#endif
	.get_ts_info		= ksz_fec_enet_get_ts_info,
	.get_tunable		= ksz_fec_enet_get_tunable,
	.set_tunable		= ksz_fec_enet_set_tunable,
	.get_wol		= ksz_fec_enet_get_wol,
	.set_wol		= ksz_fec_enet_set_wol,
	.get_eee		= ksz_fec_enet_get_eee,
	.set_eee		= ksz_fec_enet_set_eee,
	.get_link_ksettings	= phy_ethtool_get_link_ksettings,
	.set_link_ksettings	= phy_ethtool_set_link_ksettings,
};
