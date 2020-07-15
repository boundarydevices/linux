linux-imx6
==========

Boundary Devices Kernel tree for i.MX6/i.MX7/i.MX8.

This repository contains kernel source trees for Boundary Devices'
i.MX6/i.MX7/i.MX8 based boards:

* [BD-SL-i.MX6 (SABRE Lite)][sabrelite]
* [Nitrogen6X][nitrogen6x]
* [Nitrogen6X SOM][nitrogen6x-som]
* [Nitrogen6X SOM v2][nitrogen6x-somv2]
* [Nitrogen6 Lite][nitrogen6-lite]
* [Nitrogen6 MAX][nitrogen6-max]
* [Nitrogen6 VM][nitrogen6-vm]
* [Nitrogen6 SoloX][nitrogen6-sx]
* [Nitrogen7][nitrogen7]
* [Nitrogen8M][nitrogen8m]
* [Nitrogen8M SOM][nitrogen8m-som]
* [Nitrogen8M Mini][nitrogen8mm]
* [Nitrogen8M Mini SOM][nitrogen8mm-som]
* [Nitrogen8M Nano][nitrogen8mn]
* [Nitrogen8M Nano SOM][nitrogen8mn-som]

It is based largely on NXP's kernel tree at [codeaurora.org/external/imx][nxp].

Latest kernel releases:

Android
-----------
Latest KitKat sources are in branch [boundary-imx-kk4.4.3\_2.0.1-ga][latest-kitkat]

Latest Lollipop sources are in branch [boundary-imx-l5.1.1\_2.1.0-ga][latest-lollipop]

Latest Marshmallow sources are in branch [boundary-imx-m6.0.1\_1.0.0-ga][latest-marshmallow]

Latest Nougat sources are in branch [boundary-imx-n7.1.1\_1.0.0-ga][latest-nougat]

Latest Oreo sources are in branch [boundary-imx-o8.1.0\_1.3.0\_8m-ga][latest-oreo]

Latest Pie sources are in branch [boundary-imx-p9.0.0\_1.0.0-gaa][latest-pie]

Latest Android 10 sources are in branch [boundary-android-10.0.0\_1.0.0][latest-10]

Non-Android
------------------
The latest 3.0.35 kernel is in branch [boundary-imx\_3.0.35\_4.1.0][latest-3.0.35]

The latest 3.10.x kernel is in branch [boundary-imx\_3.10.53\_1.1.1\_ga][latest-3.10.x]

The latest 3.14.x kernel is in branch [boundary-imx\_3.14.52\_1.1.0\_ga][latest-3.14.x]

The latest 4.1.15 kernel is in branch [boundary-imx\_4.1.15\_2.0.0\_ga][latest-4.1.15]

The latest 4.9.x kernel is in branch [boundary-imx\_4.9.x\_2.0.0\_ga][latest-4.9.x]

The latest 4.14.x kernel is in branch [boundary-imx\_4.14.x\_2.0.0\_ga][latest-4.14.x]

The latest 4.19.x kernel is in branch [boundary-imx\_4.19.x\_1.1.0][latest-4.19.x]

The latest 5.4.x kernel is in branch [boundary-imx\_5.4.x\_2.1.0][latest-5.4.x]

These branches are based on Freescale releases of the same name (minus the 'boundary').


