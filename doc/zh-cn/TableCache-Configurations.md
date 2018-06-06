# TableCache 配置说明

## 一、配置文件

配置文件模版请参见：[配置文件模板](../../tableCache.conf)

### 配置条目说明

1. FPNN 框架通用配置

	FPNN 框架通用配置，包含，但不限于以下配置项，具体请参考：[FPNN 标准配置模板](https://github.com/highras/fpnn/blob/master/doc/conf.template)

	+ **FPNN.server.listening.ip**
	+ **FPNN.server.listening.port**
	+ **FPNN.server.name**
	+ **FPNN.server.log.level**
	+ **FPNN.server.log.endpoint**
	+ **FPNN.server.log.route**

	**如需监听 IPv6 地址和端口**，亦请参考 [FPNN 标准配置模板](https://github.com/highras/fpnn/blob/master/doc/conf.template)

1. TableCache 专属配置

	+ **TableCache.cluster.endpointsSet.configFile**

		TableCache 集群成员地址列表文件路径。  
		列表文件每行一个 TableCache 服务的 endpoint。  
		endpoint 格式：host:port

	+ **TableCache.dbproxy.endpoint**

		TableCache 使用的 DBProxy 的地址。格式：host:port

	+ **TableCache.dbproxy.questTimeout**

		TableCache 访问 DBProxy 的超时时间。单位：秒

	+ **TableCache.cache.hashSize**

		指定 TableCache 的缓存表大小。可留空，自动使用默认值。


1. FPZK集群配置(**可选配置**)

	**未配置以下诸项时，TableCache 将不会向 FPZK 注册。**


	+ **TableCache.cluster.FPZK.serverList** 或 **FPZK.client.fpzkserver_list**

		FPZK 服务集群地址列表。**半角**逗号分隔。

	+ **TableCache.cluster.FPZK.projectName** 或 **FPZK.client.project_name**

		TableCache 从属的项目名称

	+ **TableCache.cluster.FPZK.projectToken** 或 **FPZK.client.project_token**

		TableCache 从属的项目 token

	+ **FPNN.server.cluster.name**

		TableCache 在 FPZK 中的业务分组名称。**可选配置**。

