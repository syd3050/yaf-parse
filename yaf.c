/*
  +----------------------------------------------------------------------+
  | Yet Another Framework                                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Xinchen Hui  <laruence@php.net>                              |
  +----------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "main/SAPI.h"
#include "Zend/zend_alloc.h"
#include "ext/standard/info.h"
#include "ext/standard/php_string.h"

#include "php_yaf.h"
#include "yaf_logo.h"
#include "yaf_loader.h"
#include "yaf_exception.h"
#include "yaf_application.h"
#include "yaf_dispatcher.h"
#include "yaf_config.h"
#include "yaf_view.h"
#include "yaf_controller.h"
#include "yaf_action.h"
#include "yaf_request.h"
#include "yaf_response.h"
#include "yaf_router.h"
#include "yaf_bootstrap.h"
#include "yaf_plugin.h"
#include "yaf_registry.h"
#include "yaf_session.h"

ZEND_DECLARE_MODULE_GLOBALS(yaf);

/* {{{ yaf_functions[]
*/
zend_function_entry yaf_functions[] = {
	{NULL, NULL, NULL}
};
/* }}} */

/** {{{ PHP_INI_MH(OnUpdateSeparator)
 */
PHP_INI_MH(OnUpdateSeparator) {
	YAF_G(name_separator) = ZSTR_VAL(new_value);
	YAF_G(name_separator_len) = ZSTR_LEN(new_value);
	return SUCCESS;
}
/* }}} */

/**
很多配置都是从php.ini里读取的,配合全局变量
把configuration_hash中的值，更新到各个模块自己定义的xxx_globals变量中，就是所谓的将ini配置作用到模块。
在随后的执行阶段，php模块无需再次访问configuration_hash，
模块仅需要访问自己的XXX_globals，就可以拿到用户设定的配置。

扩展可以在php.ini中写自己的配置信息，或者在编译PHP时--with-config-file-scan-dir指定目录中的配置文件比如yaf.ini中写配置信息
关于 ini_set 可以设置的配置变量解释如下:
可修改范围是 PHP_INI_PERDIR 的指令可以在php.ini、httpd.conf、.htaccess文件中修改。
可修改范围是 PHP_INI_SYSTEM 的指令可以在php.ini、httpd.conf文件中修改
可修改范围是 PHP_INI_ALL 的指令可以用int_set修改。
*/

/** {{{ PHP_INI
 */
PHP_INI_BEGIN()
    /*
	STD_PHP_INI_ENTRY：注册一个ini配置的配置项，
	OnUpdateString：我们知道ini是通过配置文件配置的，也就是说读取的来源于文件，对于文件内容，只能是string型，
	不管你在文件里面配置的是布尔值、数值型，在内核里面，读出来就是string。所以，如果你配置的是数值或者布尔值的时候，
	肯定在使用的时候也希望它是布尔值或者数值型而不是字符型。那咋办？没关系，zend提供了一个方法，
	就是在你取出的ini值里面可以各种转换类型。比如上面的第四个参数OnUpdateBool，功能就像它的名字一样，
	把取出来的值转成bool类型。
	*/
    /* 参数说明：
	1。这个配置项是在yaf扩展下，名称是library。 
	2。配置项的默认值；
	3。 PHP_INI_ALL：可以用int_set修改
	4。OnUpdateString 把取出来的值转成String类型
	5。global_library：这个全局变量的名，其实就是在全局结构体里面的成员变量名
	6.zend_yaf_globals 结构体名称
	7.yaf_globals定义的结构体名称：联合第六个参数，也即是 zend_yaf_globals *yaf_globals
	*/
	/*全局类库的目录路径*/
	STD_PHP_INI_ENTRY("yaf.library",         	"",  PHP_INI_ALL, OnUpdateString, global_library, zend_yaf_globals, yaf_globals)
	/**/
	STD_PHP_INI_BOOLEAN("yaf.action_prefer",   	"0", PHP_INI_ALL, OnUpdateBool, action_prefer, zend_yaf_globals, yaf_globals)
	STD_PHP_INI_BOOLEAN("yaf.lowcase_path",    	"0", PHP_INI_ALL, OnUpdateBool, lowcase_path, zend_yaf_globals, yaf_globals)
	/*开启的情况下, Yaf在加载不成功的情况下, 会继续让PHP的自动加载函数加载, 从性能考虑, 除非特殊情况, 否则保持这个选项关闭*/
	STD_PHP_INI_BOOLEAN("yaf.use_spl_autoload", "0", PHP_INI_ALL, OnUpdateBool, use_spl_autoload, zend_yaf_globals, yaf_globals)
	/*forward最大嵌套深度*/
	STD_PHP_INI_ENTRY("yaf.forward_limit", 		"5", PHP_INI_ALL, OnUpdateLongGEZero, forward_limit, zend_yaf_globals, yaf_globals)
	/*在处理Controller, Action, Plugin, Model的时候, 类名中关键信息是否是后缀式, 比如UserModel, 而在前缀模式下则是ModelUser*/
	STD_PHP_INI_BOOLEAN("yaf.name_suffix", 		"1", PHP_INI_ALL, OnUpdateBool, name_suffix, zend_yaf_globals, yaf_globals)
	/*在处理Controller, Action, Plugin, Model的时候, 前缀和名字之间的分隔符, 默认为空, 也就是UserPlugin, 加入设置为"_", 则判断的依据就会变成:"User_Plugin", 这个主要是为了兼容ST已有的命名规范*/
	PHP_INI_ENTRY("yaf.name_separator", 		"",  PHP_INI_ALL, OnUpdateSeparator)
