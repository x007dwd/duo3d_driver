#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("usb:v04B4pA1E0d*dc*dsc*dp*ic*isc*ip*in*");
MODULE_ALIAS("usb:vC0DEpA1E0d*dc*dsc*dp*ic*isc*ip*in*");

MODULE_INFO(srcversion, "85D4F6F85B5601175A5896B");
