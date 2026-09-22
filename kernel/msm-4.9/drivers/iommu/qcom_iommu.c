/*
 * IOMMU API for QCOM secure IOMMUs.  Somewhat based on arm-smmu.c
 *
 * Copyright (C) 2013 ARM Limited
 * Copyright (C) 2017 Red Hat
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/io-64-nonatomic-hi-lo.h>
#include <linux/iommu.h>
#include <linux/iopoll.h>
#include <linux/kconfig.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_iommu.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include <soc/qcom/scm.h>
#include <soc/qcom/msm_tz_smmu.h>

#undef writel_relaxed
#undef writeq_relaxed
#define writel_relaxed(v, c)	\
	((void)__raw_writel((__force u32)cpu_to_le32(v), (c)))
#define writeq_relaxed(v, c)	\
	((void)__raw_writeq((__force u64)cpu_to_le64(v), (c)))

#include "io-pgtable.h"

#define SMMU_INTR_SEL_NS     0x2000

#define ARM_SMMU_CB_SCTLR		0x0
#define ARM_SMMU_CB_ACTLR		0x4
#define ARM_SMMU_CB_RESUME		0x8
#define ARM_SMMU_CB_TTBCR2		0x10
#define ARM_SMMU_CB_TTBR0		0x20
#define ARM_SMMU_CB_TTBR1		0x28
#define ARM_SMMU_CB_TTBCR		0x30
#define ARM_SMMU_CB_CONTEXTIDR		0x34
#define ARM_SMMU_CB_S1_MAIR0		0x38
#define ARM_SMMU_CB_S1_MAIR1		0x3c
#define ARM_SMMU_CB_FSR			0x58
#define ARM_SMMU_CB_FAR			0x60
#define ARM_SMMU_CB_FSYNR0		0x68
#define ARM_SMMU_CB_S1_TLBIVA		0x600
#define ARM_SMMU_CB_S1_TLBIASID		0x610
#define ARM_SMMU_CB_S1_TLBIVAL		0x620
#define ARM_SMMU_CB_TLBSYNC		0x7f0
#define ARM_SMMU_CB_TLBSTATUS		0x7f4

#define SCTLR_S1_ASIDPNE		(1 << 12)
#define SCTLR_CFCFG			(1 << 7)
#define SCTLR_CFIE			(1 << 6)
#define SCTLR_CFRE			(1 << 5)
#define SCTLR_E				(1 << 4)
#define SCTLR_AFE			(1 << 2)
#define SCTLR_TRE			(1 << 1)
#define SCTLR_M				(1 << 0)

#define RESUME_TERMINATE		(1 << 0)

#define TTBCR2_SEP_SHIFT		15
#define TTBCR2_SEP_UPSTREAM		(0x7 << TTBCR2_SEP_SHIFT)

#define TTBRn_ASID_SHIFT		48

#define FSR_MULTI			(1 << 31)
#define FSR_SS				(1 << 30)
#define FSR_UUT				(1 << 8)
#define FSR_ASF				(1 << 7)
#define FSR_TLBLKF			(1 << 6)
#define FSR_TLBMCF			(1 << 5)
#define FSR_EF				(1 << 4)
#define FSR_PF				(1 << 3)
#define FSR_AFF				(1 << 2)
#define FSR_TF				(1 << 1)
#define FSR_FAULT			(FSR_MULTI | FSR_SS | FSR_UUT | \
					 FSR_EF | FSR_PF | FSR_TF | FSR_ASF | \
					 FSR_TLBMCF | FSR_TLBLKF)

#define QCOM_IOMMU_MAX_CLKS		8

struct qcom_iommu_ctx;

struct qcom_iommu_dev {
	struct device		*dev;
	struct clk		*clks[QCOM_IOMMU_MAX_CLKS];
	unsigned int		 num_clks;
	void __iomem		*local_base;
	u32			 sec_id;
	/* qcom,enable-static-cb: TZ pre-assigns the banks.  Recorded only. */
	bool			 static_cb;
	u8			 max_asid;
	struct qcom_iommu_ctx	*ctxs[0];   /* indexed by asid */
};

struct qcom_iommu_ctx {
	struct device		*dev;
	void __iomem		*base;
	phys_addr_t		 phys_base;
	bool			 static_cb;
	bool			 secure_init;
	u8			 asid;      /* asid and ctx bank # are 1:1 */
	struct iommu_domain	*domain;
};