/* {{{ This only effects internally */
	STD_PHP_INI_BOOLEAN("yaf.st_compatible",     "0", PHP_INI_ALL, OnUpdateBool, st_compatible, zend_yaf_globals, yaf_globals)
/* }}} */
    /*环境名称，默认值product, 当用INI作为Yaf的配置文件时, 这个指明了Yaf将要在INI配置中读取的节的名字*/
	STD_PHP_INI_ENTRY("yaf.environ",        	"product", PHP_INI_SYSTEM, OnUpdateString, environ_name, zend_yaf_globals, yaf_globals)
	/*开启的情况下, Yaf将会使用命名空间方式注册自己的类, 比如Yaf_Application将会变成Yaf\Application*/
	STD_PHP_INI_BOOLEAN("yaf.use_namespace",   	"0", PHP_INI_SYSTEM, OnUpdateBool, use_namespace, zend_yaf_globals, yaf_globals)
PHP_INI_END();
/* }}} */

/** {{{ PHP_GINIT_FUNCTION
*/
PHP_GINIT_FUNCTION(yaf)
{
	memset(yaf_globals, 0, sizeof(*yaf_globals));
}
/* }}} */

/**
PHP程序的启动可以看作有两个概念上的启动，终止也有两个概念上的终止。
其中一个是 PHP 作为Apache(拿它举例，板砖勿扔)的一个模块的启动与终止，
这次启动 PHP 会初始化一些必要数据，比如与宿主 Apache 有关的，并且这些数据是常驻内存的，终止与之相对。
还有一个概念上的启动就是当 Apache 分配一个页面请求过来的时候，PHP会有一次启动与终止，
这也是我们最常讨论的一种。
a。在最初初始化的时候，就是 PHP 随着 Apache 的启动而诞生在内存里的时候，
它会把自己所有已加载扩展的 MINIT 方法(全称 Module Initialization，是由每个模块自己定义的函数)都执行一遍。
在这个时间里，扩展可以定义一些自己的常量、类、资源等所有会被用户端的 PHP 脚本用到的东西。
但你要记住，这里定义的东西都会随着 Apache 常驻内存，可以被所有请求使用，直到 Apache 卸载掉 PHP 模块。
内核中预置了 PHP_MINIT_FUNCTION 宏函数，来帮助我们实现这个功能：
//walu是扩展的名称
int time_of_minit; // 在MINIT()中初始化，在每次页面请求中输出，看看是否变化
PHP_MINIT_FUNCTION(walu)
{
    time_of_minit=time(NULL); //我们在MINIT启动中对它初始化
    return SUCCESS; //返回SUCCESS代表正常，返回FALIURE就不会加载这个扩展了。
}
模块初始化阶段


*/

