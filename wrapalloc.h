#ifndef _WRAPALLOC_H_
#define _WRAPALLOC_H_

int   sunxi_alloc_open(void);
int   sunxi_alloc_close(void);
void* sunxi_alloc_alloc(int size);
void  sunxi_alloc_free(void *addr);
void* sunxi_alloc_vir2phy(void *addr);
void* sunxi_alloc_phy2vir(void *addr);
void  sunxi_flush_cache(void *addr, int size);
void  sunxi_flush_cache_all(void);

#endif