struct qcom_iommu_domain {
	struct io_pgtable_ops	*pgtbl_ops;
	spinlock_t		 pgtbl_lock;
	struct mutex		 init_mutex; /* Protects iommu pointer */
	struct iommu_domain	 domain;
	struct qcom_iommu_dev	*iommu;
	bool			 static_cb;
	/*
	 * Set at attach, cleared at detach.  The io-pgtable cookie is this
	 * domain rather than the fwspec, because free_io_pgtable_ops() from
	 * qcom_iommu_domain_free() runs TLB callbacks and the device's fwspec
	 * may already have been freed by remove_device() -- reaching through
	 * it there was a use-after-free.
	 */
	struct iommu_fwspec	*fwspec;
};

static struct qcom_iommu_domain *to_qcom_iommu_domain(struct iommu_domain *dom)
{
	return container_of(dom, struct qcom_iommu_domain, domain);
}

static const struct iommu_ops qcom_iommu_ops;

static struct qcom_iommu_dev *to_iommu(struct iommu_fwspec *fwspec)
{
	if (!fwspec || fwspec->ops != &qcom_iommu_ops)
		return NULL;
	return fwspec->iommu_priv;
}

static struct qcom_iommu_ctx *to_ctx(struct qcom_iommu_domain *d,
				     unsigned int asid)
{
	struct qcom_iommu_dev *qcom_iommu = d->iommu;

	if (!qcom_iommu)
		return NULL;
	return qcom_iommu->ctxs[asid];
}

static inline void
iommu_writel(struct qcom_iommu_ctx *ctx, unsigned int reg, u32 val)
{
	writel_relaxed(val, ctx->base + reg);
}

static inline void
iommu_writeq(struct qcom_iommu_ctx *ctx, unsigned int reg, u64 val)
{
	writeq_relaxed(val, ctx->base + reg);
}

static inline u32
iommu_readl(struct qcom_iommu_ctx *ctx, unsigned int reg)
{
	return readl_relaxed(ctx->base + reg);
}

static inline u64
iommu_readq(struct qcom_iommu_ctx *ctx, unsigned int reg)
{
	return readq_relaxed(ctx->base + reg);
}

static void qcom_iommu_tlb_sync(void *cookie)
{
	struct qcom_iommu_domain *qcom_domain = cookie;
	struct iommu_fwspec *fwspec = qcom_domain->fwspec;
	unsigned int i;

	if (!fwspec)
		return;

	for (i = 0; i < fwspec->num_ids; i++) {
		struct qcom_iommu_ctx *ctx = to_ctx(qcom_domain, fwspec->ids[i]);
		unsigned int val, ret;

		iommu_writel(ctx, ARM_SMMU_CB_TLBSYNC, 0);

		ret = readl_poll_timeout(ctx->base + ARM_SMMU_CB_TLBSTATUS, val,
					 (val & 0x1) == 0, 0, 5000000);
		if (ret)
			dev_err(ctx->dev, "timeout waiting for TLB SYNC\n");
	}
}

static void qcom_iommu_tlb_inv_context(void *cookie)
{
	struct qcom_iommu_domain *qcom_domain = cookie;
	struct iommu_fwspec *fwspec = qcom_domain->fwspec;
	unsigned int i;

	if (!fwspec)
		return;

	for (i = 0; i < fwspec->num_ids; i++) {
		struct qcom_iommu_ctx *ctx = to_ctx(qcom_domain, fwspec->ids[i]);

		iommu_writel(ctx, ARM_SMMU_CB_S1_TLBIASID, ctx->asid);
	}

	qcom_iommu_tlb_sync(cookie);
}

static void qcom_iommu_tlb_inv_range_nosync(unsigned long iova, size_t size,
					    size_t granule, bool leaf,
					    void *cookie)
{
	struct qcom_iommu_domain *qcom_domain = cookie;
	struct iommu_fwspec *fwspec = qcom_domain->fwspec;
	unsigned int i, reg;

	if (!fwspec)
		return;

	reg = leaf ? ARM_SMMU_CB_S1_TLBIVAL : ARM_SMMU_CB_S1_TLBIVA;

	for (i = 0; i < fwspec->num_ids; i++) {
		struct qcom_iommu_ctx *ctx = to_ctx(qcom_domain, fwspec->ids[i]);
		size_t s = size;

		iova = (iova >> 12) << 12;
		iova |= ctx->asid;
		do {
			iommu_writel(ctx, reg, iova);
			iova += granule;
		} while (s -= granule);
	}
}

