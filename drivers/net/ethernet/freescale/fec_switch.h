#if defined(CONFIG_KSZ_SWITCH) || defined(CONFIG_LAN937X_SWITCH)
#include "../micrel/ksz_sw.h"
#endif

struct fec_switch_common {
	struct fec_enet_private	*fep;
	struct platform_device	*sw_pdev;
	u32			ready:1;
	u8			opened;
	u8			hw_multi;
	u8			hw_promisc;
	spinlock_t		tx_lock;
};

struct fec_switch {
	struct fec_switch_common *common;
	struct ksz_port		port;
	struct net_device	*netdev;
	struct phy_device	dummy_phy;
	int			phy_addr;
	u8			state;
	u32			multi:1;
	u32			promisc:1;
	u32			promisc_addr:1;
	u32			promisc_mode:1;
	void			*parent;
	struct delayed_work	promisc_reset;
	struct ksz_sw_sysfs	sysfs;
#ifdef CONFIG_1588_PTP
	struct ksz_ptp_sysfs	ptp_sysfs;
#endif
};

/* fec_main routines called */
static int fec_enet_open_start(struct net_device *ndev, struct fec_enet_private *fep);
static int fec_get_stat_size(void);
static void fec_enet_adjust_link(struct net_device *ndev);
#ifdef CONFIG_KSZ_SMI
static int fec_enet_mdio_read(struct mii_bus *bus, int mii_id, int regnum);
static int fec_enet_mdio_write(struct mii_bus *bus, int mii_id, int regnum,
			   u16 value);
#endif
void fec_set_qos(struct net_device *ndev, struct fec_enet_private *fep);
static void fec_stop(struct net_device *ndev);
static int fec_enet_close_finish(struct net_device *ndev, struct fec_enet_private *fep);
static netdev_tx_t fec_enet_start_xmit(struct sk_buff *skb,
		struct net_device *ndev, struct fec_enet_private *fep,
		unsigned short queue, struct netdev_queue *nq);
u16 fec_enet_select_queue(struct net_device *ndev, struct sk_buff *skb,
			  void *accel_priv, select_queue_fallback_t fallback,
			  struct fec_enet_private *fep);
static int check_promisc_allmulti(struct net_device *ndev, struct fec_enet_private *fep);
static void set_multicast_list1(struct net_device *ndev,
		struct fec_enet_private *fep);
static void fec_timeout(struct net_device *ndev, struct fec_enet_private *fep);
static int fec_set_mac_address(struct net_device *ndev, struct fec_enet_private *fep);
static int fec_enet_ioctl(struct net_device *ndev, struct ifreq *rq, int cmd,
		struct fec_enet_private *fep);
static int fec_set_features(struct net_device *netdev,
		netdev_features_t features,
		struct fec_enet_private *fep);
#ifdef CONFIG_NET_POLL_CONTROLLER
static void fec_poll_controller(struct net_device *dev,
		struct fec_enet_private *fep);
#endif
static void fec_enet_get_drvinfo(struct net_device *ndev,
				 struct ethtool_drvinfo *info,
				 struct fec_enet_private *fep);
static int fec_enet_get_regs_len(struct net_device *ndev,
		struct fec_enet_private *fep);
static void fec_enet_get_regs(struct net_device *ndev,
			      struct ethtool_regs *regs, void *regbuf,
			      struct fec_enet_private *fep);
static int fec_enet_get_coalesce(struct net_device *ndev,
		struct ethtool_coalesce *ec,
		struct fec_enet_private *fep);
static int fec_enet_set_coalesce(struct net_device *ndev,
		struct ethtool_coalesce *ec,
		struct fec_enet_private *fep);
static void fec_enet_get_pauseparam(struct net_device *ndev,
				    struct ethtool_pauseparam *pause,
				    struct fec_enet_private *fep);
static int fec_enet_set_pauseparam(struct net_device *ndev,
				   struct ethtool_pauseparam *pause,
				   struct fec_enet_private *fep);
static void fec_enet_get_ethtool_stats(struct net_device *dev,
				       struct ethtool_stats *stats, u64 *data,
				       struct fec_enet_private *fep);
static int fec_enet_get_ts_info(struct net_device *ndev,
				struct ethtool_ts_info *info,
				struct fec_enet_private *fep);
static int fec_enet_get_tunable(struct net_device *netdev,
				const struct ethtool_tunable *tuna,
				void *data,
				struct fec_enet_private *fep);
static int fec_enet_set_tunable(struct net_device *netdev,
				const struct ethtool_tunable *tuna,
				const void *data,
				struct fec_enet_private *fep);
static int fec_enet_get_tunable(struct net_device *netdev,
				const struct ethtool_tunable *tuna,
				void *data,
				struct fec_enet_private *fep);
static int fec_enet_set_tunable(struct net_device *netdev,
				const struct ethtool_tunable *tuna,
				const void *data,
				struct fec_enet_private *fep);
static void fec_enet_get_wol(struct net_device *ndev,
		struct ethtool_wolinfo *wol,
		struct fec_enet_private *fep);
static int fec_enet_set_wol(struct net_device *ndev,
		struct ethtool_wolinfo *wol,
		struct fec_enet_private *fep);
static int fec_enet_get_eee(struct net_device *ndev,
		struct ethtool_eee *edata,
		struct fec_enet_private *fep);
static int fec_enet_set_eee(struct net_device *ndev,
		struct ethtool_eee *edata,
		struct fec_enet_private *fep);
static void fec_enet_get_strings(struct net_device *netdev,
	u32 stringset, u8 *data);
static int fec_enet_get_sset_count(struct net_device *dev, int sset);

