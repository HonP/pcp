/*
** Copyright (C) 2015-2019 Red Hat.
** Copyright (C) 2000-2012 Gerlof Langeveld.
**
** This program is free software; you can redistribute it and/or modify it
** under the terms of the GNU General Public License as published by the
** Free Software Foundation; either version 2, or (at your option) any
** later version.
**
** This program is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
** See the GNU General Public License for more details.
*/

#include <pcp/pmapi.h>

#include "atop.h"
#include "photosyst.h"
#include "systmetrics.h"

/*
** Allocate the fixes space for system statistics and the associated
** dynamically-sized buffers (initially either zero length or having
** one null entry used for name-based-iteration termination).
*/
struct sstat *
sstat_alloc(const char *purpose)
{
	struct sstat *sstat;
	void *ptr;

	sstat = (struct sstat *)calloc(1, sizeof(struct sstat));
	ptrverify(sstat, "Alloc failed for %s\n", purpose);

	ptr = calloc(1, sizeof(struct percpu));
	ptrverify(ptr, "Alloc failed for %s (processors)\n", purpose);
	sstat->cpu.cpu = (struct percpu *)ptr;

	ptr = calloc(1, sizeof(struct perintf));
	ptrverify(ptr, "Alloc failed for %s (interfaces)\n", purpose);
	sstat->intf.intf = (struct perintf *)ptr;

	ptr = calloc(1, sizeof(struct perifb));
	ptrverify(ptr, "Alloc failed for %s (IB ports)\n", purpose);
	sstat->ifb.ifb = (struct perifb *)ptr;

	ptr = calloc(1, sizeof(struct perdsk));
	ptrverify(ptr, "Alloc failed for %s (disk devices)\n", purpose);
	sstat->dsk.dsk = (struct perdsk *)ptr;

	ptr = calloc(1, sizeof(struct perdsk));
	ptrverify(ptr, "Alloc failed for %s (LVM devices)\n", purpose);
	sstat->dsk.lvm = (struct perdsk *)ptr;

	ptr = calloc(1, sizeof(struct perdsk));
	ptrverify(ptr, "Alloc failed for %s (MD devices)\n", purpose);
	sstat->dsk.mdd = (struct perdsk *)ptr;

	ptr = calloc(1, sizeof(struct pernfsmount));
	ptrverify(ptr, "Alloc failed for %s (NFS mounts)\n", purpose);
	sstat->nfs.nfsmounts.nfsmnt = (struct pernfsmount *)ptr;

	ptr = calloc(1, sizeof(struct pergpu));
	ptrverify(ptr, "Alloc failed for %s (GPUs)\n", purpose);
	sstat->gpu.gpu = (struct pergpu *)ptr;

	return sstat;
}

/*
** Clear all statistics, maintaining associated dynamically-sized
** buffers memory allocation (also clearing those buffers contents).
*/
void
sstat_reset(struct sstat *sstat)
{
	void		*cpu, *gpu, *intf, *ifb, *mdd, *lvm, *dsk, *nfs;
	unsigned int	nrcpu, nrgpus, nrintf, nrports, nrdsk, nrlvm, nrmdd, nrnfs;

	cpu = sstat->cpu.cpu;
	intf = sstat->intf.intf;
	ifb = sstat->ifb.ifb;
	dsk = sstat->dsk.dsk;
	lvm = sstat->dsk.lvm;
	mdd = sstat->dsk.mdd;
	nfs = sstat->nfs.nfsmounts.nfsmnt;
	gpu = sstat->gpu.gpu;

	nrcpu = sstat->cpu.nrcpu;
	nrintf = sstat->intf.nrintf;
	nrports = sstat->ifb.nrports;
	nrdsk = sstat->dsk.ndsk;
	nrlvm = sstat->dsk.nlvm;
	nrmdd = sstat->dsk.nmdd;
	nrnfs = sstat->nfs.nfsmounts.nrmounts;
	nrgpus = sstat->gpu.nrgpus;

	/* clear fixed portion now that pointers/sized are safe */
	memset(sstat, 0, sizeof(struct sstat));

	/* clear the dynamically-sized buffers and restore ptrs */
	memset(cpu, 0, sizeof(struct percpu) * nrcpu);
	memset(intf, 0, sizeof(struct perintf) * nrintf);
	memset(ifb, 0, sizeof(struct perifb) * nrports);
	memset(dsk, 0, sizeof(struct perdsk) * nrdsk);
	memset(lvm, 0, sizeof(struct perdsk) * nrlvm);
	memset(mdd, 0, sizeof(struct perdsk) * nrmdd);
	memset(nfs, 0, sizeof(struct pernfsmount) * nrnfs);
	memset(gpu, 0, sizeof(struct pergpu) * nrgpus);

	/* stitch the main sstat buffer back together once more */
	sstat->cpu.cpu = cpu;
	sstat->intf.intf = intf;
	sstat->ifb.ifb = ifb;
	sstat->dsk.dsk = dsk;
	sstat->dsk.lvm = lvm;
	sstat->dsk.mdd = mdd;
	sstat->nfs.nfsmounts.nfsmnt = nfs;
	sstat->gpu.gpu = gpu;

	sstat->cpu.nrcpu = nrcpu;
	sstat->intf.nrintf = nrintf;
	sstat->ifb.nrports = nrports;
	sstat->dsk.ndsk = nrdsk;
	sstat->dsk.nlvm = nrlvm;
	sstat->dsk.nmdd = nrmdd;
	sstat->nfs.nfsmounts.nrmounts = nrnfs;
	sstat->gpu.nrgpus = nrgpus;
}