static const struct iommu_gather_ops qcom_gather_ops = {
	.tlb_flush_all	= qcom_iommu_tlb_inv_context,
	.tlb_add_flush	= qcom_iommu_tlb_inv_range_nosync,
	.tlb_sync	= qcom_iommu_tlb_sync,
};

static irqreturn_t qcom_iommu_fault(int irq, void *dev)
{
	struct qcom_iommu_ctx *ctx = dev;
	u32 fsr, fsynr;
	u64 iova;

	fsr = iommu_readl(ctx, ARM_SMMU_CB_FSR);

	if (!(fsr & FSR_FAULT))
		return IRQ_NONE;

	fsynr = iommu_readl(ctx, ARM_SMMU_CB_FSYNR0);
	iova = iommu_readq(ctx, ARM_SMMU_CB_FAR);

	if (!report_iommu_fault(ctx->domain, ctx->dev, iova, 0)) {
		dev_err_ratelimited(ctx->dev,
				    "Unhandled context fault: fsr=0x%x, "
				    "iova=0x%016llx, fsynr=0x%x, cb=%d\n",
				    fsr, iova, fsynr, ctx->asid);
	}

	iommu_writel(ctx, ARM_SMMU_CB_FSR, fsr);
	iommu_writel(ctx, ARM_SMMU_CB_RESUME, RESUME_TERMINATE);

	return IRQ_HANDLED;
}

/*
 * Program one context bank from an already-allocated page table.  Split out of
 * qcom_iommu_init_domain() because detach disables the bank (SCTLR = 0) and
 * cam_smmu attaches and detaches once per camera session -- so a re-attach has
 * to program it again or the master streams untranslated, which on this SoC is
 * an XPU violation and a silent reset to EDL.
 */
static void qcom_iommu_program_ctx(struct qcom_iommu_ctx *ctx,
				   struct io_pgtable_cfg *pgtbl_cfg)
{
	u32 reg;

	/* Disable context bank before programming */
	iommu_writel(ctx, ARM_SMMU_CB_SCTLR, 0);

	/* Clear context bank fault address fault status registers */
	iommu_writel(ctx, ARM_SMMU_CB_FAR, 0);
	iommu_writel(ctx, ARM_SMMU_CB_FSR, FSR_FAULT);

	/* TTBRs */
	iommu_writeq(ctx, ARM_SMMU_CB_TTBR0,
			pgtbl_cfg->arm_lpae_s1_cfg.ttbr[0] |
			((u64)ctx->asid << TTBRn_ASID_SHIFT));
	iommu_writeq(ctx, ARM_SMMU_CB_TTBR1,
			pgtbl_cfg->arm_lpae_s1_cfg.ttbr[1] |
			((u64)ctx->asid << TTBRn_ASID_SHIFT));

	/*
	 * TTBCR.  arm_32_lpae_alloc_pgtable_s1() already folds in TCR_EAE and
	 * truncates tcr to 32 bits, so TTBCR2 only needs the SEP field.
	 */
	iommu_writel(ctx, ARM_SMMU_CB_TTBCR2,
			(pgtbl_cfg->arm_lpae_s1_cfg.tcr >> 32) |
			TTBCR2_SEP_UPSTREAM);
	iommu_writel(ctx, ARM_SMMU_CB_TTBCR,
			pgtbl_cfg->arm_lpae_s1_cfg.tcr);

	/* MAIRs (stage-1 only) */
	iommu_writel(ctx, ARM_SMMU_CB_S1_MAIR0,
			pgtbl_cfg->arm_lpae_s1_cfg.mair[0]);
	iommu_writel(ctx, ARM_SMMU_CB_S1_MAIR1,
			pgtbl_cfg->arm_lpae_s1_cfg.mair[1]);

	/*
	 * SCTLR.  Deliberately no CFCFG: we do not write SMMU_INTR_SEL_NS
	 * (that lives outside the CB window), so a stalled fault would never
	 * be resumed and would look exactly like the GR0 bus hang.  Terminate
	 * instead.  msm_buf_mgr.c asks for stall-disable at attach anyway.
	 */
	reg = SCTLR_CFIE | SCTLR_CFRE | SCTLR_AFE | SCTLR_TRE |
		SCTLR_M | SCTLR_S1_ASIDPNE;

