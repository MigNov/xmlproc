//#define DEBUG_CPU

#include "xml.h"

#ifdef DEBUG_CPU
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "cpu: " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
do {} while(0)
#endif

int bHasCpuid = -1;

int CPUIDRead(unsigned int ax, unsigned int *p)
{
	/* Initialize output values */
	p[0] = 0;
	p[1] = 0;
	p[2] = 0;
	p[3] = 0;

	__asm __volatile
	("movl %%ebx, %%esi\n\t"
	"cpuid\n\t"
	"xchgl %%ebx, %%esi"
	: "=a" (p[0]), "=S" (p[1]),
	  "=c" (p[2]), "=d" (p[3])
	: "0" (ax));

	DPRINTF("CPUID[0x%08x] = { eax = 0x%08x, ebx = 0x%08x, ecx = 0x%08x, "
		"edx = 0x%08x }\n",ax, p[0], p[1], p[2], p[3]);

	return 0;
}


int MSRRead(int cpu, uint32_t reg, uint64_t *value)
{
	int fd;
	uint64_t data;
	char msrfn[256];

	if (value == NULL)
		return -EINVAL;

	snprintf(msrfn, sizeof(msrfn), "/dev/cpu/%d/msr", cpu);
	fd = open(msrfn, O_RDONLY);
	if (fd < 0) {
		if (errno == ENXIO)
			return -EMSRNOCPU;
		else
		if (errno == EIO)
			return -EMSRNOSUP;
		else
		if ((errno == EACCES)
			|| (errno == EPERM))
			return -EMSRNOPERM;
		else
			return -EMSRUERROR;
	}

	if (pread(fd, &data, sizeof(data), reg) != sizeof(data)) {
		close(fd);
		return (errno == EIO) ? -EMSRNODATA : -EMSRUERROR;
	}

	close(fd);

	*value = data;

	DPRINTF("CPU %d reg %04x: 0x%" PRIx64 "\n", cpu, reg, data);
	return 0;
}

char *MSRGetError(int err)
{        
	/* We make it always positive error code */
	if (err < 0)
		err = -err;
    
	switch (err) {
		case EMSROK:	return "No error";
				break;
		case EMSRNOCPU: return "Desired CPU MSR not found";
				break;
		case EMSRNOSUP:	return "Desired CPU MSR is not supported";
				break;
		case EMSRNODATA:return "MSR didn't return any data";
				break;
		case EMSRNOPERM:return "No permissions to access MSR";
				break;
		default:	return "Unknown error";
				break;
	}

	return NULL;
}

char *CPUGetVendor(int allowSubst) {
	int i, idx, tmp, shift;
	unsigned int regs[4];
	char *cpuvendor;

	if ((cpuVendor != NULL) && (allowSubst))
		return cpuVendor;

	cpuvendor = (char *)malloc( 13 * sizeof(char));

	CPUIDRead(0x00000000, regs);
	for (i = 0; i < 12; i++) {
		tmp = (i / 4) + 1;
		shift = (4 - ((i % 4) + 1)) * 8;

		idx = i;

		/* Swap bytes 2 and 3 */
		if (tmp == 3)
			idx = i - 4;
		if (tmp == 2)
			idx = i + 4;

		cpuvendor[idx] = ( regs[tmp] << shift ) >> 24;
	}
	cpuvendor[12] = 0;

	return cpuvendor;
}

int CPUGetModel(int *bIntel)
{
	int i, idx, type, model, tmp, shift;
	unsigned int regs[4];
	unsigned int regs2[4];
	char cpuvendor[13] = { 0 };

	CPUIDRead(0x00000000, regs);

	if (bIntel != NULL) {
		for (i = 0; i < 12; i++) {
			tmp = (i / 4) + 1;
			shift = (4 - ((i % 4) + 1)) * 8;

			idx = i;

			/* Swap bytes 2 and 3 */
			if (tmp == 3)
				idx = i - 4;
			if (tmp == 2)
				idx = i + 4;

			cpuvendor[idx] = ( regs[tmp] << shift ) >> 24;
		}
		cpuvendor[12] = 0;

		DPRINTF("CPUVendor: %s\n", cpuvendor);
	        *bIntel = (strcmp(cpuvendor, "GenuineIntel") == 0);
	}

	if (regs[0]>=0x00000001) {
		CPUIDRead(0x00000001, regs2);

		type = (regs2[0] >> 8) & 0xF;
		model=(regs2[0] >> 4) & 0xF;
		if (type == 0xF)
			type = ((regs2[0] >> 20) & 0xFF) + 0xF;

		if ((type == 0xF) || (type == 6))
			model |= ((regs2[0] >> 16) & 0xF) << 4;

		DPRINTF("CPU Model = 0x%02x\n", model);
		return model;
	}

	DPRINTF("Unknown CPU model\n");
	return -EINVAL;
}

int CPUGetCount()
{
	struct dirent *entry;
	int val, maxVal = -ENOTSUP;
	DIR *d;

	d = opendir("/dev/cpu");
	if (d == NULL) {
		DPRINTF("Cannot get the CPU list\n");
		return maxVal;
	}

	while ((entry = readdir(d)) != NULL) {
		if ((strlen(entry->d_name) > 0) && (entry->d_name[0] != '.')
			&& (entry->d_type == 4)) { 
			val = atoi(entry->d_name);
			if (val > maxVal)
				maxVal = val;
		 }
	}
	closedir(d);

	if (maxVal <= 0)
		DPRINTF("No CPU entries found\n");
	else
		DPRINTF("Highest CPU id = %d\n", maxVal);

	return maxVal;
}