static void
update_processor(struct percpu *cpu, int id, pmResult *result, pmDesc *descs)
{
	cpu->cpunr = id;

	cpu->stime = extract_count_t_inst(result, descs, PERCPU_STIME, id);
	cpu->utime = extract_count_t_inst(result, descs, PERCPU_UTIME, id);
	cpu->ntime = extract_count_t_inst(result, descs, PERCPU_NTIME, id);
	cpu->itime = extract_count_t_inst(result, descs, PERCPU_ITIME, id);
	cpu->wtime = extract_count_t_inst(result, descs, PERCPU_WTIME, id);
	cpu->Itime = extract_count_t_inst(result, descs, PERCPU_HARDIRQ, id);
	cpu->Stime = extract_count_t_inst(result, descs, PERCPU_SOFTIRQ, id);
	cpu->steal = extract_count_t_inst(result, descs, PERCPU_STEAL, id);
	cpu->guest = extract_count_t_inst(result, descs, PERCPU_GUEST, id);

	memset(&cpu->freqcnt, 0, sizeof(cpu->freqcnt));
	cpu->freqcnt.cnt = extract_count_t_inst(result, descs, PERCPU_FREQCNT_CNT, id);
	cpu->instr = extract_count_t_inst(result, descs, PERCPU_PERF_INSTR, id);
	cpu->cycle = extract_count_t_inst(result, descs, PERCPU_PERF_CYCLE, id);
}

static void
update_interface(struct perintf *in, int id, char *name, pmResult *rp, pmDesc *dp)
{
	strncpy(in->name, name, sizeof(in->name));
	in->name[sizeof(in->name)-1] = '\0';

	in->rbyte = extract_count_t_inst(rp, dp, PERINTF_RBYTE, id);
	in->rpack = extract_count_t_inst(rp, dp, PERINTF_RPACK, id);
	in->rerrs = extract_count_t_inst(rp, dp, PERINTF_RERRS, id);
	in->rdrop = extract_count_t_inst(rp, dp, PERINTF_RDROP, id);
	in->rfifo = extract_count_t_inst(rp, dp, PERINTF_RFIFO, id);
	in->rframe = extract_count_t_inst(rp, dp, PERINTF_RFRAME, id);
	in->rcompr = extract_count_t_inst(rp, dp, PERINTF_RCOMPR, id);
	in->rmultic = extract_count_t_inst(rp, dp, PERINTF_RMULTIC, id);
	in->sbyte = extract_count_t_inst(rp, dp, PERINTF_SBYTE, id);
	in->spack = extract_count_t_inst(rp, dp, PERINTF_SPACK, id);
	in->serrs = extract_count_t_inst(rp, dp, PERINTF_SERRS, id);
	in->sdrop = extract_count_t_inst(rp, dp, PERINTF_SDROP, id);
	in->sfifo = extract_count_t_inst(rp, dp, PERINTF_SFIFO, id);
	in->scollis = extract_count_t_inst(rp, dp, PERINTF_SCOLLIS, id);
	in->scarrier = extract_count_t_inst(rp, dp, PERINTF_SCARRIER, id);
	in->scompr = extract_count_t_inst(rp, dp, PERINTF_SCOMPR, id);
}

static void
update_ibport(struct perifb *ib, int id, char *name, pmResult *rp, pmDesc *dp)
{
	strncpy(ib->ibname, name, sizeof(ib->ibname));
	ib->ibname[sizeof(ib->ibname)-1] = '\0';

	ib->portnr = extract_integer_inst(rp, dp, PERIFB_PORT_LID, id);
	ib->lanes = extract_integer_inst(rp, dp, PERIFB_PORT_WIDTH, id);
	ib->rate = extract_count_t_inst(rp, dp, PERIFB_PORT_RATE, id);
	ib->rcvb = extract_count_t_inst(rp, dp, PERIFB_PORT_INB, id);
	ib->sndb = extract_count_t_inst(rp, dp, PERIFB_PORT_OUTB, id);
	ib->rcvp = extract_count_t_inst(rp, dp, PERIFB_PORT_INPKT, id);
	ib->sndp = extract_count_t_inst(rp, dp, PERIFB_PORT_OUTPKT, id);
}

static void
update_disk(struct perdsk *dsk, int id, char *name, pmResult *rp, pmDesc *dp)
{
	strncpy(dsk->name, name, sizeof(dsk->name));
	dsk->name[sizeof(dsk->name)-1] = '\0';

	dsk->nread = extract_count_t_inst(rp, dp, PERDISK_NREAD, id);
	dsk->nrsect = extract_count_t_inst(rp, dp, PERDISK_NRSECT, id);
	dsk->nwrite = extract_count_t_inst(rp, dp, PERDISK_NWRITE, id);
	dsk->nwsect = extract_count_t_inst(rp, dp, PERDISK_NWSECT, id);
	dsk->io_ms = extract_count_t_inst(rp, dp, PERDISK_IO_MS, id);
	dsk->avque = extract_count_t_inst(rp, dp, PERDISK_AVEQ, id);
}

static void
update_lvm(struct perdsk *dsk, int id, char *name, pmResult *rp, pmDesc *dp)
{
	strncpy(dsk->name, name, sizeof(dsk->name));
	dsk->name[sizeof(dsk->name)-1] = '\0';

	dsk->nread = extract_count_t_inst(rp, dp, PERDM_NREAD, id);
	dsk->nrsect = extract_count_t_inst(rp, dp, PERDM_NRSECT, id);
	dsk->nwrite = extract_count_t_inst(rp, dp, PERDM_NWRITE, id);
	dsk->nwsect = extract_count_t_inst(rp, dp, PERDM_NWSECT, id);
	dsk->io_ms = 0;
	dsk->avque = 0;
}