	if (IS_ENABLED(CONFIG_CPU_BIG_ENDIAN))
		reg |= SCTLR_E;

	iommu_writel(ctx, ARM_SMMU_CB_SCTLR, reg);
}

static int qcom_iommu_init_domain(struct iommu_domain *domain,
				  struct qcom_iommu_dev *qcom_iommu,
				  struct iommu_fwspec *fwspec)
{
	struct qcom_iommu_domain *qcom_domain = to_qcom_iommu_domain(domain);
	struct io_pgtable_ops *pgtbl_ops;
	struct io_pgtable_cfg pgtbl_cfg;
	int i, ret = 0;
	u32 reg;

	mutex_lock(&qcom_domain->init_mutex);
	if (qcom_domain->iommu) {
		/*
		 * Re-attach of a domain we already built.  The page table is
		 * still valid, but detach turned the banks off, so program
		 * them again from the table io-pgtable is holding.
		 */
		struct io_pgtable_cfg *cfg =
			&io_pgtable_ops_to_pgtable(qcom_domain->pgtbl_ops)->cfg;

		qcom_domain->fwspec = fwspec;

		for (i = 0; i < fwspec->num_ids; i++) {
			struct qcom_iommu_ctx *ctx =
				to_ctx(qcom_domain, fwspec->ids[i]);

			qcom_iommu_program_ctx(ctx, cfg);
			ctx->domain = domain;
		}

		goto out_unlock;
	}

	qcom_domain->static_cb = qcom_iommu->static_cb;

	pgtbl_cfg = (struct io_pgtable_cfg) {
		.pgsize_bitmap	= SZ_4K | SZ_2M | SZ_1G,
		.ias		= 32,
		.oas		= 40,
		.tlb		= &qcom_gather_ops,
		.iommu_dev	= qcom_iommu->dev,
	};

	qcom_domain->iommu = qcom_iommu;
	qcom_domain->fwspec = fwspec;

	pgtbl_ops = alloc_io_pgtable_ops(ARM_32_LPAE_S1, &pgtbl_cfg,
					 qcom_domain);
	if (!pgtbl_ops) {
		dev_err(qcom_iommu->dev, "failed to allocate pagetable ops\n");
		ret = -ENOMEM;
		goto out_clear_iommu;
	}

	/* Update the domain's page sizes to reflect the page table format */
	domain->pgsize_bitmap = pgtbl_cfg.pgsize_bitmap;
	domain->geometry.aperture_end = (1ULL << pgtbl_cfg.ias) - 1;
	domain->geometry.force_aperture = true;

	for (i = 0; i < fwspec->num_ids; i++) {
		struct qcom_iommu_ctx *ctx = to_ctx(qcom_domain, fwspec->ids[i]);

		if (!ctx->secure_init) {
			int scm_ret = 0;

			ret = scm_restore_sec_cfg(qcom_iommu->sec_id,
						  ctx->asid, &scm_ret);
			if (ret || scm_ret) {
				dev_err(qcom_iommu->dev,
					"secure init failed: %d (scm %d)\n",
					ret, scm_ret);
				if (!ret)
					ret = -EINVAL;
				goto out_clear_iommu;
			}
			ctx->secure_init = true;
		}

		qcom_iommu_program_ctx(ctx, &pgtbl_cfg);

		ctx->domain = domain;
	}

	mutex_unlock(&qcom_domain->init_mutex);

	/* Publish page table ops for map/unmap */
	qcom_domain->pgtbl_ops = pgtbl_ops;

	return 0;

out_clear_iommu:
	qcom_domain->iommu = NULL;
	qcom_domain->fwspec = NULL;
out_unlock:
	mutex_unlock(&qcom_domain->init_mutex);
	return ret;
}

static struct iommu_domain *qcom_iommu_domain_alloc(unsigned int type)
{
	struct qcom_iommu_domain *qcom_domain;

	if (type != IOMMU_DOMAIN_UNMANAGED)
		return NULL;
	/*
	 * Allocate the domain and initialise some of its data structures.
	 * We can't really do anything meaningful until we've added a
	 * master.
	 */
	qcom_domain = kzalloc(sizeof(*qcom_domain), GFP_KERNEL);
	if (!qcom_domain)
		return NULL;

