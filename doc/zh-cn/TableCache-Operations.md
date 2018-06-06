# TableCache 运维管理

## 一、集群变动

1. TablaCache 集群变动，请修改配置项 TableCache.cluster.endpointsSet.configFile 指向的集群成员地址列表文件。

1. 保存集群成员地址列表文件的改动后，使用 FPNN 管理工具 [cmd](https://github.com/highras/fpnn/blob/master/doc/zh-cn/fpnn-tools.md) 向 TableCache 集群发送 refreshCluster 指令。

	refreshCluster 指令请参见 [TableCache Protocol](../../TableCache.protocol)