/** {{{ PHP_MINIT_FUNCTION
*/
PHP_MINIT_FUNCTION(yaf)
{
	/*
	php_module_startup会调用php_init_config，其目的是解析ini文件并生成configuration_hash。
	那么接下来在php_module_startup中还会做什么事情呢？很显然，就是会将configuration_hash中的配置作用于Zend，Core，
	Standard，SPL等不同模块。当然这并非一个一蹴而就的过程，因为php通常会包含有很多模块，
	php启动的过程中这些模块也会依次进行启动。那么，对模块A进行配置的过程，便是发生在模块A的启动过程中。
	如果模块A需要配置，那么在PHP_MINIT_FUNCTION中，可以调用REGISTER_INI_ENTRIES()来完成。
	REGISTER_INI_ENTRIES会根据当前模块所需要的配置项名称，去configuration_hash查找用户设置的配置值，
	并更新到模块自己的全局空间中。

	参数值填充到 init_entries 结构体中
	*/
	REGISTER_INI_ENTRIES();
	/*
	注册常量或者类等初始化操作
	*/
	if (YAF_G(use_namespace)) {

		REGISTER_STRINGL_CONSTANT("YAF\\VERSION", PHP_YAF_VERSION, 	sizeof(PHP_YAF_VERSION) - 1, CONST_PERSISTENT | CONST_CS);
		REGISTER_STRINGL_CONSTANT("YAF\\ENVIRON", YAF_G(environ_name), strlen(YAF_G(environ_name)), CONST_PERSISTENT | CONST_CS);

		REGISTER_LONG_CONSTANT("YAF\\ERR\\STARTUP_FAILED", 		YAF_ERR_STARTUP_FAILED, CONST_PERSISTENT | CONST_CS);
		REGISTER_LONG_CONSTANT("YAF\\ERR\\ROUTE_FAILED", 		YAF_ERR_ROUTE_FAILED, CONST_PERSISTENT | CONST_CS);
		REGISTER_LONG_CONSTANT("YAF\\ERR\\DISPATCH_FAILED", 	YAF_ERR_DISPATCH_FAILED, CONST_PERSISTENT | CONST_CS);
		REGISTER_LONG_CONSTANT("YAF\\ERR\\AUTOLOAD_FAILED", 	YAF_ERR_AUTOLOAD_FAILED, CONST_PERSISTENT | CONST_CS);
		REGISTER_LONG_CONSTANT("YAF\\ERR\\NOTFOUND\\MODULE", 	YAF_ERR_NOTFOUND_MODULE, CONST_PERSISTENT | CONST_CS);
		REGISTER_LONG_CONSTANT("YAF\\ERR\\NOTFOUND\\CONTROLLER",YAF_ERR_NOTFOUND_CONTROLLER, CONST_PERSISTENT | CONST_CS);
		REGISTER_LONG_CONSTANT("YAF\\ERR\\NOTFOUND\\ACTION", 	YAF_ERR_NOTFOUND_ACTION, CONST_PERSISTENT | CONST_CS);
		REGISTER_LONG_CONSTANT("YAF\\ERR\\NOTFOUND\\VIEW", 		YAF_ERR_NOTFOUND_VIEW, CONST_PERSISTENT | CONST_CS);
		REGISTER_LONG_CONSTANT("YAF\\ERR\\CALL_FAILED",			YAF_ERR_CALL_FAILED, CONST_PERSISTENT | CONST_CS);
		REGISTER_LONG_CONSTANT("YAF\\ERR\\TYPE_ERROR",			YAF_ERR_TYPE_ERROR, CONST_PERSISTENT | CONST_CS);

	} else {
		REGISTER_STRINGL_CONSTANT("YAF_VERSION", PHP_YAF_VERSION, 	sizeof(PHP_YAF_VERSION) - 1, 	CONST_PERSISTENT | CONST_CS);
		REGISTER_STRINGL_CONSTANT("YAF_ENVIRON", YAF_G(environ_name),strlen(YAF_G(environ_name)), 	CONST_PERSISTENT | CONST_CS);

		REGISTER_LONG_CONSTANT("YAF_ERR_STARTUP_FAILED", 		YAF_ERR_STARTUP_FAILED, CONST_PERSISTENT | CONST_CS);
		REGISTER_LONG_CONSTANT("YAF_ERR_ROUTE_FAILED", 			YAF_ERR_ROUTE_FAILED, CONST_PERSISTENT | CONST_CS);
		REGISTER_LONG_CONSTANT("YAF_ERR_DISPATCH_FAILED", 		YAF_ERR_DISPATCH_FAILED, CONST_PERSISTENT | CONST_CS);
		REGISTER_LONG_CONSTANT("YAF_ERR_AUTOLOAD_FAILED", 		YAF_ERR_AUTOLOAD_FAILED, CONST_PERSISTENT | CONST_CS);
		REGISTER_LONG_CONSTANT("YAF_ERR_NOTFOUND_MODULE", 		YAF_ERR_NOTFOUND_MODULE, CONST_PERSISTENT | CONST_CS);
		REGISTER_LONG_CONSTANT("YAF_ERR_NOTFOUND_CONTROLLER", 	YAF_ERR_NOTFOUND_CONTROLLER, CONST_PERSISTENT | CONST_CS);
		REGISTER_LONG_CONSTANT("YAF_ERR_NOTFOUND_ACTION", 		YAF_ERR_NOTFOUND_ACTION, CONST_PERSISTENT | CONST_CS);
		REGISTER_LONG_CONSTANT("YAF_ERR_NOTFOUND_VIEW", 		YAF_ERR_NOTFOUND_VIEW, CONST_PERSISTENT | CONST_CS);
		REGISTER_LONG_CONSTANT("YAF_ERR_CALL_FAILED",			YAF_ERR_CALL_FAILED, CONST_PERSISTENT | CONST_CS);
		REGISTER_LONG_CONSTANT("YAF_ERR_TYPE_ERROR",			YAF_ERR_TYPE_ERROR, CONST_PERSISTENT | CONST_CS);
	}

    /**
	Yaf的一个优点是所有的框架类，不需要编译，在PHP启动的时候加载, 并常驻内存。如何做到这点呢？
	Yaf定义了一个YAF_STARTUP宏来加载类，加载类在 Module init阶段完成

    #define YAF_STARTUP(module)	ZEND_MODULE_STARTUP_N(yaf_##module)(INIT_FUNC_ARGS_PASSTHRU)
    #define ZEND_MODULE_STARTUP_N(module)       zm_startup_##module
    相当于：
    ZEND_MODULE_STARTUP_N(yaf_application)(INIT_FUNC_ARGS_PASSTHRU)
    即：
    zm_startup_yaf_application(INIT_FUNC_ARGS_PASSTHRU)
    */
	/* startup components */
	YAF_STARTUP(application);
	YAF_STARTUP(bootstrap);
	YAF_STARTUP(dispatcher);
	YAF_STARTUP(loader);
	YAF_STARTUP(request);
	YAF_STARTUP(response);
	YAF_STARTUP(controller);
	YAF_STARTUP(action);
	YAF_STARTUP(config);
	YAF_STARTUP(view);
	YAF_STARTUP(router);
	YAF_STARTUP(plugin);
	YAF_STARTUP(registry);
	YAF_STARTUP(session);
	YAF_STARTUP(exception);

	return SUCCESS;
}
/* }}} */

