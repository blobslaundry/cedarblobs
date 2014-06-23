#include "all.h"
#if 0
#define DEBUG(...) fprintf(stderr, "WRAPALLOC: " __VA_ARGS__)
#else
#define DEBUG(...)
#endif

#define BLOCKPAGESIZE 0x1000
typedef struct block_struct {
	void *vir_start;
	void *phy_start;
	unsigned int size;
	struct block_struct *prev;
	struct block_struct *next;
} block_t;
static block_t *headblock = NULL;
static block_t *block_new(void *start, unsigned int size)
{
	block_t *b = malloc(sizeof(block_t));
	b->vir_start = 0x0;
	b->phy_start = start;
	b->size = size;
	b->prev = b->next = NULL;
	return b;
}
static void block_init(void *start, unsigned int size)
{
	if(headblock == NULL) headblock = block_new(start, size);
}
static void block_remove(block_t *b)
{
	if(b->prev) b->prev->next = b->next;
	if(b->next) b->next->prev = b->prev;
	if(b == headblock) headblock = b->next;
}
static block_t *block_split(block_t *b, unsigned int newsize)
{
	block_t *n = b;
	if(b->size > newsize) {
		n = block_new(b->phy_start, newsize);
		b->phy_start += newsize;
		b->size -= newsize;

		if(b->prev) b->prev->next = n;
		n->prev = b->prev;
		n->next = b;
		b->prev = n;
		if(b == headblock) headblock = n;
	}
	return n;
}
static block_t *block_find_free(unsigned int size)
{
	unsigned int aligned = (size + BLOCKPAGESIZE-1) & ~(BLOCKPAGESIZE-1);
	block_t *n = NULL;
	block_t *b = headblock;
	while(b) {
		if(b->vir_start == 0x0 && b->size >= aligned) {
			n = block_split(b, aligned);
			break;
		}
		b = b->next;
	}
	return n;
}
static block_t *block_find_vir(void *vir)
{
	block_t *b = headblock;
	while(b) {
		if(b->vir_start != 0 && b->vir_start <= vir && vir < (b->vir_start + b->size)) break;
		b = b->next;
	}
	return b;
}
static block_t *block_find_phy(void *phy)
{
	block_t *b = headblock;
	while(b) {
		if(b->vir_start != 0 && b->phy_start <= phy && phy < (b->phy_start + b->size)) break;
		b = b->next;
	}
	return b;
}
static block_t *block_merge(block_t *b)
{
	block_t *a = NULL;
	if(b->prev && b->prev->vir_start == 0x0) {
		a = b->prev;
		block_remove(a);
		b->phy_start = a->phy_start;
		b->size += a->size;
		free(a);
	}
	if(b->next && b->next->vir_start == 0x0) {
		a = b->next;
		block_remove(a);
		b->size += a->size;
		free(a);
	}
	return b;
}
static block_t *block_free(void *vir)
{
	block_t *b = block_find_vir(vir);
	if(b) {
		b->vir_start = 0x0;
		block_merge(b);
	}
	return b;
}
static void block_finish(void)
{
	block_t *a = NULL;
	block_t *b = headblock;
	while(b) {
		a = b->next;
		free(b);
		b = a;
	}
	headblock = NULL;
}
static void block_validity(void)
{
	block_t *b = headblock;
	while(b) {
		/*printf("block: (%08x)%08x %8x\n", b->vir_start, b->phy_start, b->size);*/
		if(b->size < BLOCKPAGESIZE) {
			printf("size < BLOCKPAGESIZE\n");
			goto error;
		}
		b = b->next;
		if(b) {
			if(b->phy_start != (b->prev->phy_start + b->prev->size)) {
				printf("continuous failed\n");
				goto error;
			}
			if(b->vir_start == 0x0 && b->prev->vir_start == 0x0) {
				printf("zero virtual adjacent\n");
				goto error;
			}
		}
	}
	return;
error:
	DEBUG("block validity check failed.\n");
	exit(EXIT_FAILURE);
}


static int devicefd = -1;
struct {
	unsigned int phy_start;
	int          phy_size;
	unsigned int reg_addr;
} device_info;
enum {
	IOCTRL_GET_ENV_INFO	= 0x101,
	IOCTRL_FLUSH_CACHE	= 0x20b,
	IOCTRL_FLUSH_CACHE_ALL	= 0x20d,
};
typedef struct {
	long start;
	long end;
} flush_cache_t;

int sunxi_alloc_open(void)
{
	DEBUG("open\n");
	devicefd = open("/dev/cedar_dev", O_RDWR);
	if(devicefd < 0)
		return -1;
	
	ioctl(devicefd, IOCTRL_GET_ENV_INFO, &device_info);
	DEBUG("info: %08x %8x %08x\n", device_info.phy_start, device_info.phy_size, device_info.reg_addr);

	block_init((void *)(device_info.phy_start & 0x0fffffff), device_info.phy_size);

	return 0;
}

int sunxi_alloc_close(void)
{
	DEBUG("close\n");
	if(devicefd >= 0)
		close(devicefd);
	block_finish();
	return 0;
}

void* sunxi_alloc_alloc(int size)
{
	block_validity();
	DEBUG("alloc %8x\n", size);
	void *addr = 0x0;
	block_t *b = block_find_free(size);
	if(b) {
		addr = mmap(0x0, size, PROT_READ | PROT_WRITE, MAP_SHARED, devicefd, (off_t)b->phy_start | 0xc0000000);
		if(addr == MAP_FAILED)
			return 0x0;
		b->vir_start = addr;
	}
	return addr;
}

void sunxi_alloc_free(void *addr)
{
	DEBUG("free %08x\n", addr);
	block_t *b = block_find_vir(addr);
	if(b && b->vir_start == addr) {
		munmap(b->vir_start, b->size);
		block_free(b->vir_start);
		return;
	}
	DEBUG("Attemp to free non allocated block.\n");
	exit(EXIT_FAILURE);
}

void* sunxi_alloc_vir2phy(void *addr)
{
	void *r = 0x0;
	block_t *b = block_find_vir(addr);
	if(b && addr >= b->vir_start)
		r = b->phy_start + (addr - b->vir_start);
	DEBUG("vir2phy %08x -> %08x\n", addr, r);
	return r;
}

void* sunxi_alloc_phy2vir(void *addr)
{
	void *r = 0x0;
	block_t *b = block_find_phy(addr);
	if(b && addr >= b->phy_start)
		r = b->vir_start + (addr - b->phy_start);
	DEBUG("phy2vir %08x -> %08x\n", addr, r);
	return r;
}

void sunxi_flush_cache(void *addr, int size)
{
	DEBUG("flush cache %08x %8x\n", addr, size);
	flush_cache_t ifc;
	ifc.start = (long)addr;
	ifc.end = (long)addr + size - 1;
	ioctl(devicefd, IOCTRL_FLUSH_CACHE, &ifc);
}

void sunxi_flush_cache_all(void)
{
	DEBUG("flush cache all\n");
	ioctl(devicefd, IOCTRL_FLUSH_CACHE_ALL, 0);
}

