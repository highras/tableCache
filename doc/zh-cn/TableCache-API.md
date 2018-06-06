# TableCache 接口说明

## 一、接口标识说明

接口描述格式说明：

| 标识 | 说明 |
|-----|------|
| => | 请求 |
| <= | 回复 |
| ~ | oneway 请求(无回复) |
| ? | 可选参数 |
| [] | 列表 |
| {} | 字典 |
| % | 格式标识 |

参数格式说明：

| 标识 | 说明 |
|-----|------|
| b | 布尔型 |
| B | Blob 类型 |
| d, i | 整形 |
| f | 浮点型 |
| s | 字符串 |
| x | b, B, d, i, f, s 某种类型 |
| X | b, B, d, i, f, s, 列表, 字典 某种类型 |


**详细描述请参见 [FPNN 协议说明](https://github.com/highras/fpnn/blob/master/doc/zh-cn/fpnn-protocol-introduction.md)**

**本页所列仅为 TableCache 对业务用接口，完整接口协议请参见 [TableCache Protocol](../../TableCache.protocol)**

## 二、接口清单

| 接口名称 | 接口说明 |
|---------|---------|
| modify | 增加或者修改数据。 |
| fetch | 查询数据。 |
| delete | 从**集群缓存**和**数据库**删除数据。 |

## 三、接口明细

### modify

增加或者修改数据。

	=> modify { hintId:%?, table:%s, values:{%s:%s} }
	<= {}

* 参数说明

	+ **hintId**：DBProxy 使用的分表/分库参考值。一般为分表的分表键，或分段的分段键。为整型或者字符串类型。
	+ **table**：数据库中数据表的名字。
	+ **values**：增加或者修改的键值对。

* 注意

	hintId 无法修改，也**无需修改**。hintId 对应数据表中的 cloumn 由服务自动处理，不能出现在 values 中。



### fetch

查询数据。

	=> fetch { ?hintId:%?, ?hintIds:[%?], table:%s, fields:[%s], ?jsonCompatible:%b }
	<= { data:{%?:[%s] } }  //-- jsonCompatible:false
	<= { data:{%s:[%s] } }  //-- jsonCompatible:true

* 参数说明

	+ **hintId/hintIds**：DBProxy 使用的分表/分库参考值。一般为分表的分表键，或分段的分段键。为整型或者字符串类型。
	+ **table**：数据库中数据表的名字。
	+ **fields**：要查询的字段。
	+ **jsonCompatible**：返回的结果，是否采用 json 兼容的格式。默认为 false。

* 注意

	+ hintId 和 hintIds 必有一个，且只能有一个，类型为**整型**或者**字符串**。
	+ 返回对象的 data 为 hintId 为 key 的字典。字典的每项，以传入的fields的顺序为准。
	+ 如果 jsonCompatible 为 false，返回对象 data 的 key 的类型，取决于传入的 hintId 的类型。



### delete

从**集群缓存**和**数据库**删除数据。

	=> delete { hintId:%?, table:%s }
	<= {}

* 参数说明

	+ **hintId**：DBProxy 使用的分表/分库参考值。一般为分表的分表键，或分段的分段键。为整型或者字符串类型。
	+ **table**：数据库中数据表的名字。



## 四、错误代码

以上请求，如果发生错误，则会返回字典：`{ code:%d, ex:%s }`

错误代码为 FPNN 框架错误代码加上以下错误代码：

+ 100402: Query DBProxy Failed.
+ 100403: Disable operation.
+ 100404: Table is not found.

FPNN 错误代码请参见：[FPNN 错误代码](https://github.com/highras/fpnn/blob/master/doc/zh-cn/fpnn-error-code.md)