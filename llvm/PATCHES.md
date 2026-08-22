# 这棵 vendored 树被动过的地方

⭐ **规矩三条,与 `openkal-musl` 的一致:**

1. **可以动源码。** 移植就是动源码;假装不动只会把差异藏进别处。
2. **动过的地方要标注清楚。** 每一处都夹在
   `// ─── openkal ─── BEGIN` / `// ─── openkal ─── END` 之间,`grep` 一次能数完。
3. **能不动的就不动。** 上游能自己走对的路,让它走 —— 见下面的「不需要动的」。

```sh
# 数一遍
grep -rn "openkal ─── BEGIN" llvm/
```

---

## 判据:换的是「平台」,不是「库」

上游按**操作系统**选实现;openkal 的答案按**下面配置的是什么**。所以每一处替换
都必须能填进这句话:

> upstream 在这里问「这是哪个 OS」,而这个问题真正的答案是 openkal 的 `<接口>`。

⚠️ 填不进去的**不要动**。`sizeof(long)`、`int64_t` 的拼法、目标格式、调用约定
不是「下面是谁」,是**目标自己的定义** —— 那些归 C 库(`musl-generated/` 按
(arch, os) 分档),不归这里。

---

## 已替换

### `libcxx/src/atomic.cpp` — `std::atomic` 的等待与唤醒

上游一条链,五个分支,四个操作系统:

```
__linux__   → syscall(SYS_futex, …)
__FreeBSD__ → _umtx_op
__OpenBSD__ → futex()
_WIN32      → WaitOnAddress / WakeByAddressSingle
否则         → 完全不等待(自旋)
```

平台面**恰好两个函数**(`__platform_wait_on_address` / `__platform_wake_by_address`),
其余 495 行全是可移植的 —— 所以这里是**原地打标记的补丁**而不是整文件拷贝,
漂移面从 495 行缩到约 50 行。

openkal 的答案是 `kal_task_wait` / `kal_task_wake` —— 规范管它叫**挂起原语**,
而它在每一个目标上是同样的两个调用,**包括底下没有操作系统的那个**。

⚠️ **它本来就已经到 openkal 了,只是绕了一圈。** 在 Linux 上,上游那条分支发
`SYS_futex`,openkal-musl 的 port 拦下这个系统调用,它的 `__okm_futex` 调
`kal_task_wait`。直接走去掉了那一圈,并且让另外两种目标格式也能用 —— 在那里
「拦系统调用」没有系统调用可拦。

⚠️ 两处细节按上游的语义保留:两秒的默认超时(没有超时的等待注意不到与它竞争的
唤醒),以及 `-1` 表示「全部唤醒」(上游写作 `INT_MAX`,openkal 的计数是
`kal_uintptr`,所以按回绕写)。

---

## 不需要动的 —— 而这一节比上一节重要

⭐ 整棵树里 `#include <windows.h>` 共 **19 处**。按守卫分类:

| 守卫 | 处数 | 怎么解决的 |
|---|---|---|
| `_LIBCPP_WIN32API` | **12** | ✅ `port/include/__config` 撤回了它 → **自己落到 POSIX 分支** |
| `__SEH__` / `_LIBUNWIND_SUPPORT_DWARF_UNWIND` | 2 | ✅ `-fdwarf-exceptions`,异常机制跟随我们带的 unwinder |
| 裸 `_WIN32` | 3 | 1 处已换(见上),2 处待办(见下) |

⭐⭐ **12 处不用换,因为 libc++ 的 POSIX 分支本来就已经在 openkal 上了。** 它调
`fopen` / `clock_gettime` / `pthread_*`,那些是 musl,而 musl 就在 openkal 上。
让谓词答对,它自己就走到那条路。

⇒ 这就是为什么整份移植是几十行而不是把 libc++ 重做一遍。**先问「上游有没有一条
路已经通向 openkal」,再考虑换。**

---

## 待办

| 文件 | 平台面 | openkal 的答案 |
|---|---|---|
| `libunwind/src/RWMutex.hpp` | 读写锁 | `openkal.task`;或 `_LIBUNWIND_HAS_NO_THREADS`(unwinder 的锁只在缓存上) |
| `libunwind/src/AddressSpace.hpp` | 段查询的 Windows 分支 | 已由 DWARF 路线绕开,待确认没有残留 |

---

## 与 `port/include/` 的分工

| | 放哪 | 为什么 |
|---|---|---|
| **头文件**里的平台分派 | `port/include/` 覆盖 | 靠 include 顺序遮蔽,vendored 树逐字节不动,**不随上游漂移** |
| **源码**里的平台分派 | 本文件记录的原地标记 | 消费者不 include `.cpp`,遮蔽不了 |

⚠️ 顺序是有偏好的:**能用覆盖就别用补丁**。覆盖的漂移面是零,补丁的漂移面是被
标记的那几十行。