	mutex_init(&qcom_domain->init_mutex);
	spin_lock_init(&qcom_domain->pgtbl_lock);

	return &qcom_domain->domain;
}

static void qcom_iommu_domain_free(struct iommu_domain *domain)
{
	struct qcom_iommu_domain *qcom_domain = to_qcom_iommu_domain(domain);

	if (qcom_domain->iommu)
		free_io_pgtable_ops(qcom_domain->pgtbl_ops);

	kfree(qcom_domain);
}

static int qcom_iommu_attach_dev(struct iommu_domain *domain,
				 struct device *dev)
{
	struct qcom_iommu_dev *qcom_iommu = to_iommu(dev->iommu_fwspec);
	struct qcom_iommu_domain *qcom_domain = to_qcom_iommu_domain(domain);
	int ret;

	if (!qcom_iommu) {
		dev_err(dev, "cannot attach to IOMMU, is it on the same bus?\n");
		return -ENXIO;
	}

	/* Ensure that the domain is finalized */
	ret = qcom_iommu_init_domain(domain, qcom_iommu, dev->iommu_fwspec);
	if (ret < 0)
		return ret;

	/*
	 * Sanity check the domain. We don't support domains across
	 * different IOMMUs.
	 */
	if (qcom_domain->iommu != qcom_iommu) {
		dev_err(dev, "cannot attach to foreign IOMMU\n");
		return -EINVAL;
	}

	return 0;
}

static void qcom_iommu_detach_dev(struct iommu_domain *domain,
				  struct device *dev)
{
	struct iommu_fwspec *fwspec = dev->iommu_fwspec;
	struct qcom_iommu_domain *qcom_domain = to_qcom_iommu_domain(domain);
	unsigned int i;

	if (WARN_ON(!qcom_domain->iommu))
		return;

	for (i = 0; i < fwspec->num_ids; i++) {
		struct qcom_iommu_ctx *ctx = to_ctx(qcom_domain, fwspec->ids[i]);

		/* Disable the context bank: */
		iommu_writel(ctx, ARM_SMMU_CB_SCTLR, 0);

		ctx->domain = NULL;
	}

	qcom_domain->fwspec = NULL;
}

static int qcom_iommu_map(struct iommu_domain *domain, unsigned long iova,
			  phys_addr_t paddr, size_t size, int prot)
{
	int ret;
	unsigned long flags;
	struct qcom_iommu_domain *qcom_domain = to_qcom_iommu_domain(domain);
	struct io_pgtable_ops *ops = qcom_domain->pgtbl_ops;

	if (!ops) {
		pr_err_ratelimited("qcom-iommu: map with no pgtbl_ops (domain=%p iommu=%p static_cb=%d)\n",
				   domain, qcom_domain->iommu,
				   qcom_domain->static_cb);
		return -ENODEV;
	}

	spin_lock_irqsave(&qcom_domain->pgtbl_lock, flags);
	ret = ops->map(ops, iova, paddr, size, prot);
	spin_unlock_irqrestore(&qcom_domain->pgtbl_lock, flags);
	return ret;
}


static size_t qcom_iommu_unmap(struct iommu_domain *domain, unsigned long iova,
			       size_t size)
{
	size_t ret;
	unsigned long flags;
	struct qcom_iommu_domain *qcom_domain = to_qcom_iommu_domain(domain);
	struct io_pgtable_ops *ops = qcom_domain->pgtbl_ops;

	if (!ops)
		return 0;

	spin_lock_irqsave(&qcom_domain->pgtbl_lock, flags);
	ret = ops->unmap(ops, iova, size);
	spin_unlock_irqrestore(&qcom_domain->pgtbl_lock, flags);

	return ret;
}

static phys_addr_t qcom_iommu_iova_to_phys(struct iommu_domain *domain,
					   dma_addr_t iova)
{
	phys_addr_t ret;
	unsigned long flags;
	struct qcom_iommu_domain *qcom_domain = to_qcom_iommu_domain(domain);
	struct io_pgtable_ops *ops = qcom_domain->pgtbl_ops;

	if (!ops)
		return 0;

	spin_lock_irqsave(&qcom_domain->pgtbl_lock, flags);
	ret = ops->iova_to_phys(ops, iova);
	spin_unlock_irqrestore(&qcom_domain->pgtbl_lock, flags);

	return ret;
}