static void
update_mdd(struct perdsk *dsk, int id, char *name, pmResult *rp, pmDesc *dp)
{
	strncpy(dsk->name, name, sizeof(dsk->name));
	dsk->name[sizeof(dsk->name)-1] = '\0';

	dsk->nread = extract_count_t_inst(rp, dp, PERMD_NREAD, id);
	dsk->nrsect = extract_count_t_inst(rp, dp, PERMD_NRSECT, id);
	dsk->nwrite = extract_count_t_inst(rp, dp, PERMD_NWRITE, id);
	dsk->nwsect = extract_count_t_inst(rp, dp, PERMD_NWSECT, id);
	dsk->io_ms = 0;
	dsk->avque = 0;
}

static void
update_mnt(struct pernfsmount *mp, int id, char *name, pmResult *rp, pmDesc *dp)
{
	/* use local client mount unless server export is available */
	strncpy(mp->mountdev, name, sizeof(mp->mountdev)-1);
	extract_string_inst(rp, dp, PERNFS_EXPORT, &mp->mountdev[0],
				sizeof(mp->mountdev)-1, id);
	mp->mountdev[sizeof(mp->mountdev)-1] = '\0';

	mp->age = extract_count_t_inst(rp, dp, PERNFS_AGE, id);
	mp->bytesread = extract_count_t_inst(rp, dp, PERNFS_RDBYTES, id);
	mp->byteswrite = extract_count_t_inst(rp, dp, PERNFS_WRBYTES, id);
	mp->bytesdread = extract_count_t_inst(rp, dp, PERNFS_DRDBYTES, id);
	mp->bytesdwrite = extract_count_t_inst(rp, dp, PERNFS_DWRBYTES, id);
	mp->bytestotread = extract_count_t_inst(rp, dp, PERNFS_TOTRDBYTES, id);
	mp->bytestotwrite = extract_count_t_inst(rp, dp, PERNFS_TOTWRBYTES, id);
	mp->pagesmread = extract_count_t_inst(rp, dp, PERNFS_RDPAGES, id);
	mp->pagesmwrite = extract_count_t_inst(rp, dp, PERNFS_WRPAGES, id);
}

