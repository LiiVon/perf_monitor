#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xbd03ed67, "phys_base" },
	{ 0x992ecee6, "kernel_cpustat" },
	{ 0x8db9b6ac, "__usecs_to_jiffies" },
	{ 0xf296206e, "nr_cpu_ids" },
	{ 0xb5c51982, "__cpu_possible_mask" },
	{ 0x86632fd6, "_find_next_bit" },
	{ 0x5ae9ee26, "__per_cpu_offset" },
	{ 0x40a621c5, "snprintf" },
	{ 0x911be47f, "get_cpu_idle_time_us" },
	{ 0x911be47f, "get_cpu_iowait_time_us" },
	{ 0x90a48d82, "__ubsan_handle_out_of_bounds" },
	{ 0x85acaba2, "cancel_delayed_work_sync" },
	{ 0xd17123e4, "device_destroy" },
	{ 0x07a5cde6, "class_destroy" },
	{ 0x2e921116, "cdev_del" },
	{ 0x0bc5fb0d, "unregister_chrdev_region" },
	{ 0x02e1dca7, "free_pages" },
	{ 0x6bded543, "get_free_pages_noprof" },
	{ 0x9f222e1e, "alloc_chrdev_region" },
	{ 0xd2554727, "cdev_init" },
	{ 0xdb375fb3, "cdev_add" },
	{ 0x326b4c7f, "class_create" },
	{ 0x160b81b4, "device_create" },
	{ 0x71798f7e, "delayed_work_timer_fn" },
	{ 0x02f9bbf0, "timer_init_key" },
	{ 0xaef1f20d, "system_percpu_wq" },
	{ 0x8ce83585, "queue_delayed_work_on" },
	{ 0xd272d446, "__fentry__" },
	{ 0xe8213e80, "_printk" },
	{ 0xd272d446, "__x86_return_thunk" },
	{ 0xb1ad3f2f, "boot_cpu_data" },
	{ 0xbd03ed67, "page_offset_base" },
	{ 0x487bfbdf, "remap_pfn_range" },
	{ 0x84f07bf7, "cachemode2protval" },
	{ 0xd954c786, "module_layout" },
};

static const u32 ____version_ext_crcs[]
__used __section("__version_ext_crcs") = {
	0xbd03ed67,
	0x992ecee6,
	0x8db9b6ac,
	0xf296206e,
	0xb5c51982,
	0x86632fd6,
	0x5ae9ee26,
	0x40a621c5,
	0x911be47f,
	0x911be47f,
	0x90a48d82,
	0x85acaba2,
	0xd17123e4,
	0x07a5cde6,
	0x2e921116,
	0x0bc5fb0d,
	0x02e1dca7,
	0x6bded543,
	0x9f222e1e,
	0xd2554727,
	0xdb375fb3,
	0x326b4c7f,
	0x160b81b4,
	0x71798f7e,
	0x02f9bbf0,
	0xaef1f20d,
	0x8ce83585,
	0xd272d446,
	0xe8213e80,
	0xd272d446,
	0xb1ad3f2f,
	0xbd03ed67,
	0x487bfbdf,
	0x84f07bf7,
	0xd954c786,
};
static const char ____version_ext_names[]
__used __section("__version_ext_names") =
	"phys_base\0"
	"kernel_cpustat\0"
	"__usecs_to_jiffies\0"
	"nr_cpu_ids\0"
	"__cpu_possible_mask\0"
	"_find_next_bit\0"
	"__per_cpu_offset\0"
	"snprintf\0"
	"get_cpu_idle_time_us\0"
	"get_cpu_iowait_time_us\0"
	"__ubsan_handle_out_of_bounds\0"
	"cancel_delayed_work_sync\0"
	"device_destroy\0"
	"class_destroy\0"
	"cdev_del\0"
	"unregister_chrdev_region\0"
	"free_pages\0"
	"get_free_pages_noprof\0"
	"alloc_chrdev_region\0"
	"cdev_init\0"
	"cdev_add\0"
	"class_create\0"
	"device_create\0"
	"delayed_work_timer_fn\0"
	"timer_init_key\0"
	"system_percpu_wq\0"
	"queue_delayed_work_on\0"
	"__fentry__\0"
	"_printk\0"
	"__x86_return_thunk\0"
	"boot_cpu_data\0"
	"page_offset_base\0"
	"remap_pfn_range\0"
	"cachemode2protval\0"
	"module_layout\0"
;

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "D930E158C941A75C66D20FF");