/**
前面该启动的也启动了，该结束的也结束了，现在该 Apache 老人家歇歇的时候，
当 Apache 通知 PHP 自己要 Stop 的时候，PHP 便进入 MSHUTDOWN（俗称Module Shutdown）阶段。
这时候 PHP 便会给所有扩展下最后通牒，如果哪个扩展还有未了的心愿，就放在自己 MSHUTDOWN 方法里，
这可是最后的机会了，一旦 PHP 把扩展的 MSHUTDOWN 执行完，便会进入自毁程序，
这里一定要把自己擅自申请的内存给释放掉，否则就杯具了。

内核中预置了 PHP_MSHUTDOWN_FUNCTION 宏函数来帮助我们实现这个功能：
PHP_MSHUTDOWN_FUNCTION(walu)
{
    FILE *fp=fopen("time_mshutdown.txt","a+");
    fprintf(fp,"%ld\n",time(NULL));
    return SUCCESS;
}

*/

/** {{{ PHP_MSHUTDOWN_FUNCTION
*/
PHP_MSHUTDOWN_FUNCTION(yaf)
{
	UNREGISTER_INI_ENTRIES();

	if (YAF_G(configs)) {
		zend_hash_destroy(YAF_G(configs));
		pefree(YAF_G(configs), 1);
	}

	return SUCCESS;
}
/* }}} */

