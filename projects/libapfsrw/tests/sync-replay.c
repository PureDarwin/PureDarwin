/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

/* Exercises the real kernel library/port over a delayed-write buffer-cache model.
 * The fixture must be a disposable copy of an APFS container with a volume.
 */
#include "sync-replay-env.h"
struct test_buf { int64_t block; int dirty; unsigned char data[4096]; };
static struct test_buf buffers[65536];
static size_t nbuffers;
static int fd, discard_enabled = 1, ioctl_error;
static uint64_t writes, discarded, barriers, flushes;
static unsigned wait_depth;
static int fail_after = -1;

uint64_t mach_absolute_time(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (uint64_t)t.tv_sec*1000000000+t.tv_nsec; }
void absolutetime_to_nanoseconds(uint64_t t, uint64_t *ns) { *ns=t; }
void microtime(struct timeval *t) { gettimeofday(t,NULL); }
void *vfs_context_kernel(void) { return NULL; }
void *buf_dataptr(buf_t b) { return b->data; }
void buf_brelse(buf_t b) { (void)b; }
void buf_bdwrite(buf_t b) { b->dirty=1; }
buf_t buf_getblk(vnode_t v, daddr64_t b, int size, int sf, int st, int op) {
    (void)v; (void)sf; (void)st; (void)op; assert(size==4096);
    if (fail_after == 0) { fail_after = -1; return NULL; }
    if (fail_after > 0) fail_after--;
    for(size_t i=0;i<nbuffers;i++) if(buffers[i].block==b) return &buffers[i];
    assert(nbuffers<65536);
    buf_t p=&buffers[nbuffers++]; p->block=b; p->dirty=0;
    assert(pread(fd,p->data,4096,b*512)==4096); return p;
}
int buf_meta_bread(vnode_t v,daddr64_t b,int s,void *cred,buf_t *out) {
    (void)cred;
    int saved=fail_after; fail_after=-1;
    *out=buf_getblk(v,b,s,0,0,0); fail_after=saved; return 0;
}
int buf_invalblkno(vnode_t v,daddr64_t b,int flags) {
    (void)v; assert(flags==BUF_WAIT);
    if(!discard_enabled) return 0;
    for(size_t i=0;i<nbuffers;i++) if(buffers[i].block==b) {
        discarded+=buffers[i].dirty;
        buffers[i]=buffers[--nbuffers]; return 0;
    }
    return 0;
}
void buf_flushdirtyblks(vnode_t v,int wait,int flags,const char *msg) {
    (void)v; (void)wait; (void)flags; (void)msg; flushes++;
    for(size_t i=0;i<nbuffers;i++) if(buffers[i].dirty) {
        assert(pwrite(fd,buffers[i].data,4096,buffers[i].block*512)==4096);
        buffers[i].dirty=0; writes++;
    }
}
int VNOP_IOCTL(vnode_t v,int cmd,char *arg,int flags,void *ctx) {
    (void)v; (void)arg; (void)flags; (void)ctx; assert(cmd==DKIOCSYNCHRONIZE);
    barriers++; assert(fsync(fd)==0); return ioctl_error;
}
static void sync_wait(void *ref,int done) { (void)ref; if(done) { assert(wait_depth==1); wait_depth--; } else { assert(wait_depth==0); wait_depth++; } }
static void check_at(int err,int line) { if(err) { fprintf(stderr,"APFS at line %d: %s\n",line,apfsrw_strerror(err)); exit(1); } }
#define check(err) check_at((err),__LINE__)
static void verify(struct apfsrw *fs,const unsigned char *expected,size_t n) {
    unsigned char *got=NULL; size_t len=0;
    check(apfsrw_read_file(fs,"/sync-replay",&got,&len));
    assert(len==n && memcmp(got,expected,n)==0); free(got);
}
static int direct_io(void *ref,void *buf,size_t n,uint64_t off,int write) {
    (void)ref; (void)buf; (void)n; (void)off; (void)write;
    assert(0); return -1;
}
static void selection_checks(struct apfsrw *fs,struct apfsrw_kern_dev *dev) {
    // All owned keys collide in the table. Only the intersection may be discarded.
    uint64_t owned[]={40,56,72}, retired[]={0,40,80,72,88};
    int saved=discard_enabled; discard_enabled=1; nbuffers=0;
    int64_t sectors[]={0,320,448,576,640,704};
    for(size_t i=0;i<6;i++) { buffers[i].block=sectors[i]; buffers[i].dirty=1; }
    nbuffers=6;
    dev->io=direct_io;
    apfsrw_discard_superseded(fs,owned,3,retired,5); assert(nbuffers==6);
    dev->io=NULL;
    apfsrw_discard_superseded(fs,owned,0,retired,5); assert(nbuffers==6);
    apfsrw_discard_superseded(fs,owned,3,retired,0); assert(nbuffers==6);
    apfsrw_discard_superseded(fs,owned,3,retired,5); assert(nbuffers==4);
    for(size_t i=0;i<nbuffers;i++) assert(buffers[i].block!=320 && buffers[i].block!=576);
    // Live owned blocks, committed retired blocks, and block zero stayed cached.
    nbuffers=0; discard_enabled=saved;
}
int main(int argc,char **argv) {
    assert(argc==3); discard_enabled=atoi(argv[2]);
    fd=open(argv[1],O_RDWR); assert(fd>=0);
    struct stat st; assert(fstat(fd,&st)==0);
    struct apfsrw_kern_dev dev={.devvp=&fd,.dev_bsize=512,.block_size=4096,.sync_wait=sync_wait};
    struct apfsrw *fs=NULL; check(apfsrw_open_kernel(&dev,(uint64_t)st.st_size/4096,1,0,0,&fs));
    // Seed a multi-leaf tree so writes exercise physical omap paths and root counts.
    unsigned char seed_data[4096]="sentinel";
    check(apfsrw_batch_begin(fs));
    for (int i=0;i<300;i++) {
        char path[32]; snprintf(path,sizeof(path),"/sync-seed-%03d",i);
        check(apfsrw_create_file(fs,path,seed_data,sizeof(seed_data),0644,0,0));
    }
    check(apfsrw_batch_end(fs));
    unsigned char expected[4096]; memset(expected,0,sizeof(expected));
    check(apfsrw_create_file(fs,"/sync-replay",expected,sizeof(expected),0644,0,0));
    uint64_t before=writes, bd=discarded, bs=barriers;
    for(int batch=0;batch<4;batch++) {
        check(apfsrw_batch_begin(fs));
        for(int i=0;i<64;i++) {
            expected[(batch*64+i)*8]=(unsigned char)(i+1);
            check(apfsrw_write_range(fs,"/sync-replay",(batch*64+i)*8,expected+(batch*64+i)*8,8));
            verify(fs,expected,sizeof(expected));
            apfsrw_batch_discard_superseded(fs);
        }
        // A failed mutation must preserve every successful op in this batch.
        assert(apfsrw_mkdir(fs,"/absent/child",0755,0,0)==APFSRW_ENOENT);
        verify(fs,expected,sizeof(expected));
        // Fail after data and tree writes have begun, then roll back only that op.
        fail_after=2;
        unsigned char failed_byte=0xfe;
        assert(apfsrw_write_range(fs,"/sync-replay",4095,&failed_byte,1)==APFSRW_EIO);
        assert(fail_after==-1);
        verify(fs,expected,sizeof(expected));
        check(apfsrw_batch_end(fs)); verify(fs,expected,sizeof(expected));
        // Force reloads from the durable image, without our buffer-cache model.
        apfsrw_close(fs); nbuffers=0;
        check(apfsrw_open_kernel(&dev,(uint64_t)st.st_size/4096,1,0,0,&fs));
        verify(fs,expected,sizeof(expected));
        unsigned char *sentinel=NULL; size_t sentinel_len=0;
        check(apfsrw_read_file(fs,"/sync-seed-123",&sentinel,&sentinel_len));
        assert(sentinel_len==sizeof(seed_data) && memcmp(sentinel,seed_data,sizeof(seed_data))==0); free(sentinel);
    }
    assert(barriers-bs==8);
    printf("discard=%d disk_blocks=%"PRIu64" obsolete_dirty_blocks=%"PRIu64" barriers=%"PRIu64" (256 writes, 4 batches)\n",discard_enabled,writes-before,discarded-bd,barriers-bs);
    uint64_t bs_empty=barriers;
    check(apfsrw_batch_begin(fs)); check(apfsrw_batch_end(fs)); assert(barriers==bs_empty);
    selection_checks(fs,&dev);
    apfsrw_close(fs); close(fd); return 0;
}
