# TableCache

## 一、简介

+ FPNN 技术生态基础服务
+ 数据库行级缓存
+ 缓存即数据源
+ 搭配[FPNN DBProxy](https://github.com/highras/dbproxy)使用


## 二、运行环境 & 编译

运行环境：

+ 操作系统：CentOS 6.5、CentOS 7
+ 第三方依赖：FPNN 框架、[FPNN DBProxy](https://github.com/highras/dbproxy)

编译：

1. 请先编译 [FPNN 框架](https://github.com/highras/fpnn)
1. 确保 tableCache 目录和 fpnn 目录处于同一父目录下
1. cd tableCache; make
1. make deploy


## 三、适用场景 & 功能

+ 适用场景

	缓存读多写少的数据，提升响应速度。

+ 功能

	+ 行级数据缓存，缓存数据表的整行数据。
	+ 缓存即数据源，TableCache 自动处理缓存失效的问题。无需再次访问数据库，或者 FPNN DBProxy。
	+ TableCache 自动处理脏数据问题。脏数据出现的条件和概率与**数据库**脏数据出现的概率和条件相同。
	+ 支持 FPNN 体系所有功能。参见：[FPNN 功能介绍](https://github.com/highras/fpnn/blob/master/doc/zh-cn/fpnn-introduction.md)


## 四、限制

+ **每个 hintId 必须能对应到唯一的数据条目**

+ **暂不支持 FPNN DBProxy 的数据业务分组**

	TableCache 对 DBProxy 的数据操作，等价于操作 DBProxy 代理的 cluster 为空串的数据分组。


## 五、使用

+ TableCache 配置请参考 [TableCache 配置说明](doc/zh-cn/TableCache-Configurations.md)
+ TableCache API 请参考 [TableCache 接口说明](doc/zh-cn/TableCache-API.md)
+ TableCache 运维管理请参考 [DBProxy 运维管理](doc/zh-cn/TableCache-Operations.md)
+ TableCache 管理工具请参考 [TableCache 管理工具](doc/zh-cn/TableCache-Tools.md)