static bool qcom_iommu_capable(enum iommu_cap cap)
{
	switch (cap) {
	case IOMMU_CAP_CACHE_COHERENCY:
		/*
		 * Return true here as the SMMU can always send out coherent
		 * requests.
		 */
		return true;
	case IOMMU_CAP_NOEXEC:
		return true;
	default:
		return false;
	}
}

static int qcom_iommu_add_device(struct device *dev)
{
	struct qcom_iommu_dev *qcom_iommu = to_iommu(dev->iommu_fwspec);
	struct iommu_group *group;

	if (!qcom_iommu)
		return -ENODEV;

	group = iommu_group_get_for_dev(dev);
	if (IS_ERR_OR_NULL(group))
		return PTR_ERR_OR_ZERO(group);

	iommu_group_put(group);

	return 0;
}

static void qcom_iommu_remove_device(struct device *dev)
{
	struct qcom_iommu_dev *qcom_iommu = to_iommu(dev->iommu_fwspec);

	if (!qcom_iommu)
		return;

	iommu_group_remove_device(dev);
	iommu_fwspec_free(dev);
}

static int qcom_iommu_of_xlate(struct device *dev,
			       struct of_phandle_args *args)
{
	struct qcom_iommu_dev *qcom_iommu;
	struct platform_device *iommu_pdev;
	unsigned int asid = args->args[0];

	if (args->args_count != 1) {
		dev_err(dev, "incorrect number of iommu params found for %s "
			"(found %d, expected 1)\n",
			args->np->full_name, args->args_count);
		return -EINVAL;
	}

	iommu_pdev = of_find_device_by_node(args->np);
	if (WARN_ON(!iommu_pdev))
		return -EINVAL;

	qcom_iommu = platform_get_drvdata(iommu_pdev);

	/* make sure the asid specified in dt is valid, so we don't have
	 * to sanity check this elsewhere:
	 */
	if (WARN_ON(asid > qcom_iommu->max_asid) ||
	    WARN_ON(qcom_iommu->ctxs[asid] == NULL))
		return -EINVAL;

	if (!dev->iommu_fwspec->iommu_priv) {
		dev->iommu_fwspec->iommu_priv = qcom_iommu;
	} else {
		/* make sure devices iommus dt node isn't referring to
		 * multiple different iommu devices.  Multiple context
		 * banks are ok, but multiple devices are not:
		 */
		if (WARN_ON(qcom_iommu != dev->iommu_fwspec->iommu_priv))
			return -EINVAL;
	}

	return iommu_fwspec_add_ids(dev, &asid, 1);
}

static const struct iommu_ops qcom_iommu_ops = {
	.capable	= qcom_iommu_capable,
	.domain_alloc	= qcom_iommu_domain_alloc,
	.domain_free	= qcom_iommu_domain_free,
	.attach_dev	= qcom_iommu_attach_dev,
	.detach_dev	= qcom_iommu_detach_dev,
	.map		= qcom_iommu_map,
	.unmap		= qcom_iommu_unmap,
	.map_sg		= default_iommu_map_sg,
	.iova_to_phys	= qcom_iommu_iova_to_phys,
	.add_device	= qcom_iommu_add_device,
	.remove_device	= qcom_iommu_remove_device,
	.device_group	= generic_device_group,
	.of_xlate	= qcom_iommu_of_xlate,
	.pgsize_bitmap	= SZ_4K | SZ_2M | SZ_1G,
};

static int qcom_iommu_enable_clocks(struct qcom_iommu_dev *qcom_iommu)
{
	int i, ret;

	for (i = 0; i < qcom_iommu->num_clks; i++) {
		ret = clk_prepare_enable(qcom_iommu->clks[i]);
		if (ret) {
			dev_err(qcom_iommu->dev,
				"failed to enable clock %d: %d\n", i, ret);
			goto err;
		}
	}

	return 0;
err:
	while (i--)
		clk_disable_unprepare(qcom_iommu->clks[i]);
	return ret;
}

static void qcom_iommu_disable_clocks(struct qcom_iommu_dev *qcom_iommu)
{
	int i = qcom_iommu->num_clks;

	while (i--)
		clk_disable_unprepare(qcom_iommu->clks[i]);
}