[nxp]: https://source.codeaurora.org/external/imx/linux-imx/ "NXP Git repository"
[sabrelite]:https://boundarydevices.com/wiki/bd-sl-imx6 "SABRE Lite product page"
[nitrogen6x]:https://boundarydevices.com/wiki/nitrogen6x "Nitrogen6X product page"
[nitrogen6x-som]:https://boundarydevices.com/wiki/nitrogen6x-som-v1 "Nitrogen6X SOM product page"
[nitrogen6x-somv2]:https://boundarydevices.com/wiki/nitrogen6x-som-v2 "Nitrogen6X SOM v2 product page"
[nitrogen6-lite]:https://boundarydevices.com/wiki/nitrogen6_lite "Nitrogen6_Lite product page"
[nitrogen6-max]:https://boundarydevices.com/wiki/nitrogen6max "Nitrogen6_MAX product page"
[nitrogen6-vm]:https://boundarydevices.com/wiki/nitrogen6vm "Nitrogen6_VM product page"
[nitrogen6-sx]:https://boundarydevices.com/wiki/nitrogen6_solox "Nit6_SoloX product page"
[nitrogen7]:https://boundarydevices.com/wiki/nitrogen7 "Nitrogen7 product page"
[nitrogen8m]:https://boundarydevices.com/wiki/nitrogen8m-sbc "Nitrogen8M product page"
[nitrogen8m-som]:https://boundarydevices.com/wiki/nitrogen8m-som "Nitrogen8M SOM product page"
[nitrogen8mm]:https://boundarydevices.com/wiki/nitrogen8m-mini-sbc "Nitrogen8M_Mini product page"
[nitrogen8mm-som]:https://boundarydevices.com/wiki/Nitrogen8M_Mini-SOM "Nitrogen8M_Mini SOM product page"
[nitrogen8mn]:https://boundarydevices.com/wiki/nitrogen8m-nano-sbc "Nitrogen8M Nano product page"
[nitrogen8mn-som]:https://boundarydevices.com/wiki/nitrogen8m-nano-som "Nitrogen8M Nano SOM product page"
[latest-jellybean]:http://github.com/boundarydevices/linux-imx6/tree/boundary-jb4.3_1.0.0-ga "Boundary Jellybean kernel tree"
[latest-kitkat]:http://github.com/boundarydevices/linux-imx6/tree/boundary-imx-kk4.4.3_2.0.1-ga "Boundary KitKat kernel tree"
[latest-lollipop]:http://github.com/boundarydevices/linux-imx6/tree/boundary-imx-l5.1.1_2.1.0-ga "Boundary Lollipop kernel tree"
[latest-marshmallow]:http://github.com/boundarydevices/linux-imx6/tree/boundary-imx-m6.0.1_1.0.0-ga "Boundary Marshmallow kernel tree"
[latest-nougat]:http://github.com/boundarydevices/linux-imx6/tree/boundary-imx-n7.1.1_1.0.0-ga "Boundary Nougat kernel tree"
[latest-oreo]:http://github.com/boundarydevices/linux-imx6/tree/boundary-imx-o8.1.0_1.3.0_8m-ga "Boundary Oreo kernel tree"
[latest-pie]:http://github.com/boundarydevices/linux-imx6/tree/boundary-imx-p9.0.0_1.0.0-ga "Boundary Pie kernel tree"
[latest-10]:http://github.com/boundarydevices/linux-imx6/tree/boundary-android-10.0.0_1.0.0 "Boundary Android 10 kernel tree"
[latest-3.0.35]:http://github.com/boundarydevices/linux-imx6/tree/boundary-imx_3.0.35_4.1.0 "Boundary 3.0.35 4.1.0 kernel tree"
[latest-3.10.x]:http://github.com/boundarydevices/linux-imx6/tree/boundary-imx_3.10.53_1.1.1_ga "Boundary 3.10.53-1.1.1 GA kernel tree"
[latest-3.14.x]:http://github.com/boundarydevices/linux-imx6/tree/boundary-imx_3.14.52_1.1.0_ga "Boundary 3.14.52-1.1.0 GA kernel tree"
[latest-4.1.15]:http://github.com/boundarydevices/linux-imx6/tree/boundary-imx_4.1.15_2.0.0_ga "Boundary 4.1.15-2.0.0 GA kernel tree"
[latest-4.9.x]:https://github.com/boundarydevices/linux-imx6/tree/boundary-imx_4.9.x_2.0.0_ga "Boundary 4.9.x-2.0.0 GA kernel tree"
[latest-4.14.x]:https://github.com/boundarydevices/linux-imx6/tree/boundary-imx_4.14.x_2.0.0_ga "Boundary 4.14.x-2.0.0 GA kernel tree"
[latest-4.19.x]:https://github.com/boundarydevices/linux-imx6/tree/boundary-imx_4.19.x_1.1.0 "Boundary 4.19.x-1.1.0 kernel tree"
[latest-5.4.x]:https://github.com/boundarydevices/linux-imx6/tree/boundary-imx_5.4.x_2.1.0 "Boundary 5.4.x-2.1.0 kernel tree"
