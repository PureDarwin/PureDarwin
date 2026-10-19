#ifndef _FBSD_COMPAT_SYS_MODULE_H_
#define _FBSD_COMPAT_SYS_MODULE_H_


#include <sys/linker_set.h>


typedef struct module* module_t;

typedef enum modeventtype {
	MOD_LOAD,
	MOD_UNLOAD,
	MOD_SHUTDOWN,
	MOD_QUIESCE
} modeventtype_t;


#ifdef PD_LINUXKPI
#include <sys/kernel.h>

// net80211 and linuxkpi register through module events, so run them as sysinits
typedef int (*modeventhand_t)(module_t, int, void *);

typedef struct moduledata {
	const char *name;
	modeventhand_t evhand;
	void *priv;
} moduledata_t;

#define DECLARE_MODULE(name, data, sub, order)				\
static void								\
name##_pd_modevent_load(void *arg)					\
{									\
	const moduledata_t *md = (const moduledata_t *)arg;		\
	if (md->evhand != NULL)						\
		md->evhand(NULL, MOD_LOAD, md->priv);			\
}									\
static void								\
name##_pd_modevent_unload(void *arg)					\
{									\
	const moduledata_t *md = (const moduledata_t *)arg;		\
	if (md->evhand != NULL)						\
		md->evhand(NULL, MOD_UNLOAD, md->priv);			\
}									\
SYSINIT(name##_pd_load, sub, order, name##_pd_modevent_load, &data);	\
SYSUNINIT(name##_pd_unload, sub, order, name##_pd_modevent_unload, &data)
#else
#define DECLARE_MODULE(name, data, sub, order)
#endif

#define MODULE_VERSION(name, version)
#define MODULE_DEPEND(module, mdepend, vmin, vpref, vmax)
#define	MODULE_PNP_INFO(d, b, unique, t, n)

#endif