static int qcom_iommu_get_clocks(struct qcom_iommu_dev *qcom_iommu)
{
	struct device *dev = qcom_iommu->dev;
	int count, i;

	count = of_count_phandle_with_args(dev->of_node, "clocks",
					   "#clock-cells");
	if (count <= 0) {
		dev_err(dev, "no clocks specified\n");
		return -EINVAL;
	}

	if (count > QCOM_IOMMU_MAX_CLKS) {
		dev_err(dev, "too many clocks (%d > %d)\n",
			count, QCOM_IOMMU_MAX_CLKS);
		return -EINVAL;
	}

	for (i = 0; i < count; i++) {
		qcom_iommu->clks[i] = of_clk_get(dev->of_node, i);
		if (IS_ERR(qcom_iommu->clks[i])) {
			int ret = PTR_ERR(qcom_iommu->clks[i]);

			dev_err(dev, "failed to get clock %d: %d\n", i, ret);
			while (i--)
				clk_put(qcom_iommu->clks[i]);
			return ret;
		}
	}

	qcom_iommu->num_clks = count;

	return 0;
}

static void qcom_iommu_put_clocks(struct qcom_iommu_dev *qcom_iommu)
{
	int i = qcom_iommu->num_clks;

	while (i--)
		clk_put(qcom_iommu->clks[i]);
}

static int get_asid(const struct device_node *np)
{
	u32 reg;

	/* read the "reg" property directly to get the relative address
	 * of the context bank, and calculate the asid from that:
	 */
	if (of_property_read_u32_index(np, "reg", 0, &reg))
		return -ENODEV;

	return reg / 0x1000;
}

static int qcom_iommu_ctx_probe(struct platform_device *pdev)
{
	struct qcom_iommu_ctx *ctx;
	struct device *dev = &pdev->dev;
	struct qcom_iommu_dev *qcom_iommu = dev_get_drvdata(dev->parent);
	struct resource *res;
	int ret, irq;

	if (!qcom_iommu)
		return -ENODEV;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->dev = dev;
	platform_set_drvdata(pdev, ctx);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ctx->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(ctx->base))
		return PTR_ERR(ctx->base);
	ctx->phys_base = res->start;
	ctx->static_cb = qcom_iommu->static_cb;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "failed to get irq\n");
		return -ENODEV;
	}

	/* clear IRQs before registering fault handler, just in case the
	 * boot-loader left us a surprise (not on TZ-owned banks):
	 */
	iommu_writel(ctx, ARM_SMMU_CB_FSR, iommu_readl(ctx, ARM_SMMU_CB_FSR));

	ret = devm_request_irq(dev, irq,
			       qcom_iommu_fault,
			       IRQF_SHARED,
			       "qcom-iommu-fault",
			       ctx);
	if (ret) {
		dev_err(dev, "failed to request IRQ %u\n", irq);
		return ret;
	}

	ret = get_asid(dev->of_node);
	if (ret < 0) {
		dev_err(dev, "missing reg property\n");
		return ret;
	}

	ctx->asid = ret;

	dev_dbg(dev, "found asid %u\n", ctx->asid);

	qcom_iommu->ctxs[ctx->asid] = ctx;

	return 0;
}

static int qcom_iommu_ctx_remove(struct platform_device *pdev)
{
	struct qcom_iommu_dev *qcom_iommu = dev_get_drvdata(pdev->dev.parent);
	struct qcom_iommu_ctx *ctx = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);

	qcom_iommu->ctxs[ctx->asid] = NULL;

	return 0;
}

static const struct of_device_id ctx_of_match[] = {
	{ .compatible = "qcom,msm-iommu-v1-ns" },
	{ .compatible = "qcom,msm-iommu-v1-sec" },
	{ /* sentinel */ }
};

static struct platform_driver qcom_iommu_ctx_driver = {
	.driver	= {
		.name		= "qcom-iommu-ctx",
		.of_match_table	= of_match_ptr(ctx_of_match),
	},
	.probe	= qcom_iommu_ctx_probe,
	.remove = qcom_iommu_ctx_remove,
};

static bool qcom_iommu_has_secure_context(struct qcom_iommu_dev *qcom_iommu)
{
	return of_find_property(qcom_iommu->dev->of_node,
				"qcom,iommu-secure-id", NULL) != NULL;
}

static bool sec_ptbl_done;

