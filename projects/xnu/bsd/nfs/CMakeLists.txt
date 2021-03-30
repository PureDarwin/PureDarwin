install_xnu_headers(TARGET_NAME xnu.headers.bsd_nfs DESTINATION user SUBDIRECTORY nfs FILES
    krpc.h nfs.h nfsdiskless.h nfsm_subs.h nfsmount.h nfsnode.h
    nfs_lock.h nfs_gss.h nfs_ioctl.h nfsproto.h nfsrvcache.h rpcv2.h xdr_subs.h
)