/**
当一个页面请求到来时候，PHP 会迅速开辟一个新的环境，并重新扫描自己的各个扩展，
遍历执行它们各自的RINIT 方法(俗称 Request Initialization)，
这时候一个扩展可能会初始化在本次请求中会使用到的变量等，
还会初始化用户端（即 PHP 脚本）中的变量之类的，
内核预置了 PHP_RINIT_FUNCTION() 这个宏函数来帮我们实现这个功能：
int time_of_rinit; //在RINIT里初始化，看看每次页面请求的时候是否变化。
PHP_RINIT_FUNCTION(walu)
{
    time_of_rinit=time(NULL);
    return SUCCESS;
}

ps：对于每个请求，当请求到达后，PHP会初始化执行脚本的基本环境，包括保存PHP运行过程中变量名称
和变量值内容的符号表，以及当前所有的函数以及类等信息的符号表。
---------例如记录请求开始时间，随后在请求结束的时候记录结束时间，这样就能够记录处理请求所花费的时间了----------
*/

/** {{{ PHP_RINIT_FUNCTION
*/
PHP_RINIT_FUNCTION(yaf)
{
	YAF_G(throw_exception) = 1;
	YAF_G(ext) = zend_string_init(YAF_DEFAULT_EXT, sizeof(YAF_DEFAULT_EXT) - 1, 0);
	YAF_G(view_ext) = zend_string_init(YAF_DEFAULT_VIEW_EXT, sizeof(YAF_DEFAULT_VIEW_EXT) - 1, 0);
	YAF_G(default_module) = zend_string_init(
			YAF_ROUTER_DEFAULT_MODULE, sizeof(YAF_ROUTER_DEFAULT_MODULE) - 1, 0);
	YAF_G(default_controller) = zend_string_init(
			YAF_ROUTER_DEFAULT_CONTROLLER, sizeof(YAF_ROUTER_DEFAULT_CONTROLLER) - 1, 0);
	YAF_G(default_action) = zend_string_init(
			YAF_ROUTER_DEFAULT_ACTION, sizeof(YAF_ROUTER_DEFAULT_ACTION) - 1, 0);
	return SUCCESS;
}
/* }}} */

/**
好了，现在这个页面请求执行的差不多了，可能是顺利的走到了自己文件的最后，也可能是出师未捷，
半道被用户给 die 或者 exit 了， 这时候 PHP 便会启动回收程序，收拾这个请求留下的烂摊子。
它这次会执行所有已加载扩展的 RSHUTDOWN（俗称 Request Shutdown）方法，
这时候扩展可以抓紧利用内核中的变量表之类的做一些事情， 因为一旦 PHP 把所有扩展的 RSHUTDOWN 方法执行完，
便会释放掉这次请求使用过的所有东西， 包括变量表的所有变量、所有在这次请求中申请的内存等等。
内核预置了 PHP_RSHUTDOWN_FUNCTION 宏函数来帮助我们实现这个功能:
PHP_RSHUTDOWN_FUNCTION(walu)
{
    FILE *fp=fopen("time_rshutdown.txt","a+");
    fprintf(fp,"%ld\n",time(NULL)); //让我们看看是不是每次请求结束都会在这个文件里追加数据
    fclose(fp);
    return SUCCESS;
}
ps:请求处理完成后进入结束阶段，一般脚本执行到末尾或者通过调用exit()或者die()函数，PHP都将进入结束阶段
*/