static int qcom_iommu_device_probe(struct platform_device *pdev)
{
	struct device_node *child;
	struct qcom_iommu_dev *qcom_iommu;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret, sz, max_asid = 0;

	/* find the max asid (which is 1:1 to ctx bank idx), so we know how
	 * many child ctx devices we have:
	 */
	for_each_child_of_node(dev->of_node, child)
		max_asid = max(max_asid, get_asid(child));

	sz = sizeof(*qcom_iommu) +
		((max_asid + 1) * sizeof(qcom_iommu->ctxs[0]));
	qcom_iommu = devm_kzalloc(dev, sz, GFP_KERNEL);
	if (!qcom_iommu)
		return -ENOMEM;
	qcom_iommu->max_asid = max_asid;
	qcom_iommu->dev = dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) {
		qcom_iommu->local_base = devm_ioremap_resource(dev, res);
		if (IS_ERR(qcom_iommu->local_base))
			return PTR_ERR(qcom_iommu->local_base);
	}

	ret = qcom_iommu_get_clocks(qcom_iommu);
	if (ret)
		return ret;

	qcom_iommu->static_cb = of_property_read_bool(dev->of_node,
						      "qcom,enable-static-cb");

	if (of_property_read_u32(dev->of_node, "qcom,iommu-secure-id",
				 &qcom_iommu->sec_id)) {
		dev_err(dev, "missing qcom,iommu-secure-id property\n");
		ret = -ENODEV;
		goto err_put_clocks;
	}

	/*
	 * msm_iommu_sec_pgtbl_init() allocates unconditionally, so only ever
	 * let the first SMMU instance run it.
	 */
	if (!sec_ptbl_done && qcom_iommu_has_secure_context(qcom_iommu)) {
		ret = msm_iommu_sec_pgtbl_init();
		if (ret) {
			dev_err(dev, "cannot init secure pg table(%d)\n", ret);
			goto err_put_clocks;
		}
		sec_ptbl_done = true;
	}

	platform_set_drvdata(pdev, qcom_iommu);

	ret = qcom_iommu_enable_clocks(qcom_iommu);
	if (ret)
		goto err_put_clocks;

	/* register context bank devices, which are child nodes: */
	ret = of_platform_populate(dev->of_node, NULL, NULL, dev);
	if (ret) {
		dev_err(dev, "Failed to populate iommu contexts\n");
		goto err_clocks;
	}

	of_iommu_set_ops(dev->of_node, &qcom_iommu_ops);

	if (qcom_iommu->local_base) {
		writel_relaxed(0xffffffff,
			       qcom_iommu->local_base + SMMU_INTR_SEL_NS);
		dev_info(dev, "context faults routed to non-secure\n");
	} else {
		dev_info(dev, "no local base: context faults will be silent\n");
	}

	if (!iommu_present(&platform_bus_type))
		bus_set_iommu(&platform_bus_type, &qcom_iommu_ops);

	dev_info(dev, "qcom-iommu: %d context bank(s), sec-id %d\n",
		 max_asid, qcom_iommu->sec_id);

	return 0;

err_clocks:
	qcom_iommu_disable_clocks(qcom_iommu);
err_put_clocks:
	qcom_iommu_put_clocks(qcom_iommu);
	return ret;
}

static int qcom_iommu_device_remove(struct platform_device *pdev)
{
	struct qcom_iommu_dev *qcom_iommu = platform_get_drvdata(pdev);

	of_platform_depopulate(&pdev->dev);
	qcom_iommu_disable_clocks(qcom_iommu);
	qcom_iommu_put_clocks(qcom_iommu);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id qcom_iommu_of_match[] = {
	{ .compatible = "qcom,msm-iommu-v1" },
	{ /* sentinel */ }
};

static struct platform_driver qcom_iommu_driver = {
	.driver	= {
		.name		= "qcom-iommu",
		.of_match_table	= of_match_ptr(qcom_iommu_of_match),
	},
	.probe	= qcom_iommu_device_probe,
	.remove = qcom_iommu_device_remove,
};

static int __init qcom_iommu_init(void)
{
	int ret;

	ret = platform_driver_register(&qcom_iommu_ctx_driver);
	if (ret)
		return ret;

	ret = platform_driver_register(&qcom_iommu_driver);
	if (ret)
		platform_driver_unregister(&qcom_iommu_ctx_driver);

	return ret;
}
subsys_initcall(qcom_iommu_init);
