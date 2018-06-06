# TableCache 管理工具

**管理工具目录**

| 管理工具 | 用途 |
|-----|------|
| Fetch | 查询缓存。 |
| invalidate | 清除缓存，或同时从数据库中删除数据。 |
| Modify | 修改缓存及**数据库**。 |


**所有工具空参数运行时，均会出现提示。提示格式为 BNF 范式。**

## Fetch

使用：

	./Fetch host:port <table> < -i | -s > <hintId> field1 [field2 ...]
	./Fetch host:port <table> < -mi | -ms > <hintIds> -f field1 [field2 ...]

参数：

+ -i 表示 只有一个 hindId，且为整型
+ -s 表示 只有一个 hindId，且为字符串类型
+ -mi 表示 有多个 hindId，且均为整型
+ -ms 表示 有多个 hindId，且均为字符串类型

例：

	./Fetch localhost:13520 demo_table -i 12345 field1
	./Fetch localhost:13520 demo_table -s abcde field1 field2
	./Fetch localhost:13520 demo_table -mi 123 456 789 -f field1 field2 field3
	./Fetch localhost:13520 demo_table -ms abc def ghi -f field1 field2 field3 field4


## invalidate

使用：

+ 清除缓存

		./invalidate host:port <table>

+ 删除特定条目的数据

		./invalidate host:port <table> < -i | -s > <hintId>


## Modify

使用：

	./Modify host:port <table> < -i | -s > <hintId> key1:value1 [key2:value2 ...]

一次只能修改一条数据。

参数：

+ -i 表示 只有一个 hindId，且为整型
+ -s 表示 只有一个 hindId，且为字符串类型