char
photosyst(struct sstat *si)
{
	static int	setup;
	count_t		count;
	unsigned int	nrcpu, nrdisk, nrintf, nrports;
	unsigned int	onrcpu, onrdisk, onrintf, onrports;
	unsigned int	nrlvm, nrmdd, nrnfs;
	unsigned int	onrlvm, onrmdd, onrnfs;
	static pmID	pmids[SYST_NMETRICS];
	static pmDesc	descs[SYST_NMETRICS];
	pmResult	*result;
	size_t		size;
	char		**insts;
	int		*ids, i;

	if (!setup)
	{
		setup_metrics(systmetrics, pmids, descs, SYST_NMETRICS);
		setup = 1;
	}

	if (fetch_metrics("system", SYST_NMETRICS, pmids, &result) < 0)
		return 'r';

	onrcpu  = si->cpu.nrcpu;
	onrintf = si->intf.nrintf;
	onrports = si->ifb.nrports;
	onrdisk = si->dsk.ndsk;
	onrlvm  = si->dsk.nlvm;
	onrmdd  = si->dsk.nmdd;
	onrnfs  = si->nfs.nfsmounts.nrmounts;

	sstat_reset(si);
	si->stamp = result->timestamp;

	/* /proc/loadavg */
	si->cpu.lavg1 = extract_float_inst(result, descs, CPU_LOAD, 1);
	si->cpu.lavg5 = extract_float_inst(result, descs, CPU_LOAD, 5);
	si->cpu.lavg15 = extract_float_inst(result, descs, CPU_LOAD, 15);

	/* /proc/stat */
	si->cpu.csw = extract_count_t(result, descs, CPU_CSW);
	si->cpu.devint = extract_count_t(result, descs, CPU_DEVINT);
	si->cpu.nprocs = extract_count_t(result, descs, CPU_NPROCS);
	si->cpu.all.utime = extract_count_t(result, descs, CPU_UTIME);
	si->cpu.all.ntime = extract_count_t(result, descs, CPU_NTIME);
	si->cpu.all.stime = extract_count_t(result, descs, CPU_STIME);
	si->cpu.all.itime = extract_count_t(result, descs, CPU_ITIME);
	si->cpu.all.wtime = extract_count_t(result, descs, CPU_WTIME);
	si->cpu.all.Itime = extract_count_t(result, descs, CPU_HARDIRQ);
	si->cpu.all.Stime = extract_count_t(result, descs, CPU_SOFTIRQ);
	si->cpu.all.steal = extract_count_t(result, descs, CPU_STEAL);
	si->cpu.all.guest = extract_count_t(result, descs, CPU_GUEST);

	nrcpu = get_instances("processors", PERCPU_UTIME, descs, &ids, &insts);
	if (nrcpu == 0)
	{
		fprintf(stderr, "%s: no per-processor values available\n",
			pmGetProgname());
		cleanstop(0);
	}
	if (nrcpu > onrcpu)
	{
		size = nrcpu * sizeof(struct percpu);
		si->cpu.cpu = (struct percpu *)realloc(si->cpu.cpu, size);
		ptrverify(si->cpu.cpu, "photosyst cpus [%ld]", (long)size);
	}
	for (i=0; i < nrcpu; i++)
	{
		struct percpu	*percpu = &si->cpu.cpu[i];

		if (pmDebugOptions.appl0)
			fprintf(stderr, "%s: updating processor %d: %s\n",
				pmGetProgname(), ids[i], insts[i]);
		update_processor(percpu, ids[i], result, descs);

		si->cpu.all.instr += percpu->instr;
		si->cpu.all.cycle += percpu->cycle;
	}
	si->cpu.nrcpu = nrcpu;
	free(insts);
	free(ids);

	/* /proc/vmstat */
	si->mem.swins = extract_count_t(result, descs, MEM_SWINS);
	si->mem.swouts = extract_count_t(result, descs, MEM_SWOUTS);
	count = 0;
	count += extract_count_t(result, descs, MEM_SCAN_DDMA);
	count += extract_count_t(result, descs, MEM_SCAN_DDMA32);
	count += extract_count_t(result, descs, MEM_SCAN_DHIGH);
	count += extract_count_t(result, descs, MEM_SCAN_DMOVABLE);
	count += extract_count_t(result, descs, MEM_SCAN_DNORMAL);
	count += extract_count_t(result, descs, MEM_SCAN_KDMA);
	count += extract_count_t(result, descs, MEM_SCAN_KDMA32);
	count += extract_count_t(result, descs, MEM_SCAN_KHIGH);
	count += extract_count_t(result, descs, MEM_SCAN_KMOVABLE);
	count += extract_count_t(result, descs, MEM_SCAN_KNORMAL);
	si->mem.pgscans = count;
	count = 0;
	count += extract_count_t(result, descs, MEM_STEAL_DMA);
	count += extract_count_t(result, descs, MEM_STEAL_DMA32);
	count += extract_count_t(result, descs, MEM_STEAL_HIGH);
	count += extract_count_t(result, descs, MEM_STEAL_MOVABLE);
	count += extract_count_t(result, descs, MEM_STEAL_NORMAL);
	si->mem.pgsteal = count;
	si->mem.allocstall = extract_count_t(result, descs, MEM_ALLOCSTALL);

	/* /proc/meminfo */
	si->mem.cachemem = extract_count_t(result, descs, MEM_CACHEMEM);
	si->mem.cachedrt = extract_count_t(result, descs, MEM_CACHEDRT);
	si->mem.physmem = extract_count_t(result, descs, MEM_PHYSMEM);
	si->mem.freemem = extract_count_t(result, descs, MEM_FREEMEM);
	si->mem.buffermem = extract_count_t(result, descs, MEM_BUFFERMEM);
	si->mem.shmem = extract_count_t(result, descs, MEM_SHMEM);
	si->mem.totswap = extract_count_t(result, descs, MEM_TOTSWAP) / 1024;
	si->mem.freeswap = extract_count_t(result, descs, MEM_FREESWAP) / 1024;
	si->mem.slabmem  = extract_count_t(result, descs, MEM_SLABMEM);
	si->mem.slabreclaim = extract_count_t(result, descs, MEM_SLABRECLAIM);
	si->mem.committed = extract_count_t(result, descs, MEM_COMMITTED);
	si->mem.commitlim = extract_count_t(result, descs, MEM_COMMITLIM);
	si->mem.tothugepage = extract_count_t(result, descs, MEM_TOTHUGEPAGE);
	si->mem.freehugepage = extract_count_t(result, descs, MEM_FREEHUGEPAGE);
	si->mem.hugepagesz = extract_count_t(result, descs, HUGEPAGESZ);

	/* shmctl(2) */
	si->mem.shmrss = extract_count_t(result, descs, MEM_SHMRSS) * 1024;
	si->mem.shmswp = extract_count_t(result, descs, MEM_SHMSWP) * 1024;

	/* /proc/net/dev */
	insts = NULL; /* silence coverity */
	ids = NULL;
	nrintf = get_instances("interfaces", PERINTF_RBYTE, descs, &ids, &insts);
	if (nrintf == 0)
	{
		fprintf(stderr, "%s: no per-interface values available\n",
			pmGetProgname());
		cleanstop(0);
	}
	if (nrintf > onrintf)
	{
		size = (nrintf + 1) * sizeof(struct perintf);
		si->intf.intf = (struct perintf *)realloc(si->intf.intf, size);
		ptrverify(si->intf.intf, "photosyst intf [%d]\n", (long)size);
	}
	for (i=0; i < nrintf; i++)
	{
		if (pmDebugOptions.appl0)
			fprintf(stderr, "%s: updating interface %d: %s\n",
				pmGetProgname(), ids[i], insts[i]);
		update_interface(&si->intf.intf[i], ids[i], insts[i], result, descs);
	}
	si->intf.intf[nrintf].name[0] = '\0';
	si->intf.nrintf = nrintf;
	free(insts);
	free(ids);

	/* /proc/net/snmp */
	si->net.ipv4.Forwarding = extract_count_t(result, descs, IPV4_Forwarding);
	si->net.ipv4.DefaultTTL = extract_count_t(result, descs, IPV4_DefaultTTL);
	si->net.ipv4.InReceives = extract_count_t(result, descs, IPV4_InReceives);
	si->net.ipv4.InHdrErrors = extract_count_t(result, descs, IPV4_InHdrErrors);
	si->net.ipv4.InAddrErrors = extract_count_t(result, descs, IPV4_InAddrErrors);
	si->net.ipv4.ForwDatagrams = extract_count_t(result, descs, IPV4_ForwDatagrams);
	si->net.ipv4.InUnknownProtos = extract_count_t(result, descs, IPV4_InUnknownProtos);
	si->net.ipv4.InDiscards = extract_count_t(result, descs, IPV4_InDiscards);
	si->net.ipv4.InDelivers = extract_count_t(result, descs, IPV4_InDelivers);
	si->net.ipv4.OutRequests = extract_count_t(result, descs, IPV4_OutRequests);
	si->net.ipv4.OutDiscards = extract_count_t(result, descs, IPV4_OutDiscards);
	si->net.ipv4.OutNoRoutes = extract_count_t(result, descs, IPV4_OutNoRoutes);
	si->net.ipv4.ReasmTimeout = extract_count_t(result, descs, IPV4_ReasmTimeout);
	si->net.ipv4.ReasmReqds = extract_count_t(result, descs, IPV4_ReasmReqds);
	si->net.ipv4.ReasmOKs = extract_count_t(result, descs, IPV4_ReasmOKs);
	si->net.ipv4.ReasmFails = extract_count_t(result, descs, IPV4_ReasmFails);
	si->net.ipv4.FragOKs = extract_count_t(result, descs, IPV4_FragOKs);
	si->net.ipv4.FragFails = extract_count_t(result, descs, IPV4_FragFails);
	si->net.ipv4.FragCreates = extract_count_t(result, descs, IPV4_FragCreates);
	si->net.icmpv4.InMsgs = extract_count_t(result, descs, ICMP4_InMsgs);
	si->net.icmpv4.InErrors = extract_count_t(result, descs, ICMP4_InErrors);
	si->net.icmpv4.InDestUnreachs = extract_count_t(result, descs, ICMP4_InDestUnreachs);
	si->net.icmpv4.InTimeExcds = extract_count_t(result, descs, ICMP4_InTimeExcds);
	si->net.icmpv4.InParmProbs = extract_count_t(result, descs, ICMP4_InParmProbs);
	si->net.icmpv4.InSrcQuenchs = extract_count_t(result, descs, ICMP4_InSrcQuenchs);
	si->net.icmpv4.InRedirects = extract_count_t(result, descs, ICMP4_InRedirects);
	si->net.icmpv4.InEchos = extract_count_t(result, descs, ICMP4_InEchos);
	si->net.icmpv4.InEchoReps = extract_count_t(result, descs, ICMP4_InEchoReps);
	si->net.icmpv4.InTimestamps = extract_count_t(result, descs, ICMP4_InTimestamps);
	si->net.icmpv4.InTimestampReps = extract_count_t(result, descs, ICMP4_InTimestampReps);
	si->net.icmpv4.InAddrMasks = extract_count_t(result, descs, ICMP4_InAddrMasks);
	si->net.icmpv4.InAddrMaskReps = extract_count_t(result, descs, ICMP4_InAddrMaskReps);
	si->net.icmpv4.OutMsgs = extract_count_t(result, descs, ICMP4_OutMsgs);
	si->net.icmpv4.OutErrors = extract_count_t(result, descs, ICMP4_OutErrors);
	si->net.icmpv4.OutDestUnreachs = extract_count_t(result, descs, ICMP4_OutDestUnreachs);
	si->net.icmpv4.OutTimeExcds = extract_count_t(result, descs, ICMP4_OutTimeExcds);
	si->net.icmpv4.OutParmProbs = extract_count_t(result, descs, ICMP4_OutParmProbs);
	si->net.icmpv4.OutSrcQuenchs = extract_count_t(result, descs, ICMP4_OutSrcQuenchs);
	si->net.icmpv4.OutRedirects = extract_count_t(result, descs, ICMP4_OutRedirects);
	si->net.icmpv4.OutEchos = extract_count_t(result, descs, ICMP4_OutEchos);
	si->net.icmpv4.OutEchoReps = extract_count_t(result, descs, ICMP4_OutEchoReps);
	si->net.icmpv4.OutTimestamps = extract_count_t(result, descs, ICMP4_OutTimestamps);
	si->net.icmpv4.OutTimestampReps = extract_count_t(result, descs, ICMP4_OutTimestampReps);
	si->net.icmpv4.OutAddrMasks = extract_count_t(result, descs, ICMP4_OutAddrMasks);
	si->net.icmpv4.OutAddrMaskReps = extract_count_t(result, descs, ICMP4_OutAddrMaskReps);
	si->net.tcp.RtoAlgorithm = extract_count_t(result, descs, TCP_RtoAlgorithm);
	si->net.tcp.RtoMin = extract_count_t(result, descs, TCP_RtoMin);
	si->net.tcp.RtoMax = extract_count_t(result, descs, TCP_RtoMax);
	si->net.tcp.MaxConn = extract_count_t(result, descs, TCP_MaxConn);
	si->net.tcp.ActiveOpens = extract_count_t(result, descs, TCP_ActiveOpens);
	si->net.tcp.PassiveOpens = extract_count_t(result, descs, TCP_PassiveOpens);
	si->net.tcp.AttemptFails = extract_count_t(result, descs, TCP_AttemptFails);
	si->net.tcp.EstabResets = extract_count_t(result, descs, TCP_EstabResets);
	si->net.tcp.CurrEstab = extract_count_t(result, descs, TCP_CurrEstab);
	si->net.tcp.InSegs = extract_count_t(result, descs, TCP_InSegs);
	si->net.tcp.OutSegs = extract_count_t(result, descs, TCP_OutSegs);
	si->net.tcp.RetransSegs = extract_count_t(result, descs, TCP_RetransSegs);
	si->net.tcp.InErrs = extract_count_t(result, descs, TCP_InErrs);
	si->net.tcp.OutRsts = extract_count_t(result, descs, TCP_OutRsts);
	si->net.udpv4.InDatagrams = extract_count_t(result, descs, UDPV4_InDatagrams);
	si->net.udpv4.NoPorts = extract_count_t(result, descs, UDPV4_NoPorts);
	si->net.udpv4.InErrors = extract_count_t(result, descs, UDPV4_InErrors);
	si->net.udpv4.OutDatagrams = extract_count_t(result, descs, UDPV4_OutDatagrams);

	/* /proc/net/snmp6 */
	si->net.ipv6.Ip6InReceives = extract_count_t(result, descs, IPV6_InReceives);
	si->net.ipv6.Ip6InHdrErrors = extract_count_t(result, descs, IPV6_InHdrErrors);
	si->net.ipv6.Ip6InTooBigErrors = extract_count_t(result, descs, IPV6_InTooBigErrors);
	si->net.ipv6.Ip6InNoRoutes = extract_count_t(result, descs, IPV6_InNoRoutes);
	si->net.ipv6.Ip6InAddrErrors = extract_count_t(result, descs, IPV6_InAddrErrors);
	si->net.ipv6.Ip6InUnknownProtos = extract_count_t(result, descs, IPV6_InUnknownProtos);
	si->net.ipv6.Ip6InTruncatedPkts = extract_count_t(result, descs, IPV6_InTruncatedPkts);
	si->net.ipv6.Ip6InDiscards = extract_count_t(result, descs, IPV6_InDiscards);
	si->net.ipv6.Ip6InDelivers = extract_count_t(result, descs, IPV6_InDelivers);
	si->net.ipv6.Ip6OutForwDatagrams = extract_count_t(result, descs, IPV6_OutForwDatagrams);
	si->net.ipv6.Ip6OutRequests = extract_count_t(result, descs, IPV6_OutRequests);
	si->net.ipv6.Ip6OutDiscards = extract_count_t(result, descs, IPV6_OutDiscards);
	si->net.ipv6.Ip6OutNoRoutes = extract_count_t(result, descs, IPV6_OutNoRoutes);
	si->net.ipv6.Ip6ReasmTimeout = extract_count_t(result, descs, IPV6_ReasmTimeout);
	si->net.ipv6.Ip6ReasmReqds = extract_count_t(result, descs, IPV6_ReasmReqds);
	si->net.ipv6.Ip6ReasmOKs = extract_count_t(result, descs, IPV6_ReasmOKs);
	si->net.ipv6.Ip6ReasmFails = extract_count_t(result, descs, IPV6_ReasmFails);
	si->net.ipv6.Ip6FragOKs = extract_count_t(result, descs, IPV6_FragOKs);
	si->net.ipv6.Ip6FragFails = extract_count_t(result, descs, IPV6_FragFails);
	si->net.ipv6.Ip6FragCreates = extract_count_t(result, descs, IPV6_FragCreates);
	si->net.ipv6.Ip6InMcastPkts = extract_count_t(result, descs, IPV6_InMcastPkts);
	si->net.ipv6.Ip6OutMcastPkts = extract_count_t(result, descs, IPV6_OutMcastPkts);
	si->net.icmpv6.Icmp6InMsgs = extract_count_t(result, descs, ICMPV6_InMsgs);
	si->net.icmpv6.Icmp6InErrors = extract_count_t(result, descs, ICMPV6_InErrors);
	si->net.icmpv6.Icmp6InDestUnreachs = extract_count_t(result, descs, ICMPV6_InDestUnreachs);
	si->net.icmpv6.Icmp6InPktTooBigs = extract_count_t(result, descs, ICMPV6_InPktTooBigs);
	si->net.icmpv6.Icmp6InTimeExcds = extract_count_t(result, descs, ICMPV6_InTimeExcds);
	si->net.icmpv6.Icmp6InParmProblems = extract_count_t(result, descs, ICMPV6_InParmProblems);
	si->net.icmpv6.Icmp6InEchos = extract_count_t(result, descs, ICMPV6_InEchos);
	si->net.icmpv6.Icmp6InEchoReplies = extract_count_t(result, descs, ICMPV6_InEchoReplies);
	si->net.icmpv6.Icmp6InGroupMembQueries = extract_count_t(result, descs, ICMPV6_InGroupMembQueries);
	si->net.icmpv6.Icmp6InGroupMembResponses = extract_count_t(result, descs, ICMPV6_InGroupMembResponses);
	si->net.icmpv6.Icmp6InGroupMembReductions = extract_count_t(result, descs, ICMPV6_InGroupMembReductions);
	si->net.icmpv6.Icmp6InRouterSolicits = extract_count_t(result, descs, ICMPV6_InRouterSolicits);
	si->net.icmpv6.Icmp6InRouterAdvertisements = extract_count_t(result, descs, ICMPV6_InRouterAdvertisements);
	si->net.icmpv6.Icmp6InNeighborSolicits = extract_count_t(result, descs, ICMPV6_InNeighborSolicits);
	si->net.icmpv6.Icmp6InNeighborAdvertisements = extract_count_t(result, descs, ICMPV6_InNeighborAdvertisements);
	si->net.icmpv6.Icmp6InRedirects = extract_count_t(result, descs, ICMPV6_InRedirects);
	si->net.icmpv6.Icmp6OutMsgs = extract_count_t(result, descs, ICMPV6_OutMsgs);
	si->net.icmpv6.Icmp6OutDestUnreachs = extract_count_t(result, descs, ICMPV6_OutDestUnreachs);
	si->net.icmpv6.Icmp6OutPktTooBigs = extract_count_t(result, descs, ICMPV6_OutPktTooBigs);
	si->net.icmpv6.Icmp6OutTimeExcds = extract_count_t(result, descs, ICMPV6_OutTimeExcds);
	si->net.icmpv6.Icmp6OutParmProblems = extract_count_t(result, descs, ICMPV6_OutParmProblems);
	si->net.icmpv6.Icmp6OutEchoReplies = extract_count_t(result, descs, ICMPV6_OutEchoReplies);
	si->net.icmpv6.Icmp6OutRouterSolicits = extract_count_t(result, descs, ICMPV6_OutRouterSolicits);
	si->net.icmpv6.Icmp6OutNeighborSolicits = extract_count_t(result, descs, ICMPV6_OutNeighborSolicits);
	si->net.icmpv6.Icmp6OutNeighborAdvertisements = extract_count_t(result, descs, ICMPV6_OutNeighborAdvertisements);
	si->net.icmpv6.Icmp6OutRedirects = extract_count_t(result, descs, ICMPV6_OutRedirects);
	si->net.icmpv6.Icmp6OutGroupMembResponses = extract_count_t(result, descs, ICMPV6_OutGroupMembResponses);
	si->net.icmpv6.Icmp6OutGroupMembReductions = extract_count_t(result, descs, ICMPV6_OutGroupMembReductions);
	si->net.udpv6.Udp6InDatagrams = extract_count_t(result, descs, UDPV6_InDatagrams);
	si->net.udpv6.Udp6NoPorts = extract_count_t(result, descs, UDPV6_NoPorts);
	si->net.udpv6.Udp6InErrors = extract_count_t(result, descs, UDPV6_InErrors);
	si->net.udpv6.Udp6OutDatagrams = extract_count_t(result, descs, UDPV6_OutDatagrams);

	/* /proc/diskstats or /proc/partitions */
	insts = NULL; /* silence coverity */
	ids = NULL;
	nrdisk = get_instances("disks", PERDISK_NREAD, descs, &ids, &insts);
	if (nrdisk == 0)
	{
		fprintf(stderr, "%s: no per-disk values available\n",
			pmGetProgname());
		cleanstop(0);
	}
	if (nrdisk > onrdisk)
	{
		size = (nrdisk + 1) * sizeof(struct perdsk);
		si->dsk.dsk = (struct perdsk *)realloc(si->dsk.dsk, size);
		ptrverify(si->dsk.dsk, "photosyst disk [%ld]\n", (long)size);
	}
	for (i=0; i < nrdisk; i++)
	{
		if (pmDebugOptions.appl0)
			fprintf(stderr, "%s: updating disk %d: %s\n",
				pmGetProgname(), ids[i], insts[i]);
		update_disk(&si->dsk.dsk[i], ids[i], insts[i], result, descs);
	}
	si->dsk.dsk[nrdisk].name[0] = '\0';
	si->dsk.ndsk = nrdisk;
	free(insts);
	free(ids);

	/* Device mapper and logical volume (DM/LVM) devices */
	insts = NULL; /* silence coverity */
	ids = NULL;
	nrlvm = get_instances("lvm", PERDM_NREAD, descs, &ids, &insts);
	if (nrlvm > onrlvm)
	{
		size = (nrlvm + 1) * sizeof(struct perdsk);
		si->dsk.lvm = (struct perdsk *)realloc(si->dsk.lvm, size);
		ptrverify(si->dsk.lvm, "photosyst lvm [%ld]\n", (long)size);
	}
	for (i=0; i < nrlvm; i++)
	{
		if (pmDebugOptions.appl0)
			fprintf(stderr, "%s: updating lvm %d: %s\n",
				pmGetProgname(), ids[i], insts[i]);
		update_lvm(&si->dsk.lvm[i], ids[i], insts[i], result, descs);
	}
	si->dsk.lvm[nrlvm].name[0] = '\0';
	si->dsk.nlvm = nrlvm;
	free(insts);
	free(ids);

	/* Multi-device driver (MD) devices */
	insts = NULL; /* silence coverity */
	ids = NULL;
	nrmdd = get_instances("md", PERMD_NREAD, descs, &ids, &insts);
	if (nrmdd > onrmdd)
	{
		size = (nrmdd + 1) * sizeof(struct perdsk);
		si->dsk.mdd = (struct perdsk *)realloc(si->dsk.mdd, size);
		ptrverify(si->dsk.mdd, "photosyst md [%ld]\n", (long)size);
	}

	for (i=0; i < nrmdd; i++)
	{
		if (pmDebugOptions.appl0)
			fprintf(stderr, "%s: updating md %d: %s\n",
				pmGetProgname(), ids[i], insts[i]);
		update_mdd(&si->dsk.mdd[i], ids[i], insts[i], result, descs);
	}
	si->dsk.mdd[nrmdd].name[0] = '\0'; 
	si->dsk.nmdd = nrmdd;
	free(insts);
	free(ids);

	/* NFS server statistics */
	si->nfs.server.rchits = extract_count_t(result, descs, NFS_RCHITS);
	si->nfs.server.rcmiss = extract_count_t(result, descs, NFS_RCMISS);
	si->nfs.server.rcnoca = extract_count_t(result, descs, NFS_RCNOCACHE);
	si->nfs.server.nrbytes = extract_count_t(result, descs, NFS_IORD);
	si->nfs.server.nwbytes = extract_count_t(result, descs, NFS_IOWR);
	si->nfs.server.netcnt = extract_count_t(result, descs, NFS_NETCNT);
	si->nfs.server.netudpcnt = extract_count_t(result, descs, NFS_UDP);
	si->nfs.server.nettcpcnt = extract_count_t(result, descs, NFS_TCP);
	si->nfs.server.nettcpcon = extract_count_t(result, descs, NFS_TCPCONN);
	si->nfs.server.rpccnt = extract_count_t(result, descs, NFS_RPCCNT);
	si->nfs.server.rpcbadfmt = extract_count_t(result, descs, NFS_RPCBADFMT);
	si->nfs.server.rpcbadaut = extract_count_t(result, descs, NFS_RPCBADAUTH);
	si->nfs.server.rpcbadcln = extract_count_t(result, descs, NFS_RPCBADCLNT);

	si->nfs.server.rpcread += extract_count_t_inst(result, descs, NFS_REQS, 6 /*read*/);
	si->nfs.server.rpcwrite += extract_count_t_inst(result, descs, NFS_REQS, 8 /*write*/);
	si->nfs.server.rpcread += extract_count_t_inst(result, descs, NFS3_REQS, 6 /*read*/);
	si->nfs.server.rpcwrite += extract_count_t_inst(result, descs, NFS3_REQS, 7 /*write*/);
	si->nfs.server.rpcread += extract_count_t_inst(result, descs, NFS4_REQS, 26 /*read*/);
	si->nfs.server.rpcwrite += extract_count_t_inst(result, descs, NFS4_REQS, 39 /*write*/);

	/* NFS client statistics */
	si->nfs.client.rpccnt = extract_count_t(result, descs, NFC_RPCCNT);
	si->nfs.client.rpcretrans = extract_count_t(result, descs, NFC_RETRANS);
	si->nfs.client.rpcautrefresh = extract_count_t(result, descs, NFC_AUTHREFRESH);
	si->nfs.client.rpcread += extract_count_t_inst(result, descs, NFC_REQS, 6/*read*/);
	si->nfs.client.rpcwrite += extract_count_t_inst(result, descs, NFC_REQS, 8/*write*/);
	si->nfs.client.rpcread += extract_count_t_inst(result, descs, NFC3_REQS, 6/*read*/);
	si->nfs.client.rpcwrite += extract_count_t_inst(result, descs, NFC3_REQS, 7/*write*/);
	si->nfs.client.rpcread += extract_count_t_inst(result, descs, NFC4_REQS, 1/*read*/);
	si->nfs.client.rpcwrite += extract_count_t_inst(result, descs, NFC4_REQS, 2/*write*/);

	/* NFS client mount statistics */
	insts = NULL;
	ids = NULL;
	nrnfs = get_instances("nfsclient", PERNFS_EXPORT, descs, &ids, &insts);
	if (nrnfs > onrnfs)
	{
		size = (nrnfs + 1) * sizeof(struct pernfsmount);
		si->nfs.nfsmounts.nfsmnt = (struct pernfsmount *)realloc(si->nfs.nfsmounts.nfsmnt, size);
		ptrverify(si->nfs.nfsmounts.nfsmnt, "photosyst nfs [%ld]\n", (long)size);
	}

	for (i=0; i < nrnfs; i++)
	{
		if (pmDebugOptions.appl0)
			fprintf(stderr, "%s: updating nfsmnt %d: %s\n",
				pmGetProgname(), ids[i], insts[i]);
		update_mnt(&si->nfs.nfsmounts.nfsmnt[i], ids[i], insts[i], result, descs);
	}
	si->nfs.nfsmounts.nfsmnt[nrnfs].mountdev[0] = '\0'; 
	si->nfs.nfsmounts.nrmounts = nrnfs;
	free(insts);
	free(ids);

	/*
	** pressure statistics in /proc/pressure (>= 4.20)
	*/
	si->psi.present = present_metric_value(result, PSI_CPUSOME_TOTAL);
	si->psi.cpusome.avg10 = extract_count_t_inst(result, descs, PSI_CPUSOME_AVG, 10);
	si->psi.cpusome.avg60 = extract_count_t_inst(result, descs, PSI_CPUSOME_AVG, 60);
	si->psi.cpusome.avg300 = extract_count_t_inst(result, descs, PSI_CPUSOME_AVG, 300);
	si->psi.cpusome.total = extract_count_t(result, descs, PSI_CPUSOME_TOTAL);
	si->psi.memsome.avg10 = extract_count_t_inst(result, descs, PSI_MEMSOME_AVG, 10);
	si->psi.memsome.avg60 = extract_count_t_inst(result, descs, PSI_MEMSOME_AVG, 60);
	si->psi.memsome.avg300 = extract_count_t_inst(result, descs, PSI_MEMSOME_AVG, 300);
	si->psi.memsome.total = extract_count_t(result, descs, PSI_MEMSOME_TOTAL);
	si->psi.memfull.avg10 = extract_count_t_inst(result, descs, PSI_MEMFULL_AVG, 10);
	si->psi.memfull.avg60 = extract_count_t_inst(result, descs, PSI_MEMFULL_AVG, 60);
	si->psi.memfull.avg300 = extract_count_t_inst(result, descs, PSI_MEMFULL_AVG, 300);
	si->psi.memfull.total = extract_count_t(result, descs, PSI_MEMFULL_TOTAL);
	si->psi.iosome.avg10 = extract_count_t_inst(result, descs, PSI_IOSOME_AVG, 10);
	si->psi.iosome.avg60 = extract_count_t_inst(result, descs, PSI_IOSOME_AVG, 60);
	si->psi.iosome.avg300 = extract_count_t_inst(result, descs, PSI_IOSOME_AVG, 300);
	si->psi.iosome.total = extract_count_t(result, descs, PSI_IOSOME_TOTAL);
	si->psi.iofull.avg10 = extract_count_t_inst(result, descs, PSI_IOFULL_AVG, 10);
	si->psi.iofull.avg60 = extract_count_t_inst(result, descs, PSI_IOFULL_AVG, 60);
	si->psi.iofull.avg300 = extract_count_t_inst(result, descs, PSI_IOFULL_AVG, 300);
	si->psi.iofull.total = extract_count_t(result, descs, PSI_IOFULL_TOTAL);

	/* Infiniband statistics */
	insts = NULL; /* silence coverity */
	ids = NULL;
	nrports = get_instances("ibports", PERIFB_PORT_RATE, descs, &ids, &insts);
	if (nrports > onrports)
	{
		size = (nrports + 1) * sizeof(struct perifb);
		si->ifb.ifb = (struct perifb *)realloc(si->ifb.ifb, size);
		ptrverify(si->ifb.ifb, "photosyst ifb [%d]\n", (long)size);
	}

	for (i=0; i < nrports; i++)
	{
		if (pmDebugOptions.appl0)
			fprintf(stderr, "%s: updating Infiniband port %d: %s\n",
				pmGetProgname(), ids[i], insts[i]);
		update_ibport(&si->ifb.ifb[i], ids[i], insts[i], result, descs);
	}
	si->ifb.ifb[nrports].ibname[0] = '\0';
	si->ifb.nrports = nrports;
	free(insts);
	free(ids);

	/* Apache status */
	si->www.accesses  = extract_count_t(result, descs, WWW_ACCESSES);
	si->www.totkbytes = extract_count_t(result, descs, WWW_TOTKBYTES);
	si->www.uptime    = extract_count_t(result, descs, WWW_UPTIME);
	si->www.bworkers  = extract_integer(result, descs, WWW_BWORKERS);
	si->www.iworkers  = extract_integer(result, descs, WWW_IWORKERS);

	pmFreeResult(result);
	return '\0';
}