/** {{{ PHP_RSHUTDOWN_FUNCTION
*/
PHP_RSHUTDOWN_FUNCTION(yaf)
{
	YAF_G(running) = 0;
	YAF_G(in_exception)	= 0;
	YAF_G(catch_exception) = 0;

	if (YAF_G(directory)) {
		zend_string_release(YAF_G(directory));
		YAF_G(directory) = NULL;
	}
	if (YAF_G(local_library)) {
		zend_string_release(YAF_G(local_library));
		YAF_G(local_library) = NULL;
	}
	if (YAF_G(local_namespaces)) {
		zend_string_release(YAF_G(local_namespaces));
		YAF_G(local_namespaces) = NULL;
	}
	if (YAF_G(bootstrap)) {
		zend_string_release(YAF_G(bootstrap));
		YAF_G(bootstrap) = NULL;
	}
	if (Z_TYPE(YAF_G(modules)) == IS_ARRAY) {
		zval_ptr_dtor(&YAF_G(modules));
		ZVAL_UNDEF(&YAF_G(modules));
	}
	if (YAF_G(base_uri)) {
		zend_string_release(YAF_G(base_uri));
		YAF_G(base_uri) = NULL;
	}
	if (YAF_G(view_directory)) {
		zend_string_release(YAF_G(view_directory));
		YAF_G(view_directory) = NULL;
	}
	if (YAF_G(view_ext)) {
		zend_string_release(YAF_G(view_ext));
	}
	if (YAF_G(default_module)) {
		zend_string_release(YAF_G(default_module));
	}
	if (YAF_G(default_controller)) {
		zend_string_release(YAF_G(default_controller));
	}
	if (YAF_G(default_action)) {
		zend_string_release(YAF_G(default_action));
	}
	if (YAF_G(ext)) {
		zend_string_release(YAF_G(ext));
	}
	YAF_G(default_route) = NULL;

	return SUCCESS;
}
/* }}} */

/** {{{ PHP_MINFO_FUNCTION
*/
PHP_MINFO_FUNCTION(yaf)
{
	php_info_print_table_start();
	if (PG(expose_php) && !sapi_module.phpinfo_as_text) {
		php_info_print_table_header(2, "yaf support", YAF_LOGO_IMG"enabled");
	} else {
		php_info_print_table_header(2, "yaf support", "enabled");
	}

	php_info_print_table_row(2, "Version", PHP_YAF_VERSION);
	php_info_print_table_row(2, "Supports", YAF_SUPPORT_URL);
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */

/** {{{ DL support
 */
#ifdef COMPILE_DL_YAF
ZEND_GET_MODULE(yaf)
#endif
/* }}} */

/** {{{ module depends
 */
#if ZEND_MODULE_API_NO >= 20050922
zend_module_dep yaf_deps[] = {
	ZEND_MOD_REQUIRED("spl")
	ZEND_MOD_REQUIRED("pcre")
	ZEND_MOD_OPTIONAL("session")
	{NULL, NULL, NULL}
};
#endif
/* }}} */

/** {{{ yaf_module_entry
*/
zend_module_entry yaf_module_entry = {
#if ZEND_MODULE_API_NO >= 20050922
	STANDARD_MODULE_HEADER_EX, NULL,
	yaf_deps,
#else
	STANDARD_MODULE_HEADER,
#endif
	"yaf",
	yaf_functions,
	PHP_MINIT(yaf),
	PHP_MSHUTDOWN(yaf),
	PHP_RINIT(yaf),
	PHP_RSHUTDOWN(yaf),
	PHP_MINFO(yaf),
	PHP_YAF_VERSION,
	PHP_MODULE_GLOBALS(yaf),
	PHP_GINIT(yaf),
	NULL,
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
