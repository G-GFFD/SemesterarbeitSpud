#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x141d70f7, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x7fd02f4a, __VMLINUX_SYMBOL_STR(nf_unregister_hook) },
	{ 0x26d838eb, __VMLINUX_SYMBOL_STR(netlink_kernel_release) },
	{ 0x66524450, __VMLINUX_SYMBOL_STR(nf_register_hook) },
	{ 0xd3aef583, __VMLINUX_SYMBOL_STR(__netlink_kernel_create) },
	{ 0x477a3441, __VMLINUX_SYMBOL_STR(init_net) },
	{ 0x69acdf38, __VMLINUX_SYMBOL_STR(memcpy) },
	{ 0x3ab349fb, __VMLINUX_SYMBOL_STR(netlink_unicast) },
	{ 0x950de3d3, __VMLINUX_SYMBOL_STR(__alloc_skb) },
	{ 0x2a1ccd04, __VMLINUX_SYMBOL_STR(__nlmsg_put) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "AC67DCD463953AAE8B597C8");
